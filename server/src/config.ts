import { DEFAULT_SESSION_TIMEOUT_SECONDS } from "./modules/session/policy.js";

export interface AppConfig {
  appEnvironment: string;
  appEnvironmentProductionLike: boolean;
  authAdminRecoverySecret: string;
  authAdminUsernames: string[];
  allowLegacyUsernameLogin: boolean;
  authLoginLockoutSeconds: number;
  authLoginLockoutThreshold: number;
  authLoginRateLimitMaxAttempts: number;
  authLoginRateLimitWindowSeconds: number;
  authPasswordResetExposeToken: boolean;
  authPasswordResetDeliveryProvider: "response" | "webhook" | "log";
  authPasswordResetDeliveryRetryIntervalSeconds: number;
  authPasswordResetDeliveryRetentionDays: number;
  authPasswordResetTokenTtlSeconds: number;
  authPasswordResetWebhookSecret: string;
  authPasswordResetWebhookUrl: string;
  authRefreshTokenTtlSeconds: number;
  authTokenSecret: string;
  authTokenTtlSeconds: number;
  databaseUrl: string;
  host: string;
  logLevel: string;
  opsAdminSecret: string;
  opsAuditPruneIntervalSeconds: number;
  opsAuditRetentionDays: number;
  opsRequireAdminSecret: boolean;
  port: number;
  sessionServerHost: string;
  sessionServerPort: number;
  sessionServerMap: string;
  sessionReaperIntervalSeconds: number;
  sessionServerSecret: string;
  previousSessionServerSecrets: string[];
  sessionTimeoutSeconds: number;
}

const DEFAULT_PORT = 3000;
const DEV_ADMIN_RECOVERY_SECRET = "dev-only-change-me-mobammo-admin-recovery-secret";
const DEV_AUTH_TOKEN_SECRET = "dev-only-change-me-mobammo-auth-secret";
const DEV_OPS_ADMIN_SECRET = "dev-only-change-me-mobammo-ops-admin-secret";
const DEV_SESSION_SERVER_SECRET = "dev-only-change-me-mobammo-session-server-secret";

function isProductionLikeEnvironment(): boolean {
  const environmentName = (process.env.APP_ENV ?? process.env.NODE_ENV ?? "development").trim().toLowerCase();
  return !["", "dev", "development", "local", "test"].includes(environmentName);
}

function parseBoolean(name: string, value: string | undefined, fallback: boolean): boolean {
  if (!value) {
    return fallback;
  }

  const normalized = value.trim().toLowerCase();
  if (["1", "true", "yes", "on"].includes(normalized)) {
    return true;
  }

  if (["0", "false", "no", "off"].includes(normalized)) {
    return false;
  }

  throw new Error(`Invalid ${name} value: "${value}"`);
}

function parsePort(value: string | undefined): number {
  if (!value) {
    return DEFAULT_PORT;
  }

  const port = Number.parseInt(value, 10);
  if (Number.isNaN(port)) {
    throw new Error(`Invalid PORT value: "${value}"`);
  }

  return port;
}

function parseRequiredPort(name: string, value: string | undefined, fallback: number): number {
  if (!value) {
    return fallback;
  }

  const port = Number.parseInt(value, 10);
  if (Number.isNaN(port)) {
    throw new Error(`Invalid ${name} value: "${value}"`);
  }

  return port;
}

function parsePositiveInteger(name: string, value: string | undefined, fallback: number): number {
  if (!value) {
    return fallback;
  }

  const parsedValue = Number.parseInt(value, 10);
  if (Number.isNaN(parsedValue) || parsedValue < 1) {
    throw new Error(`Invalid ${name} value: "${value}"`);
  }

  return parsedValue;
}

function parseNonNegativeInteger(name: string, value: string | undefined, fallback: number): number {
  if (!value) {
    return fallback;
  }

  const parsedValue = Number.parseInt(value, 10);
  if (Number.isNaN(parsedValue) || parsedValue < 0) {
    throw new Error(`Invalid ${name} value: "${value}"`);
  }

  return parsedValue;
}

function parseSecretList(value: string | undefined): string[] {
  if (!value) {
    return [];
  }

  return value
    .split(",")
    .map((secret) => secret.trim())
    .filter((secret) => secret.length > 0);
}

function parseStringList(value: string | undefined): string[] {
  if (!value) {
    return [];
  }

  return value
    .split(",")
    .map((item) => item.trim().toLowerCase())
    .filter((item) => item.length > 0);
}

