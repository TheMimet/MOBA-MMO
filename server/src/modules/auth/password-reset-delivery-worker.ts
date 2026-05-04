import type { FastifyPluginAsync } from "fastify";

import {
  prunePasswordResetDeliveryOutbox,
  recordPasswordResetDeliveryWorkerRun,
  retryDuePasswordResetDeliveries,
} from "./password-reset-delivery.js";
import { recordSystemOpsAuditEvent } from "../ops/audit.js";

export const registerPasswordResetDeliveryWorker: FastifyPluginAsync = async (app) => {
  const intervalSeconds = app.appConfig.authPasswordResetDeliveryRetryIntervalSeconds;
  if (intervalSeconds <= 0) {
    app.log.info("Password reset delivery retry worker disabled");
    return;
  }

  const intervalMs = intervalSeconds * 1000;
  let isRetrying = false;

  const retryDue = async (): Promise<void> => {
    if (isRetrying) {
      return;
    }

    isRetrying = true;
    try {
      const result = await retryDuePasswordResetDeliveries(app, app.log);
      recordPasswordResetDeliveryWorkerRun(result);
      const pruneResult = await prunePasswordResetDeliveryOutbox(app, app.appConfig.authPasswordResetDeliveryRetentionDays);
      if (result.scanned > 0) {
        app.log.info({ ...result, pruned: pruneResult.deletedCount }, "Password reset delivery retry worker completed");
        await recordSystemOpsAuditEvent(app, "password_reset_delivery_worker", result.failed > 0 ? "failed" : "accepted", {
          message: "Password reset delivery worker completed.",
          metadata: { ...result, pruned: pruneResult.deletedCount, retentionCutoff: pruneResult.cutoff },
        });
      } else if (pruneResult.deletedCount > 0) {
        app.log.info(pruneResult, "Password reset delivery outbox pruned");
      }
    } catch (error) {
      app.log.error(error, "Password reset delivery retry worker failed");
    } finally {
      isRetrying = false;
    }
  };

  const timer = setInterval(() => {
    void retryDue();
  }, intervalMs);
  timer.unref();

  app.addHook("onClose", async () => {
    clearInterval(timer);
  });
};
