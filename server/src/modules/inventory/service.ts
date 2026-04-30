import type { InventoryItem, PrismaClient } from "../../../generated/prisma/index.js";

import { getItem, isValidItem, type ItemDefinition, type ItemStat } from "./itemDefinitions.js";

export interface AddItemRequest {
  itemDefinitionId: string;
  quantity: number;
  source?: "loot_drop" | "quest_reward" | "debug" | "trade";
}

export interface EquipRequest {
  itemId: string;
  equipSlot: number;
}

export interface InventoryItemDto extends InventoryItem {
  itemDefinitionId: string;
  equippedSlot: number | null;
  definition: ItemDefinition | null;
}

export type InventoryError =
  | "CHARACTER_NOT_FOUND"
  | "INVALID_ITEM_ID"
  | "INVALID_QUANTITY"
  | "ITEM_NOT_FOUND"
  | "ITEM_NOT_EQUIPPABLE"
  | "ITEM_NOT_USABLE"
  | "WRONG_EQUIP_SLOT";

export type InventoryResult<T> =
  | { success: true; data: T }
  | { success: false; error: InventoryError };

async function assertCharacterOwnership(prisma: PrismaClient, characterId: string, accountId: string): Promise<boolean> {
  const character = await prisma.character.findFirst({
    where: {
      id: characterId,
      accountId,
    },
    select: {
      id: true,
    },
  });

  return character !== null;
}

function enrichItem(item: InventoryItem): InventoryItemDto {
  return {
    ...item,
    itemDefinitionId: item.itemId,
    equippedSlot: item.slotIndex,
    definition: getItem(item.itemId) ?? null,
  };
}

function normalizeQuantity(quantity: number): number | null {
  if (!Number.isFinite(quantity)) {
    return null;
  }

  const normalized = Math.trunc(quantity);
  return normalized >= 1 && normalized <= 9999 ? normalized : null;
}

export async function getInventory(
  prisma: PrismaClient,
  characterId: string,
  accountId: string,
): Promise<InventoryResult<InventoryItemDto[]>> {
  if (!(await assertCharacterOwnership(prisma, characterId, accountId))) {
    return { success: false, error: "CHARACTER_NOT_FOUND" };
  }

  const items = await prisma.inventoryItem.findMany({
    where: {
      characterId,
    },
    orderBy: [
      {
        slotIndex: "asc",
      },
      {
        createdAt: "asc",
      },
    ],
  });

  return { success: true, data: items.map(enrichItem) };
}

export async function addItem(
  prisma: PrismaClient,
  characterId: string,
  accountId: string,
  request: AddItemRequest,
): Promise<InventoryResult<InventoryItemDto>> {
  if (!(await assertCharacterOwnership(prisma, characterId, accountId))) {
    return { success: false, error: "CHARACTER_NOT_FOUND" };
  }

  if (!isValidItem(request.itemDefinitionId)) {
    return { success: false, error: "INVALID_ITEM_ID" };
  }

  const quantity = normalizeQuantity(request.quantity);
  if (quantity === null) {
    return { success: false, error: "INVALID_QUANTITY" };
  }

  const definition = getItem(request.itemDefinitionId);
  if (!definition || quantity > definition.maxStack) {
    return { success: false, error: "INVALID_QUANTITY" };
  }

  if (definition.stackable) {
    const existing = await prisma.inventoryItem.findFirst({
      where: {
        characterId,
        itemId: request.itemDefinitionId,
      },
    });

    if (existing) {
      const updated = await prisma.inventoryItem.update({
        where: {
          id: existing.id,
        },
        data: {
          quantity: Math.min(existing.quantity + quantity, definition.maxStack),
        },
      });

      return { success: true, data: enrichItem(updated) };
    }
  }

  const item = await prisma.inventoryItem.create({
    data: {
      characterId,
      itemId: request.itemDefinitionId,
      quantity,
    },
  });

  return { success: true, data: enrichItem(item) };
}

