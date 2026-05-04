// Source/MOBAMMO/MOBAMMOPlayerState_InventoryAdditions.h
// Bu dosya doğrudan kullanılmaz — MOBAMMOPlayerState.h ve .cpp dosyalarına
// eklenmesi gereken kısımları gösterir.
//
// ────────────────────────────────────────────────────────────────────────────
// MOBAMMOPlayerState.h'ye eklenecekler:
// ────────────────────────────────────────────────────────────────────────────
//
// public:
//     // Equipment bonusları (InventoryComponent tarafından güncellenir)
//     UFUNCTION(BlueprintCallable, Category="PlayerState|Inventory")
//     void SetEquipmentBonuses(int32 MaxHpBonus, int32 MaxManaBonus);
//
//     UFUNCTION(BlueprintCallable, Category="PlayerState|Inventory")
//     int32 GetEquipMaxHpBonus()   const { return EquipMaxHpBonus; }
//
//     UFUNCTION(BlueprintCallable, Category="PlayerState|Inventory")
//     int32 GetEquipMaxManaBonus() const { return EquipMaxManaBonus; }
//
//     // Mevcut GetMaxHp / GetMaxMana'yı bonus'u da hesaba katacak şekilde güncelle:
//     int32 GetEffectiveMaxHp()   const { return MaxHp   + EquipMaxHpBonus; }
//     int32 GetEffectiveMaxMana() const { return MaxMana + EquipMaxManaBonus; }
//
// private:
//     UPROPERTY(Replicated)
//     int32 EquipMaxHpBonus   = 0;
//
//     UPROPERTY(Replicated)
//     int32 EquipMaxManaBonus = 0;
//
// ────────────────────────────────────────────────────────────────────────────
// MOBAMMOPlayerState.cpp'ye eklenecekler:
// ────────────────────────────────────────────────────────────────────────────
//
// GetLifetimeReplicatedProps'a:
//     DOREPLIFETIME(AMOBAMMOPlayerState, EquipMaxHpBonus);
//     DOREPLIFETIME(AMOBAMMOPlayerState, EquipMaxManaBonus);
//
// Implementasyon:
//
// void AMOBAMMOPlayerState::SetEquipmentBonuses(int32 MaxHpBonus, int32 MaxManaBonus)
// {
//     if (!HasAuthority()) return;
//     EquipMaxHpBonus   = MaxHpBonus;
//     EquipMaxManaBonus = MaxManaBonus;
//     // HP ve mana üst limitini clamp et (bonus düşünce HP taşmasın)
//     Hp   = FMath::Min(Hp,   GetEffectiveMaxHp());
//     Mana = FMath::Min(Mana, GetEffectiveMaxMana());
//     ForceNetUpdate();
// }

// ────────────────────────────────────────────────────────────────────────────
// MOBAMMOCharacter.h/.cpp'ye eklenecekler:
// ────────────────────────────────────────────────────────────────────────────
//
// Header:
//     UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Inventory")
//     UMOBAMMOInventoryComponent* InventoryComponent;
//
// Constructor (.cpp):
//     InventoryComponent = CreateDefaultSubobject<UMOBAMMOInventoryComponent>(
//         TEXT("InventoryComponent"));
//
// PossessedBy override (backend'den load tetikle):
//     void AMOBAMMOCharacter::PossessedBy(AController* NewController)
//     {
//         Super::PossessedBy(NewController);
//         // BackendSubsystem'dan inventory json'ı al ve component'a yükle
//         if (UMOBAMMOBackendSubsystem* Backend = ...)
//         {
//             Backend->LoadInventory(CharacterId, [this](const FString& Json) {
//                 if (InventoryComponent)
//                     InventoryComponent->LoadFromBackendJson(Json);
//             });
//         }
//     }

// ────────────────────────────────────────────────────────────────────────────
// MOBAMMO.Build.cs'ye eklenecekler:
// ────────────────────────────────────────────────────────────────────────────
//
// PublicDependencyModuleNames içine:
//     "Json",
//     "JsonUtilities",
