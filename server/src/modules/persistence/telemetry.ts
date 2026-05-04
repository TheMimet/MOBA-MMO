import type { PrismaClient } from "../../../generated/prisma/index.js";

export interface PersistenceTelemetrySnapshot {
  saveAccepted: number;
  staleIgnored: number;
  rejected: number;
  timedOut: number;
  reconnectAttempted: number;
  heartbeatAccepted: number;
  sessionStarted: number;
  sessionEnded: number;
  lastEventAt: string | null;
}

export interface PersistenceEventDetails {
  characterId?: string | null;
  sessionId?: string | null;
  statusCode?: number | null;
  errorCode?: string | null;
  message?: string | null;
  metadata?: Record<string, unknown> | null;
}

export interface PersistenceEventSnapshot {
  id: string;
  eventType: string;
  characterId: string | null;
  sessionId: string | null;
  statusCode: number | null;
  errorCode: string | null;
  message: string | null;
  metadata: Record<string, unknown> | null;
  occurredAt: string;
}

type PersistenceTelemetryEvent = keyof Omit<PersistenceTelemetrySnapshot, "lastEventAt">;

const TELEMETRY_ROW_ID = "global";

const counters: PersistenceTelemetrySnapshot = {
  saveAccepted: 0,
  staleIgnored: 0,
  rejected: 0,
  timedOut: 0,
  reconnectAttempted: 0,
  heartbeatAccepted: 0,
  sessionStarted: 0,
  sessionEnded: 0,
  lastEventAt: null,
};

let prismaClient: PrismaClient | null = null;
let persistQueue: Promise<void> = Promise.resolve();

function applySnapshot(snapshot: PersistenceTelemetrySnapshot): void {
  counters.saveAccepted = snapshot.saveAccepted;
  counters.staleIgnored = snapshot.staleIgnored;
  counters.rejected = snapshot.rejected;
  counters.timedOut = snapshot.timedOut;
  counters.reconnectAttempted = snapshot.reconnectAttempted;
  counters.heartbeatAccepted = snapshot.heartbeatAccepted;
  counters.sessionStarted = snapshot.sessionStarted;
  counters.sessionEnded = snapshot.sessionEnded;
  counters.lastEventAt = snapshot.lastEventAt;
}

function snapshotFromRecord(record: {
  saveAccepted: number;
  staleIgnored: number;
  rejected: number;
  timedOut: number;
  reconnectAttempted: number;
  heartbeatAccepted: number;
  sessionStarted: number;
  sessionEnded: number;
  lastEventAt: Date | null;
}): PersistenceTelemetrySnapshot {
  return {
    saveAccepted: record.saveAccepted,
    staleIgnored: record.staleIgnored,
    rejected: record.rejected,
    timedOut: record.timedOut,
    reconnectAttempted: record.reconnectAttempted,
    heartbeatAccepted: record.heartbeatAccepted,
    sessionStarted: record.sessionStarted,
    sessionEnded: record.sessionEnded,
    lastEventAt: record.lastEventAt?.toISOString() ?? null,
  };
}

async function readPersistedSnapshot(prisma: PrismaClient): Promise<PersistenceTelemetrySnapshot | null> {
  const record = await prisma.persistenceTelemetry.findUnique({
    where: {
      id: TELEMETRY_ROW_ID,
    },
  });

  return record ? snapshotFromRecord(record) : null;
}

function stringifyMetadata(metadata: Record<string, unknown> | null | undefined): string | null {
  if (!metadata) {
    return null;
  }

  try {
    return JSON.stringify(metadata);
  } catch {
    return JSON.stringify({ serializationError: true });
  }
}

function parseMetadata(metadataJson: string | null): Record<string, unknown> | null {
  if (!metadataJson) {
    return null;
  }

  try {
    const parsed = JSON.parse(metadataJson);
    return parsed && typeof parsed === "object" && !Array.isArray(parsed) ? parsed : null;
  } catch {
    return { parseError: true };
  }
}

async function persistEvent(event: PersistenceTelemetryEvent, occurredAt: Date, details: PersistenceEventDetails): Promise<void> {
  const prisma = prismaClient;
  if (!prisma) {
    return;
  }

  await prisma.$transaction([
    prisma.persistenceTelemetry.upsert({
      where: {
        id: TELEMETRY_ROW_ID,
      },
      update: {
        [event]: {
          increment: 1,
        },
        lastEventAt: occurredAt,
      },
      create: {
        id: TELEMETRY_ROW_ID,
        [event]: 1,
        lastEventAt: occurredAt,
      },
    }),
    prisma.persistenceEvent.create({
      data: {
        eventType: event,
        characterId: details.characterId ?? null,
        sessionId: details.sessionId ?? null,
        statusCode: details.statusCode ?? null,
        errorCode: details.errorCode ?? null,
        message: details.message ?? null,
        metadataJson: stringifyMetadata(details.metadata),
        occurredAt,
      },
    }),
  ]);
}

export async function configurePersistenceTelemetryStore(prisma: PrismaClient): Promise<void> {
  prismaClient = prisma;
  const persistedSnapshot = await readPersistedSnapshot(prisma);
  if (persistedSnapshot) {
    applySnapshot(persistedSnapshot);
  }
}

export function recordPersistenceEvent(event: PersistenceTelemetryEvent, details: PersistenceEventDetails = {}): void {
  const occurredAt = new Date();
  counters[event] += 1;
  counters.lastEventAt = occurredAt.toISOString();

  persistQueue = persistQueue
    .then(() => persistEvent(event, occurredAt, details))
    .catch(() => {
      // Keep persistence writes best-effort so gameplay requests are not blocked by ops telemetry.
    });
}

export async function getPersistenceTelemetrySnapshot(): Promise<PersistenceTelemetrySnapshot> {
  if (prismaClient) {
    const persistedSnapshot = await readPersistedSnapshot(prismaClient);
    if (persistedSnapshot) {
      applySnapshot(persistedSnapshot);
      return persistedSnapshot;
    }
  }

  return { ...counters };
}

export async function getRecentPersistenceEvents(limit = 50): Promise<PersistenceEventSnapshot[]> {
  if (!prismaClient) {
    return [];
  }

  const take = Math.min(Math.max(Math.trunc(limit), 1), 100);
  const events = await prismaClient.persistenceEvent.findMany({
    orderBy: {
      occurredAt: "desc",
    },
    take,
  });

  return events.map((event) => ({
    id: event.id,
    eventType: event.eventType,
    characterId: event.characterId,
    sessionId: event.sessionId,
    statusCode: event.statusCode,
    errorCode: event.errorCode,
    message: event.message,
    metadata: parseMetadata(event.metadataJson),
    occurredAt: event.occurredAt.toISOString(),
  }));
}

export async function resetPersistenceTelemetry(): Promise<void> {
  for (const key of Object.keys(counters) as Array<keyof PersistenceTelemetrySnapshot>) {
    if (key === "lastEventAt") {
      counters[key] = null;
    } else {
      counters[key] = 0;
    }
  }

  if (prismaClient) {
    await prismaClient.$transaction([
      prismaClient.persistenceTelemetry.upsert({
        where: {
          id: TELEMETRY_ROW_ID,
        },
        update: {
          saveAccepted: 0,
          staleIgnored: 0,
          rejected: 0,
          timedOut: 0,
          reconnectAttempted: 0,
          heartbeatAccepted: 0,
          sessionStarted: 0,
          sessionEnded: 0,
          lastEventAt: null,
        },
        create: {
          id: TELEMETRY_ROW_ID,
        },
      }),
      prismaClient.persistenceEvent.deleteMany(),
    ]);
  }
}
