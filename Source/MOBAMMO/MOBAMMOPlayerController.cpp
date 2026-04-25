#include "MOBAMMOPlayerController.h"

#include "InputKeyEventArgs.h"
#include "GameFramework/GameStateBase.h"
#include "InputCoreTypes.h"
#include "MOBAMMOGameMode.h"
#include "MOBAMMOGameState.h"
#include "MOBAMMOBackendSubsystem.h"
#include "MOBAMMOPlayerState.h"
#include "MOBAMMOTrainingDummyActor.h"
#include "MOBAMMOTrainingMinionActor.h"
#include "MOBAMMOGameUISubsystem.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"

void AMOBAMMOPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (UInputMappingContext* DefaultMappingContext = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_Default.IMC_Default")))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void AMOBAMMOPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (WasInputKeyJustPressed(EKeys::One))
	{
		TriggerDebugDamage();
	}
	if (WasInputKeyJustPressed(EKeys::Two))
	{
		TriggerDebugHeal();
	}
	if (WasInputKeyJustPressed(EKeys::Three))
	{
		TriggerDebugSpendMana();
	}
	if (WasInputKeyJustPressed(EKeys::Four))
	{
		TriggerDebugRestoreMana();
	}
	if (WasInputKeyJustPressed(EKeys::Five))
	{
		TriggerDebugRespawn();
	}
	if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		TriggerSelectLookTarget(false);
	}
	if (WasInputKeyJustPressed(EKeys::E))
	{
		TriggerSelectLookTarget(true);
	}
	if (WasInputKeyJustPressed(EKeys::I))
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UMOBAMMOGameUISubsystem* UISubsystem = GameInstance->GetSubsystem<UMOBAMMOGameUISubsystem>())
			{
				UISubsystem->ToggleInventory();
			}
		}
	}
	if (WasInputKeyJustPressed(EKeys::Six))
	{
		TriggerCycleDebugTarget();
	}
	if (WasInputKeyJustPressed(EKeys::Seven))
	{
		TriggerClearDebugTarget();
	}
	if (WasInputKeyJustPressed(EKeys::R))
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UMOBAMMOBackendSubsystem* BackendSubsystem = GameInstance->GetSubsystem<UMOBAMMOBackendSubsystem>())
			{
				const AMOBAMMOPlayerState* MOBAPlayerState = GetPlayerState<AMOBAMMOPlayerState>();
				const bool bReplicatedSessionInvalid = MOBAPlayerState && MOBAPlayerState->GetPersistenceStatus() == TEXT("SessionInvalid");
				const bool bCanReconnect = BackendSubsystem->GetSessionStatus() == TEXT("ReconnectRequired") || bReplicatedSessionInvalid;
				UE_LOG(LogTemp, Log, TEXT("[Backend] Reconnect key pressed. SessionStatus=%s PersistenceStatus=%s CanReconnect=%s"),
					*BackendSubsystem->GetSessionStatus(),
					MOBAPlayerState ? *MOBAPlayerState->GetPersistenceStatus() : TEXT("<no player state>"),
					bCanReconnect ? TEXT("true") : TEXT("false"));
				if (bCanReconnect)
				{
					BackendSubsystem->ReconnectCurrentSession(this);
				}
			}
		}
	}
}

FString AMOBAMMOPlayerController::GetSelectedTargetDisplayText() const
{
	const AMOBAMMOPlayerState* MOBAPlayerState = GetPlayerState<AMOBAMMOPlayerState>();
	if (!MOBAPlayerState)
	{
		return TEXT("No target");
	}

	const FString SelectedTargetName = MOBAPlayerState->GetSelectedTargetName();
	return SelectedTargetName.IsEmpty() ? TEXT("No target") : FString::Printf(TEXT("Target %s"), *SelectedTargetName);
}

void AMOBAMMOPlayerController::TriggerDebugDamage()
{
	if (HasAuthority())
	{
		ServerTriggerDebugDamage_Implementation();
		return;
	}

	ServerTriggerDebugDamage();
}

void AMOBAMMOPlayerController::TriggerDebugHeal()
{
	if (HasAuthority())
	{
		ServerTriggerDebugHeal_Implementation();
		return;
	}

	ServerTriggerDebugHeal();
}

void AMOBAMMOPlayerController::TriggerDebugSpendMana()
{
	if (HasAuthority())
	{
		ServerTriggerDebugSpendMana_Implementation();
		return;
	}

	ServerTriggerDebugSpendMana();
}

void AMOBAMMOPlayerController::TriggerDebugRestoreMana()
{
	if (HasAuthority())
	{
		ServerTriggerDebugRestoreMana_Implementation();
		return;
	}

	ServerTriggerDebugRestoreMana();
}

void AMOBAMMOPlayerController::TriggerDebugRespawn()
{
	if (HasAuthority())
	{
		ServerTriggerDebugRespawn_Implementation();
		return;
	}

	ServerTriggerDebugRespawn();
}

