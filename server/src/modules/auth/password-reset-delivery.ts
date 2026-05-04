import { createCipheriv, createDecipheriv, createHash, randomBytes } from "node:crypto";

import type { FastifyBaseLogger, FastifyInstance } from "fastify";

export const PASSWORD_RESET_DELIVERY_STATUS = {
  DELIVERED: "delivered",
  FAILED: "failed",
  PENDING: "pending",
  SKIPPED: "skipped",
} as const;

export interface PasswordResetDeliveryWorkerSnapshot {
  runs: number;
  scanned: number;
  delivered: number;
  skipped: number;
  failed: number;
  lastRunAt: string | null;
  lastResult: {
    scanned: number;
    delivered: number;
    skipped: number;
    failed: number;
  } | null;
}

interface PasswordResetDeliveryTarget {
  id: string;
  username: string;
}

interface DeliveryAttemptResult {
  delivered: boolean;
  statusCode?: number;
  error?: string;
}

const workerSnapshot: PasswordResetDeliveryWorkerSnapshot = {
  runs: 0,
  scanned: 0,
  delivered: 0,
  skipped: 0,
  failed: 0,
  lastRunAt: null,
  lastResult: null,
};

function encryptionKey(secret: string): Buffer {
  return createHash("sha256").update(secret).digest();
}

function encryptResetToken(secret: string, resetToken: string): string {
  const iv = randomBytes(12);
  const cipher = createCipheriv("aes-256-gcm", encryptionKey(secret), iv);
  const encrypted = Buffer.concat([cipher.update(resetToken, "utf8"), cipher.final()]);
  const authTag = cipher.getAuthTag();

  return JSON.stringify({
    v: 1,
    alg: "aes-256-gcm",
    iv: iv.toString("base64"),
    tag: authTag.toString("base64"),
    data: encrypted.toString("base64"),
  });
}

function decryptResetToken(secret: string, encryptedTokenJson: string): string {
  const payload = JSON.parse(encryptedTokenJson) as {
    v: number;
    alg: string;
    iv: string;
    tag: string;
    data: string;
  };
  if (payload.v !== 1 || payload.alg !== "aes-256-gcm") {
    throw new Error("Unsupported password reset token encryption payload.");
  }

  const decipher = createDecipheriv("aes-256-gcm", encryptionKey(secret), Buffer.from(payload.iv, "base64"));
  decipher.setAuthTag(Buffer.from(payload.tag, "base64"));
  return Buffer.concat([
    decipher.update(Buffer.from(payload.data, "base64")),
    decipher.final(),
  ]).toString("utf8");
}

export async function deliverPasswordResetToken(
  app: FastifyInstance,
  log: FastifyBaseLogger,
  account: PasswordResetDeliveryTarget,
  resetToken: string,
  expiresAt: Date,
): Promise<DeliveryAttemptResult> {
  const provider = app.appConfig.authPasswordResetDeliveryProvider;
  if (provider === "response") {
    return { delivered: true };
  }

  if (provider === "log") {
    log.warn({
      accountId: account.id,
      username: account.username,
      resetToken,
      expiresAt: expiresAt.toISOString(),
    }, "Password reset token issued");
    return { delivered: true };
  }

  try {
    const response = await fetch(app.appConfig.authPasswordResetWebhookUrl, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        ...(app.appConfig.authPasswordResetWebhookSecret
          ? { "X-Password-Reset-Webhook-Secret": app.appConfig.authPasswordResetWebhookSecret }
          : {}),
      },
      body: JSON.stringify({
        event: "password_reset_requested",
        accountId: account.id,
        username: account.username,
        resetToken,
        expiresAt: expiresAt.toISOString(),
        expiresIn: app.appConfig.authPasswordResetTokenTtlSeconds,
      }),
    });

    if (!response.ok) {
      log.error({
        statusCode: response.status,
        statusText: response.statusText,
        accountId: account.id,
      }, "Password reset webhook delivery failed");
      return {
        delivered: false,
        statusCode: response.status,
        error: response.statusText,
      };
    }

    return {
      delivered: true,
      statusCode: response.status,
    };
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    log.error({ error, accountId: account.id }, "Password reset delivery failed");
    return {
      delivered: false,
      error: message,
    };
  }
}

