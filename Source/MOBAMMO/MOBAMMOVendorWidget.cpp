#include "MOBAMMOVendorWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Fonts/SlateFontInfo.h"
#include "GameFramework/PlayerController.h"
#include "MOBAMMOPlayerController.h"
#include "MOBAMMOPlayerState.h"

namespace VendorColors
{
    static const FLinearColor BgDark      (0.020f, 0.025f, 0.045f, 0.95f);
    static const FLinearColor TitleBar    (0.030f, 0.038f, 0.068f, 1.0f);
    static const FLinearColor BorderAccent(0.220f, 0.180f, 0.080f, 0.55f);
    static const FLinearColor RowEven     (0.040f, 0.048f, 0.078f, 1.0f);
    static const FLinearColor RowOdd      (0.028f, 0.034f, 0.058f, 1.0f);
    static const FLinearColor BuyNormal   (0.050f, 0.140f, 0.060f, 1.0f);
    static const FLinearColor BuyHover    (0.080f, 0.220f, 0.090f, 1.0f);
    static const FLinearColor TextPrimary (0.920f, 0.940f, 0.960f, 1.0f);
    static const FLinearColor TextGold    (0.960f, 0.820f, 0.280f, 1.0f);
    static const FLinearColor TextGreen   (0.340f, 0.920f, 0.440f, 1.0f);
    static const FLinearColor TextHint    (0.440f, 0.500f, 0.580f, 1.0f);
    static const FLinearColor CloseRed    (0.180f, 0.060f, 0.060f, 1.0f);
    static const FLinearColor Separator   (0.180f, 0.160f, 0.060f, 0.40f);
}

static UTextBlock* VndMakeText(UWidgetTree* Tree,
                                const FString& Content,
                                int32 FontSize,
                                const FLinearColor& Color,
                                bool bBold = false)
{
    UTextBlock* T = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    T->SetText(FText::FromString(Content));
    FSlateFontInfo F = T->GetFont();
    F.Size = FontSize;
    if (bBold) { F.TypefaceFontName = TEXT("Bold"); }
    T->SetFont(F);
    T->SetColorAndOpacity(FSlateColor(Color));
    return T;
}

// ─────────────────────────────────────────────────────────────────────────────
void UMOBAMMOVendorWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    BuildLayout();
}

void UMOBAMMOVendorWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetRenderOpacity(0.0f);
    SetVisibility(ESlateVisibility::Collapsed);
    RefreshGoldText();
}

void UMOBAMMOVendorWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    const float Target = bIsVisible ? 1.0f : 0.0f;
    if (FMath::Abs(FadeAlpha - Target) > 0.001f)
    {
        FadeAlpha = FMath::FInterpTo(FadeAlpha, Target, InDeltaTime, FadeSpeed);
        SetRenderOpacity(FadeAlpha);
        if (FadeAlpha < 0.01f && Target == 0.0f)
        {
            SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void UMOBAMMOVendorWidget::SetVendorVisible(bool bVisible)
{
    bIsVisible = bVisible;
    if (bVisible)
    {
        SetVisibility(ESlateVisibility::Visible);
        RefreshGoldText();
    }
}

void UMOBAMMOVendorWidget::RefreshGoldText()
{
    if (!GoldText) { return; }

    const AMOBAMMOPlayerState* PS = GetOwningPlayerState<AMOBAMMOPlayerState>();
    const int32 Gold = PS ? PS->GetGold() : 0;

    // Format with thousands separator: 1,234 g
    const FString Raw = FString::FromInt(Gold);
    FString Formatted;
    int32 StartIdx = Raw.Len() % 3;
    if (StartIdx > 0) { Formatted = Raw.Left(StartIdx); }
    for (int32 i = StartIdx; i < Raw.Len(); i += 3)
    {
        if (!Formatted.IsEmpty()) { Formatted += TEXT(","); }
        Formatted += Raw.Mid(i, 3);
    }

    GoldText->SetText(FText::FromString(FString::Printf(TEXT("%s g"), *Formatted)));
}

// ─────────────────────────────────────────────────────────────────────────────
void UMOBAMMOVendorWidget::BuildLayout()
{
    // ── Outer glow border ────────────────────────────────────────────────────
    UBorder* OuterBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("VndOuter"));
    OuterBorder->SetBrushColor(VendorColors::BorderAccent);
    OuterBorder->SetPadding(FMargin(1.0f));
    WidgetTree->RootWidget = OuterBorder;

    BackgroundPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("VndBg"));
    BackgroundPanel->SetBrushColor(VendorColors::BgDark);
    BackgroundPanel->SetPadding(FMargin(0.0f));
    OuterBorder->AddChild(BackgroundPanel);

    UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VndRoot"));
    Cast<UBorderSlot>(BackgroundPanel->AddChild(Root))->SetPadding(FMargin(0.0f));

    // ── Title bar ────────────────────────────────────────────────────────────
    {
        UBorder* TitleBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        TitleBg->SetBrushColor(VendorColors::TitleBar);
        TitleBg->SetPadding(FMargin(14.0f, 10.0f, 14.0f, 10.0f));
        Root->AddChildToVerticalBox(TitleBg);

        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        TitleBg->AddChild(Row);

        // Icon
        UTextBlock* Icon = VndMakeText(WidgetTree, TEXT("✦"), 18, VendorColors::TextGold, true);
        if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Icon))
        {
            S->SetVerticalAlignment(VAlign_Center);
            S->SetPadding(FMargin(0, 0, 8, 0));
        }

        TitleText = VndMakeText(WidgetTree, TEXT("SHOP"), 18, VendorColors::TextPrimary, true);
        if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(TitleText))
        {
            S->SetVerticalAlignment(VAlign_Center);
            S->SetSize(ESlateSizeRule::Fill);
        }

        GoldText = VndMakeText(WidgetTree, TEXT("0 g"), 13, VendorColors::TextGold, true);
        if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(GoldText))
        {
            S->SetVerticalAlignment(VAlign_Center);
            S->SetPadding(FMargin(0, 0, 14, 0));
        }

        // Close button
        CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("VndClose"));
        CloseButton->SetBackgroundColor(VendorColors::CloseRed);
        CloseButton->OnClicked.AddDynamic(this, &UMOBAMMOVendorWidget::HandleCloseClicked);
        UTextBlock* XLabel = VndMakeText(WidgetTree, TEXT("  X  "), 13, FLinearColor(1.0f, 0.4f, 0.4f, 1.0f), true);
        CloseButton->AddChild(XLabel);
        if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(CloseButton))
        {
            S->SetVerticalAlignment(VAlign_Center);
        }
    }

    // ── Thin separator ───────────────────────────────────────────────────────
    {
        USizeBox* Sep = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        Sep->SetHeightOverride(1.0f);
        UBorder* SepLine = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        SepLine->SetBrushColor(VendorColors::Separator);
        Sep->AddChild(SepLine);
        Root->AddChildToVerticalBox(Sep);
    }

    // ── Scrollable item list ─────────────────────────────────────────────────
    {
        ItemScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("VndScroll"));
        ItemScroll->SetOrientation(Orient_Vertical);

        if (UVerticalBoxSlot* S = Root->AddChildToVerticalBox(ItemScroll))
        {
            S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        }

        UVerticalBox* ItemList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VndItems"));
        if (UScrollBoxSlot* S = Cast<UScrollBoxSlot>(ItemScroll->AddChild(ItemList)))
        {
            S->SetPadding(FMargin(0.0f));
        }

        BuyButtons.Reset();
        const TArray<FMOBAMMOVendorItem>& Catalog = UMOBAMMOVendorCatalog::GetCatalog();
        for (int32 i = 0; i < Catalog.Num(); ++i)
        {
            BuildItemRow(ItemList, i, Catalog[i]);
        }
    }

    // ── Footer hint ──────────────────────────────────────────────────────────
    {
        USizeBox* Sep = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        Sep->SetHeightOverride(1.0f);
        UBorder* SepLine = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        SepLine->SetBrushColor(VendorColors::Separator);
        Sep->AddChild(SepLine);
        Root->AddChildToVerticalBox(Sep);

        UBorder* FooterBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        FooterBg->SetBrushColor(VendorColors::TitleBar);
        FooterBg->SetPadding(FMargin(14.0f, 7.0f));
        Root->AddChildToVerticalBox(FooterBg);

        HintText = VndMakeText(WidgetTree, TEXT("[V] Close shop"), 10, VendorColors::TextHint);
        FooterBg->AddChild(HintText);
    }
}

