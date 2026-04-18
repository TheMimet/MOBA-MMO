#pragma once

#include "AbilityBase.h"
#include "ChannelAbility.generated.h"

UCLASS(EditInlineNew, Blueprintable)
class MOBAMMOABILITIES_API UChannelAbility : public UAbilityBase
{
    GENERATED_BODY()

public:
    UChannelAbility();

    virtual void Activate(const FAbilityActivationInfo& Info) override;
    virtual void Tick(float DeltaTime) override;
    virtual void EndAbility(bool bWasSuccessful) override;
    virtual bool CanActivate(const FAbilityActivationInfo& Info) const override;

    UFUNCTION(BlueprintImplementableEvent)
    void K2_OnChannelStarted(const FAbilityActivationInfo& Info);

    UFUNCTION(BlueprintImplementableEvent)
    void K2_OnChannelTick(float ElapsedTime, float TotalDuration);

    UFUNCTION(BlueprintImplementableEvent)
    void K2_OnChannelEnded(bool bCompleted);

protected:
    virtual void ExecuteChannelTick();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Channel")
    float TotalChannelDuration = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Channel")
    float TickInterval = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Channel")
    float DamagePerTick = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Channel")
    float HealPerTick = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Channel")
    bool bChannelBreaksOnDamage = true;

private:
    float CurrentChannelTime = 0.0f;
    float TimeSinceLastTick = 0.0f;
};
