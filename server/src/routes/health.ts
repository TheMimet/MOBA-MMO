import type { FastifyPluginAsync } from "fastify";

export const registerHealthRoutes: FastifyPluginAsync = async (app) => {
  app.get("/health", async () => {
    return {
      service: "moba-mmo-server",
      status: "ok",
      timestamp: new Date().toISOString(),
    };
  });
};
