// server/src/modules/inventory/service.ts
// Tüm inventory business logic burada.
// Routes sadece HTTP katmanını yönetir; gerçek iş burada olur.

import { PrismaClient, InventoryItem } from '@prisma/client';
import { getItem, isValidItem, EQUIP_SLOT } from './itemDefinitions';

const prisma = new PrismaClient();

// ─────────────────────────────────────────────────────────
// TYPES
// ─────────────────────────────────────────────────────────

export interface AddItemRequest {
  itemDefinitionId: string;
  quantity:         number;
  // Loot kaynağı (audit için)
  source?:          'loot_drop' | 'quest_reward' | 'debug' | 'trade';
}

export interface EquipRequest {
  itemId:      string;
  equipSlot:   number;
}

export interface UseItemRequest {
  itemId: string;
}

export interface InventoryResult {
  success: boolean;
  error?:  string;
  data?:   unknown;
}

// ─────────────────────────────────────────────────────────
// CHARACTER DOĞRULAMA
// ─────────────────────────────────────────────────────────

async function assertCharacterOwnership(
  characterId: string,
  accountId: string
): Promise<boolean> {
  const char = await prisma.character.findFirst({
    where: { id: characterId, accountId },
  });
  return char !== null;
}

// ─────────────────────────────────────────────────────────
// ENVANTER YÜKLEME
// ─────────────────────────────────────────────────────────

export async function getInventory(
  characterId: string,
  accountId: string
): Promise<InventoryResult> {
  if (!(await assertCharacterOwnership(characterId, accountId))) {
    return { success: false, error: 'CHARACTER_NOT_FOUND' };
  }

  const items = await prisma.inventoryItem.findMany({
    where: { characterId },
    orderBy: [
      { equippedSlot: 'asc' },
      { acquiredAt: 'asc' },
    ],
  });

  // Item kataloğu bilgilerini birleştir
  const enriched = items.map((item) => ({
    ...item,
    definition: getItem(item.itemDefinitionId) ?? null,
  }));

  return { success: true, data: enriched };
}

// ─────────────────────────────────────────────────────────
// ITEM EKLEME
// ─────────────────────────────────────────────────────────

export async function addItem(
  characterId: string,
  accountId: string,
  req: AddItemRequest
): Promise<InventoryResult> {
  if (!(await assertCharacterOwnership(characterId, accountId))) {
    return { success: false, error: 'CHARACTER_NOT_FOUND' };
  }

  if (!isValidItem(req.itemDefinitionId)) {
    return { success: false, error: 'INVALID_ITEM_ID' };
  }

  if (req.quantity < 1 || req.quantity > 9999) {
    return { success: false, error: 'INVALID_QUANTITY' };
  }

  const def = getItem(req.itemDefinitionId)!;

  // Stackable ise var olan stack'e ekle
  if (def.stackable) {
    const existing = await prisma.inventoryItem.findFirst({
      where: { characterId, itemDefinitionId: req.itemDefinitionId },
    });

    if (existing) {
      const newQty = Math.min(existing.quantity + req.quantity, def.maxStack);
      const updated = await prisma.inventoryItem.update({
        where: { id: existing.id },
        data: { quantity: newQty, updatedAt: new Date() },
      });
      return { success: true, data: { ...updated, definition: def } };
    }
  }

  // Yeni row oluştur
  const item = await prisma.inventoryItem.create({
    data: {
      characterId,
      itemDefinitionId: req.itemDefinitionId,
      quantity: req.quantity,
    },
  });

  return { success: true, data: { ...item, definition: def } };
}

// ─────────────────────────────────────────────────────────
// ITEM EQUIP
// ─────────────────────────────────────────────────────────

export async function equipItem(
  characterId: string,
  accountId: string,
  req: EquipRequest
): Promise<InventoryResult> {
  if (!(await assertCharacterOwnership(characterId, accountId))) {
    return { success: false, error: 'CHARACTER_NOT_FOUND' };
  }

  const item = await prisma.inventoryItem.findFirst({
    where: { id: req.itemId, characterId },
  });
  if (!item) return { success: false, error: 'ITEM_NOT_FOUND' };

  const def = getItem(item.itemDefinitionId);
  if (!def || def.equipSlot === null) {
    return { success: false, error: 'ITEM_NOT_EQUIPPABLE' };
  }

  if (def.equipSlot !== req.equipSlot) {
    return { success: false, error: 'WRONG_EQUIP_SLOT' };
  }

  // Aynı slotta başka bir item varsa önce çıkar
  await prisma.inventoryItem.updateMany({
    where: { characterId, equippedSlot: req.equipSlot, id: { not: req.itemId } },
    data: { equippedSlot: null },
  });

  const updated = await prisma.inventoryItem.update({
    where: { id: req.itemId },
    data: { equippedSlot: req.equipSlot },
  });

  return { success: true, data: { ...updated, definition: def } };
}

