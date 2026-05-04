import type { FastifyPluginAsync } from "fastify";

import {
  getPasswordResetDeliveryStatusCounts,
  getPasswordResetDeliveryWorkerSnapshot,
  prunePasswordResetDeliveryOutbox,
  retryPasswordResetDeliveryOutbox,
} from "../modules/auth/password-reset-delivery.js";
import { isOpsAdminAccessAccepted, requireOpsAdminSecret, requireOpsMutationSecret } from "../modules/auth/tokens.js";
import { getOpsAuditEvents, getRecentOpsAuditEvents, pruneOpsAuditEvents, recordOpsAuditEvent } from "../modules/ops/audit.js";
import {
  type PersistenceEventSnapshot,
  type PersistenceTelemetrySnapshot,
  getPersistenceTelemetrySnapshot,
  getRecentPersistenceEvents,
  resetPersistenceTelemetry,
} from "../modules/persistence/telemetry.js";

interface PersistenceEventsQuery {
  limit?: string;
}

interface OpsAuditEventsQuery {
  action?: string;
  actor?: string;
  status?: string;
  target?: string;
  search?: string;
  limit?: string;
  offset?: string;
}

interface PasswordResetDeliveryQuery {
  status?: string;
  limit?: string;
  offset?: string;
}

interface PasswordResetDeliveryRetryParams {
  id: string;
}

interface PasswordResetProviderTestBody {
  dryRun?: boolean;
}

function parseEventLimit(value: string | undefined): number {
  if (!value) {
    return 50;
  }

  const parsed = Number.parseInt(value, 10);
  return Number.isFinite(parsed) ? Math.min(Math.max(parsed, 1), 100) : 50;
}

function parseAuditQuery(query: OpsAuditEventsQuery) {
  const parsed: {
    action?: string;
    actor?: string;
    status?: string;
    target?: string;
    search?: string;
    limit: number;
    offset: number;
  } = {
    limit: parseEventLimit(query.limit),
    offset: Math.max(Number.parseInt(query.offset ?? "0", 10) || 0, 0),
  };

  const action = query.action?.trim();
  const actor = query.actor?.trim();
  const status = query.status?.trim();
  const target = query.target?.trim();
  const search = query.search?.trim();
  if (action) parsed.action = action;
  if (actor) parsed.actor = actor;
  if (status) parsed.status = status;
  if (target) parsed.target = target;
  if (search) parsed.search = search;
  return parsed;
}

function parseDeliveryQuery(query: PasswordResetDeliveryQuery) {
  const limit = query.limit ? Number.parseInt(query.limit, 10) : 10;
  const offset = query.offset ? Number.parseInt(query.offset, 10) : 0;
  const parsed: {
    status?: string;
    limit: number;
    offset: number;
  } = {
    limit: Number.isFinite(limit) ? Math.min(Math.max(limit, 1), 100) : 10,
    offset: Number.isFinite(offset) ? Math.max(offset, 0) : 0,
  };

  if (query.status?.trim()) {
    parsed.status = query.status.trim();
  }

  return parsed;
}

function escapeHtml(value: unknown): string {
  return String(value ?? "")
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#39;");
}

function formatMetricName(name: keyof Omit<PersistenceTelemetrySnapshot, "lastEventAt">): string {
  return `mobammo_persistence_${name.replace(/[A-Z]/g, (letter) => `_${letter.toLowerCase()}`)}_total`;
}