void AMOBAMMOPlayerController::TriggerSelectLookTarget(bool bFallbackToTrainingDummy)
{
	if (TrySelectTargetFromView())
	{
		return;
	}

	if (bFallbackToTrainingDummy)
	{
		SelectTrainingMinionTarget();
	}
}

void AMOBAMMOPlayerController::TriggerCycleDebugTarget()
{
	CycleToNextDebugTarget();
}

void AMOBAMMOPlayerController::TriggerClearDebugTarget()
{
	if (HasAuthority())
	{
		ServerClearDebugTarget_Implementation();
		return;
	}

	ServerClearDebugTarget();
}

void AMOBAMMOPlayerController::CycleToNextDebugTarget()
{
	const AGameStateBase* BaseGameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!BaseGameState)
	{
		return;
	}

	const AMOBAMMOPlayerState* LocalPlayerState = GetPlayerState<AMOBAMMOPlayerState>();
	const FString CurrentTargetId = LocalPlayerState ? LocalPlayerState->GetSelectedTargetCharacterId() : FString();

	TArray<AMOBAMMOPlayerState*> Candidates;
	for (APlayerState* IteratedPlayerState : BaseGameState->PlayerArray)
	{
		AMOBAMMOPlayerState* CandidateState = Cast<AMOBAMMOPlayerState>(IteratedPlayerState);
		if (!CandidateState || CandidateState == LocalPlayerState)
		{
			continue;
		}

		Candidates.Add(CandidateState);
	}

	if (Candidates.IsEmpty())
	{
		if (CurrentTargetId == AMOBAMMOTrainingMinionActor::GetTrainingMinionCharacterId())
		{
			SelectTrainingDummyTarget();
		}
		else
		{
			SelectTrainingMinionTarget();
		}
		return;
	}

	int32 NextIndex = 0;
	if (!CurrentTargetId.IsEmpty())
	{
		const int32 CurrentIndex = Candidates.IndexOfByPredicate([&CurrentTargetId](const AMOBAMMOPlayerState* Candidate)
		{
			return Candidate && Candidate->GetCharacterId() == CurrentTargetId;
		});

		if (CurrentIndex != INDEX_NONE)
		{
			NextIndex = (CurrentIndex + 1) % Candidates.Num();
		}
	}

	AMOBAMMOPlayerState* NextTargetState = Candidates[NextIndex];
	if (HasAuthority())
	{
		ServerSetDebugTarget_Implementation(NextTargetState);
		return;
	}

	ServerSetDebugTarget(NextTargetState);
}

void AMOBAMMOPlayerController::SelectTrainingDummyTarget()
{
	if (HasAuthority())
	{
		ServerSetTrainingDummyTarget_Implementation();
		return;
	}

	ServerSetTrainingDummyTarget();
}

void AMOBAMMOPlayerController::SelectTrainingMinionTarget()
{
	if (HasAuthority())
	{
		ServerSetTrainingMinionTarget_Implementation();
		return;
	}

	ServerSetTrainingMinionTarget();
}

bool AMOBAMMOPlayerController::TrySelectTargetFromView()
{
	int32 ViewportSizeX = 0;
	int32 ViewportSizeY = 0;
	GetViewportSize(ViewportSizeX, ViewportSizeY);
	if (ViewportSizeX <= 0 || ViewportSizeY <= 0)
	{
		return false;
	}

	FVector WorldOrigin;
	FVector WorldDirection;
	if (!DeprojectScreenPositionToWorld(ViewportSizeX * 0.5f, ViewportSizeY * 0.5f, WorldOrigin, WorldDirection))
	{
		return false;
	}

	const FVector TraceEnd = WorldOrigin + (WorldDirection * 3500.0f);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MOBAMMOSelectTargetFromView), true);
	QueryParams.AddIgnoredActor(GetPawn());

	FHitResult Hit;
	if (!GetWorld() || !GetWorld()->LineTraceSingleByChannel(Hit, WorldOrigin, TraceEnd, ECC_Visibility, QueryParams))
	{
		return false;
	}

	if (Hit.GetActor() && Hit.GetActor()->IsA<AMOBAMMOTrainingDummyActor>())
	{
		SelectTrainingDummyTarget();
		return true;
	}

	if (Hit.GetActor() && Hit.GetActor()->IsA<AMOBAMMOTrainingMinionActor>())
	{
		SelectTrainingMinionTarget();
		return true;
	}

	return false;
}

void AMOBAMMOPlayerController::ServerTriggerDebugDamage_Implementation()
{
	if (AMOBAMMOGameMode* MOBAGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AMOBAMMOGameMode>() : nullptr)
	{
		MOBAGameMode->CastDebugDamageSpell(this);
	}
}

void AMOBAMMOPlayerController::ServerTriggerDebugHeal_Implementation()
{
	if (AMOBAMMOGameMode* MOBAGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AMOBAMMOGameMode>() : nullptr)
	{
		MOBAGameMode->CastDebugHealSpell(this);
	}
}

