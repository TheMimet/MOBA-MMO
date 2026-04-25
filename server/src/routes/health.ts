import type { FastifyPluginAsync } from "fastify";

import { getPersistenceTelemetrySnapshot } from "../modules/persistence/telemetry.js";

export const registerHealthRoutes: FastifyPluginAsync = async (app) => {
  app.get("/health", async () => {
    return {
      service: "moba-mmo-server",
      status: "ok",
      timestamp: new Date().toISOString(),
      persistenceTelemetry: getPersistenceTelemetrySnapshot(),
    };
  });

  app.get("/ops/persistence", async () => {
    return {
      persistenceTelemetry: getPersistenceTelemetrySnapshot(),
      sessionReaper: {
        intervalSeconds: app.appConfig.sessionReaperIntervalSeconds,
        sessionTimeoutSeconds: app.appConfig.sessionTimeoutSeconds,
      },
    };
  });
};
