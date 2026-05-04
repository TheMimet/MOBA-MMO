// server/src/modules/inventory/routes.ts
// Inventory endpoint'leri.
// Mevcut characters/routes.ts ile aynı router yapısını kullanır.
//
// KAYIT:
//   import { inventoryRoutes } from './modules/inventory/routes';
//   app.use('/characters', inventoryRoutes);
//
// veya Fastify için:
//   app.register(inventoryRoutes, { prefix: '/characters' });

import { Router, Request, Response } from 'express';
import {
  getInventory,
  addItem,
  equipItem,
  unequipItem,
  useItem,
  removeItem,
} from './service';
import { ITEM_CATALOG } from './itemDefinitions';

// accountId'yi session/token'dan al.
// Phase 2'de JWT middleware buraya gelecek.
// Şimdilik query/body'den alınıyor (mock uyumlu).
function getAccountId(req: Request): string | null {
  // JWT hazır olunca: return req.user?.accountId ?? null;
  return (req.headers['x-account-id'] as string) ?? (req.body?.accountId) ?? null;
}

const router = Router();

// ─────────────────────────────────────────────────────────
// GET /characters/:id/inventory
// Karakterin tam envanterini döner (katalog bilgisiyle zenginleştirilmiş)
// ─────────────────────────────────────────────────────────
router.get('/:id/inventory', async (req: Request, res: Response) => {
  const accountId = getAccountId(req);
  if (!accountId) return res.status(401).json({ error: 'UNAUTHORIZED' });

  const result = await getInventory(req.params.id, accountId);
  if (!result.success) {
    return res.status(404).json({ error: result.error });
  }
  return res.json({ inventory: result.data });
});

// ─────────────────────────────────────────────────────────
// POST /characters/:id/inventory/add
// Server'dan item ekle (loot drop, quest reward vs.)
// Body: { itemDefinitionId: string, quantity: number, source?: string }
// ─────────────────────────────────────────────────────────
router.post('/:id/inventory/add', async (req: Request, res: Response) => {
  const accountId = getAccountId(req);
  if (!accountId) return res.status(401).json({ error: 'UNAUTHORIZED' });

  const { itemDefinitionId, quantity = 1, source = 'debug' } = req.body;

  if (!itemDefinitionId) {
    return res.status(400).json({ error: 'MISSING_ITEM_ID' });
  }

  const result = await addItem(req.params.id, accountId, {
    itemDefinitionId,
    quantity: Number(quantity),
    source,
  });

  if (!result.success) {
    return res.status(400).json({ error: result.error });
  }
  return res.status(201).json({ item: result.data });
});

// ─────────────────────────────────────────────────────────
// POST /characters/:id/inventory/:itemId/equip
// Item'ı belirtilen slota equip et
// Body: { equipSlot: number }
// ─────────────────────────────────────────────────────────
router.post('/:id/inventory/:itemId/equip', async (req: Request, res: Response) => {
  const accountId = getAccountId(req);
  if (!accountId) return res.status(401).json({ error: 'UNAUTHORIZED' });

  const { equipSlot } = req.body;
  if (equipSlot === undefined) {
    return res.status(400).json({ error: 'MISSING_EQUIP_SLOT' });
  }

  const result = await equipItem(req.params.id, accountId, {
    itemId:    req.params.itemId,
    equipSlot: Number(equipSlot),
  });

  if (!result.success) {
    return res.status(400).json({ error: result.error });
  }
  return res.json({ item: result.data });
});

// ─────────────────────────────────────────────────────────
// POST /characters/:id/inventory/:itemId/unequip
// Equiptan çıkar, çantaya al
// ─────────────────────────────────────────────────────────
router.post('/:id/inventory/:itemId/unequip', async (req: Request, res: Response) => {
  const accountId = getAccountId(req);
  if (!accountId) return res.status(401).json({ error: 'UNAUTHORIZED' });

  const result = await unequipItem(req.params.id, accountId, req.params.itemId);
  if (!result.success) {
    return res.status(400).json({ error: result.error });
  }
  return res.json({ item: result.data });
});

// ─────────────────────────────────────────────────────────
// POST /characters/:id/inventory/:itemId/use
// Consumable kullan (can iksiri, mana iksiri vs.)
// ─────────────────────────────────────────────────────────
router.post('/:id/inventory/:itemId/use', async (req: Request, res: Response) => {
  const accountId = getAccountId(req);
  if (!accountId) return res.status(401).json({ error: 'UNAUTHORIZED' });

  const result = await useItem(req.params.id, accountId, req.params.itemId);
  if (!result.success) {
    return res.status(400).json({ error: result.error });
  }
  return res.json(result.data);
});

// ─────────────────────────────────────────────────────────
// DELETE /characters/:id/inventory/:itemId
// Item'ı çöpe at
// Query: ?quantity=N (varsayılan: tümü)
// ─────────────────────────────────────────────────────────
router.delete('/:id/inventory/:itemId', async (req: Request, res: Response) => {
  const accountId = getAccountId(req);
  if (!accountId) return res.status(401).json({ error: 'UNAUTHORIZED' });

  const quantity = req.query.quantity ? Number(req.query.quantity) : undefined;

  const result = await removeItem(req.params.id, accountId, req.params.itemId, quantity);
  if (!result.success) {
    return res.status(400).json({ error: result.error });
  }
  return res.json(result.data);
});

// ─────────────────────────────────────────────────────────
// GET /characters/:id/inventory/catalog
// Tüm item kataloğunu döner (client UI için)
// ─────────────────────────────────────────────────────────
router.get('/:id/inventory/catalog', (_req: Request, res: Response) => {
  return res.json({ catalog: Object.values(ITEM_CATALOG) });
});

export { router as inventoryRoutes };
