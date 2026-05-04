import type { FastifyPluginAsync } from "fastify";

import { pruneOpsAuditEvents } from "./audit.js";

export const registerOpsAuditRetention: FastifyPluginAsync = async (app) => {
  const intervalSeconds = app.appConfig.opsAuditPruneIntervalSeconds;
  if (intervalSeconds <= 0) {
    app.log.info("Ops audit retention cleanup disabled");
    return;
  }

  const intervalMs = intervalSeconds * 1000;
  let isPruning = false;

  const prune = async (): Promise<void> => {
    if (isPruning) {
      return;
    }

    isPruning = true;
    try {
      const result = await pruneOpsAuditEvents(app.prisma, app.appConfig.opsAuditRetentionDays);
      if (result.deletedCount > 0) {
        app.log.info(result, "Expired ops audit events pruned");
      }
    } catch (error) {
      app.log.error(error, "Ops audit retention cleanup failed");
    } finally {
      isPruning = false;
    }
  };

  void prune();

  const timer = setInterval(() => {
    void prune();
  }, intervalMs);
  timer.unref();

  app.addHook("onClose", async () => {
    clearInterval(timer);
  });
};
