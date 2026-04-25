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

type PersistenceTelemetryEvent = keyof Omit<PersistenceTelemetrySnapshot, "lastEventAt">;

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

export function recordPersistenceEvent(event: PersistenceTelemetryEvent): void {
  counters[event] += 1;
  counters.lastEventAt = new Date().toISOString();
}

export function getPersistenceTelemetrySnapshot(): PersistenceTelemetrySnapshot {
  return {
    ...counters,
  };
}

export function resetPersistenceTelemetry(): void {
  for (const key of Object.keys(counters) as Array<keyof PersistenceTelemetrySnapshot>) {
    if (key === "lastEventAt") {
      counters[key] = null;
    } else {
      counters[key] = 0;
    }
  }
}
