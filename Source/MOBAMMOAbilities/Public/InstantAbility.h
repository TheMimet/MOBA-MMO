#pragma once

#include "AbilityBase.h"
#include "InstantAbility.generated.h"

UCLASS(EditInlineNew, Blueprintable)
class MOBAMMOABILITIES_API UInstantAbility : public UAbilityBase
{
    GENERATED_BODY()

public:
    UInstantAbility();

    virtual void Activate(const FAbilityActivationInfo& Info) override;
    virtual bool CanActivate(const FAbilityActivationInfo& Info) const override;

    UFUNCTION(BlueprintImplementableEvent)
    void K2_ApplyInstantEffect(const FAbilityActivationInfo& Info, AActor* Target);

protected:
    virtual void ApplyInstantEffect(AActor* Target, const FVector& ImpactLocation);
};
