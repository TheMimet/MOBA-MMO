import "dotenv/config";
import { buildApp } from "./app.js";

async function start(): Promise<void> {
  const app = buildApp();

  try {
    await app.listen({
      host: app.appConfig.host,
      port: app.appConfig.port,
    });
  } catch (error) {
    app.log.error(error, "Failed to start MOBA-MMO server API");
    process.exitCode = 1;
  }
}

void start();
