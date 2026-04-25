#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOBAMMOInventoryTypes.h"
#include "MOBAMMOInventoryWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UCanvasPanelSlot;
class UHorizontalBox;
class UOverlay;
class USizeBox;
class UTextBlock;
class UUniformGridPanel;
class UVerticalBox;
class UWrapBox;

UCLASS()
class MOBAMMO_API UMOBAMMOInventoryWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Inventory")
    void RefreshInventory();

    UFUNCTION(BlueprintCallable, Category="Inventory")
    void ToggleVisibility();

    UFUNCTION(BlueprintPure, Category="Inventory")
    bool IsInventoryVisible() const { return bIsVisible; }

protected:
    virtual void NativeConstruct() override;
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    // Layout
    void BuildLayout();
    void BindToPlayerState();
    void BuildTitleBar(UVerticalBox* Parent);
    void BuildSlotGrid(UVerticalBox* Parent);
    void BuildFooter(UVerticalBox* Parent);
    void BuildTooltip();

    // Slot rendering
    UBorder* CreateItemSlot(int32 SlotIndex);
    void PopulateSlot(UBorder* SlotBorder, const FMOBAMMOInventoryItem& Item);
    void ClearSlot(UBorder* SlotBorder, int32 SlotIndex);

    // Tooltip
    void ShowTooltip(const FMOBAMMOInventoryItem& Item, const FGeometry& SlotGeometry);
    void HideTooltip();

    // Color helpers
    static FLinearColor GetRarityColor(const FString& ItemId);
    static FString GetItemDisplayName(const FString& ItemId);
    static FString GetItemDescription(const FString& ItemId);
    static FString GetRarityLabel(const FString& ItemId);

    UFUNCTION()
    void HandlePlayerStateUpdated();

    UFUNCTION()
    void HandleCloseClicked();

    // Constants
    static constexpr int32 GridColumns = 6;
    static constexpr int32 GridRows = 4;
    static constexpr int32 TotalSlots = GridColumns * GridRows;
    static constexpr float SlotSize = 72.0f;
    static constexpr float SlotPadding = 4.0f;

    // Root
    UPROPERTY() UBorder* OuterGlowBorder = nullptr;
    UPROPERTY() UBorder* BackgroundPanel = nullptr;
    UPROPERTY() UVerticalBox* MainContainer = nullptr;

    // Title bar
    UPROPERTY() UTextBlock* TitleText = nullptr;
    UPROPERTY() UTextBlock* ItemCountText = nullptr;
    UPROPERTY() UBorder* CloseButton = nullptr;

    // Grid
    UPROPERTY() UUniformGridPanel* SlotGrid = nullptr;
    UPROPERTY() TArray<UBorder*> SlotBorders;

    // Empty state
    UPROPERTY() UTextBlock* EmptyText = nullptr;

    // Footer
    UPROPERTY() UTextBlock* WeightText = nullptr;
    UPROPERTY() UTextBlock* HintText = nullptr;

    // Tooltip overlay
    UPROPERTY() UBorder* TooltipPanel = nullptr;
    UPROPERTY() UTextBlock* TooltipNameText = nullptr;
    UPROPERTY() UTextBlock* TooltipRarityText = nullptr;
    UPROPERTY() UTextBlock* TooltipDescText = nullptr;
    UPROPERTY() UTextBlock* TooltipQtyText = nullptr;

    // State
    bool bIsVisible = false;
    bool bBoundToPlayerState = false;
    int32 HoveredSlotIndex = -1;

    // Animation
    float FadeAlpha = 0.0f;
    float TargetFadeAlpha = 0.0f;
    static constexpr float FadeSpeed = 8.0f;
};
