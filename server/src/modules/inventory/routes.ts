import type { FastifyPluginAsync, FastifyReply } from "fastify";

import { requireAuthenticatedAccountId } from "../auth/tokens.js";
import { ITEM_CATALOG } from "./itemDefinitions.js";
import {
  addItem,
  equipItem,
  getInventory,
  removeItem,
  unequipItem,
  useItem,
  type InventoryError,
} from "./service.js";

interface CharacterInventoryParams {
  id: string;
}

interface CharacterInventoryItemParams extends CharacterInventoryParams {
  itemId: string;
}

interface AddItemBody {
  itemDefinitionId?: string;
  quantity?: number;
  source?: "loot_drop" | "quest_reward" | "debug" | "trade";
}

interface EquipItemBody {
  equipSlot?: number;
}

interface RemoveItemQuery {
  quantity?: string;
}

function parseInteger(value: unknown, fallback?: number, minValue = 1): number | null {
  if (value === undefined || value === null || value === "") {
    return fallback ?? null;
  }

  const parsed = typeof value === "number" ? value : Number(value);
  if (!Number.isFinite(parsed)) {
    return null;
  }

  const normalized = Math.trunc(parsed);
  return normalized >= minValue ? normalized : null;
}

function sendInventoryError(reply: FastifyReply, error: InventoryError): FastifyReply {
  const statusCode = error === "CHARACTER_NOT_FOUND" || error === "ITEM_NOT_FOUND" ? 404 : 400;
  return reply.code(statusCode).send({ error });
}

export const registerInventoryRoutes: FastifyPluginAsync = async (app) => {
  app.get<{ Params: CharacterInventoryParams }>("/:id/inventory", async (request, reply) => {
    const accountId = await requireAuthenticatedAccountId(app, request, reply);
    if (!accountId) {
      return;
    }

    const result = await getInventory(app.prisma, request.params.id, accountId);
    if (!result.success) {
      return sendInventoryError(reply, result.error);
    }

    return { inventory: result.data };
  });

  app.get<{ Params: CharacterInventoryParams }>("/:id/inventory/catalog", async (request, reply) => {
    const accountId = await requireAuthenticatedAccountId(app, request, reply);
    if (!accountId) {
      return;
    }

    return { catalog: Object.values(ITEM_CATALOG) };
  });

  app.post<{ Body: AddItemBody; Params: CharacterInventoryParams }>("/:id/inventory/add", async (request, reply) => {
    const accountId = await requireAuthenticatedAccountId(app, request, reply);
    if (!accountId) {
      return;
    }

    const itemDefinitionId = request.body?.itemDefinitionId?.trim();
    if (!itemDefinitionId) {
      return reply.code(400).send({ error: "MISSING_ITEM_ID" });
    }

    const quantity = parseInteger(request.body?.quantity, 1);
    if (quantity === null) {
      return reply.code(400).send({ error: "INVALID_QUANTITY" });
    }

    const result = await addItem(app.prisma, request.params.id, accountId, {
      itemDefinitionId,
      quantity,
      source: request.body?.source ?? "debug",
    });
    if (!result.success) {
      return sendInventoryError(reply, result.error);
    }

    return reply.code(201).send({ item: result.data });
  });

  app.post<{ Body: EquipItemBody; Params: CharacterInventoryItemParams }>("/:id/inventory/:itemId/equip", async (request, reply) => {
    const accountId = await requireAuthenticatedAccountId(app, request, reply);
    if (!accountId) {
      return;
    }

    const equipSlot = parseInteger(request.body?.equipSlot, undefined, 0);
    if (equipSlot === null) {
      return reply.code(400).send({ error: "MISSING_EQUIP_SLOT" });
    }

    const result = await equipItem(app.prisma, request.params.id, accountId, {
      itemId: request.params.itemId,
      equipSlot,
    });
    if (!result.success) {
      return sendInventoryError(reply, result.error);
    }

    return { item: result.data };
  });

  app.post<{ Params: CharacterInventoryItemParams }>("/:id/inventory/:itemId/unequip", async (request, reply) => {
    const accountId = await requireAuthenticatedAccountId(app, request, reply);
    if (!accountId) {
      return;
    }

    const result = await unequipItem(app.prisma, request.params.id, accountId, request.params.itemId);
    if (!result.success) {
      return sendInventoryError(reply, result.error);
    }

    return { item: result.data };
  });

  app.post<{ Params: CharacterInventoryItemParams }>("/:id/inventory/:itemId/use", async (request, reply) => {
    const accountId = await requireAuthenticatedAccountId(app, request, reply);
    if (!accountId) {
      return;
    }

    const result = await useItem(app.prisma, request.params.id, accountId, request.params.itemId);
    if (!result.success) {
      return sendInventoryError(reply, result.error);
    }

    return result.data;
  });

  app.delete<{ Params: CharacterInventoryItemParams; Querystring: RemoveItemQuery }>("/:id/inventory/:itemId", async (request, reply) => {
    const accountId = await requireAuthenticatedAccountId(app, request, reply);
    if (!accountId) {
      return;
    }

    const quantity = parseInteger(request.query.quantity);
    const result = await removeItem(app.prisma, request.params.id, accountId, request.params.itemId, quantity ?? undefined);
    if (!result.success) {
      return sendInventoryError(reply, result.error);
    }

    return result.data;
  });
};
