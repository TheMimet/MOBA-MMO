#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/EditableTextBox.h"

#include "MOBAMMOGameHUDWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UCanvasPanelSlot;
class UEditableTextBox;
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

    UFUNCTION(BlueprintCallable, Category="HUD")
    void OpenChat();

    UFUNCTION(BlueprintCallable, Category="HUD")
    void CloseChat();

    UFUNCTION(BlueprintPure, Category="HUD")
    bool IsChatOpen() const { return bChatOpen; }

    UFUNCTION(BlueprintPure, Category="HUD")
    bool IsInventoryOpen() const;

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
    void BuildChatPanel(UCanvasPanel* Canvas);
    void BuildScoreBar(UCanvasPanel* Canvas);
    void BuildCenterNotifications(UCanvasPanel* Canvas);
    void BuildMinimap(UCanvasPanel* Canvas);
    void UpdateMinimap();
    void PlaceMinimapDot(UBorder* Dot, float WorldX, float WorldY) const;
    void BuildQuestPanel(UCanvasPanel* Canvas);
    void UpdateQuestPanel();
    UFUNCTION()
    void HandleChatTextCommitted(const FText& Text, ETextCommit::Type CommitType);

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
    UPROPERTY(Transient) TObjectPtr<UProgressBar> XPBar;
    UPROPERTY(Transient) TObjectPtr<UTextBlock>   XPValueText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock>   LevelUpFlashText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock>   GoldText;

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

    // Chat panel (bottom-left, above player frame)
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ChatHistoryText;
    UPROPERTY(Transient) TObjectPtr<UBorder> ChatInputBorder;
    UPROPERTY(Transient) TObjectPtr<UEditableTextBox> ChatInputBox;
    bool bChatOpen = false;

    // Score bar (top-center)
    UPROPERTY(Transient) TObjectPtr<UTextBlock> KillDeathText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> PlayersOnlineText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> RosterText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> SaveConnectionText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> SkillPointText;

    // Center notifications
    UPROPERTY(Transient) TObjectPtr<UTextBlock> CombatEventText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ActionHintText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> RespawnHintText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ArcChargeText;

    // Floating damage text
    UPROPERTY(Transient) TObjectPtr<UTextBlock> FloatingFeedbackText;

    // Quest panel (top-left, below combat log)
    UPROPERTY(Transient) TObjectPtr<UBorder>                QuestPanelBorder;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>>     QuestRowTexts;

    // Minimap (top-right, below target frame)
    UPROPERTY(Transient) TObjectPtr<UBorder>    MinimapBg;
    UPROPERTY(Transient) TObjectPtr<UBorder>    MinimapSelfDot;
    UPROPERTY(Transient) TObjectPtr<UBorder>    MinimapDummyDot;
    UPROPERTY(Transient) TObjectPtr<UBorder>    MinimapMinionDot;
    UPROPERTY(Transient) TObjectPtr<UBorder>    MinimapVendorDot;
    UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> MinimapOtherDots;

    bool bBoundToSubsystem = false;
    bool bBoundToPlayerState = false;
    bool bBoundToGameState = false;
    FString CachedCombatEvent;
    float CombatEventHighlightRemaining = 0.0f;

    // Level-up detection / animation
    int32 CachedCharacterLevel = 0;
    float LevelUpFlashRemaining = 0.0f;
};
