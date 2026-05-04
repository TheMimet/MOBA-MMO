-- server/prisma/migrations/YYYYMMDD_add_inventory/migration.sql
-- Bu dosyayı çalıştırmak yerine:
--   npx prisma migrate dev --name add_inventory
-- komutunu kullanın. Prisma bunu otomatik üretir.
-- Referans olarak burada gösteriliyor.

-- InventoryItem tablosu
CREATE TABLE "InventoryItem" (
    "id"               TEXT NOT NULL,
    "characterId"      TEXT NOT NULL,
    "itemDefinitionId" TEXT NOT NULL,
    "quantity"         INTEGER NOT NULL DEFAULT 1,
    "equippedSlot"     INTEGER,
    "metadata"         JSONB,
    "acquiredAt"       TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "updatedAt"        TIMESTAMP(3) NOT NULL,

    CONSTRAINT "InventoryItem_pkey" PRIMARY KEY ("id")
);

-- Character'a position alanları (Phase 1 persistence)
ALTER TABLE "Character"
    ADD COLUMN IF NOT EXISTS "posX"   DOUBLE PRECISION NOT NULL DEFAULT 0,
    ADD COLUMN IF NOT EXISTS "posY"   DOUBLE PRECISION NOT NULL DEFAULT 0,
    ADD COLUMN IF NOT EXISTS "posZ"   DOUBLE PRECISION NOT NULL DEFAULT 0,
    ADD COLUMN IF NOT EXISTS "rotYaw" DOUBLE PRECISION NOT NULL DEFAULT 0;

-- Foreign key
ALTER TABLE "InventoryItem"
    ADD CONSTRAINT "InventoryItem_characterId_fkey"
    FOREIGN KEY ("characterId") REFERENCES "Character"("id")
    ON DELETE CASCADE ON UPDATE CASCADE;

-- İndeksler
CREATE INDEX "InventoryItem_characterId_idx" ON "InventoryItem"("characterId");
CREATE INDEX "InventoryItem_characterId_equippedSlot_idx"
    ON "InventoryItem"("characterId", "equippedSlot");
