#include "MOBAMMOPlayerController.h"

#include "Components/InputComponent.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/GameStateBase.h"
#include "InputCoreTypes.h"
#include "MOBAMMOGameMode.h"
#include "MOBAMMOPlayerState.h"

void AMOBAMMOPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (!InputComponent)
    {
        return;
    }

    InputComponent->BindKey(EKeys::One, IE_Pressed, this, &AMOBAMMOPlayerController::TriggerDebugDamage);
    InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AMOBAMMOPlayerController::TriggerDebugHeal);
    InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AMOBAMMOPlayerController::TriggerDebugSpendMana);
    InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &AMOBAMMOPlayerController::TriggerDebugRestoreMana);
    InputComponent->BindKey(EKeys::Five, IE_Pressed, this, &AMOBAMMOPlayerController::TriggerDebugRespawn);
    InputComponent->BindKey(EKeys::Six, IE_Pressed, this, &AMOBAMMOPlayerController::TriggerCycleDebugTarget);
    InputComponent->BindKey(EKeys::Seven, IE_Pressed, this, &AMOBAMMOPlayerController::TriggerClearDebugTarget);
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
        TriggerClearDebugTarget();
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
            return;
        }

        MOBAPlayerState->ClearSelectedTarget();
    }
}

void AMOBAMMOPlayerController::ServerClearDebugTarget_Implementation()
{
    if (AMOBAMMOPlayerState* MOBAPlayerState = GetPlayerState<AMOBAMMOPlayerState>())
    {
        MOBAPlayerState->ClearSelectedTarget();
    }
}