function nextAttemptDate(attemptCount: number): Date {
  const delayMinutes = Math.min(60, Math.max(1, 2 ** Math.min(attemptCount, 5)));
  return new Date(Date.now() + delayMinutes * 60 * 1000);
}

export async function createPasswordResetDeliveryOutbox(
  app: FastifyInstance,
  input: {
    account: PasswordResetDeliveryTarget;
    resetTokenId: string;
    resetToken: string;
    expiresAt: Date;
    delivered: boolean;
    statusCode?: number | undefined;
    error?: string | undefined;
  },
): Promise<void> {
  const authPasswordResetDelivery = (app.prisma as any).authPasswordResetDelivery;
  if (!authPasswordResetDelivery) {
    return;
  }

  const status = input.delivered
    ? PASSWORD_RESET_DELIVERY_STATUS.DELIVERED
    : app.appConfig.authPasswordResetDeliveryProvider === "response"
      ? PASSWORD_RESET_DELIVERY_STATUS.SKIPPED
      : PASSWORD_RESET_DELIVERY_STATUS.FAILED;

  await authPasswordResetDelivery.create({
    data: {
      accountId: input.account.id,
      resetTokenId: input.resetTokenId,
      provider: app.appConfig.authPasswordResetDeliveryProvider,
      status,
      attemptCount: app.appConfig.authPasswordResetDeliveryProvider === "response" ? 0 : 1,
      encryptedTokenJson: encryptResetToken(app.appConfig.authTokenSecret, input.resetToken),
      lastStatusCode: input.statusCode ?? null,
      lastError: input.error ?? null,
      nextAttemptAt: status === PASSWORD_RESET_DELIVERY_STATUS.FAILED ? nextAttemptDate(1) : null,
      deliveredAt: input.delivered ? new Date() : null,
      expiresAt: input.expiresAt,
    },
  });
}

export async function retryPasswordResetDeliveryOutbox(
  app: FastifyInstance,
  log: FastifyBaseLogger,
  deliveryId: string,
): Promise<{ delivered: boolean; skipped: boolean; reason?: string | undefined }> {
  const authPasswordResetDelivery = (app.prisma as any).authPasswordResetDelivery;
  if (!authPasswordResetDelivery) {
    return { delivered: false, skipped: true, reason: "delegate_unavailable" };
  }

  const delivery = await authPasswordResetDelivery.findUnique({
    where: { id: deliveryId },
    include: {
      account: {
        select: {
          id: true,
          username: true,
        },
      },
      resetToken: {
        select: {
          consumedAt: true,
          expiresAt: true,
        },
      },
    },
  });

  if (!delivery) {
    return { delivered: false, skipped: true, reason: "not_found" };
  }

  if (delivery.status === PASSWORD_RESET_DELIVERY_STATUS.DELIVERED) {
    return { delivered: true, skipped: true, reason: "already_delivered" };
  }

  if (delivery.resetToken.consumedAt || delivery.resetToken.expiresAt.getTime() <= Date.now()) {
    await authPasswordResetDelivery.update({
      where: { id: deliveryId },
      data: {
        status: PASSWORD_RESET_DELIVERY_STATUS.SKIPPED,
        lastError: "reset_token_expired_or_consumed",
        nextAttemptAt: null,
      },
    });
    return { delivered: false, skipped: true, reason: "reset_token_expired_or_consumed" };
  }

  const resetToken = decryptResetToken(app.appConfig.authTokenSecret, delivery.encryptedTokenJson);
  const result = await deliverPasswordResetToken(app, log, delivery.account, resetToken, delivery.resetToken.expiresAt);
  const attemptCount = delivery.attemptCount + 1;
  await authPasswordResetDelivery.update({
    where: { id: deliveryId },
    data: {
      provider: app.appConfig.authPasswordResetDeliveryProvider,
      status: result.delivered ? PASSWORD_RESET_DELIVERY_STATUS.DELIVERED : PASSWORD_RESET_DELIVERY_STATUS.FAILED,
      attemptCount,
      lastStatusCode: result.statusCode ?? null,
      lastError: result.error ?? null,
      nextAttemptAt: result.delivered ? null : nextAttemptDate(attemptCount),
      deliveredAt: result.delivered ? new Date() : null,
    },
  });

  return { delivered: result.delivered, skipped: false, reason: result.error };
}

