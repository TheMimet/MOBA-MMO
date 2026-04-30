#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "MOBAMMOPlayerController.generated.h"

class AMOBAMMOPlayerState;
struct FInputKeyEventArgs;

UCLASS()
class MOBAMMO_API AMOBAMMOPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void AcknowledgePossession(APawn* P) override;
	virtual void PlayerTick(float DeltaTime) override;

	UFUNCTION(BlueprintPure, Category="MOBAMMO|Gameplay")
	FString GetSelectedTargetDisplayText() const;

	UFUNCTION(Server, Reliable)
	void ServerSendChatMessage(const FString& Message);

	/** Public entry point called by VendorWidget to request a shop purchase. */
	UFUNCTION(BlueprintCallable, Category="MOBAMMO|Vendor")
	void RequestPurchaseFromVendor(const FString& ItemId);

	/** Spend 1 skill point on the given ability slot (0-indexed). Called locally; routes via RPC. */
	UFUNCTION(BlueprintCallable, Category="MOBAMMO|Skills")
	void RequestSpendSkillPoint(int32 AbilitySlotIndex);

private:
	void ApplyGameplayInputMode();
	void ApplyKeyboardMovementFallback();
	void EnsureDefaultInputMapping();
	void TriggerDebugDamage();
	void TriggerDebugHeal();
	void TriggerDebugSpendMana();
	void TriggerDebugRestoreMana();
	void TriggerDebugRespawn();
	void TriggerDebugGrantItem();
	void TriggerPrimaryAttack();
	void TriggerUseInventoryConsumable();
	void TriggerToggleInventoryEquipment();
	void TriggerOpenChat();
	void TriggerToggleVendor();
	void TriggerSpendSkillPoint(int32 SlotIndex);
	void ExecuteGMCommand(const FString& Command, const TArray<FString>& Args);
	void TriggerSelectLookTarget(bool bFallbackToTrainingDummy);
	void TriggerCycleDebugTarget();
	void TriggerClearDebugTarget();
	void CycleToNextDebugTarget();
	void SelectTrainingDummyTarget();
	void SelectTrainingMinionTarget();
	bool TrySelectTargetFromView();

	UFUNCTION(Server, Reliable)
	void ServerTriggerDebugDamage();

	UFUNCTION(Server, Reliable)
	void ServerTriggerDebugHeal();

	UFUNCTION(Server, Reliable)
	void ServerTriggerDebugSpendMana();

	UFUNCTION(Server, Reliable)
	void ServerTriggerDebugRestoreMana();

	UFUNCTION(Server, Reliable)
	void ServerTriggerDebugRespawn();

	UFUNCTION(Server, Reliable)
	void ServerTriggerDebugGrantItem();

	UFUNCTION(Server, Reliable)
	void ServerUseInventoryConsumable();

	UFUNCTION(Server, Reliable)
	void ServerToggleInventoryEquipment();

	UFUNCTION(Server, Reliable)
	void ServerSetDebugTarget(AMOBAMMOPlayerState* NewTargetPlayerState);

	UFUNCTION(Server, Reliable)
	void ServerClearDebugTarget();

	UFUNCTION(Server, Reliable)
	void ServerSetTrainingDummyTarget();

	UFUNCTION(Server, Reliable)
	void ServerSetTrainingMinionTarget();

	UFUNCTION(Server, Reliable)
	void ServerPurchaseFromVendor(const FString& ItemId);

	UFUNCTION(Server, Reliable)
	void ServerSpendSkillPoint(int32 AbilitySlotIndex);

	int32 DebugItemCycleIndex = 0;
};
