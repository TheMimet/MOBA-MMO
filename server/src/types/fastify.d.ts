import "fastify";

import type { AppConfig } from "../config.js";
import type { PrismaClient } from "../../generated/prisma/index.js";

declare module "fastify" {
  interface FastifyInstance {
    appConfig: AppConfig;
    prisma: PrismaClient;
  }
}