function buildPersistenceMetrics(
  snapshot: PersistenceTelemetrySnapshot,
  sessionReaper: { intervalSeconds: number; sessionTimeoutSeconds: number },
  passwordResetDeliveryStatusCounts: Record<string, number> = {},
): string {
  const counterKeys: Array<keyof Omit<PersistenceTelemetrySnapshot, "lastEventAt">> = [
    "saveAccepted",
    "staleIgnored",
    "rejected",
    "timedOut",
    "reconnectAttempted",
    "heartbeatAccepted",
    "sessionStarted",
    "sessionEnded",
  ];

  const lines: string[] = [
    "# HELP mobammo_persistence_*_total Persistence telemetry counters.",
    "# TYPE mobammo_persistence_save_accepted_total counter",
  ];

  for (const key of counterKeys) {
    lines.push(`${formatMetricName(key)} ${snapshot[key]}`);
  }

  lines.push("# HELP mobammo_session_timeout_seconds Active session timeout.");
  lines.push("# TYPE mobammo_session_timeout_seconds gauge");
  lines.push(`mobammo_session_timeout_seconds ${sessionReaper.sessionTimeoutSeconds}`);
  lines.push("# HELP mobammo_session_reaper_interval_seconds Session reaper interval.");
  lines.push("# TYPE mobammo_session_reaper_interval_seconds gauge");
  lines.push(`mobammo_session_reaper_interval_seconds ${sessionReaper.intervalSeconds}`);
  lines.push("# HELP mobammo_password_reset_delivery_status_total Password reset delivery outbox rows by status.");
  lines.push("# TYPE mobammo_password_reset_delivery_status_total gauge");
  for (const [status, count] of Object.entries(passwordResetDeliveryStatusCounts)) {
    lines.push(`mobammo_password_reset_delivery_status_total{status="${status.replaceAll("\"", "\\\"")}"} ${count}`);
  }

  if (snapshot.lastEventAt) {
    lines.push("# HELP mobammo_persistence_last_event_timestamp_seconds Last persistence event timestamp.");
    lines.push("# TYPE mobammo_persistence_last_event_timestamp_seconds gauge");
    lines.push(`mobammo_persistence_last_event_timestamp_seconds ${Math.floor(new Date(snapshot.lastEventAt).getTime() / 1000)}`);
  }

  return `${lines.join("\n")}\n`;
}

