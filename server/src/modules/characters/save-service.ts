import type { PrismaClient } from "../../../generated/prisma/index.js";

import { PERSISTENCE_ERROR, SAVE_RESPONSE_STATUS, SESSION_STATUS } from "../persistence/contracts.js";
import { recordPersistenceEvent } from "../persistence/telemetry.js";
import { DEFAULT_SESSION_TIMEOUT_SECONDS, getSessionTimeoutMs, isSessionExpired } from "../session/policy.js";
import { clampToArenaBounds, isInsideArenaBounds } from "../arena/bounds.js";
import { getItem } from "../inventory/itemDefinitions.js";

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
  gold?: number;
  saveSequence?: number;
  inventory?: Array<{
    itemId: string;
    quantity: number;
    slotIndex?: number | null;
  }>;
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

function normalizeInventoryForSave(body: SaveCharacterBody): Array<{ itemId: string; quantity: number; slotIndex: number | null }> | undefined {
  if (!body.inventory) {
    return undefined;
  }

  const equippedSlots = new Set<number>();
  const normalizedItems: Array<{ itemId: string; quantity: number; slotIndex: number | null }> = [];

  for (const item of body.inventory) {
    const itemId = typeof item.itemId === "string" ? item.itemId.trim() : "";
    const definition = getItem(itemId);
    if (!definition || !Number.isFinite(item.quantity)) {
      continue;
    }

    const quantity = Math.trunc(item.quantity);
    if (quantity < 1) {
      continue;
    }

    let slotIndex: number | null = null;
    if (typeof item.slotIndex === "number" && Number.isFinite(item.slotIndex)) {
      const incomingSlotIndex = Math.trunc(item.slotIndex);
      if (
        incomingSlotIndex >= 0 &&
        definition.equipSlot === incomingSlotIndex &&
        !equippedSlots.has(incomingSlotIndex)
      ) {
        slotIndex = incomingSlotIndex;
        equippedSlots.add(incomingSlotIndex);
      }
    }

    normalizedItems.push({
      itemId,
      quantity: Math.min(quantity, definition.maxStack),
      slotIndex,
    });
  }

  return normalizedItems;
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
    recordPersistenceEvent("rejected", {
      characterId,
      statusCode: 404,
      errorCode: "character_not_found",
      message: "Save rejected because character was not found.",
    });
    return {
      code: 404,
      body: {
        error: "character_not_found",
        message: "Kaydedilecek karakter bulunamadi.",
      },
    };
  }

  if (options.accountId && character.accountId !== options.accountId) {
    recordPersistenceEvent("rejected", {
      characterId: character.id,
      statusCode: 403,
      errorCode: PERSISTENCE_ERROR.CHARACTER_OWNER_MISMATCH,
      message: "Save rejected because account does not own character.",
    });
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
    recordPersistenceEvent("rejected", {
      characterId: character.id,
      statusCode: 401,
      errorCode: PERSISTENCE_ERROR.SESSION_ID_REQUIRED,
      message: "Save rejected because sessionId is missing.",
    });
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
    recordPersistenceEvent("rejected", {
      characterId: character.id,
      sessionId,
      statusCode: 409,
      errorCode: PERSISTENCE_ERROR.STALE_OR_INVALID_SESSION,
      message: "Save rejected because session is stale or invalid.",
      metadata: {
        currentSessionId: character.activeSession?.id ?? null,
        currentSessionStatus: character.activeSession?.status ?? null,
      },
    });
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

    recordPersistenceEvent("timedOut", {
      characterId: character.id,
      sessionId,
      statusCode: 409,
      errorCode: PERSISTENCE_ERROR.SESSION_EXPIRED,
      message: "Save rejected because session timed out.",
      metadata: {
        sessionLastSeenAt: character.activeSession.updatedAt.toISOString(),
        sessionTimeoutSeconds: Math.round(sessionTimeoutMs / 1000),
      },
    });
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
    recordPersistenceEvent("rejected", {
      characterId: character.id,
      sessionId,
      statusCode: 400,
      errorCode: positionError,
      message: "Save rejected because position is invalid.",
    });
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
  const gold = body.gold ?? character.stats?.gold ?? 0;
  const incomingSaveSequence = Number.isFinite(body.saveSequence)
    ? BigInt(Math.trunc(body.saveSequence ?? 0))
    : character.saveSequence + 1n;

  const existingDeathCount = character.stats?.deathCount ?? 0;
  const movementDistance = getDistance(currentPosition, position);
  const movementAllowance = getMovementAllowance(character.updatedAt);
  const isRespawnOrDeathSave = deathCount > existingDeathCount;
  const isArenaCorrectionSave = !isInsideArenaBounds(currentPosition) && isInsideArenaBounds(position);
  if (!isRespawnOrDeathSave && !isArenaCorrectionSave && movementDistance > movementAllowance) {
    recordPersistenceEvent("rejected", {
      characterId: character.id,
      sessionId,
      statusCode: 422,
      errorCode: "position_delta_too_large",
      message: "Save rejected because movement delta is too large.",
      metadata: {
        movementAllowance: Math.round(movementAllowance),
        movementDistance: Math.round(movementDistance),
      },
    });
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
    recordPersistenceEvent("staleIgnored", {
      characterId: character.id,
      sessionId,
      statusCode: 202,
      message: "Save ignored because a newer sequence is already stored.",
      metadata: {
        currentSaveSequence: character.saveSequence.toString(),
        incomingSaveSequence: incomingSaveSequence.toString(),
      },
    });
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
  const transactionOps: any[] = [
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
        gold,
      },
      create: {
        characterId: character.id,
        hp,
        maxHp,
        mana,
        maxMana,
        killCount,
        deathCount,
        gold,
      },
    }),
  ];

  const normalizedInventory = normalizeInventoryForSave(body);
  if (normalizedInventory) {
    transactionOps.push(
      prisma.inventoryItem.deleteMany({
        where: { characterId: character.id },
      })
    );
    
    if (normalizedInventory.length > 0) {
      transactionOps.push(
        prisma.inventoryItem.createMany({
          data: normalizedInventory.map((item) => ({
            characterId: character.id,
            itemId: item.itemId,
            quantity: item.quantity,
            slotIndex: item.slotIndex,
          })),
        })
      );
    }
  }

  await prisma.$transaction(transactionOps);

  recordPersistenceEvent("saveAccepted", {
    characterId: character.id,
    sessionId,
    statusCode: 202,
    message: options.endSession ? "Final save accepted." : "Save accepted.",
    metadata: {
      saveSequence: incomingSaveSequence.toString(),
      endedSession: options.endSession === true,
    },
  });
  if (options.endSession) {
    recordPersistenceEvent("sessionEnded", {
      characterId: character.id,
      sessionId,
      statusCode: 202,
      message: "Session ended after final save.",
    });
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
