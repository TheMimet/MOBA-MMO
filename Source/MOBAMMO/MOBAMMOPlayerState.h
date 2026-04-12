#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"

#include "MOBAMMOPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMOBAMMOPlayerStateUpdatedSignature);

UCLASS()
class MOBAMMO_API AMOBAMMOPlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    AMOBAMMOPlayerState();

    UPROPERTY(BlueprintAssignable, Category="MOBAMMO|Replication")
    FMOBAMMOPlayerStateUpdatedSignature OnReplicatedStateUpdated;

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    void ApplySessionIdentity(const FString& InAccountId, const FString& InCharacterId, const FString& InCharacterName, const FString& InClassId, int32 InLevel);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    void InitializeAttributes(float InMaxHealth, float InMaxMana);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    void SetCurrentHealth(float NewHealth);

    UFUNCTION(BlueprintCallable, Category="MOBAMMO|Replication")
    void SetCurrentMana(float NewMana);

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    FString GetAccountId() const { return AccountId; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    FString GetCharacterId() const { return CharacterId; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    FString GetCharacterName() const { return CharacterName; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    FString GetClassId() const { return ClassId; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    int32 GetCharacterLevel() const { return CharacterLevel; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    float GetCurrentHealth() const { return CurrentHealth; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    float GetMaxHealth() const { return MaxHealth; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    float GetCurrentMana() const { return CurrentMana; }

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Replication")
    float GetMaxMana() const { return MaxMana; }

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    UPROPERTY(ReplicatedUsing=OnRep_PlayerIdentity, BlueprintReadOnly, Category="MOBAMMO|Replication")
    FString AccountId;

    UPROPERTY(ReplicatedUsing=OnRep_PlayerIdentity, BlueprintReadOnly, Category="MOBAMMO|Replication")
    FString CharacterId;

    UPROPERTY(ReplicatedUsing=OnRep_PlayerIdentity, BlueprintReadOnly, Category="MOBAMMO|Replication")
    FString CharacterName;

    UPROPERTY(ReplicatedUsing=OnRep_PlayerIdentity, BlueprintReadOnly, Category="MOBAMMO|Replication")
    FString ClassId;

    UPROPERTY(ReplicatedUsing=OnRep_PlayerIdentity, BlueprintReadOnly, Category="MOBAMMO|Replication")
    int32 CharacterLevel = 1;

    UPROPERTY(ReplicatedUsing=OnRep_Attributes, BlueprintReadOnly, Category="MOBAMMO|Replication")
    float CurrentHealth = 100.0f;

    UPROPERTY(ReplicatedUsing=OnRep_Attributes, BlueprintReadOnly, Category="MOBAMMO|Replication")
    float MaxHealth = 100.0f;

    UPROPERTY(ReplicatedUsing=OnRep_Attributes, BlueprintReadOnly, Category="MOBAMMO|Replication")
    float CurrentMana = 50.0f;

    UPROPERTY(ReplicatedUsing=OnRep_Attributes, BlueprintReadOnly, Category="MOBAMMO|Replication")
    float MaxMana = 50.0f;

private:
    UFUNCTION()
    void OnRep_PlayerIdentity();

    UFUNCTION()
    void OnRep_Attributes();

    void BroadcastStateUpdated();
};
