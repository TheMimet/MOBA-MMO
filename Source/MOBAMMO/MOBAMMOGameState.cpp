#include "MOBAMMOGameState.h"

#include "Net/UnrealNetwork.h"

AMOBAMMOGameState::AMOBAMMOGameState()
{
    bReplicates = true;
}

void AMOBAMMOGameState::SetConnectedPlayers(int32 NewConnectedPlayers)
{
    if (!HasAuthority())
    {
        return;
    }

    ConnectedPlayers = FMath::Max(0, NewConnectedPlayers);
    ForceNetUpdate();
    OnReplicatedStateUpdated.Broadcast();
}

void AMOBAMMOGameState::SetLastCombatLog(const FString& NewCombatLog)
{
    if (!HasAuthority())
    {
        return;
    }

    LastCombatLog = NewCombatLog;
    ForceNetUpdate();
    OnReplicatedStateUpdated.Broadcast();
}

void AMOBAMMOGameState::PushCombatFeedEntry(const FString& NewCombatLog)
{
    if (!HasAuthority())
    {
        return;
    }

    if (!NewCombatLog.IsEmpty())
    {
        CombatFeed.Insert(NewCombatLog, 0);
        while (CombatFeed.Num() > 4)
        {
            CombatFeed.Pop();
        }
    }

    ForceNetUpdate();
    OnReplicatedStateUpdated.Broadcast();
}

void AMOBAMMOGameState::InitializeTrainingDummy()
{
    if (!HasAuthority())
    {
        return;
    }

    TrainingDummyCharacterId = TEXT("training-dummy");
    TrainingDummyName = TEXT("Training Dummy");
    TrainingDummyMaxHealth = 120.0f;
    TrainingDummyHealth = TrainingDummyMaxHealth;
    TrainingDummyMaxMana = 40.0f;
    TrainingDummyMana = TrainingDummyMaxMana;
    ForceNetUpdate();
    OnReplicatedStateUpdated.Broadcast();
}

float AMOBAMMOGameState::ApplyDamageToTrainingDummy(float Amount)
{
    if (!HasAuthority() || Amount <= 0.0f)
    {
        return 0.0f;
    }

    if (TrainingDummyHealth <= 0.0f)
    {
        InitializeTrainingDummy();
    }

    const float PreviousHealth = TrainingDummyHealth;
    TrainingDummyHealth = FMath::Clamp(TrainingDummyHealth - Amount, 0.0f, TrainingDummyMaxHealth);
    ForceNetUpdate();
    OnReplicatedStateUpdated.Broadcast();
    return PreviousHealth - TrainingDummyHealth;
}

float AMOBAMMOGameState::DrainTrainingDummyMana(float Amount)
{
    if (!HasAuthority() || Amount <= 0.0f)
    {
        return 0.0f;
    }

    const float PreviousMana = TrainingDummyMana;
    TrainingDummyMana = FMath::Clamp(TrainingDummyMana - Amount, 0.0f, TrainingDummyMaxMana);
    ForceNetUpdate();
    OnReplicatedStateUpdated.Broadcast();
    return PreviousMana - TrainingDummyMana;
}

void AMOBAMMOGameState::SetTrainingMinionThreat(const FString& ThreatName, const FString& StrikeName, float ServerWorldTimeSeconds, float AggroEndServerTime)
{
    if (!HasAuthority())
    {
        return;
    }

    TrainingMinionThreatName = ThreatName;
    TrainingMinionLastStrikeName = StrikeName;
    TrainingMinionLastStrikeServerTime = FMath::Max(0.0f, ServerWorldTimeSeconds);
    TrainingMinionAggroEndServerTime = FMath::Max(0.0f, AggroEndServerTime);
    ForceNetUpdate();
    OnReplicatedStateUpdated.Broadcast();
}

void AMOBAMMOGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AMOBAMMOGameState, ConnectedPlayers);
    DOREPLIFETIME(AMOBAMMOGameState, LastCombatLog);
    DOREPLIFETIME(AMOBAMMOGameState, CombatFeed);
    DOREPLIFETIME(AMOBAMMOGameState, TrainingDummyCharacterId);
    DOREPLIFETIME(AMOBAMMOGameState, TrainingDummyName);
    DOREPLIFETIME(AMOBAMMOGameState, TrainingDummyHealth);
    DOREPLIFETIME(AMOBAMMOGameState, TrainingDummyMaxHealth);
    DOREPLIFETIME(AMOBAMMOGameState, TrainingDummyMana);
    DOREPLIFETIME(AMOBAMMOGameState, TrainingDummyMaxMana);
    DOREPLIFETIME(AMOBAMMOGameState, TrainingMinionThreatName);
    DOREPLIFETIME(AMOBAMMOGameState, TrainingMinionLastStrikeName);
    DOREPLIFETIME(AMOBAMMOGameState, TrainingMinionLastStrikeServerTime);
    DOREPLIFETIME(AMOBAMMOGameState, TrainingMinionAggroEndServerTime);
}

void AMOBAMMOGameState::OnRep_ConnectedPlayers()
{
    OnReplicatedStateUpdated.Broadcast();
}

void AMOBAMMOGameState::OnRep_LastCombatLog()
{
    OnReplicatedStateUpdated.Broadcast();
}

void AMOBAMMOGameState::OnRep_CombatFeed()
{
    OnReplicatedStateUpdated.Broadcast();
}

void AMOBAMMOGameState::OnRep_TrainingDummy()
{
    OnReplicatedStateUpdated.Broadcast();
}

void AMOBAMMOGameState::OnRep_TrainingMinionThreat()
{
    OnReplicatedStateUpdated.Broadcast();
}