export async function equipItem(
  prisma: PrismaClient,
  characterId: string,
  accountId: string,
  request: EquipRequest,
): Promise<InventoryResult<InventoryItemDto>> {
  if (!(await assertCharacterOwnership(prisma, characterId, accountId))) {
    return { success: false, error: "CHARACTER_NOT_FOUND" };
  }

  const item = await prisma.inventoryItem.findFirst({
    where: {
      id: request.itemId,
      characterId,
    },
  });
  if (!item) {
    return { success: false, error: "ITEM_NOT_FOUND" };
  }

  const definition = getItem(item.itemId);
  if (!definition || definition.equipSlot === null) {
    return { success: false, error: "ITEM_NOT_EQUIPPABLE" };
  }

  if (definition.equipSlot !== request.equipSlot) {
    return { success: false, error: "WRONG_EQUIP_SLOT" };
  }

  await prisma.inventoryItem.updateMany({
    where: {
      characterId,
      slotIndex: request.equipSlot,
      id: {
        not: request.itemId,
      },
    },
    data: {
      slotIndex: null,
    },
  });

  const updated = await prisma.inventoryItem.update({
    where: {
      id: request.itemId,
    },
    data: {
      slotIndex: request.equipSlot,
    },
  });

  return { success: true, data: enrichItem(updated) };
}

export async function unequipItem(
  prisma: PrismaClient,
  characterId: string,
  accountId: string,
  itemId: string,
): Promise<InventoryResult<InventoryItemDto>> {
  if (!(await assertCharacterOwnership(prisma, characterId, accountId))) {
    return { success: false, error: "CHARACTER_NOT_FOUND" };
  }

  const item = await prisma.inventoryItem.findFirst({
    where: {
      id: itemId,
      characterId,
    },
  });
  if (!item) {
    return { success: false, error: "ITEM_NOT_FOUND" };
  }

  const updated = await prisma.inventoryItem.update({
    where: {
      id: itemId,
    },
    data: {
      slotIndex: null,
    },
  });

  return { success: true, data: enrichItem(updated) };
}

export async function useItem(
  prisma: PrismaClient,
  characterId: string,
  accountId: string,
  itemId: string,
): Promise<InventoryResult<{ applied: ItemStat; itemRemoved: boolean }>> {
  if (!(await assertCharacterOwnership(prisma, characterId, accountId))) {
    return { success: false, error: "CHARACTER_NOT_FOUND" };
  }

  const item = await prisma.inventoryItem.findFirst({
    where: {
      id: itemId,
      characterId,
    },
  });
  if (!item) {
    return { success: false, error: "ITEM_NOT_FOUND" };
  }

  const definition = getItem(item.itemId);
  if (!definition?.onUse) {
    return { success: false, error: "ITEM_NOT_USABLE" };
  }

  const stats = await prisma.characterStats.findUnique({
    where: {
      characterId,
    },
  });

  if (stats) {
    const updates: { hp?: number; mana?: number } = {};
    if (definition.onUse.hp !== undefined) {
      updates.hp = Math.min(stats.hp + definition.onUse.hp, stats.maxHp);
    }
    if (definition.onUse.mana !== undefined) {
      updates.mana = Math.min(stats.mana + definition.onUse.mana, stats.maxMana);
    }

    if (Object.keys(updates).length > 0) {
      await prisma.characterStats.update({
        where: {
          characterId,
        },
        data: updates,
      });
    }
  }

  if (item.quantity <= 1) {
    await prisma.inventoryItem.delete({
      where: {
        id: itemId,
      },
    });
  } else {
    await prisma.inventoryItem.update({
      where: {
        id: itemId,
      },
      data: {
        quantity: item.quantity - 1,
      },
    });
  }

  return {
    success: true,
    data: {
      applied: definition.onUse,
      itemRemoved: item.quantity <= 1,
    },
  };
}

export async function removeItem(
  prisma: PrismaClient,
  characterId: string,
  accountId: string,
  itemId: string,
  quantity?: number,
): Promise<InventoryResult<{ removed: boolean; remainingQuantity: number }>> {
  if (!(await assertCharacterOwnership(prisma, characterId, accountId))) {
    return { success: false, error: "CHARACTER_NOT_FOUND" };
  }

  const item = await prisma.inventoryItem.findFirst({
    where: {
      id: itemId,
      characterId,
    },
  });
  if (!item) {
    return { success: false, error: "ITEM_NOT_FOUND" };
  }

  const removeQuantity = quantity === undefined ? item.quantity : normalizeQuantity(quantity);
  if (removeQuantity === null) {
    return { success: false, error: "INVALID_QUANTITY" };
  }

  if (removeQuantity >= item.quantity) {
    await prisma.inventoryItem.delete({
      where: {
        id: itemId,
      },
    });

    return { success: true, data: { removed: true, remainingQuantity: 0 } };
  }

  const updated = await prisma.inventoryItem.update({
    where: {
      id: itemId,
    },
    data: {
      quantity: item.quantity - removeQuantity,
    },
  });

  return { success: true, data: { removed: false, remainingQuantity: updated.quantity } };
}
