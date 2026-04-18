#pragma once

#include "CoreMinimal.h"
#include "AbilityTypes.h"
#include "GameplayTagContainer.h"
#include "AbilityBase.generated.h"

class UAbilityComponent;
class UNiagaraSystem;
class USoundBase;

UCLASS(Abstract, Blueprintable, EditInlineNew)
class MOBAMMOABILITIES_API UAbilityBase : public UObject
{
    GENERATED_BODY()

public:
    UAbilityBase();

    virtual void Initialize(UAbilityComponent* InOwnerComponent);
    virtual bool CanActivate(const FAbilityActivationInfo& Info) const;
    virtual void Activate(const FAbilityActivationInfo& Info);
    virtual void Tick(float DeltaTime);
    virtual void EndAbility(bool bWasSuccessful);
    virtual void CancelAbility();

    UFUNCTION(BlueprintImplementableEvent)
    void K2_OnActivated(const FAbilityActivationInfo& Info);

    UFUNCTION(BlueprintImplementableEvent)
    void K2_OnEnded(bool bWasSuccessful);

    UFUNCTION(BlueprintImplementableEvent)
    void K2_OnCancelled();

    UFUNCTION(BlueprintImplementableEvent)
    bool K2_CanActivate(const FAbilityActivationInfo& Info) const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability")
    FName AbilityName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability")
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability")
    int32 Slot = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability")
    FKey SlotKey;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability|Type")
    EAbilityType AbilityType = EAbilityType::Instant;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability|Targeting")
    EAbilityTargetType TargetType = EAbilityTargetType::SingleTarget;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability|Cost")
    FAbilityCost Cost;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability|Requirement")
    FAbilityRequirement Requirements;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability|Damage")
    FAbilityDamage Damage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability|AoE")
    FAbilityAoE AoE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability|Projectile")
    FAbilityProjectile Projectile;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability|Channel")
    FAbilityChannel Channel;


    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability|VFX")
    UNiagaraSystem* CastVFX;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability|VFX")
    UNiagaraSystem* ImpactVFX;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability|VFX")
    UNiagaraSystem* LoopVFX;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability|SFX")
    USoundBase* CastSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability|SFX")
    USoundBase* ImpactSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability|Tags")
    FGameplayTagContainer GrantedTags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ability|Tags")
    FGameplayTagContainer ActivationBlockedTags;

    friend class UAbilityComponent;

protected:
    UPROPERTY()
    TWeakObjectPtr<UAbilityComponent> OwnerComponent;

    UPROPERTY()
    FAbilityActivationInfo CurrentActivationInfo;

    bool bIsActive = false;
    float ActiveTime = 0.0f;
    int32 TicksRemaining = 0;

    UFUNCTION()
    virtual void OnChannelTick();

    UFUNCTION()
    void ApplyEffectsToTarget(AActor* Target, const FVector& Location);

    UFUNCTION()
    TArray<AActor*> GetActorsInAoE(const FVector& Center, bool bIgnoreOwner = true) const;

    UFUNCTION()
    AActor* GetProjectileTarget(const FAbilityActivationInfo& Info) const;
};
