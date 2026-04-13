import type { FastifyPluginAsync } from "fastify";

interface CreateCharacterBody {
  accountId?: string;
  name?: string;
  classId?: string;
  presetId?: number;
  colorIndex?: number;
  shade?: number;
  transparent?: number;
  textureDetail?: number;
}

interface CharacterListQuery {
  accountId?: string;
}

interface SaveCharacterBody {
  position?: {
    x?: number;
    y?: number;
    z?: number;
  };
  hp?: number;
  level?: number;
}

export const registerCharacterRoutes: FastifyPluginAsync = async (app) => {
  app.get<{ Querystring: CharacterListQuery }>("/", async (request, reply) => {
    const accountId = request.query.accountId?.trim();

    if (!accountId) {
      return reply.code(400).send({
        error: "account_id_required",
        message: "Karakter listesini almak icin accountId gereklidir.",
      });
    }

    try {
      const items = await app.prisma.character.findMany({
        where: {
          accountId,
        },
        orderBy: {
          createdAt: "asc",
        },
        select: {
          id: true,
          name: true,
          classId: true,
          level: true,
        },
      });

      return { items };
    } catch (error) {
      request.log.error(error, "Character list request failed");

      return reply.code(503).send({
        error: "database_unavailable",
        message: "Karakter listesi su anda alinamiyor.",
      });
    }
  });

  app.post<{ Body: CreateCharacterBody }>("/", async (request, reply) => {
    const accountId = request.body?.accountId?.trim();
    const name = request.body?.name?.trim();
    const classId = request.body?.classId?.trim() ?? "fighter";

    if (!accountId) {
      return reply.code(400).send({
        error: "account_id_required",
        message: "Karakter olusturmak icin accountId alani zorunludur.",
      });
    }

    if (!name) {
      return reply.code(400).send({
        error: "name_required",
        message: "Karakter olusturmak icin name alani zorunludur.",
      });
    }

    try {
      const existingCharacter = await app.prisma.character.findUnique({
        where: {
          accountId_name: {
            accountId,
            name,
          },
        },
        select: {
          id: true,
          name: true,
          classId: true,
          level: true,
          presetId: true,
          colorIndex: true,
          shade: true,
          transparent: true,
          textureDetail: true,
        },
      });

      if (existingCharacter) {
        return reply.code(200).send(existingCharacter);
      }

      const character = await app.prisma.character.create({
        data: {
          accountId,
          classId,
          name,
          presetId: request.body.presetId ?? 4,
          colorIndex: request.body.colorIndex ?? 0,
          shade: request.body.shade ?? 58,
          transparent: request.body.transparent ?? 18,
          textureDetail: request.body.textureDetail ?? 88,
          stats: {
            create: {},
          },
        },
        select: {
          id: true,
          name: true,
          classId: true,
          level: true,
          presetId: true,
          colorIndex: true,
          shade: true,
          transparent: true,
          textureDetail: true,
        },
      });

      return reply.code(201).send(character);
    } catch (error) {
      request.log.error(error, "Character creation failed");

      return reply.code(503).send({
        error: "database_unavailable",
        message: "Karakter olusturma su anda kullanilamiyor.",
      });
    }
  });

  app.get<{ Params: { id: string } }>("/:id/load", async (request, reply) => {
    try {
      const character = await app.prisma.character.findUnique({
        where: {
          id: request.params.id,
        },
        include: {
          inventory: true,
          questStates: true,
          stats: true,
        },
      });

      if (!character) {
        return reply.code(404).send({
          error: "character_not_found",
          message: "Karakter bulunamadi.",
        });
      }

      return {
        character: {
          id: character.id,
          name: character.name,
          classId: character.classId,
          level: character.level,
          experience: character.experience,
          position: {
            x: character.positionX,
            y: character.positionY,
            z: character.positionZ,
          },
        },
        inventory: character.inventory,
        questStates: character.questStates,
        stats: character.stats,
      };
    } catch (error) {
      request.log.error(error, "Character load failed");

      return reply.code(503).send({
        error: "database_unavailable",
        message: "Karakter yukleme su anda kullanilamiyor.",
      });
    }
  });

  app.post<{ Body: SaveCharacterBody; Params: { id: string } }>("/:id/save", async (request, reply) => {
    try {
      const character = await app.prisma.character.findUnique({
        where: {
          id: request.params.id,
        },
        include: {
          stats: true,
        },
      });

      if (!character) {
        return reply.code(404).send({
          error: "character_not_found",
          message: "Kaydedilecek karakter bulunamadi.",
        });
      }

      const position = request.body?.position;
      const level = request.body?.level ?? character.level;
      const hp = request.body?.hp ?? character.stats?.hp ?? 100;

      await app.prisma.$transaction([
        app.prisma.character.update({
          where: {
            id: character.id,
          },
          data: {
            level,
            positionX: position?.x ?? character.positionX,
            positionY: position?.y ?? character.positionY,
            positionZ: position?.z ?? character.positionZ,
          },
        }),
        app.prisma.characterStats.upsert({
          where: {
            characterId: character.id,
          },
          update: {
            hp,
          },
          create: {
            characterId: character.id,
            hp,
          },
        }),
      ]);

      return reply.code(202).send({
        characterId: character.id,
        status: "saved",
      });
    } catch (error) {
      request.log.error(error, "Character save failed");

      return reply.code(503).send({
        error: "database_unavailable",
        message: "Karakter kaydi su anda kullanilamiyor.",
      });
    }
  });
};