void UMOBAMMOVendorWidget::BuildItemRow(UVerticalBox* Container, int32 SlotIndex, const FMOBAMMOVendorItem& Item)
{
    const FLinearColor RowBg = (SlotIndex % 2 == 0) ? VendorColors::RowEven : VendorColors::RowOdd;

    UBorder* RowBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    RowBorder->SetBrushColor(RowBg);
    RowBorder->SetPadding(FMargin(14.0f, 6.0f, 10.0f, 6.0f));
    Container->AddChildToVerticalBox(RowBorder);

    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    Cast<UBorderSlot>(RowBorder->AddChild(Row))->SetPadding(FMargin(0.0f));

    // Item name (fills space)
    UTextBlock* NameTxt = VndMakeText(WidgetTree, Item.DisplayName, 13, VendorColors::TextPrimary);
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(NameTxt))
    {
        S->SetVerticalAlignment(VAlign_Center);
        S->SetSize(ESlateSizeRule::Fill);
    }

    // Price label
    UTextBlock* PriceTxt = VndMakeText(WidgetTree,
        FString::Printf(TEXT("%d g"), Item.Price), 12, VendorColors::TextGold, true);
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(PriceTxt))
    {
        S->SetVerticalAlignment(VAlign_Center);
        S->SetPadding(FMargin(0, 0, 10, 0));
    }

    // Buy button
    UButton* BuyBtn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
    BuyBtn->SetBackgroundColor(VendorColors::BuyNormal);

    // AddDynamic needs a reflected member function name, not a function pointer stored in an array.
    switch (SlotIndex)
    {
    case 0: BuyBtn->OnClicked.AddDynamic(this, &UMOBAMMOVendorWidget::HandleBuy0); break;
    case 1: BuyBtn->OnClicked.AddDynamic(this, &UMOBAMMOVendorWidget::HandleBuy1); break;
    case 2: BuyBtn->OnClicked.AddDynamic(this, &UMOBAMMOVendorWidget::HandleBuy2); break;
    case 3: BuyBtn->OnClicked.AddDynamic(this, &UMOBAMMOVendorWidget::HandleBuy3); break;
    case 4: BuyBtn->OnClicked.AddDynamic(this, &UMOBAMMOVendorWidget::HandleBuy4); break;
    case 5: BuyBtn->OnClicked.AddDynamic(this, &UMOBAMMOVendorWidget::HandleBuy5); break;
    case 6: BuyBtn->OnClicked.AddDynamic(this, &UMOBAMMOVendorWidget::HandleBuy6); break;
    case 7: BuyBtn->OnClicked.AddDynamic(this, &UMOBAMMOVendorWidget::HandleBuy7); break;
    default: break;
    }

    UTextBlock* BuyLabel = VndMakeText(WidgetTree, TEXT(" Buy "), 12, VendorColors::TextGreen, true);
    BuyBtn->AddChild(BuyLabel);
    BuyButtons.Add(BuyBtn);

    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(BuyBtn))
    {
        S->SetVerticalAlignment(VAlign_Center);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Close
// ─────────────────────────────────────────────────────────────────────────────
void UMOBAMMOVendorWidget::HandleCloseClicked()
{
    SetVendorVisible(false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Buy dispatch — each handler just delegates to the indexed helper
// ─────────────────────────────────────────────────────────────────────────────
void UMOBAMMOVendorWidget::DispatchBuy(int32 SlotIndex)
{
    const TArray<FMOBAMMOVendorItem>& Catalog = UMOBAMMOVendorCatalog::GetCatalog();
    if (!Catalog.IsValidIndex(SlotIndex)) { return; }
    const FString& ItemId = Catalog[SlotIndex].ItemId;

    if (AMOBAMMOPlayerController* PC = Cast<AMOBAMMOPlayerController>(GetOwningPlayer()))
    {
        PC->RequestPurchaseFromVendor(ItemId);
    }

    // Optimistic gold refresh (true update will come via replication)
    RefreshGoldText();
}

void UMOBAMMOVendorWidget::HandleBuy0() { DispatchBuy(0); }
void UMOBAMMOVendorWidget::HandleBuy1() { DispatchBuy(1); }
void UMOBAMMOVendorWidget::HandleBuy2() { DispatchBuy(2); }
void UMOBAMMOVendorWidget::HandleBuy3() { DispatchBuy(3); }
void UMOBAMMOVendorWidget::HandleBuy4() { DispatchBuy(4); }
void UMOBAMMOVendorWidget::HandleBuy5() { DispatchBuy(5); }
void UMOBAMMOVendorWidget::HandleBuy6() { DispatchBuy(6); }
void UMOBAMMOVendorWidget::HandleBuy7() { DispatchBuy(7); }