function buildPersistenceDashboardHtml(
  snapshot: PersistenceTelemetrySnapshot,
  events: PersistenceEventSnapshot[],
  sessionReaper: { intervalSeconds: number; sessionTimeoutSeconds: number },
): string {
  const counters: Array<[string, number, string]> = [
    ["Save OK", snapshot.saveAccepted, "accepted saves"],
    ["Rejected", snapshot.rejected, "blocked writes"],
    ["Heartbeat", snapshot.heartbeatAccepted, "accepted heartbeats"],
    ["Started", snapshot.sessionStarted, "sessions"],
    ["Ended", snapshot.sessionEnded, "sessions"],
    ["Timed Out", snapshot.timedOut, "sessions"],
    ["Reconnect", snapshot.reconnectAttempted, "attempts"],
    ["Stale", snapshot.staleIgnored, "ignored saves"],
  ];

  const cards = counters
    .map(([label, value, hint]) => `
      <section class="metric">
        <span>${escapeHtml(label)}</span>
        <strong>${value}</strong>
        <small>${escapeHtml(hint)}</small>
      </section>
    `)
    .join("");

  const rows = events.length > 0
    ? events
        .map((event) => `
          <tr>
            <td>${escapeHtml(event.occurredAt)}</td>
            <td><span class="pill ${escapeHtml(event.eventType)}">${escapeHtml(event.eventType)}</span></td>
            <td>${escapeHtml(event.statusCode ?? "-")}</td>
            <td>${escapeHtml(event.errorCode ?? "-")}</td>
            <td>${escapeHtml(event.characterId ?? "-")}</td>
            <td>${escapeHtml(event.sessionId ?? "-")}</td>
            <td>${escapeHtml(event.message ?? "-")}</td>
          </tr>
        `)
        .join("")
    : `<tr><td colspan="7" class="empty">No persistence events recorded yet.</td></tr>`;

  return `<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <meta http-equiv="refresh" content="10" />
  <title>MOBAMMO Persistence Ops</title>
  <style>
    :root {
      color-scheme: dark;
      --bg: #07090d;
      --panel: rgba(24, 27, 34, 0.92);
      --panel-2: rgba(13, 16, 22, 0.92);
      --line: rgba(239, 180, 83, 0.22);
      --gold: #efb453;
      --text: #f5ead2;
      --muted: #9d9484;
      --red: #ef6b61;
      --green: #69d391;
      --blue: #76b7ff;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      background:
        radial-gradient(circle at top left, rgba(239, 180, 83, 0.12), transparent 34rem),
        radial-gradient(circle at bottom right, rgba(118, 183, 255, 0.10), transparent 38rem),
        var(--bg);
      color: var(--text);
      font-family: Georgia, "Times New Roman", serif;
    }
    main { width: min(1360px, calc(100vw - 48px)); margin: 0 auto; padding: 34px 0 44px; }
    header { display: flex; align-items: flex-end; justify-content: space-between; gap: 24px; margin-bottom: 28px; border-bottom: 1px solid var(--line); padding-bottom: 18px; }
    h1 { margin: 0; letter-spacing: 0.08em; font-size: clamp(30px, 4vw, 54px); }
    .sub { color: var(--muted); margin-top: 8px; font-family: Consolas, monospace; }
    .links { display: flex; gap: 10px; flex-wrap: wrap; justify-content: flex-end; }
    a { color: var(--gold); text-decoration: none; border: 1px solid var(--line); padding: 9px 12px; background: rgba(239, 180, 83, 0.06); }
    .grid { display: grid; grid-template-columns: repeat(4, minmax(0, 1fr)); gap: 14px; margin-bottom: 22px; }
    .metric { padding: 18px; border: 1px solid var(--line); background: linear-gradient(145deg, var(--panel), var(--panel-2)); min-height: 126px; }
    .metric span { color: var(--gold); letter-spacing: 0.08em; text-transform: uppercase; font-size: 12px; }
    .metric strong { display: block; font-size: 42px; margin: 10px 0 4px; }
    .metric small { color: var(--muted); font-family: Consolas, monospace; }
    .status { display: grid; grid-template-columns: repeat(3, 1fr); gap: 14px; margin-bottom: 22px; }
    .status div { border: 1px solid var(--line); padding: 14px 16px; background: rgba(0,0,0,0.24); color: var(--muted); font-family: Consolas, monospace; }
    .status b { color: var(--text); font-family: Georgia, "Times New Roman", serif; }
    table { width: 100%; border-collapse: collapse; background: rgba(9, 11, 15, 0.84); border: 1px solid var(--line); }
    th, td { text-align: left; padding: 12px 10px; border-bottom: 1px solid rgba(239, 180, 83, 0.12); vertical-align: top; }
    th { color: var(--gold); font-size: 12px; letter-spacing: 0.08em; text-transform: uppercase; background: rgba(239, 180, 83, 0.06); }
    td { color: #dfd3bd; font-family: Consolas, monospace; font-size: 12px; }
    .pill { display: inline-block; padding: 4px 8px; border: 1px solid rgba(255,255,255,0.15); color: var(--text); background: rgba(255,255,255,0.06); }
    .saveAccepted, .heartbeatAccepted, .sessionStarted { color: var(--green); }
    .rejected, .timedOut { color: var(--red); }
    .reconnectAttempted, .staleIgnored { color: var(--blue); }
    .empty { color: var(--muted); text-align: center; padding: 28px; }
    @media (max-width: 900px) {
      header { align-items: flex-start; flex-direction: column; }
      .grid { grid-template-columns: repeat(2, minmax(0, 1fr)); }
      .status { grid-template-columns: 1fr; }
      table { display: block; overflow-x: auto; }
    }
  </style>
</head>
<body>
  <main>
    <header>
      <div>
        <h1>PERSISTENCE OPS</h1>
        <div class="sub">MOBAMMO backend telemetry - auto-refreshes every 10 seconds</div>
      </div>
      <nav class="links">
        <a href="/health">Health</a>
        <a href="/ops/persistence">JSON</a>
        <a href="/ops/persistence/events?limit=50">Events</a>
        <a href="/ops/persistence/metrics">Metrics</a>
      </nav>
    </header>
    <section class="grid">${cards}</section>
    <section class="status">
      <div>Last Event<br><b>${escapeHtml(snapshot.lastEventAt ?? "none")}</b></div>
      <div>Session Timeout<br><b>${sessionReaper.sessionTimeoutSeconds}s</b></div>
      <div>Reaper Interval<br><b>${sessionReaper.intervalSeconds}s</b></div>
    </section>
    <table>
      <thead>
        <tr>
          <th>Time</th>
          <th>Event</th>
          <th>Status</th>
          <th>Error</th>
          <th>Character</th>
          <th>Session</th>
          <th>Message</th>
        </tr>
      </thead>
      <tbody>${rows}</tbody>
    </table>
  </main>
</body>
</html>`;
}