export async function retryDuePasswordResetDeliveries(
  app: FastifyInstance,
  log: FastifyBaseLogger,
  limit = 10,
): Promise<{ scanned: number; delivered: number; skipped: number; failed: number }> {
  const authPasswordResetDelivery = (app.prisma as any).authPasswordResetDelivery;
  if (!authPasswordResetDelivery) {
    return { scanned: 0, delivered: 0, skipped: 0, failed: 0 };
  }

  const now = new Date();
  const deliveries = await authPasswordResetDelivery.findMany({
    where: {
      status: {
        in: [PASSWORD_RESET_DELIVERY_STATUS.FAILED, PASSWORD_RESET_DELIVERY_STATUS.PENDING],
      },
      expiresAt: {
        gt: now,
      },
      OR: [
        { nextAttemptAt: null },
        { nextAttemptAt: { lte: now } },
      ],
    },
    orderBy: {
      createdAt: "asc",
    },
    take: Math.min(Math.max(Math.trunc(limit), 1), 50),
    select: {
      id: true,
    },
  });

  let delivered = 0;
  let skipped = 0;
  let failed = 0;
  for (const delivery of deliveries) {
    const result = await retryPasswordResetDeliveryOutbox(app, log, delivery.id);
    if (result.delivered) {
      delivered += 1;
    } else if (result.skipped) {
      skipped += 1;
    } else {
      failed += 1;
    }
  }

  return {
    scanned: deliveries.length,
    delivered,
    skipped,
    failed,
  };
}

export function recordPasswordResetDeliveryWorkerRun(result: {
  scanned: number;
  delivered: number;
  skipped: number;
  failed: number;
}): PasswordResetDeliveryWorkerSnapshot {
  workerSnapshot.runs += 1;
  workerSnapshot.scanned += result.scanned;
  workerSnapshot.delivered += result.delivered;
  workerSnapshot.skipped += result.skipped;
  workerSnapshot.failed += result.failed;
  workerSnapshot.lastRunAt = new Date().toISOString();
  workerSnapshot.lastResult = { ...result };

  return getPasswordResetDeliveryWorkerSnapshot();
}

export function getPasswordResetDeliveryWorkerSnapshot(): PasswordResetDeliveryWorkerSnapshot {
  return {
    runs: workerSnapshot.runs,
    scanned: workerSnapshot.scanned,
    delivered: workerSnapshot.delivered,
    skipped: workerSnapshot.skipped,
    failed: workerSnapshot.failed,
    lastRunAt: workerSnapshot.lastRunAt,
    lastResult: workerSnapshot.lastResult ? { ...workerSnapshot.lastResult } : null,
  };
}

export async function getPasswordResetDeliveryStatusCounts(app: FastifyInstance): Promise<Record<string, number>> {
  const authPasswordResetDelivery = (app.prisma as any).authPasswordResetDelivery;
  const counts: Record<string, number> = {
    delivered: 0,
    failed: 0,
    pending: 0,
    skipped: 0,
  };
  if (!authPasswordResetDelivery) {
    return counts;
  }

  const grouped = await authPasswordResetDelivery.groupBy({
    by: ["status"],
    _count: {
      _all: true,
    },
  });

  for (const item of grouped) {
    counts[item.status] = item._count._all;
  }

  return counts;
}

export async function prunePasswordResetDeliveryOutbox(
  app: FastifyInstance,
  retentionDays: number,
): Promise<{ deletedCount: number; cutoff: string }> {
  const authPasswordResetDelivery = (app.prisma as any).authPasswordResetDelivery;
  const cutoff = new Date(Date.now() - Math.max(1, retentionDays) * 24 * 60 * 60 * 1000);
  if (!authPasswordResetDelivery) {
    return {
      deletedCount: 0,
      cutoff: cutoff.toISOString(),
    };
  }

  const result = await authPasswordResetDelivery.deleteMany({
    where: {
      OR: [
        {
          status: {
            in: [PASSWORD_RESET_DELIVERY_STATUS.DELIVERED, PASSWORD_RESET_DELIVERY_STATUS.SKIPPED],
          },
          updatedAt: {
            lt: cutoff,
          },
        },
        {
          expiresAt: {
            lt: cutoff,
          },
        },
      ],
    },
  });

  return {
    deletedCount: result.count ?? 0,
    cutoff: cutoff.toISOString(),
  };
}
