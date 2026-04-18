#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AbilityTypes.h"
#include "AbilityComponent.generated.h"

class UAbilityBase;
class UAbilityDataAsset;
class AActor;
class UGameplayEffect;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAbilityActivatedDelegate, int32, Slot, UAbilityBase*, Ability);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAbilityEndedDelegate, int32, Slot, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCooldownReadyDelegate, int32, Slot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnManaCostDelegate, float, CurrentMana, float, MaxMana);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MOBAMMOABILITIES_API UAbilityComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UAbilityComponent();

    virtual void InitializeComponent() override;
    virtual void UninitializeComponent() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, Category="Abilities")
    bool TryActivateAbility(int32 Slot);

    UFUNCTION(BlueprintCallable, Category="Abilities")
    bool TryActivateAbilityWithTarget(int32 Slot, AActor* TargetActor);

    UFUNCTION(BlueprintCallable, Category="Abilities")
    bool TryActivateAbilityAtLocation(int32 Slot, const FVector& Location);

    UFUNCTION(BlueprintCallable, Category="Abilities")
    bool TryActivateAbilityInDirection(int32 Slot, const FRotator& Direction);

    UFUNCTION(BlueprintCallable, Category="Abilities")
    void CancelAbility(int32 Slot);

    UFUNCTION(BlueprintCallable, Category="Abilities")
    void CancelAllAbilities();

    UFUNCTION(BlueprintCallable, Category="Abilities")
    void InterruptChanneling();

    UFUNCTION(BlueprintCallable, Category="Abilities")
    UAbilityBase* GetAbilityInSlot(int32 Slot) const;

    UFUNCTION(BlueprintCallable, Category="Abilities")
    float GetCooldownRemaining(int32 Slot) const;

    UFUNCTION(BlueprintCallable, Category="Abilities")
    float GetMaxCooldown(int32 Slot) const;

    UFUNCTION(BlueprintCallable, Category="Abilities")
    bool IsOnCooldown(int32 Slot) const;

    UFUNCTION(BlueprintCallable, Category="Abilities")
    bool IsCasting(int32 Slot) const;

    UFUNCTION(BlueprintCallable, Category="Abilities")
    void SetAbilityInSlot(int32 Slot, UAbilityBase* Ability);

    UFUNCTION(BlueprintCallable, Category="Abilities")
    void ClearAbilityInSlot(int32 Slot);

    UFUNCTION(BlueprintCallable, Category="Abilities")
    void LoadAbilitiesFromDataAsset(UAbilityDataAsset* DataAsset);

    UFUNCTION(BlueprintCallable, Category="Abilities|State")
    float GetMana() const { return CurrentMana; }

    UFUNCTION(BlueprintCallable, Category="Abilities|State")
    float GetMaxMana() const { return MaxMana; }

    UFUNCTION(BlueprintCallable, Category="Abilities|State")
    void SetMana(float NewMana) { CurrentMana = FMath::Clamp(NewMana, 0.0f, MaxMana); }

    UFUNCTION(BlueprintCallable, Category="Abilities|State")
    void SetMaxMana(float NewMaxMana) { MaxMana = FMath::Max(NewMaxMana, 0.0f); }

    UFUNCTION(BlueprintCallable, Category="Abilities|State")
    bool HasEnoughMana(int32 Slot) const;

    UFUNCTION(BlueprintCallable, Server, Reliable, Category="Abilities")
    void ServerTryActivateAbility(int32 Slot, const FAbilityActivationInfo& Info);

    UFUNCTION(BlueprintCallable, Server, Reliable, Category="Abilities")
    void ServerCancelAbility(int32 Slot);

    UPROPERTY(BlueprintAssignable, Category="Abilities|Events")
    FOnAbilityActivatedDelegate OnAbilityActivated;

    UPROPERTY(BlueprintAssignable, Category="Abilities|Events")
    FOnAbilityEndedDelegate OnAbilityEnded;

    UPROPERTY(BlueprintAssignable, Category="Abilities|Events")
    FOnCooldownReadyDelegate OnCooldownReady;

    UPROPERTY(BlueprintAssignable, Category="Abilities|Events")
    FOnManaCostDelegate OnManaChanged;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Abilities|Config", meta=(ExposeOnSpawn="true"))
    int32 MaxAbilitySlots = 6;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Abilities|State", meta=(ExposeOnSpawn="true"))
    float CurrentMana = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Abilities|State", meta=(ExposeOnSpawn="true"))
    float MaxMana = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Abilities|State", meta=(ExposeOnSpawn="true"))
    float ManaRegenRate = 5.0f;

protected:
    UFUNCTION()
    virtual void OnRep_ActiveAbilities();

    UFUNCTION()
    bool ValidateAbilityActivation(int32 Slot, const FAbilityActivationInfo& Info) const;

    UFUNCTION()
    void ConsumeManaCost(int32 Slot);

    UFUNCTION()
    void StartCooldown(int32 Slot);

    UFUNCTION()
    void ProcessCooldown(float DeltaTime);

    UFUNCTION()
    void ProcessChanneling(float DeltaTime);

    UFUNCTION()
    void ProcessManaRegen(float DeltaTime);

    UPROPERTY(ReplicatedUsing=OnRep_ActiveAbilities)
    TArray<FActiveAbility> ActiveAbilities;

    UPROPERTY()
    TArray<TObjectPtr<UAbilityBase>> AbilitySlots;

    UPROPERTY()
    TMap<int32, FTimerHandle> CooldownTimers;

    UPROPERTY()
    TMap<int32, FTimerHandle> ChannelTimers;

    UPROPERTY()
    int32 CurrentlyChannelingSlot = -1;

    UPROPERTY()
    float ManaRegenAccumulator = 0.0f;
};
