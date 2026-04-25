import type { FastifyInstance, FastifyReply, FastifyRequest } from "fastify";
import { createHmac, timingSafeEqual } from "node:crypto";

import { PERSISTENCE_ERROR } from "../persistence/contracts.js";
import { recordPersistenceEvent } from "../persistence/telemetry.js";

const TOKEN_VERSION = "v1";

interface AuthTokenPayload {
  sub: string;
  iat: number;
  exp: number;
}

export interface AuthPrincipal {
  kind: "account" | "session-server";
  accountId?: string;
}

function extractBearerToken(request: FastifyRequest): string | null {
  const header = request.headers.authorization;
  if (!header) {
    return null;
  }

  const value = Array.isArray(header) ? header[0] : header;
  const trimmed = value.trim();
  const bearerPrefix = "Bearer ";
  return trimmed.toLowerCase().startsWith(bearerPrefix.toLowerCase())
    ? trimmed.slice(bearerPrefix.length).trim()
    : trimmed;
}

function encodeBase64Url(value: string): string {
  return Buffer.from(value, "utf8").toString("base64url");
}

function decodeBase64Url(value: string): string | null {
  try {
    return Buffer.from(value, "base64url").toString("utf8");
  } catch {
    return null;
  }
}

function signTokenBody(body: string, secret: string): string {
  return createHmac("sha256", secret).update(body).digest("base64url");
}

function isSafeEqual(left: string, right: string): boolean {
  const leftBuffer = Buffer.from(left, "base64url");
  const rightBuffer = Buffer.from(right, "base64url");
  return leftBuffer.length === rightBuffer.length && timingSafeEqual(leftBuffer, rightBuffer);
}

function readHeaderValue(request: FastifyRequest, headerName: string): string {
  const header = request.headers[headerName.toLowerCase()];
  const value = Array.isArray(header) ? header[0] : header;
  return value?.trim() ?? "";
}

export function createAuthToken(accountId: string, secret: string, ttlSeconds: number, now = new Date()): string {
  const issuedAt = Math.floor(now.getTime() / 1000);
  const payload: AuthTokenPayload = {
    sub: accountId,
    iat: issuedAt,
    exp: issuedAt + Math.max(1, ttlSeconds),
  };
  const header = encodeBase64Url(JSON.stringify({ alg: "HS256", typ: "MOBAMMO", ver: TOKEN_VERSION }));
  const body = encodeBase64Url(JSON.stringify(payload));
  const signingInput = `${header}.${body}`;
  return `${signingInput}.${signTokenBody(signingInput, secret)}`;
}

export function verifyAuthToken(token: string, secret: string, now = new Date()): string | null {
  const parts = token.split(".");
  if (parts.length !== 3) {
    return null;
  }

  const [header, body, signature] = parts as [string, string, string];
  const expectedSignature = signTokenBody(`${header}.${body}`, secret);
  if (!isSafeEqual(signature, expectedSignature)) {
    return null;
  }

  const decodedHeader = decodeBase64Url(header);
  const decodedBody = decodeBase64Url(body);
  if (!decodedHeader || !decodedBody) {
    return null;
  }

  try {
    const parsedHeader = JSON.parse(decodedHeader) as { alg?: unknown; ver?: unknown };
    const payload = JSON.parse(decodedBody) as Partial<AuthTokenPayload>;
    const nowSeconds = Math.floor(now.getTime() / 1000);
    if (parsedHeader.alg !== "HS256" || parsedHeader.ver !== TOKEN_VERSION) {
      return null;
    }

    if (!payload.sub || typeof payload.sub !== "string" || typeof payload.exp !== "number" || payload.exp < nowSeconds) {
      return null;
    }

    return payload.sub;
  } catch {
    return null;
  }
}

export async function requireAuthenticatedAccountId(
  app: FastifyInstance,
  request: FastifyRequest,
  reply: FastifyReply,
): Promise<string | null> {
  const token = extractBearerToken(request);
  const accountId = token ? verifyAuthToken(token, app.appConfig.authTokenSecret) : null;

  if (!accountId) {
    recordPersistenceEvent("rejected");
    reply.code(401).send({
      error: PERSISTENCE_ERROR.AUTH_REQUIRED,
      message: "Bu islem icin Authorization bearer token gereklidir.",
    });
    return null;
  }

  const account = await app.prisma.account.findUnique({
    where: {
      id: accountId,
    },
    select: {
      id: true,
    },
  });

  if (!account) {
    recordPersistenceEvent("rejected");
    reply.code(401).send({
      error: PERSISTENCE_ERROR.AUTH_INVALID,
      message: "Authorization token gecersiz veya hesaba bagli degil.",
    });
    return null;
  }

  return account.id;
}

export function requireSessionServerSecret(
  app: FastifyInstance,
  request: FastifyRequest,
  reply: FastifyReply,
): boolean {
  const incomingSecret = readHeaderValue(request, "x-session-server-secret");
  const expectedSecret = app.appConfig.sessionServerSecret.trim();
  if (!incomingSecret || !expectedSecret || incomingSecret !== expectedSecret) {
    recordPersistenceEvent("rejected");
    reply.code(401).send({
      error: PERSISTENCE_ERROR.AUTH_REQUIRED,
      message: "Bu islem icin gecerli session server secret gereklidir.",
    });
    return false;
  }

  return true;
}

export async function requirePersistencePrincipal(
  app: FastifyInstance,
  request: FastifyRequest,
  reply: FastifyReply,
): Promise<AuthPrincipal | null> {
  if (readHeaderValue(request, "x-session-server-secret")) {
    return requireSessionServerSecret(app, request, reply) ? { kind: "session-server" } : null;
  }

  const accountId = await requireAuthenticatedAccountId(app, request, reply);
  return accountId ? { kind: "account", accountId } : null;
}

export function sendOwnershipForbidden(reply: FastifyReply): FastifyReply {
  recordPersistenceEvent("rejected");
  return reply.code(403).send({
    error: PERSISTENCE_ERROR.CHARACTER_OWNER_MISMATCH,
    message: "Bu karakter bu hesaba ait degil.",
  });
}
