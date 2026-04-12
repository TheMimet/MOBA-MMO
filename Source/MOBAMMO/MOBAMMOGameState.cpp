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

void AMOBAMMOGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AMOBAMMOGameState, ConnectedPlayers);
}

void AMOBAMMOGameState::OnRep_ConnectedPlayers()
{
    OnReplicatedStateUpdated.Broadcast();
}
