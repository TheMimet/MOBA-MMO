import type { FastifyPluginAsync } from "fastify";

interface SessionStartBody {
  characterId?: string;
}

export const registerSessionRoutes: FastifyPluginAsync = async (app) => {
  app.post<{ Body: SessionStartBody }>("/start", async (request, reply) => {
    const characterId = request.body?.characterId?.trim();

    if (!characterId) {
      return reply.code(400).send({
        error: "character_id_required",
        message: "Session baslatmak icin characterId gereklidir.",
      });
    }

    try {
      const character = await app.prisma.character.findUnique({
        where: {
          id: characterId,
        },
        select: {
          id: true,
          name: true,
          level: true,
        },
      });

      if (!character) {
        return reply.code(404).send({
          error: "character_not_found",
          message: "Session baslatmak icin gecerli bir karakter gereklidir.",
        });
      }

      const { sessionServerHost, sessionServerMap, sessionServerPort } = app.appConfig;

      return {
        sessionId: `session-${character.id}`,
        character: {
          id: character.id,
          name: character.name,
          level: character.level,
        },
        server: {
          host: sessionServerHost,
          port: sessionServerPort,
          map: sessionServerMap,
          connectString: `${sessionServerHost}:${sessionServerPort}`,
        },
      };
    } catch (error) {
      request.log.error(error, "Session start request failed");

      return reply.code(503).send({
        error: "database_unavailable",
        message: "Session baslatma su anda kullanilamiyor.",
      });
    }
  });
};
