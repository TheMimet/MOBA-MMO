import type { PrismaClient } from "../../../generated/prisma/index.js";

import { PERSISTENCE_ERROR, SAVE_RESPONSE_STATUS, SESSION_STATUS } from "../persistence/contracts.js";
import { recordPersistenceEvent } from "../persistence/telemetry.js";
import { DEFAULT_SESSION_TIMEOUT_SECONDS, getSessionTimeoutMs, isSessionExpired } from "../session/policy.js";
import { clampToArenaBounds, isInsideArenaBounds } from "../arena/bounds.js";

export interface SaveCharacterBody {
  sessionId?: string;
  position?: {
    x?: number;
    y?: number;
    z?: number;
  };
  hp?: number;
  maxHp?: number;
  mana?: number;
  maxMana?: number;
  level?: number;
  experience?: number;
  killCount?: number;
  deathCount?: number;
  saveSequence?: number;
}

interface SavePosition {
  x: number;
  y: number;
  z: number;
}

interface SaveOptions {
  endSession?: boolean;
  accountId?: string | undefined;
  sessionTimeoutMs?: number;
}

interface SaveResult {
  code: number;
  body: Record<string, unknown>;
}

const MAX_ABSOLUTE_WORLD_COORDINATE = 1_000_000;
const BASE_SAVE_MOVEMENT_ALLOWANCE = 20_000;
const SAVE_MOVEMENT_ALLOWANCE_PER_SECOND = 5_000;

function isFiniteNumber(value: unknown): value is number {
  return typeof value === "number" && Number.isFinite(value);
}

function resolveSavePosition(body: SaveCharacterBody, currentPosition: SavePosition): { error?: string; position: SavePosition } {
  const incomingPosition = body.position;
  if (!incomingPosition) {
    return { position: currentPosition };
  }

  for (const axis of ["x", "y", "z"] as const) {
    if (incomingPosition[axis] !== undefined && !isFiniteNumber(incomingPosition[axis])) {
      return {
        error: `position_${axis}_invalid`,
        position: currentPosition,
      };
    }
  }

  const position = {
    x: incomingPosition.x ?? currentPosition.x,
    y: incomingPosition.y ?? currentPosition.y,
    z: incomingPosition.z ?? currentPosition.z,
  };

  const isOutOfBounds = Object.values(position).some((value) => Math.abs(value) > MAX_ABSOLUTE_WORLD_COORDINATE);
  if (isOutOfBounds) {
    return {
      error: "position_out_of_bounds",
      position: currentPosition,
    };
  }

  return { position: clampToArenaBounds(position) };
}

function getDistance(from: SavePosition, to: SavePosition): number {
  return Math.hypot(to.x - from.x, to.y - from.y, to.z - from.z);
}

function getMovementAllowance(lastSavedAt: Date): number {
  const elapsedSeconds = Math.max(0, (Date.now() - lastSavedAt.getTime()) / 1000);
  return BASE_SAVE_MOVEMENT_ALLOWANCE + elapsedSeconds * SAVE_MOVEMENT_ALLOWANCE_PER_SECOND;
}

