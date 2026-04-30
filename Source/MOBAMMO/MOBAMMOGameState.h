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

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Chat")
    void PushChatMessage(const FString& SenderName, const FString& Message);

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Chat")
    const TArray<FString>& GetChatMessages() const { return ChatMessages; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Training")
    FString GetTrainingDummyCharacterId() const { return TrainingDummyCharacterId; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Training")
    FString GetTrainingDummyName() const { return TrainingDummyName; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Training")
    float GetTrainingDummyHealth() const { return TrainingDummyHealth; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Training")
    float GetTrainingDummyMaxHealth() const { return TrainingDummyMaxHealth; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Training")
    float GetTrainingDummyMana() const { return TrainingDummyMana; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Training")
    float GetTrainingDummyMaxMana() const { return TrainingDummyMaxMana; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Training")
    bool IsTrainingDummyAlive() const { return TrainingDummyHealth > 0.0f; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|AI")
    FString GetTrainingMinionThreatName() const { return TrainingMinionThreatName; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|AI")
    FString GetTrainingMinionLastStrikeName() const { return TrainingMinionLastStrikeName; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|AI")
    float GetTrainingMinionLastStrikeServerTime() const { return TrainingMinionLastStrikeServerTime; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|AI")
    float GetTrainingMinionAggroEndServerTime() const { return TrainingMinionAggroEndServerTime; }

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Training")
    void InitializeTrainingDummy();

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Training")
    float ApplyDamageToTrainingDummy(float Amount);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Training")
    float DrainTrainingDummyMana(float Amount);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|AI")
    void SetTrainingMinionThreat(const FString& ThreatName, const FString& StrikeName, float ServerWorldTimeSeconds, float AggroEndServerTime);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    UPROPERTY(ReplicatedUsing=OnRep_ConnectedPlayers, BlueprintReadOnly, Category="MOBAMMO|Replication")
    int32 ConnectedPlayers = 0;

    UPROPERTY(ReplicatedUsing=OnRep_LastCombatLog, BlueprintReadOnly, Category="MOBAMMO|Replication")
    FString LastCombatLog;

    UPROPERTY(ReplicatedUsing=OnRep_CombatFeed, BlueprintReadOnly, Category="MOBAMMO|Replication")
    TArray<FString> CombatFeed;

    UPROPERTY(ReplicatedUsing=OnRep_ChatMessages, BlueprintReadOnly, Category="MOBAMMO|Chat")
    TArray<FString> ChatMessages;

    UPROPERTY(ReplicatedUsing=OnRep_TrainingDummy, BlueprintReadOnly, Category="MOBAMMO|Training")
    FString TrainingDummyCharacterId = TEXT("training-dummy");

    UPROPERTY(ReplicatedUsing=OnRep_TrainingDummy, BlueprintReadOnly, Category="MOBAMMO|Training")
    FString TrainingDummyName = TEXT("Training Dummy");

    UPROPERTY(ReplicatedUsing=OnRep_TrainingDummy, BlueprintReadOnly, Category="MOBAMMO|Training")
    float TrainingDummyHealth = 120.0f;

    UPROPERTY(ReplicatedUsing=OnRep_TrainingDummy, BlueprintReadOnly, Category="MOBAMMO|Training")
    float TrainingDummyMaxHealth = 120.0f;

    UPROPERTY(ReplicatedUsing=OnRep_TrainingDummy, BlueprintReadOnly, Category="MOBAMMO|Training")
    float TrainingDummyMana = 40.0f;

    UPROPERTY(ReplicatedUsing=OnRep_TrainingDummy, BlueprintReadOnly, Category="MOBAMMO|Training")
    float TrainingDummyMaxMana = 40.0f;

    UPROPERTY(ReplicatedUsing=OnRep_TrainingMinionThreat, BlueprintReadOnly, Category="MOBAMMO|AI")
    FString TrainingMinionThreatName;

    UPROPERTY(ReplicatedUsing=OnRep_TrainingMinionThreat, BlueprintReadOnly, Category="MOBAMMO|AI")
    FString TrainingMinionLastStrikeName;

    UPROPERTY(ReplicatedUsing=OnRep_TrainingMinionThreat, BlueprintReadOnly, Category="MOBAMMO|AI")
    float TrainingMinionLastStrikeServerTime = 0.0f;

    UPROPERTY(ReplicatedUsing=OnRep_TrainingMinionThreat, BlueprintReadOnly, Category="MOBAMMO|AI")
    float TrainingMinionAggroEndServerTime = 0.0f;

private:
    UFUNCTION()
    void OnRep_ConnectedPlayers();

    UFUNCTION()
    void OnRep_LastCombatLog();

    UFUNCTION()
    void OnRep_CombatFeed();

    UFUNCTION()
    void OnRep_ChatMessages();

    UFUNCTION()
    void OnRep_TrainingDummy();

    UFUNCTION()
    void OnRep_TrainingMinionThreat();
};
