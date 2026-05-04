import type { FastifyRequest } from "fastify";

interface LoginRateLimitEntry {
  attempts: number;
  resetAt: number;
}

interface LoginRateLimitOptions {
  maxAttempts: number;
  windowSeconds: number;
}

const loginAttempts = new Map<string, LoginRateLimitEntry>();

function normalizeRemoteAddress(request: FastifyRequest): string {
  const forwardedFor = request.headers["x-forwarded-for"];
  const forwardedValue = Array.isArray(forwardedFor) ? forwardedFor[0] : forwardedFor;
  const firstForwardedAddress = forwardedValue?.split(",")[0]?.trim();
  return firstForwardedAddress || request.ip || "unknown";
}

function buildLoginRateLimitKey(request: FastifyRequest, username: string): string {
  return `${normalizeRemoteAddress(request).toLowerCase()}::${username.trim().toLowerCase()}`;
}

function pruneExpiredEntries(now: number): void {
  if (loginAttempts.size < 1_000) {
    return;
  }

  for (const [key, entry] of loginAttempts.entries()) {
    if (entry.resetAt <= now) {
      loginAttempts.delete(key);
    }
  }
}

export function checkLoginRateLimit(
  request: FastifyRequest,
  username: string,
  options: LoginRateLimitOptions,
): { allowed: true } | { allowed: false; retryAfterSeconds: number } {
  const now = Date.now();
  pruneExpiredEntries(now);

  const key = buildLoginRateLimitKey(request, username);
  const entry = loginAttempts.get(key);
  if (!entry || entry.resetAt <= now) {
    return { allowed: true };
  }

  if (entry.attempts >= options.maxAttempts) {
    return {
      allowed: false,
      retryAfterSeconds: Math.max(1, Math.ceil((entry.resetAt - now) / 1000)),
    };
  }

  return { allowed: true };
}

export function recordFailedLoginAttempt(
  request: FastifyRequest,
  username: string,
  options: LoginRateLimitOptions,
): void {
  const now = Date.now();
  const key = buildLoginRateLimitKey(request, username);
  const currentEntry = loginAttempts.get(key);
  const resetAt = currentEntry && currentEntry.resetAt > now
    ? currentEntry.resetAt
    : now + options.windowSeconds * 1000;

  loginAttempts.set(key, {
    attempts: (currentEntry && currentEntry.resetAt > now ? currentEntry.attempts : 0) + 1,
    resetAt,
  });
}

export function clearLoginRateLimit(request: FastifyRequest, username: string): void {
  loginAttempts.delete(buildLoginRateLimitKey(request, username));
}
