import type { FastifyPluginAsync } from "fastify";

import type { PrismaClient } from "../../../generated/prisma/index.js";
import { SESSION_STATUS } from "../persistence/contracts.js";
import { recordPersistenceEvent } from "../persistence/telemetry.js";
import { getSessionTimeoutMs } from "./policy.js";

export interface ExpiredSessionReapResult {
  cutoff: string;
  timedOut: number;
}

export async function reapExpiredActiveSessions(
  prisma: PrismaClient,
  sessionTimeoutMs: number,
  now = new Date(),
): Promise<ExpiredSessionReapResult> {
  const cutoff = new Date(now.getTime() - sessionTimeoutMs);
  const result = await prisma.characterSession.updateMany({
    where: {
      status: SESSION_STATUS.ACTIVE,
      updatedAt: {
        lt: cutoff,
      },
    },
    data: {
      status: SESSION_STATUS.TIMED_OUT,
    },
  });

  for (let index = 0; index < result.count; index += 1) {
    recordPersistenceEvent("timedOut");
  }

  return {
    cutoff: cutoff.toISOString(),
    timedOut: result.count,
  };
}

export const registerSessionReaper: FastifyPluginAsync = async (app) => {
  const intervalSeconds = app.appConfig.sessionReaperIntervalSeconds;
  if (intervalSeconds <= 0) {
    app.log.info("Session reaper disabled");
    return;
  }

  const intervalMs = intervalSeconds * 1000;
  const sessionTimeoutMs = getSessionTimeoutMs(app.appConfig.sessionTimeoutSeconds);
  let isReaping = false;

  const reap = async (): Promise<void> => {
    if (isReaping) {
      return;
    }

    isReaping = true;
    try {
      const result = await reapExpiredActiveSessions(app.prisma, sessionTimeoutMs);
      if (result.timedOut > 0) {
        app.log.warn(result, "Expired active sessions marked timed_out");
      }
    } catch (error) {
      app.log.error(error, "Session reaper failed");
    } finally {
      isReaping = false;
    }
  };

  const timer = setInterval(() => {
    void reap();
  }, intervalMs);
  timer.unref();

  app.addHook("onClose", async () => {
    clearInterval(timer);
  });
};
