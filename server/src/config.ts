import { DEFAULT_SESSION_TIMEOUT_SECONDS } from "./modules/session/policy.js";

export interface AppConfig {
  authTokenSecret: string;
  authTokenTtlSeconds: number;
  databaseUrl: string;
  host: string;
  logLevel: string;
  port: number;
  sessionServerHost: string;
  sessionServerPort: number;
  sessionServerMap: string;
  sessionReaperIntervalSeconds: number;
  sessionServerSecret: string;
  sessionTimeoutSeconds: number;
}

const DEFAULT_PORT = 3000;
const DEV_AUTH_TOKEN_SECRET = "dev-only-change-me-mobammo-auth-secret";
const DEV_SESSION_SERVER_SECRET = "dev-only-change-me-mobammo-session-server-secret";

function isProductionLikeEnvironment(): boolean {
  const environmentName = (process.env.APP_ENV ?? process.env.NODE_ENV ?? "development").trim().toLowerCase();
  return !["", "dev", "development", "local", "test"].includes(environmentName);
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

export function loadConfig(): AppConfig {
  const databaseUrl = process.env.DATABASE_URL;
  if (!databaseUrl) {
    throw new Error("DATABASE_URL is required");
  }

  const authTokenSecret = process.env.AUTH_TOKEN_SECRET ?? DEV_AUTH_TOKEN_SECRET;
  const sessionServerSecret = process.env.SESSION_SERVER_SECRET ?? DEV_SESSION_SERVER_SECRET;
  if (isProductionLikeEnvironment()) {
    if (authTokenSecret === DEV_AUTH_TOKEN_SECRET) {
      throw new Error("AUTH_TOKEN_SECRET must be set to a non-dev value outside local development");
    }

    if (sessionServerSecret === DEV_SESSION_SERVER_SECRET) {
      throw new Error("SESSION_SERVER_SECRET must be set to a non-dev value outside local development");
    }
  }

  return {
    authTokenSecret,
    authTokenTtlSeconds: parsePositiveInteger("AUTH_TOKEN_TTL_SECONDS", process.env.AUTH_TOKEN_TTL_SECONDS, 86_400),
    databaseUrl,
    host: process.env.HOST ?? "127.0.0.1",
    logLevel: process.env.LOG_LEVEL ?? "info",
    port: parsePort(process.env.PORT),
    sessionServerHost: process.env.SESSION_SERVER_HOST ?? "127.0.0.1",
    sessionServerPort: parseRequiredPort("SESSION_SERVER_PORT", process.env.SESSION_SERVER_PORT, 7777),
    sessionServerMap: process.env.SESSION_SERVER_MAP ?? "/Game/ThirdPerson/Lvl_ThirdPerson",
    sessionServerSecret,
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
