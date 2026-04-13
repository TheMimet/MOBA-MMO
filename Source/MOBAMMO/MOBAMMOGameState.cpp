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

void AMOBAMMOGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AMOBAMMOGameState, ConnectedPlayers);
    DOREPLIFETIME(AMOBAMMOGameState, LastCombatLog);
    DOREPLIFETIME(AMOBAMMOGameState, CombatFeed);
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
