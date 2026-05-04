import "dotenv/config";

import assert from "node:assert/strict";
import test from "node:test";

import { buildApp } from "../src/app.js";
import { pruneOpsAuditEvents } from "../src/modules/ops/audit.js";

const DEV_OPS_ADMIN_SECRET = "dev-only-change-me-mobammo-ops-admin-secret";

test("ops audit export requires mutation auth and accepts ops admin secret", async () => {
  const app = buildApp();
  await app.ready();

  try {
    const rejected = await app.inject({
      method: "GET",
      url: "/ops/audit/export?limit=1",
    });
    assert.equal(rejected.statusCode, 401);

    const accepted = await app.inject({
      method: "GET",
      url: "/ops/audit/export?limit=1",
      headers: {
        "x-ops-admin-secret": process.env.OPS_ADMIN_SECRET ?? DEV_OPS_ADMIN_SECRET,
      },
    });
    assert.equal(accepted.statusCode, 200);
    const body = accepted.json();
    assert.equal(typeof body.exportedAt, "string");
    assert.ok(Array.isArray(body.recentAuditEvents));
  } finally {
    await app.close();
  }
});

test("ops audit pruning deletes records older than retention window", async () => {
  const app = buildApp();
  await app.ready();

  const opsAuditEvent = (app.prisma as any).opsAuditEvent;
  assert.ok(opsAuditEvent, "opsAuditEvent delegate should be available");

  const suffix = `${Date.now()}-${Math.random().toString(16).slice(2)}`;
  const oldEvent = await opsAuditEvent.create({
    data: {
      action: `test.old.${suffix}`,
      actorType: "test",
      status: "accepted",
      target: "ops-audit-retention-test",
      occurredAt: new Date(Date.now() - 3 * 24 * 60 * 60 * 1000),
    },
  });
  const freshEvent = await opsAuditEvent.create({
    data: {
      action: `test.fresh.${suffix}`,
      actorType: "test",
      status: "accepted",
      target: "ops-audit-retention-test",
      occurredAt: new Date(),
    },
  });

  try {
    const result = await pruneOpsAuditEvents(app.prisma, 1);
    assert.ok(result.deletedCount >= 1);
    assert.equal(await opsAuditEvent.findUnique({ where: { id: oldEvent.id } }), null);
    assert.ok(await opsAuditEvent.findUnique({ where: { id: freshEvent.id } }));
  } finally {
    await opsAuditEvent.deleteMany({
      where: {
        id: {
          in: [oldEvent.id, freshEvent.id],
        },
      },
    });
    await app.close();
  }
});

test("ops preflight returns readiness snapshot without exposing secrets", async () => {
  const app = buildApp();
  await app.ready();

  try {
    const response = await app.inject({
      method: "GET",
      url: "/ops/security/preflight",
      headers: {
        "x-ops-admin-secret": process.env.OPS_ADMIN_SECRET ?? DEV_OPS_ADMIN_SECRET,
      },
    });
    assert.equal(response.statusCode, 200);

    const body = response.json();
    assert.equal(body.service, "moba-mmo-server");
    assert.equal(typeof body.opsSecurity.readinessScore, "number");
    assert.ok(Array.isArray(body.opsSecurity.checks));
    assert.equal(JSON.stringify(body).includes(process.env.OPS_ADMIN_SECRET ?? DEV_OPS_ADMIN_SECRET), false);

    const auditEvent = await (app.prisma as any).opsAuditEvent.findFirst({
      where: {
        action: "ops_security_preflight",
        status: "accepted",
      },
      orderBy: {
        occurredAt: "desc",
      },
    });
    assert.ok(auditEvent);
    assert.equal(auditEvent.message, "Ops security preflight exported.");
  } finally {
    await app.close();
  }
});

