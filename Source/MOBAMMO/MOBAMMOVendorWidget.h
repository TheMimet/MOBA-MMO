#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOBAMMOVendorCatalog.h"
#include "MOBAMMOVendorWidget.generated.h"

class UBorder;
class UButton;
class UScrollBox;
class USizeBox;
class UTextBlock;
class UVerticalBox;

UCLASS()
class MOBAMMO_API UMOBAMMOVendorWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Show or hide the vendor panel. */
    void SetVendorVisible(bool bVisible);
    bool IsVendorVisible() const { return bIsVisible; }

    /** Refresh the gold display (called after a purchase reply). */
    void RefreshGoldText();

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    void BuildLayout();
    void BuildItemRow(UVerticalBox* Container, int32 SlotIndex, const FMOBAMMOVendorItem& Item);

    /** Called by each per-slot buy button. */
    void DispatchBuy(int32 SlotIndex);

    UFUNCTION() void HandleCloseClicked();
    UFUNCTION() void HandleBuy0();
    UFUNCTION() void HandleBuy1();
    UFUNCTION() void HandleBuy2();
    UFUNCTION() void HandleBuy3();
    UFUNCTION() void HandleBuy4();
    UFUNCTION() void HandleBuy5();
    UFUNCTION() void HandleBuy6();
    UFUNCTION() void HandleBuy7();

    // Root panel
    UPROPERTY(Transient) TObjectPtr<UBorder> BackgroundPanel;

    // Header
    UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> GoldText;
    UPROPERTY(Transient) TObjectPtr<UButton>    CloseButton;

    // Item list
    UPROPERTY(Transient) TObjectPtr<UScrollBox> ItemScroll;

    // Per-row buy buttons
    UPROPERTY(Transient) TArray<TObjectPtr<UButton>> BuyButtons;

    // Footer hint
    UPROPERTY(Transient) TObjectPtr<UTextBlock> HintText;

    bool  bIsVisible = false;
    float FadeAlpha  = 0.0f;
    static constexpr float FadeSpeed = 8.0f;
};
