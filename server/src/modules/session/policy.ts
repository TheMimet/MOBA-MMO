export const DEFAULT_SESSION_TIMEOUT_SECONDS = 120;

export function getSessionTimeoutMs(timeoutSeconds: number): number {
  return Math.max(1, timeoutSeconds) * 1000;
}

export function isSessionExpired(lastSeenAt: Date, now: Date, timeoutMs: number): boolean {
  return now.getTime() - lastSeenAt.getTime() > timeoutMs;
}