function buildOpsSecuritySnapshot(app: Parameters<FastifyPluginAsync>[0]) {
  const snapshot = {
    environment: app.appConfig.appEnvironment,
    productionLike: app.appConfig.appEnvironmentProductionLike,
    opsReadProtection: app.appConfig.opsRequireAdminSecret,
    opsMutationProtection: true,
    passwordResetProvider: app.appConfig.authPasswordResetDeliveryProvider,
    passwordResetExposeToken: app.appConfig.authPasswordResetExposeToken,
    adminUsernameCount: app.appConfig.authAdminUsernames.length,
    auditRetentionDays: app.appConfig.opsAuditRetentionDays,
    auditPruneIntervalSeconds: app.appConfig.opsAuditPruneIntervalSeconds,
    sessionSecretRotation: {
      previousSecretCount: app.appConfig.previousSessionServerSecrets.length,
      rotationWindowOpen: app.appConfig.previousSessionServerSecrets.length > 0,
    },
  };

  const checks = [
    {
      key: "ops_read_protection",
      label: "Ops read endpoints protected",
      passed: snapshot.opsReadProtection || !snapshot.productionLike,
      severity: snapshot.productionLike ? "critical" : "info",
      detail: "Ops read endpoints expose telemetry and operational context.",
      remediation: "Production-like environments should set OPS_REQUIRE_ADMIN_SECRET=true and provide OPS_ADMIN_SECRET.",
    },
    {
      key: "ops_mutation_protection",
      label: "Ops mutation endpoints protected",
      passed: snapshot.opsMutationProtection,
      severity: "critical",
      detail: "Reset, export and prune endpoints require ops admin, admin bearer, or session-server auth.",
      remediation: "If this fails, block deploy until mutation endpoints require privileged credentials.",
    },
    {
      key: "password_reset_delivery",
      label: "Password reset delivery is production-safe",
      passed: snapshot.productionLike
        ? snapshot.passwordResetProvider === "webhook" && !snapshot.passwordResetExposeToken
        : true,
      severity: snapshot.productionLike ? "critical" : "info",
      detail: "Reset tokens must not be returned in API responses outside local development.",
      remediation: "Set AUTH_PASSWORD_RESET_DELIVERY_PROVIDER=webhook, AUTH_PASSWORD_RESET_WEBHOOK_URL, and AUTH_PASSWORD_RESET_EXPOSE_TOKEN=false.",
    },
    {
      key: "audit_retention",
      label: "Ops audit cleanup is scheduled",
      passed: snapshot.auditPruneIntervalSeconds > 0,
      severity: "warning",
      detail: "Audit rows should be retained long enough for investigation without growing forever.",
      remediation: "Set OPS_AUDIT_RETENTION_DAYS and keep OPS_AUDIT_PRUNE_INTERVAL_SECONDS above 0.",
    },
    {
      key: "admin_seed",
      label: "Admin username seed configured",
      passed: !snapshot.productionLike || snapshot.adminUsernameCount > 0,
      severity: snapshot.productionLike ? "warning" : "info",
      detail: "At least one admin account path should exist for operations access.",
      remediation: "Configure AUTH_ADMIN_USERNAMES or promote an account through the admin role endpoint before launch.",
    },
  ];

  return {
    ...snapshot,
    checks,
    readinessScore: Math.round((checks.filter((check) => check.passed).length / checks.length) * 100),
  };
}

