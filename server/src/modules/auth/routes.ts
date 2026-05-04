import type { FastifyInstance, FastifyPluginAsync, FastifyReply } from "fastify";
import type { FastifyRequest } from "fastify";
import { timingSafeEqual } from "node:crypto";

import { hashPassword, isValidPassword, isValidUsername, normalizeUsername, verifyPassword } from "./passwords.js";
import {
  createPasswordResetDeliveryOutbox,
  deliverPasswordResetToken,
} from "./password-reset-delivery.js";
import { checkLoginRateLimit, clearLoginRateLimit, recordFailedLoginAttempt } from "./rate-limit.js";
import { createAuthToken, createRefreshToken, hashRefreshToken, requireAuthenticatedAccountId } from "./tokens.js";
import { recordOpsAuditEvent } from "../ops/audit.js";

interface LoginBody {
  username?: string;
  password?: string;
}

interface RefreshBody {
  refreshToken?: string;
}

interface RegisterBody extends LoginBody {}

interface PasswordResetRequestBody {
  username?: string;
}

interface PasswordResetConfirmBody {
  resetToken?: string;
  password?: string;
}

interface AdminPasswordResetIssueBody {
  username?: string;
}

interface AdminAccountRoleBody {
  username?: string;
  role?: string;
}

interface AccountTokenTarget {
  id: string;
  username: string;
  role: string;
}

interface LoginAccountRecord extends AccountTokenTarget {
  failedLoginCount: number;
  lockedUntil: Date | null;
  passwordHash: string | null;
}

function resolveConfiguredAccountRole(app: FastifyInstance, username: string, currentRole = "player"): string {
  return app.appConfig.authAdminUsernames.includes(normalizeUsername(username)) ? "admin" : currentRole;
}

function sendAuthUnavailable(reply: FastifyReply): FastifyReply {
  return reply.code(503).send({
    error: "database_unavailable",
    message: "Auth servisi su anda kullanilamiyor.",
  });
}

function sendInvalidCredentials(reply: FastifyReply): FastifyReply {
  return reply.code(401).send({
    error: "invalid_credentials",
    message: "Kullanici adi veya sifre hatali.",
  });
}

function sendRateLimited(reply: FastifyReply, retryAfterSeconds: number): FastifyReply {
  reply.header("Retry-After", retryAfterSeconds.toString());
  return reply.code(429).send({
    error: "rate_limited",
    retryAfterSeconds,
    message: "Cok fazla login denemesi yapildi. Kisa bir sure sonra tekrar deneyin.",
  });
}

function sendAccountLocked(reply: FastifyReply, lockedUntil: Date): FastifyReply {
  const retryAfterSeconds = Math.max(1, Math.ceil((lockedUntil.getTime() - Date.now()) / 1000));
  reply.header("Retry-After", retryAfterSeconds.toString());
  return reply.code(423).send({
    error: "account_locked",
    lockedUntil: lockedUntil.toISOString(),
    retryAfterSeconds,
    message: "Bu account gecici olarak kilitlendi. Lutfen daha sonra tekrar deneyin.",
  });
}

function readHeaderValue(request: FastifyRequest, headerName: string): string {
  const header = request.headers[headerName.toLowerCase()];
  const value = Array.isArray(header) ? header[0] : header;
  return value?.trim() ?? "";
}

function safeStringEquals(left: string, right: string): boolean {
  const leftBuffer = Buffer.from(left, "utf8");
  const rightBuffer = Buffer.from(right, "utf8");
  return leftBuffer.length === rightBuffer.length && timingSafeEqual(leftBuffer, rightBuffer);
}

function requireAdminRecoverySecret(app: FastifyInstance, request: FastifyRequest, reply: FastifyReply): boolean {
  const configuredSecret = app.appConfig.authAdminRecoverySecret.trim();
  const incomingSecret = readHeaderValue(request, "x-admin-recovery-secret");
  const isAccepted = Boolean(configuredSecret) && Boolean(incomingSecret) && safeStringEquals(incomingSecret, configuredSecret);
  if (!isAccepted) {
    reply.code(401).send({
      error: "admin_recovery_secret_required",
      message: "Bu islem icin gecerli admin recovery secret gereklidir.",
    });
    return false;
  }

  return true;
}