export function loadConfig(): AppConfig {
  const databaseUrl = process.env.DATABASE_URL;
  if (!databaseUrl) {
    throw new Error("DATABASE_URL is required");
  }

  const authTokenSecret = process.env.AUTH_TOKEN_SECRET ?? DEV_AUTH_TOKEN_SECRET;
  const productionLike = isProductionLikeEnvironment();
  const authAdminRecoverySecret = process.env.AUTH_ADMIN_RECOVERY_SECRET ?? (productionLike ? "" : DEV_ADMIN_RECOVERY_SECRET);
  const authAdminUsernames = parseStringList(process.env.AUTH_ADMIN_USERNAMES);
  const opsAdminSecret = process.env.OPS_ADMIN_SECRET ?? (productionLike ? "" : DEV_OPS_ADMIN_SECRET);
  const opsRequireAdminSecret = parseBoolean("OPS_REQUIRE_ADMIN_SECRET", process.env.OPS_REQUIRE_ADMIN_SECRET, productionLike);
  const allowLegacyUsernameLogin = parseBoolean(
    "AUTH_ALLOW_LEGACY_USERNAME_LOGIN",
    process.env.AUTH_ALLOW_LEGACY_USERNAME_LOGIN,
    !productionLike,
  );
  const authPasswordResetExposeToken = parseBoolean(
    "AUTH_PASSWORD_RESET_EXPOSE_TOKEN",
    process.env.AUTH_PASSWORD_RESET_EXPOSE_TOKEN,
    !productionLike,
  );
  const authPasswordResetDeliveryProvider = (process.env.AUTH_PASSWORD_RESET_DELIVERY_PROVIDER ?? (productionLike ? "webhook" : "response")).trim().toLowerCase();
  if (!["response", "webhook", "log"].includes(authPasswordResetDeliveryProvider)) {
    throw new Error(`Invalid AUTH_PASSWORD_RESET_DELIVERY_PROVIDER value: "${authPasswordResetDeliveryProvider}"`);
  }
  const authPasswordResetWebhookUrl = process.env.AUTH_PASSWORD_RESET_WEBHOOK_URL?.trim() ?? "";
  const authPasswordResetWebhookSecret = process.env.AUTH_PASSWORD_RESET_WEBHOOK_SECRET?.trim() ?? "";
  const sessionServerSecret = process.env.SESSION_SERVER_SECRET ?? DEV_SESSION_SERVER_SECRET;
  const previousSessionServerSecrets = parseSecretList(process.env.SESSION_SERVER_SECRET_PREVIOUS);
  if (opsRequireAdminSecret && !opsAdminSecret) {
    throw new Error("OPS_ADMIN_SECRET must be set when OPS_REQUIRE_ADMIN_SECRET is enabled");
  }

  if (productionLike) {
    if (!authAdminRecoverySecret) {
      throw new Error("AUTH_ADMIN_RECOVERY_SECRET must be set outside local development");
    }

    if (authAdminRecoverySecret === DEV_ADMIN_RECOVERY_SECRET) {
      throw new Error("AUTH_ADMIN_RECOVERY_SECRET must be set to a non-dev value outside local development");
    }

    if (authTokenSecret === DEV_AUTH_TOKEN_SECRET) {
      throw new Error("AUTH_TOKEN_SECRET must be set to a non-dev value outside local development");
    }

    if (!opsAdminSecret) {
      throw new Error("OPS_ADMIN_SECRET must be set outside local development");
    }

    if (opsAdminSecret === DEV_OPS_ADMIN_SECRET) {
      throw new Error("OPS_ADMIN_SECRET must be set to a non-dev value outside local development");
    }

    if (!opsRequireAdminSecret) {
      throw new Error("OPS_REQUIRE_ADMIN_SECRET must not be disabled outside local development");
    }

    if (sessionServerSecret === DEV_SESSION_SERVER_SECRET) {
      throw new Error("SESSION_SERVER_SECRET must be set to a non-dev value outside local development");
    }

    if (previousSessionServerSecrets.includes(DEV_SESSION_SERVER_SECRET)) {
      throw new Error("SESSION_SERVER_SECRET_PREVIOUS must not contain the dev session server secret outside local development");
    }

    if (allowLegacyUsernameLogin) {
      throw new Error("AUTH_ALLOW_LEGACY_USERNAME_LOGIN must not be enabled outside local development");
    }

    if (authPasswordResetExposeToken) {
      throw new Error("AUTH_PASSWORD_RESET_EXPOSE_TOKEN must not be enabled outside local development");
    }

    if (authPasswordResetDeliveryProvider !== "webhook") {
      throw new Error("AUTH_PASSWORD_RESET_DELIVERY_PROVIDER must be webhook outside local development");
    }
  }

  if (authPasswordResetDeliveryProvider === "webhook" && !authPasswordResetWebhookUrl) {
    throw new Error("AUTH_PASSWORD_RESET_WEBHOOK_URL must be set when AUTH_PASSWORD_RESET_DELIVERY_PROVIDER=webhook");
  }

  return {
    appEnvironment: process.env.APP_ENV ?? process.env.NODE_ENV ?? "development",
    appEnvironmentProductionLike: productionLike,
    authAdminRecoverySecret,
    authAdminUsernames,
    allowLegacyUsernameLogin,
    authLoginLockoutSeconds: parsePositiveInteger("AUTH_LOGIN_LOCKOUT_SECONDS", process.env.AUTH_LOGIN_LOCKOUT_SECONDS, 900),
    authLoginLockoutThreshold: parsePositiveInteger("AUTH_LOGIN_LOCKOUT_THRESHOLD", process.env.AUTH_LOGIN_LOCKOUT_THRESHOLD, 5),
    authLoginRateLimitMaxAttempts: parsePositiveInteger("AUTH_LOGIN_RATE_LIMIT_MAX_ATTEMPTS", process.env.AUTH_LOGIN_RATE_LIMIT_MAX_ATTEMPTS, 20),
    authLoginRateLimitWindowSeconds: parsePositiveInteger("AUTH_LOGIN_RATE_LIMIT_WINDOW_SECONDS", process.env.AUTH_LOGIN_RATE_LIMIT_WINDOW_SECONDS, 300),
    authPasswordResetExposeToken,
    authPasswordResetDeliveryProvider: authPasswordResetDeliveryProvider as AppConfig["authPasswordResetDeliveryProvider"],
    authPasswordResetDeliveryRetryIntervalSeconds: parseNonNegativeInteger(
      "AUTH_PASSWORD_RESET_DELIVERY_RETRY_INTERVAL_SECONDS",
      process.env.AUTH_PASSWORD_RESET_DELIVERY_RETRY_INTERVAL_SECONDS,
      300,
    ),
    authPasswordResetDeliveryRetentionDays: parsePositiveInteger(
      "AUTH_PASSWORD_RESET_DELIVERY_RETENTION_DAYS",
      process.env.AUTH_PASSWORD_RESET_DELIVERY_RETENTION_DAYS,
      14,
    ),
    authPasswordResetTokenTtlSeconds: parsePositiveInteger("AUTH_PASSWORD_RESET_TOKEN_TTL_SECONDS", process.env.AUTH_PASSWORD_RESET_TOKEN_TTL_SECONDS, 1_800),
    authPasswordResetWebhookSecret,
    authPasswordResetWebhookUrl,
    authRefreshTokenTtlSeconds: parsePositiveInteger("AUTH_REFRESH_TOKEN_TTL_SECONDS", process.env.AUTH_REFRESH_TOKEN_TTL_SECONDS, 2_592_000),
    authTokenSecret,
    authTokenTtlSeconds: parsePositiveInteger("AUTH_TOKEN_TTL_SECONDS", process.env.AUTH_TOKEN_TTL_SECONDS, 86_400),
    databaseUrl,
    host: process.env.HOST ?? "127.0.0.1",
    logLevel: process.env.LOG_LEVEL ?? "info",
    opsAdminSecret,
    opsAuditPruneIntervalSeconds: parseNonNegativeInteger(
      "OPS_AUDIT_PRUNE_INTERVAL_SECONDS",
      process.env.OPS_AUDIT_PRUNE_INTERVAL_SECONDS,
      21_600,
    ),
    opsAuditRetentionDays: parsePositiveInteger("OPS_AUDIT_RETENTION_DAYS", process.env.OPS_AUDIT_RETENTION_DAYS, 90),
    opsRequireAdminSecret,
    port: parsePort(process.env.PORT),
    sessionServerHost: process.env.SESSION_SERVER_HOST ?? "127.0.0.1",
    sessionServerPort: parseRequiredPort("SESSION_SERVER_PORT", process.env.SESSION_SERVER_PORT, 7777),
    sessionServerMap: process.env.SESSION_SERVER_MAP ?? "/Game/ThirdPerson/Lvl_ThirdPerson",
    sessionServerSecret,
    previousSessionServerSecrets,
    sessionReaperIntervalSeconds: parseNonNegativeInteger(
      "SESSION_REAPER_INTERVAL_SECONDS",
      process.env.SESSION_REAPER_INTERVAL_SECONDS,
      30,
    ),
    sessionTimeoutSeconds: parsePositiveInteger(
      "SESSION_TIMEOUT_SECONDS",
      process.env.SESSION_TIMEOUT_SECONDS,
      DEFAULT_SESSION_TIMEOUT_SECONDS,
    ),
  };
}
