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
	virtual void PlayerTick(float DeltaTime) override;

	UFUNCTION(BlueprintPure, Category="MOBAMMO|Gameplay")
	FString GetSelectedTargetDisplayText() const;

private:
	void TriggerDebugDamage();
	void TriggerDebugHeal();
	void TriggerDebugSpendMana();
	void TriggerDebugRestoreMana();
	void TriggerDebugRespawn();
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
	void ServerSetDebugTarget(AMOBAMMOPlayerState* NewTargetPlayerState);

	UFUNCTION(Server, Reliable)
	void ServerClearDebugTarget();

	UFUNCTION(Server, Reliable)
	void ServerSetTrainingDummyTarget();

	UFUNCTION(Server, Reliable)
	void ServerSetTrainingMinionTarget();
};
