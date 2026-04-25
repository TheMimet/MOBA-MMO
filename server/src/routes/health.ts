import type { FastifyPluginAsync } from "fastify";

import { requireSessionServerSecret } from "../modules/auth/tokens.js";
import { getPersistenceTelemetrySnapshot, resetPersistenceTelemetry } from "../modules/persistence/telemetry.js";

async function buildPersistenceOpsResponse(app: Parameters<FastifyPluginAsync>[0]) {
  return {
    persistenceTelemetry: await getPersistenceTelemetrySnapshot(),
    sessionReaper: {
      intervalSeconds: app.appConfig.sessionReaperIntervalSeconds,
      sessionTimeoutSeconds: app.appConfig.sessionTimeoutSeconds,
    },
  };
}

export const registerHealthRoutes: FastifyPluginAsync = async (app) => {
  app.get("/health", async () => {
    return {
      service: "moba-mmo-server",
      status: "ok",
      timestamp: new Date().toISOString(),
      persistenceTelemetry: await getPersistenceTelemetrySnapshot(),
    };
  });

  app.get("/ops/persistence", async () => {
    return buildPersistenceOpsResponse(app);
  });

  app.get("/ops/persistence/export", async (request, reply) => {
    if (!requireSessionServerSecret(app, request, reply)) {
      return;
    }

    const exportedAt = new Date().toISOString();
    reply.header("Content-Disposition", `attachment; filename="mobammo-persistence-telemetry-${exportedAt}.json"`);
    return {
      exportedAt,
      ...(await buildPersistenceOpsResponse(app)),
    };
  });

  app.post("/ops/persistence/reset", async (request, reply) => {
    if (!requireSessionServerSecret(app, request, reply)) {
      return;
    }

    await resetPersistenceTelemetry();
    return reply.code(202).send({
      resetAt: new Date().toISOString(),
      ...(await buildPersistenceOpsResponse(app)),
    });
  });
};
