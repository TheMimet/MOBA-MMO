import type { FastifyInstance, FastifyRequest } from "fastify";
import type { PrismaClient } from "../../../generated/prisma/index.js";
import {
  extractBearerToken,
  isOpsAdminSecretAccepted,
  isSessionServerSecretAccepted,
  verifyAuthToken,
} from "../auth/tokens.js";

export interface OpsAuditEventSnapshot {
  id: string;
  action: string;
  actorType: string;
  actorAccountId: string | null;
  actorUsername: string | null;
  status: string;
  target: string | null;
  message: string | null;
  metadata: Record<string, unknown> | null;
  occurredAt: string;
}

interface OpsAuditDetails {
  target?: string | null;
  message?: string | null;
  metadata?: Record<string, unknown> | null;
}

export interface OpsAuditQuery {
  action?: string;
  actor?: string;
  status?: string;
  target?: string;
  search?: string;
  limit?: number;
  offset?: number;
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

async function resolveActor(app: FastifyInstance, request: FastifyRequest) {
  if (isOpsAdminSecretAccepted(app, request)) {
    return {
      actorType: "ops-secret",
      actorAccountId: null,
      actorUsername: null,
    };
  }

  if (isSessionServerSecretAccepted(app, request)) {
    return {
      actorType: "session-server",
      actorAccountId: null,
      actorUsername: null,
    };
  }

  const bearerToken = extractBearerToken(request);
  const accountId = bearerToken ? verifyAuthToken(bearerToken, app.appConfig.authTokenSecret) : null;
  if (!accountId) {
    return {
      actorType: "anonymous",
      actorAccountId: null,
      actorUsername: null,
    };
  }

  const account = await app.prisma.account.findUnique({
    where: {
      id: accountId,
    },
    select: {
      id: true,
      username: true,
      role: true,
    },
  });

  return {
    actorType: account?.role === "admin" ? "admin-account" : "account",
    actorAccountId: account?.id ?? accountId,
    actorUsername: account?.username ?? null,
  };
}

export async function recordOpsAuditEvent(
  app: FastifyInstance,
  request: FastifyRequest,
  action: string,
  status: "accepted" | "rejected" | "failed",
  details: OpsAuditDetails = {},
): Promise<void> {
  try {
    const actor = await resolveActor(app, request);
    const opsAuditEvent = (app.prisma as any).opsAuditEvent;
    if (!opsAuditEvent) {
      return;
    }

    await opsAuditEvent.create({
      data: {
        action,
        actorType: actor.actorType,
        actorAccountId: actor.actorAccountId,
        actorUsername: actor.actorUsername,
        status,
        target: details.target ?? null,
        message: details.message ?? null,
        metadataJson: stringifyMetadata(details.metadata),
      },
    });
  } catch (error) {
    request.log.warn({ error, action, status }, "Ops audit event write failed");
  }
}

export async function recordSystemOpsAuditEvent(
  app: FastifyInstance,
  action: string,
  status: "accepted" | "rejected" | "failed",
  details: OpsAuditDetails = {},
): Promise<void> {
  try {
    const opsAuditEvent = (app.prisma as any).opsAuditEvent;
    if (!opsAuditEvent) {
      return;
    }

    await opsAuditEvent.create({
      data: {
        action,
        actorType: "system-worker",
        actorAccountId: null,
        actorUsername: null,
        status,
        target: details.target ?? null,
        message: details.message ?? null,
        metadataJson: stringifyMetadata(details.metadata),
      },
    });
  } catch (error) {
    app.log.warn({ error, action, status }, "System ops audit event write failed");
  }
}

function buildAuditWhere(query: OpsAuditQuery) {
  const andFilters: any[] = [];
  if (query.action) {
    andFilters.push({ action: { contains: query.action, mode: "insensitive" } });
  }
  if (query.actor) {
    andFilters.push({
      OR: [
        { actorType: { contains: query.actor, mode: "insensitive" } },
        { actorUsername: { contains: query.actor, mode: "insensitive" } },
        { actorAccountId: { contains: query.actor, mode: "insensitive" } },
      ],
    });
  }
  if (query.status) {
    andFilters.push({ status: { equals: query.status, mode: "insensitive" } });
  }
  if (query.target) {
    andFilters.push({ target: { contains: query.target, mode: "insensitive" } });
  }
  if (query.search) {
    andFilters.push({
      OR: [
        { action: { contains: query.search, mode: "insensitive" } },
        { actorType: { contains: query.search, mode: "insensitive" } },
        { actorUsername: { contains: query.search, mode: "insensitive" } },
        { target: { contains: query.search, mode: "insensitive" } },
        { message: { contains: query.search, mode: "insensitive" } },
      ],
    });
  }

  return andFilters.length > 0 ? { AND: andFilters } : {};
}

export async function getOpsAuditEvents(prisma: PrismaClient, query: OpsAuditQuery = {}): Promise<OpsAuditEventSnapshot[]> {
  const take = Math.min(Math.max(Math.trunc(query.limit ?? 25), 1), 500);
  const skip = Math.max(Math.trunc(query.offset ?? 0), 0);
  const opsAuditEvent = (prisma as any).opsAuditEvent;
  if (!opsAuditEvent) {
    return [];
  }

  const events = await opsAuditEvent.findMany({
    where: buildAuditWhere(query),
    orderBy: {
      occurredAt: "desc",
    },
    skip,
    take,
  });

  return events.map((event: any) => ({
    id: event.id,
    action: event.action,
    actorType: event.actorType,
    actorAccountId: event.actorAccountId,
    actorUsername: event.actorUsername,
    status: event.status,
    target: event.target,
    message: event.message,
    metadata: parseMetadata(event.metadataJson),
    occurredAt: event.occurredAt.toISOString(),
  }));
}

export async function getRecentOpsAuditEvents(prisma: PrismaClient, limit = 25): Promise<OpsAuditEventSnapshot[]> {
  return getOpsAuditEvents(prisma, { limit });
}

export async function pruneOpsAuditEvents(prisma: PrismaClient, retentionDays: number): Promise<{ deletedCount: number; cutoff: string }> {
  const opsAuditEvent = (prisma as any).opsAuditEvent;
  const cutoff = new Date(Date.now() - Math.max(1, retentionDays) * 24 * 60 * 60 * 1000);
  if (!opsAuditEvent) {
    return {
      deletedCount: 0,
      cutoff: cutoff.toISOString(),
    };
  }

  const result = await opsAuditEvent.deleteMany({
    where: {
      occurredAt: {
        lt: cutoff,
      },
    },
  });

  return {
    deletedCount: result.count ?? 0,
    cutoff: cutoff.toISOString(),
  };
}
