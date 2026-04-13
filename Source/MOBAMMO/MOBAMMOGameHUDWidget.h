#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "MOBAMMOGameHUDWidget.generated.h"

class UBorder;
class UHorizontalBox;
class UOverlay;
class UProgressBar;
class UTextBlock;
class UMOBAMMOBackendSubsystem;
class AMOBAMMOGameState;
class AMOBAMMOPlayerState;

UCLASS(BlueprintType, Blueprintable)
class MOBAMMO_API UMOBAMMOGameHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UFUNCTION(BlueprintCallable, Category="HUD")
    void RefreshFromBackend();

    UFUNCTION(BlueprintPure, Category="HUD")
    bool ShouldBeVisible() const;

protected:
    UFUNCTION(BlueprintCallable, Category="HUD")
    UMOBAMMOBackendSubsystem* GetBackendSubsystem() const;

private:
    void BuildLayout();
    void BindToSubsystem(UMOBAMMOBackendSubsystem* BackendSubsystem);
    void BindToReplicatedState();
    void UpdateTexts();

    UFUNCTION()
    void HandleBackendStateChanged();

    UFUNCTION()
    void HandleReplicatedStateChanged();

    UPROPERTY(Transient)
    TObjectPtr<UOverlay> RootOverlay;

    UPROPERTY(Transient)
    TObjectPtr<UBorder> RootBorder;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> StatusText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> DetailText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ControlsText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ActionHintText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> ArcChargeText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> CombatLogText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> CombatEventText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> AbilityTrayText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> TargetPanelText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> TargetStatusText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> RosterText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> FloatingTargetFeedbackText;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> RespawnHintText;

    UPROPERTY(Transient)
    TObjectPtr<UProgressBar> HealthBar;

    UPROPERTY(Transient)
    TObjectPtr<UProgressBar> ManaBar;

    UPROPERTY(Transient)
    TObjectPtr<UProgressBar> TargetHealthBar;

    UPROPERTY(Transient)
    TObjectPtr<UProgressBar> TargetManaBar;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UProgressBar>> AbilityCooldownBars;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextBlock>> AbilityCooldownTexts;

    bool bBoundToSubsystem = false;
    bool bBoundToPlayerState = false;
    bool bBoundToGameState = false;
    FString CachedCombatEvent;
    float CombatEventHighlightRemaining = 0.0f;
};
