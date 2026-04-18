#pragma once

#include "AbilityBase.h"
#include "ProjectileAbility.generated.h"

class AActor;

UCLASS(EditInlineNew, Blueprintable)
class MOBAMMOABILITIES_API UProjectileAbility : public UAbilityBase
{
    GENERATED_BODY()

public:
    UProjectileAbility();

    virtual void Activate(const FAbilityActivationInfo& Info) override;
    virtual bool CanActivate(const FAbilityActivationInfo& Info) const override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Projectile")
    TSubclassOf<AActor> ProjectileClass;

    UFUNCTION(BlueprintImplementableEvent)
    void K2_OnProjectileSpawned(AActor* SpawnedProjectile);

protected:
    UFUNCTION()
    virtual void SpawnProjectile(const FAbilityActivationInfo& Info);
};