function getEffectiveFailedLoginCount(account: LoginAccountRecord): number {
  return account.lockedUntil && account.lockedUntil.getTime() <= Date.now() ? 0 : account.failedLoginCount;
}

async function recordInvalidLogin(app: FastifyInstance, account: LoginAccountRecord | null, now = new Date()): Promise<void> {
  if (!account) {
    return;
  }

  const failedLoginCount = getEffectiveFailedLoginCount(account) + 1;
  const shouldLock = failedLoginCount >= app.appConfig.authLoginLockoutThreshold;
  await app.prisma.account.update({
    where: {
      id: account.id,
    },
    data: {
      failedLoginCount,
      lastFailedLoginAt: now,
      lockedUntil: shouldLock ? new Date(now.getTime() + app.appConfig.authLoginLockoutSeconds * 1000) : null,
    },
  });
}

function isCurrentlyLocked(account: LoginAccountRecord): boolean {
  return Boolean(account.lockedUntil && account.lockedUntil.getTime() > Date.now());
}

function validateCredentialsInput(username: string, password: string | undefined, reply: FastifyReply): boolean {
  if (!isValidUsername(username)) {
    reply.code(400).send({
      error: "invalid_username",
      message: "Username 3-32 karakter olmali ve sadece harf, rakam, nokta, tire veya alt cizgi icermelidir.",
    });
    return false;
  }

  if (password !== undefined && !isValidPassword(password)) {
    reply.code(400).send({
      error: "invalid_password",
      message: "Sifre 6-128 karakter araliginda olmalidir.",
    });
    return false;
  }

  return true;
}

async function issueAuthSession(app: FastifyInstance, account: AccountTokenTarget) {
  const refreshToken = createRefreshToken();
  const refreshExpiresIn = app.appConfig.authRefreshTokenTtlSeconds;
  const expiresAt = new Date(Date.now() + refreshExpiresIn * 1000);
  const role = resolveConfiguredAccountRole(app, account.username, account.role);

  await app.prisma.$transaction([
    app.prisma.account.update({
      where: {
        id: account.id,
      },
      data: {
        role,
        lastLoginAt: new Date(),
        failedLoginCount: 0,
        lockedUntil: null,
        lastFailedLoginAt: null,
      },
    }),
    app.prisma.authRefreshToken.create({
      data: {
        accountId: account.id,
        tokenHash: hashRefreshToken(refreshToken),
        expiresAt,
      },
    }),
  ]);

  const accessToken = createAuthToken(account.id, app.appConfig.authTokenSecret, app.appConfig.authTokenTtlSeconds);
  return {
    accountId: account.id,
    token: accessToken,
    accessToken,
    tokenType: "Bearer",
    expiresIn: app.appConfig.authTokenTtlSeconds,
    refreshToken,
    refreshExpiresIn,
    username: account.username,
    role,
  };
}