test("password reset provider dry-run reports status and writes audit", async () => {
  const app = buildApp();
  await app.ready();

  try {
    const response = await app.inject({
      method: "POST",
      url: "/ops/password-reset/provider/test",
      headers: {
        "content-type": "application/json",
        "x-ops-admin-secret": process.env.OPS_ADMIN_SECRET ?? DEV_OPS_ADMIN_SECRET,
      },
      payload: {
        dryRun: true,
      },
    });
    assert.equal(response.statusCode, 202);
    const body = response.json();
    assert.equal(body.dryRun, true);
    assert.equal(body.delivered, false);
    assert.equal(typeof body.passwordResetProvider.provider, "string");

    const auditEvent = await (app.prisma as any).opsAuditEvent.findFirst({
      where: {
        action: "password_reset_provider_test",
        status: "accepted",
      },
      orderBy: {
        occurredAt: "desc",
      },
    });
    assert.ok(auditEvent);
    assert.equal(auditEvent.message, "Password reset provider dry-run completed.");
  } finally {
    await app.close();
  }
});

test("password reset requests create delivery outbox records without exposing encrypted token payloads", async () => {
  const app = buildApp();
  await app.ready();
  const username = `outbox_${Date.now()}_${Math.random().toString(16).slice(2, 8)}`;

  try {
    const registerResponse = await app.inject({
      method: "POST",
      url: "/auth/register",
      headers: {
        "content-type": "application/json",
      },
      payload: {
        username,
        password: "test-password-123",
      },
    });
    assert.equal(registerResponse.statusCode, 201);

    const resetResponse = await app.inject({
      method: "POST",
      url: "/auth/password-reset/request",
      headers: {
        "content-type": "application/json",
      },
      payload: {
        username,
      },
    });
    assert.equal(resetResponse.statusCode, 202);

    const deliveriesResponse = await app.inject({
      method: "GET",
      url: "/ops/password-reset/deliveries?limit=5",
      headers: {
        "x-ops-admin-secret": process.env.OPS_ADMIN_SECRET ?? DEV_OPS_ADMIN_SECRET,
      },
    });
    assert.equal(deliveriesResponse.statusCode, 200);
    const body = deliveriesResponse.json();
    const delivery = body.deliveries.find((item: { username?: string }) => item.username === username);
    assert.ok(delivery);
    assert.equal(delivery.status, "delivered");
    assert.equal("encryptedTokenJson" in delivery, false);

    const storedDelivery = await (app.prisma as any).authPasswordResetDelivery.findFirst({
      where: {
        account: {
          username,
        },
      },
    });
    assert.ok(storedDelivery.encryptedTokenJson);
  } finally {
    await app.prisma.account.deleteMany({
      where: {
        username,
      },
    });
    await app.close();
  }
});

test("password reset delivery prune endpoint removes old delivered outbox rows", async () => {
  const app = buildApp();
  await app.ready();
  const username = `prune_${Date.now()}_${Math.random().toString(16).slice(2, 8)}`;

  try {
    const account = await app.prisma.account.create({
      data: {
        username,
      },
    });
    const resetToken = await app.prisma.authPasswordResetToken.create({
      data: {
        accountId: account.id,
        tokenHash: `test-hash-${username}`,
        expiresAt: new Date(Date.now() - 30 * 24 * 60 * 60 * 1000),
        consumedAt: new Date(Date.now() - 30 * 24 * 60 * 60 * 1000),
      },
    });
    const delivery = await (app.prisma as any).authPasswordResetDelivery.create({
      data: {
        accountId: account.id,
        resetTokenId: resetToken.id,
        provider: "response",
        status: "delivered",
        encryptedTokenJson: "{}",
        deliveredAt: new Date(Date.now() - 30 * 24 * 60 * 60 * 1000),
        expiresAt: new Date(Date.now() - 30 * 24 * 60 * 60 * 1000),
      },
    });

    const response = await app.inject({
      method: "POST",
      url: "/ops/password-reset/deliveries/prune",
      headers: {
        "x-ops-admin-secret": process.env.OPS_ADMIN_SECRET ?? DEV_OPS_ADMIN_SECRET,
      },
    });
    assert.equal(response.statusCode, 202);
    assert.ok(response.json().deletedCount >= 1);
    assert.equal(await (app.prisma as any).authPasswordResetDelivery.findUnique({ where: { id: delivery.id } }), null);
  } finally {
    await app.prisma.account.deleteMany({
      where: {
        username,
      },
    });
    await app.close();
  }
});
