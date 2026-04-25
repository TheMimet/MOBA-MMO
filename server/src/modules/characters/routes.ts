import type { FastifyPluginAsync } from "fastify";

import { requireAuthenticatedAccountId, requirePersistencePrincipal, sendOwnershipForbidden } from "../auth/tokens.js";
import { saveCharacterState, type SaveCharacterBody } from "./save-service.js";
import { serializeCharacterState } from "./serializers.js";
import { getSessionTimeoutMs } from "../session/policy.js";

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

export const registerCharacterRoutes: FastifyPluginAsync = async (app) => {
  app.get<{ Querystring: CharacterListQuery }>("/", async (request, reply) => {
    const authenticatedAccountId = await requireAuthenticatedAccountId(app, request, reply);
    if (!authenticatedAccountId) {
      return;
    }

    const accountId = request.query.accountId?.trim();

    if (!accountId) {
      return reply.code(400).send({
        error: "account_id_required",
        message: "Karakter listesini almak icin accountId gereklidir.",
      });
    }

    if (accountId !== authenticatedAccountId) {
      return sendOwnershipForbidden(reply);
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
    const authenticatedAccountId = await requireAuthenticatedAccountId(app, request, reply);
    if (!authenticatedAccountId) {
      return;
    }

    const accountId = request.body?.accountId?.trim();
    const name = request.body?.name?.trim();
    const classId = request.body?.classId?.trim() ?? "fighter";

    if (!accountId) {
      return reply.code(400).send({
        error: "account_id_required",
        message: "Karakter olusturmak icin accountId alani zorunludur.",
      });
    }

    if (accountId !== authenticatedAccountId) {
      return sendOwnershipForbidden(reply);
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
    const authenticatedAccountId = await requireAuthenticatedAccountId(app, request, reply);
    if (!authenticatedAccountId) {
      return;
    }

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

      if (character.accountId !== authenticatedAccountId) {
        return sendOwnershipForbidden(reply);
      }

      return serializeCharacterState(character);
    } catch (error) {
      request.log.error(error, "Character load failed");

      return reply.code(503).send({
        error: "database_unavailable",
        message: "Karakter yukleme su anda kullanilamiyor.",
      });
    }
  });

  app.post<{ Body: SaveCharacterBody; Params: { id: string } }>("/:id/save", async (request, reply) => {
    const principal = await requirePersistencePrincipal(app, request, reply);
    if (!principal) {
      return;
    }

    try {
      const result = await saveCharacterState(app.prisma, request.params.id, request.body ?? {}, {
        accountId: principal.kind === "account" ? principal.accountId : undefined,
        sessionTimeoutMs: getSessionTimeoutMs(app.appConfig.sessionTimeoutSeconds),
      });
      return reply.code(result.code).send(result.body);
    } catch (error) {
      request.log.error(error, "Character save failed");

      return reply.code(503).send({
        error: "database_unavailable",
        message: "Karakter kaydi su anda kullanilamiyor.",
      });
    }
  });
};