void AMOBAMMOPlayerController::ServerTriggerDebugSpendMana_Implementation()
{
	if (AMOBAMMOGameMode* MOBAGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AMOBAMMOGameMode>() : nullptr)
	{
		MOBAGameMode->CastDebugDrainSpell(this);
	}
}

void AMOBAMMOPlayerController::ServerTriggerDebugRestoreMana_Implementation()
{
	if (AMOBAMMOGameMode* MOBAGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AMOBAMMOGameMode>() : nullptr)
	{
		MOBAGameMode->TriggerDebugManaRestore(this);
	}
}

void AMOBAMMOPlayerController::ServerTriggerDebugRespawn_Implementation()
{
	if (AMOBAMMOGameMode* MOBAGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AMOBAMMOGameMode>() : nullptr)
	{
		MOBAGameMode->RespawnPlayer(this);
	}
}

void AMOBAMMOPlayerController::ServerSetDebugTarget_Implementation(AMOBAMMOPlayerState* NewTargetPlayerState)
{
	if (AMOBAMMOPlayerState* MOBAPlayerState = GetPlayerState<AMOBAMMOPlayerState>())
	{
		if (NewTargetPlayerState)
		{
			MOBAPlayerState->SetSelectedTargetIdentity(NewTargetPlayerState->GetCharacterId(), NewTargetPlayerState->GetCharacterName());
			if (AMOBAMMOGameState* MOBAGameState = GetWorld() ? GetWorld()->GetGameState<AMOBAMMOGameState>() : nullptr)
			{
				const FString CombatEntry = FString::Printf(TEXT("%s selected %s as target."), *MOBAPlayerState->GetPlayerName(), *NewTargetPlayerState->GetCharacterName());
				MOBAGameState->SetLastCombatLog(CombatEntry);
				MOBAGameState->PushCombatFeedEntry(CombatEntry);
			}
			return;
		}

		MOBAPlayerState->ClearSelectedTarget();
		if (AMOBAMMOGameState* MOBAGameState = GetWorld() ? GetWorld()->GetGameState<AMOBAMMOGameState>() : nullptr)
		{
			const FString CombatEntry = FString::Printf(TEXT("%s cleared target selection."), *MOBAPlayerState->GetPlayerName());
			MOBAGameState->SetLastCombatLog(CombatEntry);
			MOBAGameState->PushCombatFeedEntry(CombatEntry);
		}
	}
}

void AMOBAMMOPlayerController::ServerClearDebugTarget_Implementation()
{
	if (AMOBAMMOPlayerState* MOBAPlayerState = GetPlayerState<AMOBAMMOPlayerState>())
	{
		MOBAPlayerState->ClearSelectedTarget();
	}
}

void AMOBAMMOPlayerController::ServerSetTrainingDummyTarget_Implementation()
{
	const AMOBAMMOGameState* MOBAGameState = GetWorld() ? GetWorld()->GetGameState<AMOBAMMOGameState>() : nullptr;
	if (!MOBAGameState)
	{
		return;
	}

	if (AMOBAMMOPlayerState* MOBAPlayerState = GetPlayerState<AMOBAMMOPlayerState>())
	{
		MOBAPlayerState->SetSelectedTargetIdentity(MOBAGameState->GetTrainingDummyCharacterId(), MOBAGameState->GetTrainingDummyName());
		if (AMOBAMMOGameState* MutableGameState = GetWorld() ? GetWorld()->GetGameState<AMOBAMMOGameState>() : nullptr)
		{
			const FString CombatEntry = FString::Printf(
				TEXT("%s selected %s. Press 1 for Arc Burst or 3 for Drain Pulse."),
				*MOBAPlayerState->GetPlayerName(),
				*MOBAGameState->GetTrainingDummyName()
			);
			MutableGameState->SetLastCombatLog(CombatEntry);
			MutableGameState->PushCombatFeedEntry(CombatEntry);
		}
	}
}

void AMOBAMMOPlayerController::ServerSetTrainingMinionTarget_Implementation()
{
	if (AMOBAMMOPlayerState* MOBAPlayerState = GetPlayerState<AMOBAMMOPlayerState>())
	{
		MOBAPlayerState->SetSelectedTargetIdentity(
			AMOBAMMOTrainingMinionActor::GetTrainingMinionCharacterId(),
			AMOBAMMOTrainingMinionActor::GetTrainingMinionName()
		);

		if (AMOBAMMOGameState* MutableGameState = GetWorld() ? GetWorld()->GetGameState<AMOBAMMOGameState>() : nullptr)
		{
			const FString CombatEntry = FString::Printf(
				TEXT("%s selected %s. Press 1 for Arc Burst."),
				*MOBAPlayerState->GetPlayerName(),
				*AMOBAMMOTrainingMinionActor::GetTrainingMinionName()
			);
			MutableGameState->SetLastCombatLog(CombatEntry);
			MutableGameState->PushCombatFeedEntry(CombatEntry);
		}
	}
}