function buildPasswordResetProviderSnapshot(app: Parameters<FastifyPluginAsync>[0]) {
  const provider = app.appConfig.authPasswordResetDeliveryProvider;
  const webhookConfigured = app.appConfig.authPasswordResetWebhookUrl.length > 0;
  const webhookSecretConfigured = app.appConfig.authPasswordResetWebhookSecret.length > 0;
  const responseTokenExposure = app.appConfig.authPasswordResetExposeToken;
  const productionSafe = app.appConfig.appEnvironmentProductionLike
    ? provider === "webhook" && webhookConfigured && !responseTokenExposure
    : true;

  return {
    provider,
    webhookConfigured,
    webhookSecretConfigured,
    responseTokenExposure,
    tokenTtlSeconds: app.appConfig.authPasswordResetTokenTtlSeconds,
    retryIntervalSeconds: app.appConfig.authPasswordResetDeliveryRetryIntervalSeconds,
    retentionDays: app.appConfig.authPasswordResetDeliveryRetentionDays,
    productionSafe,
    testModeAvailable: provider === "webhook" && webhookConfigured,
  };
}

async function getPasswordResetDeliverySnapshots(
  app: Parameters<FastifyPluginAsync>[0],
  query: { status?: string; limit?: number; offset?: number } = {},
) {
  const authPasswordResetDelivery = (app.prisma as any).authPasswordResetDelivery;
  if (!authPasswordResetDelivery) {
    return [];
  }

  const where = query.status ? { status: query.status } : {};
  const deliveries = await authPasswordResetDelivery.findMany({
    where,
    orderBy: {
      createdAt: "desc",
    },
    skip: Math.max(Math.trunc(query.offset ?? 0), 0),
    take: Math.min(Math.max(Math.trunc(query.limit ?? 10), 1), 100),
    include: {
      account: {
        select: {
          username: true,
        },
      },
    },
  });

  return deliveries.map((delivery: any) => ({
    id: delivery.id,
    accountId: delivery.accountId,
    username: delivery.account?.username ?? null,
    resetTokenId: delivery.resetTokenId,
    provider: delivery.provider,
    status: delivery.status,
    attemptCount: delivery.attemptCount,
    lastStatusCode: delivery.lastStatusCode,
    lastError: delivery.lastError,
    nextAttemptAt: delivery.nextAttemptAt?.toISOString() ?? null,
    deliveredAt: delivery.deliveredAt?.toISOString() ?? null,
    expiresAt: delivery.expiresAt.toISOString(),
    createdAt: delivery.createdAt.toISOString(),
    updatedAt: delivery.updatedAt.toISOString(),
  }));
}

async function buildPersistenceOpsResponse(app: Parameters<FastifyPluginAsync>[0]) {
  const opsSecurity = buildOpsSecuritySnapshot(app);
  return {
    persistenceTelemetry: await getPersistenceTelemetrySnapshot(),
    recentEvents: await getRecentPersistenceEvents(25),
    recentAuditEvents: await getRecentOpsAuditEvents(app.prisma, 25),
    lastPreflightEvent: (await getOpsAuditEvents(app.prisma, {
      action: "ops_security_preflight",
      status: "accepted",
      limit: 1,
    }))[0] ?? null,
    opsSecurity,
    passwordResetProvider: buildPasswordResetProviderSnapshot(app),
    passwordResetDeliveryMetrics: {
      statusCounts: await getPasswordResetDeliveryStatusCounts(app),
      worker: getPasswordResetDeliveryWorkerSnapshot(),
    },
    recentPasswordResetDeliveries: await getPasswordResetDeliverySnapshots(app, { limit: 5 }),
    sessionReaper: {
      intervalSeconds: app.appConfig.sessionReaperIntervalSeconds,
      sessionTimeoutSeconds: app.appConfig.sessionTimeoutSeconds,
    },
  };
}

