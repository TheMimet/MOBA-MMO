import type { FastifyPluginAsync } from "fastify";
import { randomUUID } from "node:crypto";

import { requireAuthenticatedAccountId, requirePersistencePrincipal, sendOwnershipForbidden } from "../auth/tokens.js";
import { saveCharacterState, type SaveCharacterBody } from "../characters/save-service.js";
import { PERSISTENCE_ERROR, SESSION_STATUS } from "../persistence/contracts.js";
import { recordPersistenceEvent } from "../persistence/telemetry.js";
import { serializeCharacterState } from "../characters/serializers.js";
import { getSessionTimeoutMs, isSessionExpired } from "./policy.js";
import { clampToArenaBounds, positionsMatch } from "../arena/bounds.js";

interface SessionStartBody {
  characterId?: string;
}

interface SessionEndBody extends SaveCharacterBody {
  characterId?: string;
}

interface SessionHeartbeatBody {
  characterId?: string;
  sessionId?: string;
}

export const registerSessionRoutes: FastifyPluginAsync = async (app) => {
  app.post<{ Body: SessionStartBody }>("/start", async (request, reply) => {
    const authenticatedAccountId = await requireAuthenticatedAccountId(app, request, reply);
    if (!authenticatedAccountId) {
      return;
    }

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
        include: {
          activeSession: true,
          inventory: true,
          questStates: true,
          stats: true,
        },
      });

      if (!character) {
        return reply.code(404).send({
          error: "character_not_found",
          message: "Session baslatmak icin gecerli bir karakter gereklidir.",
        });
      }

      if (character.accountId !== authenticatedAccountId) {
        return sendOwnershipForbidden(reply);
      }

      const { sessionServerHost, sessionServerMap, sessionServerPort, sessionTimeoutSeconds } = app.appConfig;
      const characterSnapshot = serializeCharacterState(character);
      const safePosition = clampToArenaBounds({
        x: character.positionX,
        y: character.positionY,
        z: character.positionZ,
      });
      const sessionId = `session-${character.id}-${randomUUID()}`;
      const previousSession = character.activeSession;
      const previousSessionTimedOut =
        previousSession?.status === SESSION_STATUS.TIMED_OUT ||
        (previousSession?.status === SESSION_STATUS.ACTIVE &&
          isSessionExpired(previousSession.updatedAt, new Date(), getSessionTimeoutMs(sessionTimeoutSeconds)));

      const sessionUpsert = app.prisma.characterSession.upsert({
        where: {
          characterId: character.id,
        },
        update: {
          id: sessionId,
          status: SESSION_STATUS.ACTIVE,
          startedAt: new Date(),
        },
        create: {
          id: sessionId,
          characterId: character.id,
          status: SESSION_STATUS.ACTIVE,
        },
      });

      if (
        positionsMatch(safePosition, {
          x: character.positionX,
          y: character.positionY,
          z: character.positionZ,
        })
      ) {
        await sessionUpsert;
      } else {
        await app.prisma.$transaction([
          app.prisma.character.update({
            where: {
              id: character.id,
            },
            data: {
              positionX: safePosition.x,
              positionY: safePosition.y,
              positionZ: safePosition.z,
            },
          }),
          sessionUpsert,
        ]);
      }

      recordPersistenceEvent("sessionStarted", {
        characterId: character.id,
        sessionId,
        statusCode: 200,
        message: "Session started.",
        metadata: {
          previousSessionId: previousSession?.id ?? null,
          previousSessionStatus: previousSession?.status ?? null,
        },
      });
      if (previousSessionTimedOut) {
        recordPersistenceEvent("reconnectAttempted", {
          characterId: character.id,
          sessionId,
          statusCode: 200,
          message: "New session started after previous session timed out.",
          metadata: {
            previousSessionId: previousSession.id,
            previousLastSeenAt: previousSession.updatedAt.toISOString(),
          },
        });
      }

      return {
        sessionId,
        character: {
          id: character.id,
          name: character.name,
          classId: character.classId,
          level: character.level,
          experience: character.experience,
          position: characterSnapshot.character.position,
          appearance: characterSnapshot.character.appearance,
        },
        server: {
          host: sessionServerHost,
          port: sessionServerPort,
          map: sessionServerMap,
          connectString: `${sessionServerHost}:${sessionServerPort}`,
        },
        characterSnapshot,
        previousSession: previousSessionTimedOut
          ? {
              id: previousSession.id,
              status: SESSION_STATUS.TIMED_OUT,
              lastSeenAt: previousSession.updatedAt.toISOString(),
            }
          : null,
      };
    } catch (error) {
      request.log.error(error, "Session start request failed");

      return reply.code(503).send({
        error: "database_unavailable",
        message: "Session baslatma su anda kullanilamiyor.",
      });
    }
  });

  app.post<{ Body: SessionEndBody }>("/end", async (request, reply) => {
    const principal = await requirePersistencePrincipal(app, request, reply);
    if (!principal) {
      return;
    }

    const characterId = request.body?.characterId?.trim();

    if (!characterId) {
      return reply.code(400).send({
        error: "character_id_required",
        message: "Session kapatmak icin characterId gereklidir.",
      });
    }

    try {
      const result = await saveCharacterState(app.prisma, characterId, request.body ?? {}, {
        accountId: principal.kind === "account" ? principal.accountId : undefined,
        endSession: true,
        sessionTimeoutMs: getSessionTimeoutMs(app.appConfig.sessionTimeoutSeconds),
      });
      return reply.code(result.code).send(result.body);
    } catch (error) {
      request.log.error(error, "Session end request failed");

      return reply.code(503).send({
        error: "database_unavailable",
        message: "Session kapatma su anda kullanilamiyor.",
      });
    }
  });

  app.post<{ Body: SessionHeartbeatBody }>("/heartbeat", async (request, reply) => {
    const principal = await requirePersistencePrincipal(app, request, reply);
    if (!principal) {
      return;
    }

    const characterId = request.body?.characterId?.trim();
    const sessionId = request.body?.sessionId?.trim();

    if (!characterId || !sessionId) {
      recordPersistenceEvent("rejected", {
        characterId: characterId ?? null,
        sessionId: sessionId ?? null,
        statusCode: 400,
        errorCode: "session_identity_required",
        message: "Heartbeat rejected because characterId or sessionId is missing.",
      });
      return reply.code(400).send({
        error: "session_identity_required",
        message: "Heartbeat icin characterId ve sessionId gereklidir.",
      });
    }

    try {
      const character = await app.prisma.character.findUnique({
        where: {
          id: characterId,
        },
        include: {
          activeSession: true,
        },
      });

      if (!character) {
        recordPersistenceEvent("rejected", {
          characterId,
          sessionId,
          statusCode: 404,
          errorCode: "character_not_found",
          message: "Heartbeat rejected because character was not found.",
        });
        return reply.code(404).send({
          error: "character_not_found",
          message: "Heartbeat icin gecerli bir karakter gereklidir.",
        });
      }

      if (principal.kind === "account" && character.accountId !== principal.accountId) {
        return sendOwnershipForbidden(reply);
      }

      if (character.activeSession?.id === sessionId && character.activeSession.status === SESSION_STATUS.TIMED_OUT) {
        recordPersistenceEvent("rejected", {
          characterId: character.id,
          sessionId,
          statusCode: 409,
          errorCode: PERSISTENCE_ERROR.SESSION_EXPIRED,
          message: "Heartbeat rejected because session is already timed out.",
          metadata: {
            sessionLastSeenAt: character.activeSession.updatedAt.toISOString(),
          },
        });
        return reply.code(409).send({
          characterId: character.id,
          currentSessionId: character.activeSession.id,
          incomingSessionId: sessionId,
          sessionLastSeenAt: character.activeSession.updatedAt.toISOString(),
          sessionTimeoutSeconds: app.appConfig.sessionTimeoutSeconds,
          error: PERSISTENCE_ERROR.SESSION_EXPIRED,
          message: "Bu session heartbeat timeout suresini asti.",
        });
      }

      if (
        !character.activeSession ||
        character.activeSession.id !== sessionId ||
        character.activeSession.status !== SESSION_STATUS.ACTIVE
      ) {
        recordPersistenceEvent("rejected", {
          characterId: character.id,
          sessionId,
          statusCode: 409,
          errorCode: PERSISTENCE_ERROR.STALE_OR_INVALID_SESSION,
          message: "Heartbeat rejected because session is stale or invalid.",
          metadata: {
            currentSessionId: character.activeSession?.id ?? null,
            currentSessionStatus: character.activeSession?.status ?? null,
          },
        });
        return reply.code(409).send({
          characterId: character.id,
          currentSessionId: character.activeSession?.id ?? null,
          incomingSessionId: sessionId,
          error: PERSISTENCE_ERROR.STALE_OR_INVALID_SESSION,
          message: "Heartbeat eski veya gecersiz session icin reddedildi.",
        });
      }

      const sessionTimeoutMs = getSessionTimeoutMs(app.appConfig.sessionTimeoutSeconds);
      if (isSessionExpired(character.activeSession.updatedAt, new Date(), sessionTimeoutMs)) {
        await app.prisma.characterSession.update({
          where: {
            characterId: character.id,
          },
          data: {
            status: SESSION_STATUS.TIMED_OUT,
          },
        });

        recordPersistenceEvent("timedOut", {
          characterId: character.id,
          sessionId,
          statusCode: 409,
          errorCode: PERSISTENCE_ERROR.SESSION_EXPIRED,
          message: "Heartbeat timed out the active session.",
          metadata: {
            sessionLastSeenAt: character.activeSession.updatedAt.toISOString(),
            sessionTimeoutSeconds: app.appConfig.sessionTimeoutSeconds,
          },
        });
        return reply.code(409).send({
          characterId: character.id,
          currentSessionId: character.activeSession.id,
          incomingSessionId: sessionId,
          sessionLastSeenAt: character.activeSession.updatedAt.toISOString(),
          sessionTimeoutSeconds: app.appConfig.sessionTimeoutSeconds,
          error: PERSISTENCE_ERROR.SESSION_EXPIRED,
          message: "Bu session heartbeat timeout suresini asti.",
        });
      }

      await app.prisma.characterSession.update({
        where: {
          characterId: character.id,
        },
        data: {
          status: SESSION_STATUS.ACTIVE,
        },
      });

      recordPersistenceEvent("heartbeatAccepted", {
        characterId: character.id,
        sessionId,
        statusCode: 202,
        message: "Heartbeat accepted.",
      });
      return reply.code(202).send({
        characterId: character.id,
        sessionId,
        status: SESSION_STATUS.ACTIVE,
      });
    } catch (error) {
      request.log.error(error, "Session heartbeat request failed");

      return reply.code(503).send({
        error: "database_unavailable",
        message: "Session heartbeat su anda kullanilamiyor.",
      });
    }
  });
};
