// Source/MOBAMMO/MOBAMMOBackendSubsystem_InventoryAdditions.cpp
// MOBAMMOBackendSubsystem'a eklenecek inventory HTTP çağrıları.
// Mevcut BackendSubsystem yapısına (FHttpModule kullanımı) uygun yazıldı.

// ──────────────────────────────────────────────────────────────────
// .h'ye eklenecek public metodlar:
// ──────────────────────────────────────────────────────────────────

/*
UFUNCTION(BlueprintCallable, Category="Backend|Inventory")
void LoadInventory(const FString& CharacterId,
                   TFunction<void(const FString& Json)> OnComplete);

UFUNCTION(BlueprintCallable, Category="Backend|Inventory")
void SaveInventory(const FString& CharacterId,
                   const FString& InventoryJson,
                   TFunction<void(bool bSuccess)> OnComplete);

UFUNCTION(BlueprintCallable, Category="Backend|Inventory")
void AddItemToInventory(const FString& CharacterId,
                        const FString& ItemDefinitionId,
                        int32 Quantity,
                        const FString& Source,
                        TFunction<void(bool bSuccess, const FString& ItemJson)> OnComplete);

UFUNCTION(BlueprintCallable, Category="Backend|Inventory")
void UseItemOnBackend(const FString& CharacterId,
                      const FString& ItemId,
                      TFunction<void(bool bSuccess)> OnComplete);
*/

// ──────────────────────────────────────────────────────────────────
// .cpp implementasyonları:
// ──────────────────────────────────────────────────────────────────

#include "MOBAMMOBackendSubsystem.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Interfaces/IHttpRequest.h"

// Karakter envanterini backend'den çek
void UMOBAMMOBackendSubsystem::LoadInventory(
    const FString& CharacterId,
    TFunction<void(const FString& Json)> OnComplete)
{
    FString Url = FString::Printf(
        TEXT("%s/characters/%s/inventory"), *BackendBaseUrl, *CharacterId);

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req =
        FHttpModule::Get().CreateRequest();
    Req->SetURL(Url);
    Req->SetVerb(TEXT("GET"));
    Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    // Phase 2'de JWT eklenecek:
    // Req->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *AccessToken));

    // Mock auth için accountId header'ı
    Req->SetHeader(TEXT("x-account-id"), AccountId);

    Req->OnProcessRequestComplete().BindLambda(
        [OnComplete](FHttpRequestPtr, FHttpResponsePtr Response, bool bSuccess)
        {
            if (bSuccess && Response.IsValid() && Response->GetResponseCode() == 200)
            {
                OnComplete(Response->GetContentAsString());
            }
            else
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("[BackendSubsystem] LoadInventory failed: %d"),
                    Response.IsValid() ? Response->GetResponseCode() : -1);
                OnComplete(TEXT(""));
            }
        });

    Req->ProcessRequest();
}

// Loot drop veya debug ile item ekle
void UMOBAMMOBackendSubsystem::AddItemToInventory(
    const FString& CharacterId,
    const FString& ItemDefinitionId,
    int32 Quantity,
    const FString& Source,
    TFunction<void(bool bSuccess, const FString& ItemJson)> OnComplete)
{
    FString Url = FString::Printf(
        TEXT("%s/characters/%s/inventory/add"), *BackendBaseUrl, *CharacterId);

    FString Body = FString::Printf(
        TEXT("{\"itemDefinitionId\":\"%s\",\"quantity\":%d,\"source\":\"%s\"}"),
        *ItemDefinitionId, Quantity, *Source);

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req =
        FHttpModule::Get().CreateRequest();
    Req->SetURL(Url);
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Req->SetHeader(TEXT("x-account-id"), AccountId);
    Req->SetContentAsString(Body);

    Req->OnProcessRequestComplete().BindLambda(
        [OnComplete](FHttpRequestPtr, FHttpResponsePtr Response, bool bSuccess)
        {
            bool bOk = bSuccess && Response.IsValid() && Response->GetResponseCode() == 201;
            OnComplete(bOk, bOk ? Response->GetContentAsString() : TEXT(""));
        });

    Req->ProcessRequest();
}

// Consumable kullan — backend'de quantity düşür, stat uygula
void UMOBAMMOBackendSubsystem::UseItemOnBackend(
    const FString& CharacterId,
    const FString& ItemId,
    TFunction<void(bool bSuccess)> OnComplete)
{
    FString Url = FString::Printf(
        TEXT("%s/characters/%s/inventory/%s/use"),
        *BackendBaseUrl, *CharacterId, *ItemId);

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req =
        FHttpModule::Get().CreateRequest();
    Req->SetURL(Url);
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Req->SetHeader(TEXT("x-account-id"), AccountId);
    Req->SetContentAsString(TEXT("{}"));

    Req->OnProcessRequestComplete().BindLambda(
        [OnComplete](FHttpRequestPtr, FHttpResponsePtr Response, bool bSuccess)
        {
            bool bOk = bSuccess && Response.IsValid() && Response->GetResponseCode() == 200;
            OnComplete(bOk);
        });

    Req->ProcessRequest();
}

// ──────────────────────────────────────────────────────────────────
// GAMEMODE'a eklenecek loot drop örneği:
// ──────────────────────────────────────────────────────────────────

/*
// MOBAMMOGameMode.cpp içinde, minion ölünce çağrılır:

void AMOBAMMOGameMode::GrantMinionLootToPlayer(APlayerController* Killer)
{
    if (!Killer) return;

    AMOBAMMOPlayerState* PS = Killer->GetPlayerState<AMOBAMMOPlayerState>();
    if (!PS) return;

    FString CharId = PS->GetCharacterId();

    // Loot tablosu: sparring_token her zaman, %30 ihtimalle health_potion
    TArray<TPair<FString,int32>> LootTable = {
        { TEXT("sparring_token"), 1 },
    };
    if (FMath::RandRange(0, 100) < 30)
        LootTable.Add({ TEXT("health_potion_small"), 1 });

    UMOBAMMOBackendSubsystem* Backend = GetWorld()->GetSubsystem<UMOBAMMOBackendSubsystem>();
    if (!Backend) return;

    for (const TPair<FString,int32>& Loot : LootTable)
    {
        Backend->AddItemToInventory(
            CharId, Loot.Key, Loot.Value, TEXT("loot_drop"),
            [this, Killer, ItemDefId = Loot.Key](bool bSuccess, const FString& ItemJson)
            {
                if (!bSuccess) return;

                // Unreal tarafında da local state'i güncelle
                AMOBAMMOCharacter* Char = Cast<AMOBAMMOCharacter>(Killer->GetPawn());
                if (Char && Char->InventoryComponent)
                {
                    FMOBAMMOInventoryItem NewItem;
                    NewItem.ItemId           = FGuid::NewGuid().ToString();
                    NewItem.ItemDefinitionId = ItemDefId;
                    NewItem.Quantity         = 1;
                    NewItem.DisplayName      = ItemDefId; // Geçici, backend'den parse edilebilir
                    Char->InventoryComponent->Server_AddItem(NewItem);
                }

                // Combat log'a bildir
                FString Msg = FString::Printf(TEXT("Loot: %s dropped!"), *ItemDefId);
                // BroadcastCombatLog(Msg); // Mevcut combat log sisteminize uyarlayın
            });
    }
}
*/
