import { PrismaPg } from "@prisma/adapter-pg";
import fp from "fastify-plugin";
import type { FastifyPluginAsync } from "fastify";
import { PrismaClient } from "../../generated/prisma/index.js";
import { configurePersistenceTelemetryStore } from "../modules/persistence/telemetry.js";

export const registerPrismaPlugin: FastifyPluginAsync = fp(async (app) => {
  const adapter = new PrismaPg({ connectionString: app.appConfig.databaseUrl });
  const prisma = new PrismaClient({ adapter });

  app.decorate("prisma", prisma);
  await configurePersistenceTelemetryStore(prisma);

  app.addHook("onClose", async () => {
    await prisma.$disconnect();
  });
});