export const registerHealthRoutes: FastifyPluginAsync = async (app) => {
  app.get("/health", async (request) => {
    const response = {
      service: "moba-mmo-server",
      status: "ok",
      timestamp: new Date().toISOString(),
    };

    if (!app.appConfig.opsRequireAdminSecret || await isOpsAdminAccessAccepted(app, request)) {
      return {
        ...response,
        persistenceTelemetry: await getPersistenceTelemetrySnapshot(),
      };
    }

    return response;
  });

  app.get("/ops/persistence", async (request, reply) => {
    if (!await requireOpsAdminSecret(app, request, reply)) {
      return;
    }

    return buildPersistenceOpsResponse(app);
  });

  app.get("/ops/persistence/dashboard", async (request, reply) => {
    if (!await requireOpsAdminSecret(app, request, reply)) {
      return;
    }

    const snapshot = await getPersistenceTelemetrySnapshot();
    const recentEvents = await getRecentPersistenceEvents(50);
    const sessionReaper = {
      intervalSeconds: app.appConfig.sessionReaperIntervalSeconds,
      sessionTimeoutSeconds: app.appConfig.sessionTimeoutSeconds,
    };

    reply.type("text/html; charset=utf-8");
    return buildPersistenceDashboardHtml(snapshot, recentEvents, sessionReaper);
  });

  app.get("/ops/persistence/metrics", async (request, reply) => {
    if (!await requireOpsAdminSecret(app, request, reply)) {
      return;
    }

    const snapshot = await getPersistenceTelemetrySnapshot();
    reply.type("text/plain; version=0.0.4; charset=utf-8");
    return buildPersistenceMetrics(
      snapshot,
      {
        intervalSeconds: app.appConfig.sessionReaperIntervalSeconds,
        sessionTimeoutSeconds: app.appConfig.sessionTimeoutSeconds,
      },
      await getPasswordResetDeliveryStatusCounts(app),
    );
  });

  app.get<{ Querystring: PersistenceEventsQuery }>("/ops/persistence/events", async (request, reply) => {
    if (!await requireOpsAdminSecret(app, request, reply)) {
      return;
    }

    return {
      recentEvents: await getRecentPersistenceEvents(parseEventLimit(request.query.limit)),
    };
  });

  app.get("/ops/persistence/export", async (request, reply) => {
    if (!await requireOpsMutationSecret(app, request, reply)) {
      await recordOpsAuditEvent(app, request, "persistence_export", "rejected", {
        message: "Export authorization failed.",
      });
      return;
    }

    const exportedAt = new Date().toISOString();
    await recordOpsAuditEvent(app, request, "persistence_export", "accepted", {
      message: "Persistence telemetry exported.",
      metadata: { exportedAt },
    });
    reply.header("Content-Disposition", `attachment; filename="mobammo-persistence-telemetry-${exportedAt}.json"`);
    return {
      exportedAt,
      ...(await buildPersistenceOpsResponse(app)),
    };
  });

  app.get<{ Querystring: OpsAuditEventsQuery }>("/ops/audit", async (request, reply) => {
    if (!await requireOpsAdminSecret(app, request, reply)) {
      return;
    }

    return {
      query: parseAuditQuery(request.query),
      recentAuditEvents: await getOpsAuditEvents(app.prisma, parseAuditQuery(request.query)),
    };
  });

  app.get<{ Querystring: OpsAuditEventsQuery }>("/ops/audit/export", async (request, reply) => {
    if (!await requireOpsMutationSecret(app, request, reply)) {
      await recordOpsAuditEvent(app, request, "ops_audit_export", "rejected", {
        message: "Audit export authorization failed.",
      });
      return;
    }

    const exportedAt = new Date().toISOString();
    const query = parseAuditQuery(request.query);
    await recordOpsAuditEvent(app, request, "ops_audit_export", "accepted", {
      message: "Ops audit exported.",
      metadata: { exportedAt, query },
    });
    reply.header("Content-Disposition", `attachment; filename="mobammo-ops-audit-${exportedAt}.json"`);
    return {
      exportedAt,
      query,
      recentAuditEvents: await getOpsAuditEvents(app.prisma, query),
    };
  });

  app.get("/ops/security/preflight", async (request, reply) => {
    if (!await requireOpsAdminSecret(app, request, reply)) {
      await recordOpsAuditEvent(app, request, "ops_security_preflight", "rejected", {
        message: "Security preflight authorization failed.",
      });
      return;
    }

    const exportedAt = new Date().toISOString();
    const opsSecurity = buildOpsSecuritySnapshot(app);
    await recordOpsAuditEvent(app, request, "ops_security_preflight", "accepted", {
      message: "Ops security preflight exported.",
      metadata: {
        exportedAt,
        readinessScore: opsSecurity.readinessScore,
        productionLike: opsSecurity.productionLike,
        failedChecks: opsSecurity.checks.filter((check) => !check.passed).map((check) => check.key),
      },
    });
    reply.header("Content-Disposition", `attachment; filename="mobammo-ops-preflight-${exportedAt}.json"`);
    return {
      exportedAt,
      service: "moba-mmo-server",
      opsSecurity,
    };
  });

  app.get("/ops/password-reset/provider", async (request, reply) => {
    if (!await requireOpsAdminSecret(app, request, reply)) {
      return;
    }

    return {
      checkedAt: new Date().toISOString(),
      passwordResetProvider: buildPasswordResetProviderSnapshot(app),
    };
  });

  app.post<{ Body: PasswordResetProviderTestBody }>("/ops/password-reset/provider/test", async (request, reply) => {
    if (!await requireOpsMutationSecret(app, request, reply)) {
      await recordOpsAuditEvent(app, request, "password_reset_provider_test", "rejected", {
        message: "Password reset provider test authorization failed.",
      });
      return;
    }

    const snapshot = buildPasswordResetProviderSnapshot(app);
    const dryRun = request.body?.dryRun !== false;
    if (dryRun) {
      await recordOpsAuditEvent(app, request, "password_reset_provider_test", "accepted", {
        message: "Password reset provider dry-run completed.",
        metadata: { dryRun, provider: snapshot.provider, productionSafe: snapshot.productionSafe },
      });
      return reply.code(202).send({
        testedAt: new Date().toISOString(),
        dryRun,
        delivered: false,
        passwordResetProvider: snapshot,
      });
    }

    if (snapshot.provider !== "webhook" || !snapshot.webhookConfigured) {
      await recordOpsAuditEvent(app, request, "password_reset_provider_test", "failed", {
        message: "Password reset webhook test requested without webhook configuration.",
        metadata: { dryRun, provider: snapshot.provider },
      });
      return reply.code(400).send({
        error: "password_reset_webhook_not_configured",
        passwordResetProvider: snapshot,
      });
    }

    try {
      const response = await fetch(app.appConfig.authPasswordResetWebhookUrl, {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
          ...(app.appConfig.authPasswordResetWebhookSecret
            ? { "X-Password-Reset-Webhook-Secret": app.appConfig.authPasswordResetWebhookSecret }
            : {}),
        },
        body: JSON.stringify({
          type: "password_reset_provider_test",
          username: "ops-preflight-test",
          resetToken: "test-token-not-valid",
          expiresAt: new Date(Date.now() + app.appConfig.authPasswordResetTokenTtlSeconds * 1000).toISOString(),
          issuedAt: new Date().toISOString(),
        }),
      });
      const delivered = response.ok;
      await recordOpsAuditEvent(app, request, "password_reset_provider_test", delivered ? "accepted" : "failed", {
        message: delivered ? "Password reset webhook test delivered." : "Password reset webhook test failed.",
        metadata: { dryRun, statusCode: response.status, provider: snapshot.provider },
      });
      return reply.code(delivered ? 202 : 502).send({
        testedAt: new Date().toISOString(),
        dryRun,
        delivered,
        statusCode: response.status,
        passwordResetProvider: snapshot,
      });
    } catch (error) {
      await recordOpsAuditEvent(app, request, "password_reset_provider_test", "failed", {
        message: "Password reset webhook test threw an error.",
        metadata: { dryRun, provider: snapshot.provider, error: error instanceof Error ? error.message : String(error) },
      });
      return reply.code(502).send({
        error: "password_reset_webhook_test_failed",
        message: error instanceof Error ? error.message : "Webhook test failed.",
        passwordResetProvider: snapshot,
      });
    }
  });

  app.get<{ Querystring: PasswordResetDeliveryQuery }>("/ops/password-reset/deliveries", async (request, reply) => {
    if (!await requireOpsAdminSecret(app, request, reply)) {
      return;
    }

    const query = parseDeliveryQuery(request.query);
    return {
      query,
      deliveries: await getPasswordResetDeliverySnapshots(app, query),
    };
  });

  app.post<{ Params: PasswordResetDeliveryRetryParams }>("/ops/password-reset/deliveries/:id/retry", async (request, reply) => {
    if (!await requireOpsMutationSecret(app, request, reply)) {
      await recordOpsAuditEvent(app, request, "password_reset_delivery_retry", "rejected", {
        target: request.params.id,
        message: "Password reset delivery retry authorization failed.",
      });
      return;
    }

    const result = await retryPasswordResetDeliveryOutbox(app, request.log, request.params.id);
    await recordOpsAuditEvent(app, request, "password_reset_delivery_retry", result.delivered ? "accepted" : "failed", {
      target: request.params.id,
      message: result.delivered ? "Password reset delivery retry delivered." : "Password reset delivery retry did not deliver.",
      metadata: result,
    });

    return reply.code(result.delivered ? 202 : result.skipped ? 409 : 502).send({
      retriedAt: new Date().toISOString(),
      ...result,
    });
  });

  app.post("/ops/password-reset/deliveries/prune", async (request, reply) => {
    if (!await requireOpsMutationSecret(app, request, reply)) {
      await recordOpsAuditEvent(app, request, "password_reset_delivery_prune", "rejected", {
        message: "Password reset delivery prune authorization failed.",
      });
      return;
    }

    const result = await prunePasswordResetDeliveryOutbox(app, app.appConfig.authPasswordResetDeliveryRetentionDays);
    await recordOpsAuditEvent(app, request, "password_reset_delivery_prune", "accepted", {
      message: "Password reset delivery outbox pruned.",
      metadata: result,
    });
    return reply.code(202).send({
      prunedAt: new Date().toISOString(),
      ...result,
    });
  });

  app.post("/ops/audit/prune", async (request, reply) => {
    if (!await requireOpsMutationSecret(app, request, reply)) {
      await recordOpsAuditEvent(app, request, "ops_audit_prune", "rejected", {
        message: "Audit prune authorization failed.",
      });
      return;
    }

    const result = await pruneOpsAuditEvents(app.prisma, app.appConfig.opsAuditRetentionDays);
    await recordOpsAuditEvent(app, request, "ops_audit_prune", "accepted", {
      message: "Ops audit retention prune completed.",
      metadata: result,
    });
    return reply.code(202).send({
      status: "ops_audit_pruned",
      retentionDays: app.appConfig.opsAuditRetentionDays,
      ...result,
    });
  });

  app.post("/ops/persistence/reset", async (request, reply) => {
    if (!await requireOpsMutationSecret(app, request, reply)) {
      await recordOpsAuditEvent(app, request, "persistence_reset", "rejected", {
        message: "Reset authorization failed.",
      });
      return;
    }

    await resetPersistenceTelemetry();
    await recordOpsAuditEvent(app, request, "persistence_reset", "accepted", {
      message: "Persistence telemetry reset.",
    });
    return reply.code(202).send({
      resetAt: new Date().toISOString(),
      ...(await buildPersistenceOpsResponse(app)),
    });
  });
};
