#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"

#include "MOBAMMOGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOBAMMOGameStateUpdatedSignature);

UCLASS()
class MOBAMMO_API AMOBAMMOGameState : public AGameState
{
    GENERATED_BODY()

public:
    AMOBAMMOGameState();

    UPROPERTY(BlueprintAssignable, Category="MOBAMMO|Replication")
    FMOBAMMOGameStateUpdatedSignature OnReplicatedStateUpdated;

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    int32 GetConnectedPlayers() const { return ConnectedPlayers; }

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    void SetConnectedPlayers(int32 NewConnectedPlayers);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    UPROPERTY(ReplicatedUsing=OnRep_ConnectedPlayers, BlueprintReadOnly, Category="MOBAMMO|Replication")
    int32 ConnectedPlayers = 0;

private:
    UFUNCTION()
    void OnRep_ConnectedPlayers();
};
