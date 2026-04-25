#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "MOBAMMOGameHUDWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UCanvasPanelSlot;
class UHorizontalBox;
class UImage;
class UOverlay;
class UProgressBar;
class USizeBox;
class UTextBlock;
class UVerticalBox;
class UMOBAMMOBackendSubsystem;
class AMOBAMMOGameState;
class AMOBAMMOPlayerState;
class UMOBAMMOInventoryWidget;

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

    UFUNCTION(BlueprintCallable, Category="HUD")
    void ToggleInventory();

    // Inventory
    UPROPERTY(Transient) TObjectPtr<UMOBAMMOInventoryWidget> InventoryWidget;

    UFUNCTION(BlueprintPure, Category="HUD")
    bool ShouldBeVisible() const;

    bool IsAbilityBarReady() const;

protected:
    UFUNCTION(BlueprintCallable, Category="HUD")
    UMOBAMMOBackendSubsystem* GetBackendSubsystem() const;

private:
    void BuildLayout();
    void BuildPlayerFrame(UCanvasPanel* Canvas);
    void BuildAbilityBar(UCanvasPanel* Canvas);
    void BuildTargetFrame(UCanvasPanel* Canvas);
    void BuildCombatLog(UCanvasPanel* Canvas);
    void BuildScoreBar(UCanvasPanel* Canvas);
    void BuildCenterNotifications(UCanvasPanel* Canvas);

    void BindToSubsystem(UMOBAMMOBackendSubsystem* BackendSubsystem);
    void BindToReplicatedState();
    void UpdateTexts();
    void EnsureAbilityBarVisible();

    UBorder* MakeGlassPanel(const FLinearColor& Tint, float Alpha, float CornerRadius = 0.0f) const;
    UProgressBar* MakeStyledBar(const FLinearColor& FillColor, const FLinearColor& BgColor, float Height) const;

    UFUNCTION()
    void HandleBackendStateChanged();

    UFUNCTION()
    void HandleReplicatedStateChanged();

    // Root
    UPROPERTY(Transient) TObjectPtr<UCanvasPanel> RootCanvas;

    // Player frame (bottom-left)
    UPROPERTY(Transient) TObjectPtr<UTextBlock> PlayerNameText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> PlayerClassLevelText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> PlayerLifeStateText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> HealthValueText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ManaValueText;
    UPROPERTY(Transient) TObjectPtr<UProgressBar> HealthBar;
    UPROPERTY(Transient) TObjectPtr<UProgressBar> ManaBar;

    // Ability bar (bottom-center)
    UPROPERTY(Transient) TObjectPtr<UBorder> AbilityBarBorder;
    UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> AbilitySlotBorders;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> AbilityKeyTexts;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> AbilityNameTexts;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> AbilityStateTexts;
    UPROPERTY(Transient) TArray<TObjectPtr<UProgressBar>> AbilityCooldownBars;

    // Target frame (top-right)
    UPROPERTY(Transient) TObjectPtr<UBorder> TargetFrameBorder;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> TargetNameText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> TargetClassText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> TargetRangeText;
    UPROPERTY(Transient) TObjectPtr<UProgressBar> TargetHealthBar;
    UPROPERTY(Transient) TObjectPtr<UProgressBar> TargetManaBar;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> TargetHealthValueText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> TargetManaValueText;

    // Combat log (left side)
    UPROPERTY(Transient) TObjectPtr<UTextBlock> CombatLogText;

    // Score bar (top-center)
    UPROPERTY(Transient) TObjectPtr<UTextBlock> KillDeathText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> PlayersOnlineText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> RosterText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> SaveConnectionText;

    // Center notifications
    UPROPERTY(Transient) TObjectPtr<UTextBlock> CombatEventText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ActionHintText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> RespawnHintText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ArcChargeText;

    // Floating damage text
    UPROPERTY(Transient) TObjectPtr<UTextBlock> FloatingFeedbackText;

    bool bBoundToSubsystem = false;
    bool bBoundToPlayerState = false;
    bool bBoundToGameState = false;
    FString CachedCombatEvent;
    float CombatEventHighlightRemaining = 0.0f;
};