// ─────────────────────────────────────────────────────────
// ITEM UNEQUIP
// ─────────────────────────────────────────────────────────

export async function unequipItem(
  characterId: string,
  accountId: string,
  itemId: string
): Promise<InventoryResult> {
  if (!(await assertCharacterOwnership(characterId, accountId))) {
    return { success: false, error: 'CHARACTER_NOT_FOUND' };
  }

  const item = await prisma.inventoryItem.findFirst({
    where: { id: itemId, characterId },
  });
  if (!item) return { success: false, error: 'ITEM_NOT_FOUND' };

  const updated = await prisma.inventoryItem.update({
    where: { id: itemId },
    data: { equippedSlot: null },
  });

  return { success: true, data: updated };
}

// ─────────────────────────────────────────────────────────
// ITEM KULLANMA (consumable)
// ─────────────────────────────────────────────────────────

export async function useItem(
  characterId: string,
  accountId: string,
  itemId: string
): Promise<InventoryResult> {
  if (!(await assertCharacterOwnership(characterId, accountId))) {
    return { success: false, error: 'CHARACTER_NOT_FOUND' };
  }

  const item = await prisma.inventoryItem.findFirst({
    where: { id: itemId, characterId },
  });
  if (!item) return { success: false, error: 'ITEM_NOT_FOUND' };

  const def = getItem(item.itemDefinitionId);
  if (!def || !def.onUse) {
    return { success: false, error: 'ITEM_NOT_USABLE' };
  }

  // Stat değişimini character stats'a uygula
  const stats = await prisma.characterStats.findUnique({ where: { characterId } });
  if (stats) {
    const updates: Record<string, number> = {};
    if (def.onUse.hp)   updates['hp']   = Math.min(stats.hp   + def.onUse.hp,   stats.maxHp);
    if (def.onUse.mana) updates['mana'] = Math.min(stats.mana + def.onUse.mana, stats.maxMana);
    if (Object.keys(updates).length > 0) {
      await prisma.characterStats.update({ where: { characterId }, data: updates });
    }
  }

  // Quantity azalt, 0 ise sil
  if (item.quantity <= 1) {
    await prisma.inventoryItem.delete({ where: { id: itemId } });
  } else {
    await prisma.inventoryItem.update({
      where: { id: itemId },
      data: { quantity: item.quantity - 1 },
    });
  }

  return {
    success: true,
    data: {
      applied: def.onUse,
      itemRemoved: item.quantity <= 1,
    },
  };
}

// ─────────────────────────────────────────────────────────
// ITEM SİLME (çöpe atma)
// ─────────────────────────────────────────────────────────

export async function removeItem(
  characterId: string,
  accountId: string,
  itemId: string,
  quantity: number = 1
): Promise<InventoryResult> {
  if (!(await assertCharacterOwnership(characterId, accountId))) {
    return { success: false, error: 'CHARACTER_NOT_FOUND' };
  }

  const item = await prisma.inventoryItem.findFirst({
    where: { id: itemId, characterId },
  });
  if (!item) return { success: false, error: 'ITEM_NOT_FOUND' };

  if (quantity >= item.quantity) {
    await prisma.inventoryItem.delete({ where: { id: itemId } });
    return { success: true, data: { deleted: true } };
  }

  const updated = await prisma.inventoryItem.update({
    where: { id: itemId },
    data: { quantity: item.quantity - quantity },
  });

  return { success: true, data: updated };
}

// ─────────────────────────────────────────────────────────
// EQUIP EDİLMİŞ STAT BONUSLARI HESAPLA
// (Save sırasında PlayerState'e uygulamak için kullanılır)
// ─────────────────────────────────────────────────────────

export async function getEquippedStatBonuses(characterId: string): Promise<{
  hpBonus: number;
  maxHpBonus: number;
  manaBonus: number;
  maxManaBonus: number;
}> {
  const equippedItems = await prisma.inventoryItem.findMany({
    where: { characterId, equippedSlot: { not: null } },
  });

  let hpBonus = 0, maxHpBonus = 0, manaBonus = 0, maxManaBonus = 0;

  for (const item of equippedItems) {
    const def = getItem(item.itemDefinitionId);
    if (!def) continue;
    hpBonus      += def.statBonus.hp      ?? 0;
    maxHpBonus   += def.statBonus.maxHp   ?? 0;
    manaBonus    += def.statBonus.mana    ?? 0;
    maxManaBonus += def.statBonus.maxMana ?? 0;
  }

  return { hpBonus, maxHpBonus, manaBonus, maxManaBonus };
}
