// server/src/modules/inventory/itemDefinitions.ts
// Server-side item kataloğu.
// Client itemDefinitionId string'ini gönderir, server buradan doğrular.
// İleride DB'ye taşınabilir ama başlangıç için sabit katalog yeterli.

export const EQUIP_SLOT = {
  WEAPON:    0,
  OFFHAND:   1,
  HEAD:      2,
  BODY:      3,
  BOOTS:     4,
  ACCESSORY: 5,
} as const;

export type EquipSlot = typeof EQUIP_SLOT[keyof typeof EQUIP_SLOT];

export type ItemRarity = 'common' | 'uncommon' | 'rare' | 'epic';

export interface ItemStat {
  hp?:      number;
  maxHp?:   number;
  mana?:    number;
  maxMana?: number;
}

export interface ItemDefinition {
  id:            string;
  name:          string;
  description:   string;
  rarity:        ItemRarity;
  stackable:     boolean;
  maxStack:      number;
  // Equip edilebilir slot. null = equip edilemez (consumable)
  equipSlot:     EquipSlot | null;
  // Equip edilince verilen stat bonusları
  statBonus:     ItemStat;
  // Consume edilince ne olur (consumable itemlar)
  onUse?:        ItemStat;
}

export const ITEM_CATALOG: Record<string, ItemDefinition> = {

  // ── CONSUMABLES ──────────────────────────────────────────
  'health_potion_small': {
    id:          'health_potion_small',
    name:        'Küçük Can İksiri',
    description: 'Anında 25 HP yeniler.',
    rarity:      'common',
    stackable:   true,
    maxStack:    20,
    equipSlot:   null,
    statBonus:   {},
    onUse:       { hp: 25 },
  },
  'mana_potion_small': {
    id:          'mana_potion_small',
    name:        'Küçük Mana İksiri',
    description: 'Anında 20 Mana yeniler.',
    rarity:      'common',
    stackable:   true,
    maxStack:    20,
    equipSlot:   null,
    statBonus:   {},
    onUse:       { mana: 20 },
  },

  // ── WEAPONS ──────────────────────────────────────────────
  'iron_sword': {
    id:          'iron_sword',
    name:        'Demir Kılıç',
    description: 'Temel bir savaş silahı.',
    rarity:      'common',
    stackable:   false,
    maxStack:    1,
    equipSlot:   EQUIP_SLOT.WEAPON,
    statBonus:   { maxHp: 10 },
  },
  'crystal_staff': {
    id:          'crystal_staff',
    name:        'Kristal Asa',
    description: 'Büyücüler için mana artırıcı asa.',
    rarity:      'uncommon',
    stackable:   false,
    maxStack:    1,
    equipSlot:   EQUIP_SLOT.WEAPON,
    statBonus:   { maxMana: 20 },
  },

  // ── ARMOR ────────────────────────────────────────────────
  'leather_helm': {
    id:          'leather_helm',
    name:        'Deri Miğfer',
    description: 'Hafif baş zırhı.',
    rarity:      'common',
    stackable:   false,
    maxStack:    1,
    equipSlot:   EQUIP_SLOT.HEAD,
    statBonus:   { maxHp: 15 },
  },
  'chain_body': {
    id:          'chain_body',
    name:        'Zincir Zırh',
    description: 'Orta ağırlıklı gövde zırhı.',
    rarity:      'uncommon',
    stackable:   false,
    maxStack:    1,
    equipSlot:   EQUIP_SLOT.BODY,
    statBonus:   { maxHp: 30 },
  },
  'swift_boots': {
    id:          'swift_boots',
    name:        'Hız Botları',
    description: 'Mana tüketimini azaltan büyülü botlar.',
    rarity:      'uncommon',
    stackable:   false,
    maxStack:    1,
    equipSlot:   EQUIP_SLOT.BOOTS,
    statBonus:   { maxMana: 10 },
  },

  // ── ACCESSORIES ──────────────────────────────────────────
  'mana_ring': {
    id:          'mana_ring',
    name:        'Mana Yüzüğü',
    description: 'Saf kristalden işlenmiş yüzük.',
    rarity:      'rare',
    stackable:   false,
    maxStack:    1,
    equipSlot:   EQUIP_SLOT.ACCESSORY,
    statBonus:   { maxMana: 25, maxHp: 10 },
  },

  // ── MINION DROPS ─────────────────────────────────────────
  'sparring_token': {
    id:          'sparring_token',
    name:        'Antrenman Rozeti',
    description: 'Sparring Minion\'u yenince düşer. Koleksiyonluk.',
    rarity:      'common',
    stackable:   true,
    maxStack:    99,
    equipSlot:   null,
    statBonus:   {},
  },
};

export function getItem(id: string): ItemDefinition | undefined {
  return ITEM_CATALOG[id];
}

export function isValidItem(id: string): boolean {
  return id in ITEM_CATALOG;
}