export const registerAuthRoutes: FastifyPluginAsync = async (app) => {
  app.post<{ Body: RegisterBody }>("/register", async (request, reply) => {
    const username = normalizeUsername(request.body?.username);
    const password = request.body?.password ?? "";

    if (!validateCredentialsInput(username, password, reply)) {
      return;
    }

    try {
      const passwordHash = await hashPassword(password);
      const existingAccount = await app.prisma.account.findUnique({
        where: {
          username,
        },
        select: {
          id: true,
          username: true,
          role: true,
          passwordHash: true,
        },
      });

      if (existingAccount?.passwordHash) {
        return reply.code(409).send({
          error: "username_taken",
          message: "Bu username zaten kayitli.",
        });
      }

      const account = existingAccount
        ? await app.prisma.account.update({
            where: {
              id: existingAccount.id,
            },
            data: {
              passwordHash,
            },
            select: {
              id: true,
              username: true,
              role: true,
            },
          })
        : await app.prisma.account.create({
            data: {
              username,
              passwordHash,
            },
            select: {
              id: true,
              username: true,
              role: true,
            },
          });

      return reply.code(existingAccount ? 200 : 201).send(await issueAuthSession(app, account));
    } catch (error) {
      request.log.error(error, "Register request failed");
      return sendAuthUnavailable(reply);
    }
  });

  app.post<{ Body: LoginBody }>("/login", async (request, reply) => {
    const username = normalizeUsername(request.body?.username);
    const password = request.body?.password;

    if (!validateCredentialsInput(username, password, reply)) {
      return;
    }

    try {
      const rateLimit = checkLoginRateLimit(request, username, {
        maxAttempts: app.appConfig.authLoginRateLimitMaxAttempts,
        windowSeconds: app.appConfig.authLoginRateLimitWindowSeconds,
      });
      if (!rateLimit.allowed) {
        return sendRateLimited(reply, rateLimit.retryAfterSeconds);
      }

      if (!password) {
        if (!app.appConfig.allowLegacyUsernameLogin) {
          return reply.code(400).send({
            error: "password_required",
            message: "Login icin sifre gereklidir.",
          });
        }

        const account = await app.prisma.account.upsert({
          where: {
            username,
          },
          update: {},
          create: {
            username,
          },
          select: {
            id: true,
            username: true,
            role: true,
            passwordHash: true,
            failedLoginCount: true,
            lockedUntil: true,
          },
        });

        if (isCurrentlyLocked(account) && account.lockedUntil) {
          return sendAccountLocked(reply, account.lockedUntil);
        }

        clearLoginRateLimit(request, username);
        return issueAuthSession(app, account);
      }

      const account = await app.prisma.account.findUnique({
        where: {
          username,
        },
        select: {
          id: true,
          username: true,
          role: true,
          passwordHash: true,
          failedLoginCount: true,
          lockedUntil: true,
        },
      });

      if (!account) {
        recordFailedLoginAttempt(request, username, {
          maxAttempts: app.appConfig.authLoginRateLimitMaxAttempts,
          windowSeconds: app.appConfig.authLoginRateLimitWindowSeconds,
        });
        return sendInvalidCredentials(reply);
      }

      if (isCurrentlyLocked(account) && account.lockedUntil) {
        return sendAccountLocked(reply, account.lockedUntil);
      }

      if (!account.passwordHash) {
        if (!app.appConfig.allowLegacyUsernameLogin) {
          recordFailedLoginAttempt(request, username, {
            maxAttempts: app.appConfig.authLoginRateLimitMaxAttempts,
            windowSeconds: app.appConfig.authLoginRateLimitWindowSeconds,
          });
          await recordInvalidLogin(app, account);
          return sendInvalidCredentials(reply);
        }

        const claimedAccount = await app.prisma.account.update({
          where: {
            id: account.id,
          },
          data: {
            passwordHash: await hashPassword(password),
          },
          select: {
            id: true,
            username: true,
            role: true,
          },
        });

        clearLoginRateLimit(request, username);
        return issueAuthSession(app, claimedAccount);
      }

      if (!(await verifyPassword(password, account.passwordHash))) {
        recordFailedLoginAttempt(request, username, {
          maxAttempts: app.appConfig.authLoginRateLimitMaxAttempts,
          windowSeconds: app.appConfig.authLoginRateLimitWindowSeconds,
        });
        await recordInvalidLogin(app, account);
        return sendInvalidCredentials(reply);
      }

      clearLoginRateLimit(request, username);
      return issueAuthSession(app, account);
    } catch (error) {
      request.log.error(error, "Login request failed");
      return sendAuthUnavailable(reply);
    }
  });

  app.post<{ Body: RefreshBody }>("/refresh", async (request, reply) => {
    const refreshToken = request.body?.refreshToken?.trim();
    if (!refreshToken) {
      return reply.code(400).send({
        error: "refresh_token_required",
        message: "Refresh token gereklidir.",
      });
    }

    try {
      const storedToken = await app.prisma.authRefreshToken.findUnique({
        where: {
          tokenHash: hashRefreshToken(refreshToken),
        },
        include: {
          account: {
            select: {
              id: true,
              username: true,
              role: true,
            },
          },
        },
      });

      if (!storedToken || storedToken.revokedAt || storedToken.expiresAt.getTime() <= Date.now()) {
        return reply.code(401).send({
          error: "refresh_token_invalid",
          message: "Refresh token gecersiz veya suresi dolmus.",
        });
      }

      await app.prisma.authRefreshToken.update({
        where: {
          id: storedToken.id,
        },
        data: {
          revokedAt: new Date(),
        },
      });

      return issueAuthSession(app, storedToken.account);
    } catch (error) {
      request.log.error(error, "Refresh request failed");
      return sendAuthUnavailable(reply);
    }
  });

  app.post<{ Body: RefreshBody }>("/logout", async (request, reply) => {
    const refreshToken = request.body?.refreshToken?.trim();
    if (!refreshToken) {
      return reply.code(400).send({
        error: "refresh_token_required",
        message: "Logout icin refresh token gereklidir.",
      });
    }

    try {
      await app.prisma.authRefreshToken.updateMany({
        where: {
          tokenHash: hashRefreshToken(refreshToken),
          revokedAt: null,
        },
        data: {
          revokedAt: new Date(),
        },
      });

      return reply.code(202).send({
        status: "logged_out",
      });
    } catch (error) {
      request.log.error(error, "Logout request failed");
      return sendAuthUnavailable(reply);
    }
  });

  app.post<{ Body: PasswordResetRequestBody }>("/password-reset/request", async (request, reply) => {
    const username = normalizeUsername(request.body?.username);
    if (!isValidUsername(username)) {
      return reply.code(202).send({
        status: "password_reset_requested",
      });
    }

    try {
      const account = await app.prisma.account.findUnique({
        where: {
          username,
        },
        select: {
          id: true,
          username: true,
        },
      });

      if (!account) {
        return reply.code(202).send({
          status: "password_reset_requested",
        });
      }

      const resetToken = createRefreshToken();
      const expiresAt = new Date(Date.now() + app.appConfig.authPasswordResetTokenTtlSeconds * 1000);
      const storedResetToken = await app.prisma.authPasswordResetToken.create({
        data: {
          accountId: account.id,
          tokenHash: hashRefreshToken(resetToken),
          expiresAt,
        },
      });
      const delivery = await deliverPasswordResetToken(app, request.log, account, resetToken, expiresAt);
      await createPasswordResetDeliveryOutbox(app, {
        account,
        resetTokenId: storedResetToken.id,
        resetToken,
        expiresAt,
        delivered: delivery.delivered,
        statusCode: delivery.statusCode,
        error: delivery.error,
      });

      return reply.code(202).send({
        status: "password_reset_requested",
        expiresIn: app.appConfig.authPasswordResetTokenTtlSeconds,
        deliveryProvider: app.appConfig.authPasswordResetDeliveryProvider,
        delivered: delivery.delivered,
        resetToken: app.appConfig.authPasswordResetExposeToken || app.appConfig.authPasswordResetDeliveryProvider === "response"
          ? resetToken
          : undefined,
      });
    } catch (error) {
      request.log.error(error, "Password reset request failed");
      return sendAuthUnavailable(reply);
    }
  });

  app.post<{ Body: PasswordResetConfirmBody }>("/password-reset/confirm", async (request, reply) => {
    const resetToken = request.body?.resetToken?.trim();
    const password = request.body?.password ?? "";
    if (!resetToken) {
      return reply.code(400).send({
        error: "reset_token_required",
        message: "Password reset token gereklidir.",
      });
    }

    if (!isValidPassword(password)) {
      return reply.code(400).send({
        error: "invalid_password",
        message: "Sifre 6-128 karakter araliginda olmalidir.",
      });
    }

    try {
      const storedToken = await app.prisma.authPasswordResetToken.findUnique({
        where: {
          tokenHash: hashRefreshToken(resetToken),
        },
        include: {
          account: {
            select: {
              id: true,
              username: true,
            },
          },
        },
      });

      if (!storedToken || storedToken.consumedAt || storedToken.expiresAt.getTime() <= Date.now()) {
        return reply.code(401).send({
          error: "reset_token_invalid",
          message: "Password reset token gecersiz veya suresi dolmus.",
        });
      }

      await app.prisma.$transaction([
        app.prisma.account.update({
          where: {
            id: storedToken.accountId,
          },
          data: {
            passwordHash: await hashPassword(password),
            failedLoginCount: 0,
            lockedUntil: null,
            lastFailedLoginAt: null,
          },
        }),
        app.prisma.authPasswordResetToken.update({
          where: {
            id: storedToken.id,
          },
          data: {
            consumedAt: new Date(),
          },
        }),
        app.prisma.authRefreshToken.updateMany({
          where: {
            accountId: storedToken.accountId,
            revokedAt: null,
          },
          data: {
            revokedAt: new Date(),
          },
        }),
      ]);

      return {
        status: "password_reset_confirmed",
        username: storedToken.account.username,
      };
    } catch (error) {
      request.log.error(error, "Password reset confirm failed");
      return sendAuthUnavailable(reply);
    }
  });

  app.post<{ Body: AdminPasswordResetIssueBody }>("/admin/password-reset/issue-token", async (request, reply) => {
    if (!requireAdminRecoverySecret(app, request, reply)) {
      return;
    }

    const username = normalizeUsername(request.body?.username);
    if (!isValidUsername(username)) {
      return reply.code(400).send({
        error: "invalid_username",
        message: "Username 3-32 karakter olmali ve sadece harf, rakam, nokta, tire veya alt cizgi icermelidir.",
      });
    }

    try {
      const account = await app.prisma.account.findUnique({
        where: {
          username,
        },
        select: {
          id: true,
          username: true,
          role: true,
        },
      });

      if (!account) {
        return reply.code(404).send({
          error: "account_not_found",
          message: "Bu username icin account bulunamadi.",
        });
      }

      const resetToken = createRefreshToken();
      const expiresAt = new Date(Date.now() + app.appConfig.authPasswordResetTokenTtlSeconds * 1000);
      const issuedAt = new Date();
      await app.prisma.$transaction([
        app.prisma.authPasswordResetToken.updateMany({
          where: {
            accountId: account.id,
            consumedAt: null,
            expiresAt: {
              gt: issuedAt,
            },
          },
          data: {
            consumedAt: issuedAt,
          },
        }),
        app.prisma.authPasswordResetToken.create({
          data: {
            accountId: account.id,
            tokenHash: hashRefreshToken(resetToken),
            expiresAt,
          },
        }),
      ]);

      await recordOpsAuditEvent(app, request, "admin_password_reset_issue", "accepted", {
        target: account.username,
        message: "Admin password reset token issued.",
      });

      return reply.code(201).send({
        status: "admin_password_reset_token_issued",
        accountId: account.id,
        username: account.username,
        resetToken,
        expiresAt: expiresAt.toISOString(),
        expiresIn: app.appConfig.authPasswordResetTokenTtlSeconds,
      });
    } catch (error) {
      request.log.error(error, "Admin password reset token issue failed");
      return sendAuthUnavailable(reply);
    }
  });

  app.get("/me", async (request, reply) => {
    const accountId = await requireAuthenticatedAccountId(app, request, reply);
    if (!accountId) {
      return;
    }

    try {
      const account = await app.prisma.account.findUnique({
        where: {
          id: accountId,
        },
        select: {
          id: true,
          username: true,
          role: true,
          lastLoginAt: true,
          createdAt: true,
        },
      });

      if (!account) {
        return sendInvalidCredentials(reply);
      }

      return { account };
    } catch (error) {
      request.log.error(error, "Me request failed");
      return sendAuthUnavailable(reply);
    }
  });

  app.post<{ Body: AdminAccountRoleBody }>("/admin/accounts/role", async (request, reply) => {
    if (!requireAdminRecoverySecret(app, request, reply)) {
      return;
    }

    const username = normalizeUsername(request.body?.username);
    const role = request.body?.role?.trim().toLowerCase() ?? "";
    if (!isValidUsername(username)) {
      return reply.code(400).send({
        error: "invalid_username",
        message: "Username 3-32 karakter olmali ve sadece harf, rakam, nokta, tire veya alt cizgi icermelidir.",
      });
    }

    if (!["admin", "player"].includes(role)) {
      return reply.code(400).send({
        error: "invalid_role",
        message: "Role admin veya player olmali.",
      });
    }

    try {
      const account = await app.prisma.account.update({
        where: {
          username,
        },
        data: {
          role,
        },
        select: {
          id: true,
          username: true,
          role: true,
        },
      });

      await recordOpsAuditEvent(app, request, "admin_account_role_update", "accepted", {
        target: account.username,
        message: `Account role updated to ${account.role}.`,
      });

      return {
        status: "account_role_updated",
        account,
      };
    } catch (error) {
      request.log.error(error, "Admin account role update failed");
      return reply.code(404).send({
        error: "account_not_found",
        message: "Bu username icin account bulunamadi.",
      });
    }
  });
};
