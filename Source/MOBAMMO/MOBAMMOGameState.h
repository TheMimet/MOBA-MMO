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

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    FString GetLastCombatLog() const { return LastCombatLog; }

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    void SetLastCombatLog(const FString& NewCombatLog);

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    const TArray<FString>& GetCombatFeed() const { return CombatFeed; }

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    void PushCombatFeedEntry(const FString& NewCombatLog);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    UPROPERTY(ReplicatedUsing=OnRep_ConnectedPlayers, BlueprintReadOnly, Category="MOBAMMO|Replication")
    int32 ConnectedPlayers = 0;

    UPROPERTY(ReplicatedUsing=OnRep_LastCombatLog, BlueprintReadOnly, Category="MOBAMMO|Replication")
    FString LastCombatLog;

    UPROPERTY(ReplicatedUsing=OnRep_CombatFeed, BlueprintReadOnly, Category="MOBAMMO|Replication")
    TArray<FString> CombatFeed;

private:
    UFUNCTION()
    void OnRep_ConnectedPlayers();

    UFUNCTION()
    void OnRep_LastCombatLog();

    UFUNCTION()
    void OnRep_CombatFeed();
};