export async function saveCharacterState(
  prisma: PrismaClient,
  characterId: string,
  body: SaveCharacterBody,
  options: SaveOptions = {},
): Promise<SaveResult> {
  const character = await prisma.character.findUnique({
    where: {
      id: characterId,
    },
    include: {
      activeSession: true,
      stats: true,
    },
  });

  if (!character) {
    recordPersistenceEvent("rejected");
    return {
      code: 404,
      body: {
        error: "character_not_found",
        message: "Kaydedilecek karakter bulunamadi.",
      },
    };
  }

  if (options.accountId && character.accountId !== options.accountId) {
    recordPersistenceEvent("rejected");
    return {
      code: 403,
      body: {
        characterId: character.id,
        error: PERSISTENCE_ERROR.CHARACTER_OWNER_MISMATCH,
        message: "Bu karakter bu hesaba ait degil.",
      },
    };
  }

  const sessionId = body.sessionId?.trim();
  if (!sessionId) {
    recordPersistenceEvent("rejected");
    return {
      code: 401,
      body: {
        error: PERSISTENCE_ERROR.SESSION_ID_REQUIRED,
        message: "Karakter kaydi icin aktif sessionId gereklidir.",
      },
    };
  }

  if (
    !character.activeSession ||
    character.activeSession.id !== sessionId ||
    character.activeSession.status !== SESSION_STATUS.ACTIVE
  ) {
    recordPersistenceEvent("rejected");
    return {
      code: 409,
      body: {
        characterId: character.id,
        currentSessionId: character.activeSession?.id ?? null,
        incomingSessionId: sessionId,
        error: PERSISTENCE_ERROR.STALE_OR_INVALID_SESSION,
        message: "Bu karakter icin daha yeni bir aktif session var; eski session kaydi reddedildi.",
      },
    };
  }

  const sessionTimeoutMs = options.sessionTimeoutMs ?? getSessionTimeoutMs(DEFAULT_SESSION_TIMEOUT_SECONDS);
  const now = new Date();
  if (isSessionExpired(character.activeSession.updatedAt, now, sessionTimeoutMs)) {
    await prisma.characterSession.update({
      where: {
        characterId: character.id,
      },
      data: {
        status: SESSION_STATUS.TIMED_OUT,
      },
    });

    recordPersistenceEvent("timedOut");
    return {
      code: 409,
      body: {
        characterId: character.id,
        currentSessionId: character.activeSession.id,
        incomingSessionId: sessionId,
        sessionLastSeenAt: character.activeSession.updatedAt.toISOString(),
        sessionTimeoutSeconds: Math.round(sessionTimeoutMs / 1000),
        error: PERSISTENCE_ERROR.SESSION_EXPIRED,
        message: "Bu session cok uzun suredir heartbeat/save gondermedi; kayit reddedildi.",
      },
    };
  }

  const currentPosition = {
    x: character.positionX,
    y: character.positionY,
    z: character.positionZ,
  };
  const { error: positionError, position } = resolveSavePosition(body, currentPosition);
  if (positionError) {
    recordPersistenceEvent("rejected");
    return {
      code: 400,
      body: {
        characterId: character.id,
        error: positionError,
        message: "Karakter pozisyon kaydi gecersiz.",
      },
    };
  }

  const level = body.level ?? character.level;
  const experience = body.experience ?? character.experience;
  const hp = body.hp ?? character.stats?.hp ?? 100;
  const maxHp = body.maxHp ?? character.stats?.maxHp ?? 100;
  const mana = body.mana ?? character.stats?.mana ?? 50;
  const maxMana = body.maxMana ?? character.stats?.maxMana ?? 50;
  const killCount = body.killCount ?? character.stats?.killCount ?? 0;
  const deathCount = body.deathCount ?? character.stats?.deathCount ?? 0;
  const incomingSaveSequence = Number.isFinite(body.saveSequence)
    ? BigInt(Math.trunc(body.saveSequence ?? 0))
    : character.saveSequence + 1n;

  const existingDeathCount = character.stats?.deathCount ?? 0;
  const movementDistance = getDistance(currentPosition, position);
  const movementAllowance = getMovementAllowance(character.updatedAt);
  const isRespawnOrDeathSave = deathCount > existingDeathCount;
  const isArenaCorrectionSave = !isInsideArenaBounds(currentPosition) && isInsideArenaBounds(position);
  if (!isRespawnOrDeathSave && !isArenaCorrectionSave && movementDistance > movementAllowance) {
    recordPersistenceEvent("rejected");
    return {
      code: 422,
      body: {
        characterId: character.id,
        currentPosition,
        incomingPosition: position,
        movementAllowance: Math.round(movementAllowance),
        movementDistance: Math.round(movementDistance),
        error: "position_delta_too_large",
        message: "Karakter pozisyon kaydi son kayda gore supheli derecede uzak.",
      },
    };
  }

  if (incomingSaveSequence < character.saveSequence) {
    recordPersistenceEvent("staleIgnored");
    return {
      code: 202,
      body: {
        characterId: character.id,
        currentSaveSequence: character.saveSequence.toString(),
        incomingSaveSequence: incomingSaveSequence.toString(),
        status: SAVE_RESPONSE_STATUS.STALE_IGNORED,
      },
    };
  }

  const sessionStatus = options.endSession ? SESSION_STATUS.ENDED : SESSION_STATUS.ACTIVE;
  await prisma.$transaction([
    prisma.character.update({
      where: {
        id: character.id,
      },
      data: {
        level,
        experience,
        positionX: position.x,
        positionY: position.y,
        positionZ: position.z,
        saveSequence: incomingSaveSequence,
      },
    }),
    prisma.characterSession.update({
      where: {
        characterId: character.id,
      },
      data: {
        status: sessionStatus,
      },
    }),
    prisma.characterStats.upsert({
      where: {
        characterId: character.id,
      },
      update: {
        hp,
        maxHp,
        mana,
        maxMana,
        killCount,
        deathCount,
      },
      create: {
        characterId: character.id,
        hp,
        maxHp,
        mana,
        maxMana,
        killCount,
        deathCount,
      },
    }),
  ]);

  recordPersistenceEvent("saveAccepted");
  if (options.endSession) {
    recordPersistenceEvent("sessionEnded");
  }

  return {
    code: 202,
    body: {
      characterId: character.id,
      saveSequence: incomingSaveSequence.toString(),
      sessionId,
      status: options.endSession ? SAVE_RESPONSE_STATUS.ENDED : SAVE_RESPONSE_STATUS.SAVED,
    },
  };
}
