export const EQUIP_SLOT = {
  WEAPON: 0,
  OFFHAND: 1,
  HEAD: 2,
  BODY: 3,
  BOOTS: 4,
  ACCESSORY: 5,
} as const;

export type EquipSlot = (typeof EQUIP_SLOT)[keyof typeof EQUIP_SLOT];
export type ItemRarity = "common" | "uncommon" | "rare" | "epic";

export interface ItemStat {
  hp?: number;
  maxHp?: number;
  mana?: number;
  maxMana?: number;
}

export interface ItemDefinition {
  id: string;
  name: string;
  description: string;
  rarity: ItemRarity;
  stackable: boolean;
  maxStack: number;
  equipSlot: EquipSlot | null;
  statBonus: ItemStat;
  onUse?: ItemStat;
}

export const ITEM_CATALOG = {
  health_potion_small: {
    id: "health_potion_small",
    name: "Small Health Potion",
    description: "Restores 25 HP instantly.",
    rarity: "common",
    stackable: true,
    maxStack: 20,
    equipSlot: null,
    statBonus: {},
    onUse: { hp: 25 },
  },
  mana_potion_small: {
    id: "mana_potion_small",
    name: "Small Mana Potion",
    description: "Restores 20 mana instantly.",
    rarity: "common",
    stackable: true,
    maxStack: 20,
    equipSlot: null,
    statBonus: {},
    onUse: { mana: 20 },
  },
  iron_sword: {
    id: "iron_sword",
    name: "Iron Sword",
    description: "A basic front-line weapon.",
    rarity: "common",
    stackable: false,
    maxStack: 1,
    equipSlot: EQUIP_SLOT.WEAPON,
    statBonus: { maxHp: 10 },
  },
  crystal_staff: {
    id: "crystal_staff",
    name: "Crystal Staff",
    description: "A mana-focused staff for mages.",
    rarity: "uncommon",
    stackable: false,
    maxStack: 1,
    equipSlot: EQUIP_SLOT.WEAPON,
    statBonus: { maxMana: 20 },
  },
  leather_helm: {
    id: "leather_helm",
    name: "Leather Helm",
    description: "Light head armor.",
    rarity: "common",
    stackable: false,
    maxStack: 1,
    equipSlot: EQUIP_SLOT.HEAD,
    statBonus: { maxHp: 15 },
  },
  chain_body: {
    id: "chain_body",
    name: "Chain Body",
    description: "Medium body armor.",
    rarity: "uncommon",
    stackable: false,
    maxStack: 1,
    equipSlot: EQUIP_SLOT.BODY,
    statBonus: { maxHp: 30 },
  },
  swift_boots: {
    id: "swift_boots",
    name: "Swift Boots",
    description: "Enchanted boots that support sustained casting.",
    rarity: "uncommon",
    stackable: false,
    maxStack: 1,
    equipSlot: EQUIP_SLOT.BOOTS,
    statBonus: { maxMana: 10 },
  },
  mana_ring: {
    id: "mana_ring",
    name: "Mana Ring",
    description: "A rare ring carved from pure crystal.",
    rarity: "rare",
    stackable: false,
    maxStack: 1,
    equipSlot: EQUIP_SLOT.ACCESSORY,
    statBonus: { maxMana: 25, maxHp: 10 },
  },
  sparring_token: {
    id: "sparring_token",
    name: "Sparring Token",
    description: "A training token dropped by Sparring Minions.",
    rarity: "common",
    stackable: true,
    maxStack: 99,
    equipSlot: null,
    statBonus: {},
  },
} satisfies Record<string, ItemDefinition>;

export type ItemDefinitionId = keyof typeof ITEM_CATALOG;

export function getItem(id: string): ItemDefinition | undefined {
  return ITEM_CATALOG[id as ItemDefinitionId];
}

export function isValidItem(id: string): id is ItemDefinitionId {
  return id in ITEM_CATALOG;
}
