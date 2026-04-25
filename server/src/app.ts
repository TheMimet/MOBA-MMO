import Fastify, { type FastifyInstance } from "fastify";

import { loadConfig } from "./config.js";
import { registerAuthRoutes } from "./modules/auth/routes.js";
import { registerCharacterRoutes } from "./modules/characters/routes.js";
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

  void app.register(registerPrismaPlugin);
  void app.register(registerHealthRoutes);
  void app.register(registerAuthRoutes, { prefix: "/auth" });
  void app.register(registerCharacterRoutes, { prefix: "/characters" });
  void app.register(registerSessionRoutes, { prefix: "/session" });
  void app.register(registerSessionReaper);

  return app;
}
