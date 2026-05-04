import Fastify, { type FastifyInstance } from "fastify";

import { loadConfig } from "./config.js";
import { registerAuthRoutes } from "./modules/auth/routes.js";
import { registerPasswordResetDeliveryWorker } from "./modules/auth/password-reset-delivery-worker.js";
import { registerCharacterRoutes } from "./modules/characters/routes.js";
import { registerInventoryRoutes } from "./modules/inventory/routes.js";
import { registerOpsAuditRetention } from "./modules/ops/audit-retention.js";
import { registerPrismaPlugin } from "./plugins/prisma.js";
import { registerHealthRoutes } from "./routes/health.js";
import { registerSessionReaper } from "./modules/session/reaper.js";
import { registerSessionRoutes } from "./modules/session/routes.js";

export function buildApp(): FastifyInstance {
  const config = loadConfig();

  const app = Fastify({
    logger: {
      level: config.logLevel,
    },
  });

  app.decorate("appConfig", config);
  app.addHook("onRequest", async (request, reply) => {
    const origin = request.headers.origin;
    reply.header("Access-Control-Allow-Origin", origin ?? "*");
    reply.header("Vary", "Origin");
    reply.header("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
    reply.header(
      "Access-Control-Allow-Headers",
      "Authorization,Content-Type,X-Admin-Recovery-Secret,X-Ops-Admin-Secret,X-Password-Reset-Webhook-Secret,X-Session-Server-Secret",
    );
    reply.header("Access-Control-Max-Age", "600");
  });

  app.options("/*", async (_request, reply) => {
    return reply.code(204).send();
  });

  void app.register(registerPrismaPlugin);
  void app.register(registerHealthRoutes);
  void app.register(registerAuthRoutes, { prefix: "/auth" });
  void app.register(registerCharacterRoutes, { prefix: "/characters" });
  void app.register(registerInventoryRoutes, { prefix: "/characters" });
  void app.register(registerSessionRoutes, { prefix: "/session" });
  void app.register(registerSessionReaper);
  void app.register(registerOpsAuditRetention);
  void app.register(registerPasswordResetDeliveryWorker);

  return app;
}
