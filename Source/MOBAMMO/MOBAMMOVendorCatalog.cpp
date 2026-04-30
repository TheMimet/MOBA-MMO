#include "MOBAMMOVendorCatalog.h"

const TArray<FMOBAMMOVendorItem>& UMOBAMMOVendorCatalog::GetCatalog()
{
    static TArray<FMOBAMMOVendorItem> Catalog = []()
    {
        TArray<FMOBAMMOVendorItem> Items;
        auto Add = [&](const TCHAR* Id, const TCHAR* Name, int32 Price)
        {
            FMOBAMMOVendorItem Entry;
            Entry.ItemId = Id;
            Entry.DisplayName = Name;
            Entry.Price = Price;
            Items.Add(Entry);
        };

        Add(TEXT("health_potion_small"), TEXT("Small Health Potion"), 25);
        Add(TEXT("mana_potion_small"),   TEXT("Small Mana Potion"),   20);
        Add(TEXT("iron_sword"),          TEXT("Iron Sword"),          100);
        Add(TEXT("crystal_staff"),       TEXT("Crystal Staff"),       120);
        Add(TEXT("leather_helm"),        TEXT("Leather Helm"),        80);
        Add(TEXT("chain_body"),          TEXT("Chain Body"),          150);
        Add(TEXT("swift_boots"),         TEXT("Swift Boots"),         90);
        Add(TEXT("mana_ring"),           TEXT("Mana Ring"),           110);

        return Items;
    }();
    return Catalog;
}

bool UMOBAMMOVendorCatalog::FindItem(const FString& ItemId, FMOBAMMOVendorItem& OutItem)
{
    for (const FMOBAMMOVendorItem& Entry : GetCatalog())
    {
        if (Entry.ItemId == ItemId)
        {
            OutItem = Entry;
            return true;
        }
    }
    return false;
}
