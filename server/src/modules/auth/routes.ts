import type { FastifyPluginAsync } from "fastify";

import { createAuthToken } from "./tokens.js";

interface LoginBody {
  username?: string;
}

export const registerAuthRoutes: FastifyPluginAsync = async (app) => {
  app.post<{ Body: LoginBody }>("/login", async (request, reply) => {
    const username = request.body?.username?.trim();

    if (!username) {
      return reply.code(400).send({
        error: "username_required",
        message: "Mock login icin username gonderilmelidir.",
      });
    }

    try {
      const account = await app.prisma.account.upsert({
        where: {
          username,
        },
        update: {},
        create: {
          username,
        },
      });

      return {
        accountId: account.id,
        token: createAuthToken(account.id, app.appConfig.authTokenSecret, app.appConfig.authTokenTtlSeconds),
        tokenType: "Bearer",
        expiresIn: app.appConfig.authTokenTtlSeconds,
        username: account.username,
      };
    } catch (error) {
      request.log.error(error, "Login request failed");

      return reply.code(503).send({
        error: "database_unavailable",
        message: "Veritabani baglantisi hazir degil.",
      });
    }
  });
};
