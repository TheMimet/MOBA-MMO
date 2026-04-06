export interface AppConfig {
  databaseUrl: string;
  host: string;
  logLevel: string;
  port: number;
  sessionServerHost: string;
  sessionServerPort: number;
  sessionServerMap: string;
}

const DEFAULT_PORT = 3000;

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

export function loadConfig(): AppConfig {
  const databaseUrl = process.env.DATABASE_URL;
  if (!databaseUrl) {
    throw new Error("DATABASE_URL is required");
  }

  return {
    databaseUrl,
    host: process.env.HOST ?? "127.0.0.1",
    logLevel: process.env.LOG_LEVEL ?? "info",
    port: parsePort(process.env.PORT),
    sessionServerHost: process.env.SESSION_SERVER_HOST ?? "127.0.0.1",
    sessionServerPort: parseRequiredPort("SESSION_SERVER_PORT", process.env.SESSION_SERVER_PORT, 7777),
    sessionServerMap: process.env.SESSION_SERVER_MAP ?? "/Game/ThirdPerson/Lvl_ThirdPerson",
  };
}
