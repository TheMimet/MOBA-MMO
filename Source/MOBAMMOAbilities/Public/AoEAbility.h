#pragma once

#include "AbilityBase.h"
#include "AoEAbility.generated.h"

UCLASS(EditInlineNew, Blueprintable)
class MOBAMMOABILITIES_API UAoEAbility : public UAbilityBase
{
    GENERATED_BODY()

public:
    UAoEAbility();

    virtual void Activate(const FAbilityActivationInfo& Info) override;
    virtual bool CanActivate(const FAbilityActivationInfo& Info) const override;

    UFUNCTION(BlueprintImplementableEvent)
    void K2_OnAoEActivated(const FVector& Center, float Radius);

protected:
    virtual void ApplyAoEEffect(const FVector& Center);

    UFUNCTION()
    void ApplyEffectToActorInAoE(AActor* Target, const FVector& Center);
};
