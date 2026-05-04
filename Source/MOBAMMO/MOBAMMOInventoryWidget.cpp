#include "MOBAMMOInventoryWidget.h"
#include "MOBAMMOPlayerController.h"
#include "MOBAMMOPlayerState.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Fonts/SlateFontInfo.h"
#include "Blueprint/WidgetTree.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

namespace InvColors
{
    static const FLinearColor PanelBg       (0.020f, 0.024f, 0.040f, 0.92f);
    static const FLinearColor PanelBorder   (0.180f, 0.220f, 0.320f, 0.50f);
    static const FLinearColor TitleBar      (0.030f, 0.035f, 0.060f, 1.0f);
    static const FLinearColor SlotBg        (0.060f, 0.065f, 0.090f, 1.0f);
    static const FLinearColor SlotEmpty     (0.040f, 0.045f, 0.065f, 0.6f);
    static const FLinearColor SlotHover     (0.120f, 0.140f, 0.220f, 1.0f);
    static const FLinearColor TextPrimary   (0.920f, 0.940f, 0.960f, 1.0f);
    static const FLinearColor TextSecondary (0.500f, 0.560f, 0.640f, 1.0f);
    static const FLinearColor TextGold      (0.960f, 0.820f, 0.440f, 1.0f);
    static const FLinearColor TextGreen     (0.340f, 0.920f, 0.560f, 1.0f);
    static const FLinearColor CloseBtn      (0.960f, 0.320f, 0.360f, 1.0f);
    static const FLinearColor TooltipBg     (0.015f, 0.018f, 0.030f, 0.96f);
    // Rarity
    static const FLinearColor RarityCommon  (0.650f, 0.680f, 0.720f, 1.0f);
    static const FLinearColor RarityUncommon(0.300f, 0.820f, 0.400f, 1.0f);
    static const FLinearColor RarityRare    (0.260f, 0.520f, 0.960f, 1.0f);
    static const FLinearColor RarityEpic    (0.640f, 0.300f, 0.920f, 1.0f);
    static const FLinearColor RarityLegend  (0.960f, 0.640f, 0.200f, 1.0f);
}

static UTextBlock* MakeText(UWidgetTree* Tree, const FString& Content, int32 FontSize, const FLinearColor& Color, bool bBold = false)
{
    UTextBlock* T = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    T->SetText(FText::FromString(Content));
    FSlateFontInfo F = T->GetFont();
    F.Size = FontSize;
    if (bBold) F.TypefaceFontName = TEXT("Bold");
    T->SetFont(F);
    T->SetColorAndOpacity(FSlateColor(Color));
    return T;
}

void UMOBAMMOInventoryWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    BuildLayout();
}

void UMOBAMMOInventoryWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetRenderOpacity(0.0f);
    SetVisibility(ESlateVisibility::Collapsed);
    BindToPlayerState();
}

void UMOBAMMOInventoryWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (FMath::Abs(FadeAlpha - TargetFadeAlpha) > 0.001f)
    {
        FadeAlpha = FMath::FInterpTo(FadeAlpha, TargetFadeAlpha, InDeltaTime, FadeSpeed);
        SetRenderOpacity(FadeAlpha);
        if (FadeAlpha < 0.01f && TargetFadeAlpha == 0.0f)
        {
            SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}

void UMOBAMMOInventoryWidget::ToggleVisibility()
{
    bIsVisible = !bIsVisible;
    if (bIsVisible)
    {
        SetVisibility(ESlateVisibility::Visible);
        TargetFadeAlpha = 1.0f;
        RefreshInventory();
    }
    else
    {
        TargetFadeAlpha = 0.0f;
        HideTooltip();
    }
}

FReply UMOBAMMOInventoryWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bIsVisible && InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        int32 SlotIndex = INDEX_NONE;
        if (TryResolveSlotUnderCursor(InMouseEvent.GetScreenSpacePosition(), SlotIndex))
        {
            UseVisibleInventorySlot(SlotIndex);
            return FReply::Handled();
        }
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UMOBAMMOInventoryWidget::BuildLayout()
{
    // Outer glow border
    OuterGlowBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("OuterGlow"));
    OuterGlowBorder->SetBrushColor(InvColors::PanelBorder);
    OuterGlowBorder->SetPadding(FMargin(1.0f));
    WidgetTree->RootWidget = OuterGlowBorder;

    BackgroundPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BgPanel"));
    BackgroundPanel->SetBrushColor(InvColors::PanelBg);
    BackgroundPanel->SetPadding(FMargin(0.0f));
    OuterGlowBorder->AddChild(BackgroundPanel);

    MainContainer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Main"));
    Cast<UBorderSlot>(BackgroundPanel->AddChild(MainContainer))->SetPadding(FMargin(0.0f));

    BuildTitleBar(MainContainer);
    BuildSlotGrid(MainContainer);
    BuildFooter(MainContainer);
}

void UMOBAMMOInventoryWidget::BuildTitleBar(UVerticalBox* Parent)
{
    UBorder* TitleBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    TitleBg->SetBrushColor(InvColors::TitleBar);
    TitleBg->SetPadding(FMargin(14.0f, 10.0f, 14.0f, 10.0f));
    Parent->AddChildToVerticalBox(TitleBg);

    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    TitleBg->AddChild(Row);

    // Icon placeholder
    UTextBlock* Icon = MakeText(WidgetTree, TEXT("\u2726"), 18, InvColors::TextGold, true);
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Icon))
    {
        S->SetVerticalAlignment(VAlign_Center);
        S->SetPadding(FMargin(0, 0, 8, 0));
    }

    TitleText = MakeText(WidgetTree, TEXT("INVENTORY"), 18, InvColors::TextPrimary, true);
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(TitleText))
    {
        S->SetVerticalAlignment(VAlign_Center);
        S->SetSize(ESlateSizeRule::Fill);
    }

    ItemCountText = MakeText(WidgetTree, TEXT("0 / 24"), 12, InvColors::TextSecondary);
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(ItemCountText))
    {
        S->SetVerticalAlignment(VAlign_Center);
        S->SetPadding(FMargin(0, 0, 12, 0));
    }

    // Close button
    CloseButton = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    CloseButton->SetBrushColor(FLinearColor(0.15f, 0.06f, 0.06f, 1.0f));
    CloseButton->SetPadding(FMargin(8.0f, 4.0f));
    UTextBlock* XText = MakeText(WidgetTree, TEXT("X"), 14, InvColors::CloseBtn, true);
    CloseButton->AddChild(XText);
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(CloseButton))
    {
        S->SetVerticalAlignment(VAlign_Center);
    }

    // Separator line
    UBorder* Sep = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Sep->SetBrushColor(InvColors::PanelBorder);
    USizeBox* SepSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    SepSize->SetHeightOverride(1.0f);
    SepSize->AddChild(Sep);
    Parent->AddChildToVerticalBox(SepSize);
}

void UMOBAMMOInventoryWidget::BuildSlotGrid(UVerticalBox* Parent)
{
    UBorder* GridBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    GridBg->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
    GridBg->SetPadding(FMargin(12.0f, 10.0f));

    if (UVerticalBoxSlot* S = Parent->AddChildToVerticalBox(GridBg))
    {
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }

    SlotGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("Grid"));
    SlotGrid->SetSlotPadding(FMargin(SlotPadding * 0.5f));
    GridBg->AddChild(SlotGrid);

    SlotBorders.Reset();
    for (int32 i = 0; i < TotalSlots; ++i)
    {
        UBorder* ItemSlotBorder = CreateItemSlot(i);
        SlotBorders.Add(ItemSlotBorder);

        USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        Box->SetWidthOverride(SlotSize);
        Box->SetHeightOverride(SlotSize);
        Box->AddChild(ItemSlotBorder);

        SlotGrid->AddChildToUniformGrid(Box, i / GridColumns, i % GridColumns);
    }

    // Empty text overlay
    EmptyText = MakeText(WidgetTree, TEXT("No items yet. Press 8 to cycle through debug items."), 13, InvColors::TextSecondary);
    EmptyText->SetJustification(ETextJustify::Center);
    EmptyText->SetVisibility(ESlateVisibility::Collapsed);
    if (UVerticalBoxSlot* S = Parent->AddChildToVerticalBox(EmptyText))
    {
        S->SetHorizontalAlignment(HAlign_Center);
        S->SetPadding(FMargin(0, -60, 0, 0));
    }
}

UBorder* UMOBAMMOInventoryWidget::CreateItemSlot(int32 SlotIndex)
{
    // Outer border (rarity highlight)
    UBorder* OuterBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    OuterBorder->SetBrushColor(FLinearColor(0.08f, 0.09f, 0.12f, 0.5f));
    OuterBorder->SetPadding(FMargin(1.0f));

    // Inner bg
    UBorder* InnerBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    InnerBg->SetBrushColor(InvColors::SlotEmpty);
    InnerBg->SetPadding(FMargin(4.0f));
    OuterBorder->AddChild(InnerBg);

    // Content layout
    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Cast<UBorderSlot>(InnerBg->AddChild(Content))->SetPadding(FMargin(0.0f));

    // Slot number (top-left)
    UTextBlock* SlotNum = MakeText(WidgetTree, FString::FromInt(SlotIndex + 1), 8, FLinearColor(0.3f, 0.34f, 0.4f, 0.5f));
    if (UVerticalBoxSlot* S = Content->AddChildToVerticalBox(SlotNum))
    {
        S->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
    }

    // Center area (item name placeholder)
    UTextBlock* CenterText = MakeText(WidgetTree, TEXT(""), 11, InvColors::TextPrimary);
    CenterText->SetAutoWrapText(true);
    CenterText->SetJustification(ETextJustify::Center);
    if (UVerticalBoxSlot* S = Content->AddChildToVerticalBox(CenterText))
    {
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        S->SetVerticalAlignment(VAlign_Center);
        S->SetHorizontalAlignment(HAlign_Center);
    }

    // Bottom-right quantity
    UTextBlock* QtyText = MakeText(WidgetTree, TEXT(""), 10, InvColors::TextGreen, true);
    QtyText->SetJustification(ETextJustify::Right);
    if (UVerticalBoxSlot* S = Content->AddChildToVerticalBox(QtyText))
    {
        S->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        S->SetHorizontalAlignment(HAlign_Right);
    }

    return OuterBorder;
}

void UMOBAMMOInventoryWidget::PopulateSlot(UBorder* SlotBorder, const FMOBAMMOInventoryItem& Item)
{
    if (!SlotBorder) return;

    const FLinearColor Rarity = GetRarityColor(Item.ItemId);

    // Outer border = rarity color
    SlotBorder->SetBrushColor(FLinearColor(Rarity.R * 0.5f, Rarity.G * 0.5f, Rarity.B * 0.5f, 0.7f));

    // Inner bg
    UBorder* Inner = Cast<UBorder>(SlotBorder->GetChildAt(0));
    if (!Inner) return;
    Inner->SetBrushColor(InvColors::SlotBg);

    UVerticalBox* Content = Cast<UVerticalBox>(Inner->GetChildAt(0));
    if (!Content || Content->GetChildrenCount() < 3) return;

    // Center text = item name
    UTextBlock* NameText = Cast<UTextBlock>(Content->GetChildAt(1));
    if (NameText)
    {
        const FString DisplayName = Item.SlotIndex >= 0
            ? FString::Printf(TEXT("[E%d] %s"), Item.SlotIndex, *GetItemDisplayName(Item.ItemId))
            : GetItemDisplayName(Item.ItemId);
        NameText->SetText(FText::FromString(DisplayName));
        NameText->SetColorAndOpacity(FSlateColor(Rarity));
    }

    // Qty text
    UTextBlock* QtyText = Cast<UTextBlock>(Content->GetChildAt(2));
    if (QtyText)
    {
        QtyText->SetText(FText::FromString(Item.Quantity > 1 ? FString::Printf(TEXT("x%d"), Item.Quantity) : TEXT("")));
    }
}

void UMOBAMMOInventoryWidget::ClearSlot(UBorder* SlotBorder, int32 SlotIndex)
{
    if (!SlotBorder) return;
    SlotBorder->SetBrushColor(FLinearColor(0.08f, 0.09f, 0.12f, 0.5f));

    UBorder* Inner = Cast<UBorder>(SlotBorder->GetChildAt(0));
    if (!Inner) return;
    Inner->SetBrushColor(InvColors::SlotEmpty);

    UVerticalBox* Content = Cast<UVerticalBox>(Inner->GetChildAt(0));
    if (!Content || Content->GetChildrenCount() < 3) return;

    Cast<UTextBlock>(Content->GetChildAt(1))->SetText(FText::GetEmpty());
    Cast<UTextBlock>(Content->GetChildAt(2))->SetText(FText::GetEmpty());
}

void UMOBAMMOInventoryWidget::BuildFooter(UVerticalBox* Parent)
{
    // Separator
    UBorder* Sep = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Sep->SetBrushColor(InvColors::PanelBorder);
    USizeBox* SepSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    SepSize->SetHeightOverride(1.0f);
    SepSize->AddChild(Sep);
    Parent->AddChildToVerticalBox(SepSize);

    UBorder* FooterBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    FooterBg->SetBrushColor(InvColors::TitleBar);
    FooterBg->SetPadding(FMargin(14.0f, 8.0f));
    Parent->AddChildToVerticalBox(FooterBg);

    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    FooterBg->AddChild(Row);

    HintText = MakeText(WidgetTree, TEXT("[I] Close   Right Click: Use/Equip   [8] Grant   [9] Use Potion   [0] Equip Cycle"), 10, InvColors::TextSecondary);
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(HintText))
    {
        S->SetSize(ESlateSizeRule::Fill);
        S->SetVerticalAlignment(VAlign_Center);
    }

    WeightText = MakeText(WidgetTree, TEXT(""), 10, InvColors::TextSecondary);
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(WeightText))
    {
        S->SetVerticalAlignment(VAlign_Center);
    }
}

void UMOBAMMOInventoryWidget::BuildTooltip() { /* reserved for future */ }
void UMOBAMMOInventoryWidget::ShowTooltip(const FMOBAMMOInventoryItem&, const FGeometry&) {}
void UMOBAMMOInventoryWidget::HideTooltip() {}
void UMOBAMMOInventoryWidget::HandleCloseClicked() { ToggleVisibility(); }

void UMOBAMMOInventoryWidget::BindToPlayerState()
{
    if (bBoundToPlayerState) return;
    APlayerController* PC = GetOwningPlayer();
    if (!PC) return;
    AMOBAMMOPlayerState* PS = PC->GetPlayerState<AMOBAMMOPlayerState>();
    if (!PS) return;
    PS->OnReplicatedStateUpdated.AddDynamic(this, &UMOBAMMOInventoryWidget::HandlePlayerStateUpdated);
    bBoundToPlayerState = true;
}

void UMOBAMMOInventoryWidget::HandlePlayerStateUpdated()
{
    if (bIsVisible) RefreshInventory();
}

bool UMOBAMMOInventoryWidget::TryResolveSlotUnderCursor(const FVector2D& ScreenPosition, int32& OutSlotIndex) const
{
    OutSlotIndex = INDEX_NONE;

    for (int32 i = 0; i < SlotBorders.Num(); ++i)
    {
        const UBorder* SlotBorder = SlotBorders[i];
        if (!SlotBorder)
        {
            continue;
        }

        if (SlotBorder->GetCachedGeometry().IsUnderLocation(ScreenPosition))
        {
            OutSlotIndex = i;
            return true;
        }
    }

    return false;
}

void UMOBAMMOInventoryWidget::UseVisibleInventorySlot(int32 SlotIndex)
{
    APlayerController* PC = GetOwningPlayer();
    if (!PC)
    {
        return;
    }

    const AMOBAMMOPlayerState* PS = PC->GetPlayerState<AMOBAMMOPlayerState>();
    if (!PS)
    {
        return;
    }

    const TArray<FMOBAMMOInventoryItem>& Items = PS->GetInventoryItems();
    if (!Items.IsValidIndex(SlotIndex))
    {
        return;
    }

    if (AMOBAMMOPlayerController* MOBAController = Cast<AMOBAMMOPlayerController>(PC))
    {
        MOBAController->RequestUseInventoryItem(Items[SlotIndex].ItemId);
    }
}

void UMOBAMMOInventoryWidget::RefreshInventory()
{
    if (SlotBorders.Num() < TotalSlots) return;

    APlayerController* PC = GetOwningPlayer();
    if (!PC) return;
    AMOBAMMOPlayerState* PS = PC->GetPlayerState<AMOBAMMOPlayerState>();
    if (!PS) return;

    const TArray<FMOBAMMOInventoryItem>& Items = PS->GetInventoryItems();

    // Clear all
    for (int32 i = 0; i < TotalSlots; ++i)
    {
        ClearSlot(SlotBorders[i], i);
    }

    // Populate
    int32 FilledCount = 0;
    for (const FMOBAMMOInventoryItem& Item : Items)
    {
        int32 TargetSlot = FilledCount;
        if (TargetSlot < TotalSlots)
        {
            PopulateSlot(SlotBorders[TargetSlot], Item);
        }
        ++FilledCount;
    }

    // Update count
    if (ItemCountText)
    {
        ItemCountText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Items.Num(), TotalSlots)));
    }

    // Empty hint
    if (EmptyText)
    {
        EmptyText->SetVisibility(Items.Num() == 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
}

// ─── Static helpers ──────────────────────────────────────────
FLinearColor UMOBAMMOInventoryWidget::GetRarityColor(const FString& ItemId)
{
    if (ItemId == TEXT("mana_ring")) return InvColors::RarityRare;
    if (ItemId == TEXT("crystal_staff") || ItemId == TEXT("chain_body") || ItemId == TEXT("swift_boots")) return InvColors::RarityUncommon;
    if (ItemId == TEXT("health_potion_small") || ItemId == TEXT("mana_potion_small") || ItemId == TEXT("iron_sword") || ItemId == TEXT("leather_helm") || ItemId == TEXT("sparring_token")) return InvColors::RarityCommon;
    if (ItemId.Contains(TEXT("legend")) || ItemId.Contains(TEXT("divine"))) return InvColors::RarityLegend;
    if (ItemId.Contains(TEXT("epic")) || ItemId.Contains(TEXT("shadow")))   return InvColors::RarityEpic;
    if (ItemId.Contains(TEXT("rare")) || ItemId.Contains(TEXT("crystal"))) return InvColors::RarityRare;
    if (ItemId.Contains(TEXT("uncommon")) || ItemId.Contains(TEXT("iron"))) return InvColors::RarityUncommon;
    return InvColors::RarityCommon;
}

FString UMOBAMMOInventoryWidget::GetItemDisplayName(const FString& ItemId)
{
    if (ItemId == TEXT("health_potion_small")) return TEXT("Small Health Potion");
    if (ItemId == TEXT("mana_potion_small")) return TEXT("Small Mana Potion");
    if (ItemId == TEXT("iron_sword")) return TEXT("Iron Sword");
    if (ItemId == TEXT("crystal_staff")) return TEXT("Crystal Staff");
    if (ItemId == TEXT("leather_helm")) return TEXT("Leather Helm");
    if (ItemId == TEXT("chain_body")) return TEXT("Chain Body");
    if (ItemId == TEXT("swift_boots")) return TEXT("Swift Boots");
    if (ItemId == TEXT("mana_ring")) return TEXT("Mana Ring");
    if (ItemId == TEXT("sparring_token")) return TEXT("Sparring Token");

    // Simple prettifier: health_potion -> Health Potion
    FString Name = ItemId;
    Name.ReplaceInline(TEXT("_"), TEXT(" "));
    if (Name.Len() > 0) Name[0] = FChar::ToUpper(Name[0]);
    for (int32 i = 1; i < Name.Len(); ++i)
    {
        if (i > 0 && Name[i-1] == ' ') Name[i] = FChar::ToUpper(Name[i]);
    }
    return Name;
}

FString UMOBAMMOInventoryWidget::GetItemDescription(const FString& ItemId)
{
    if (ItemId == TEXT("health_potion_small")) return TEXT("Restores 25 HP instantly.");
    if (ItemId == TEXT("mana_potion_small")) return TEXT("Restores 20 mana instantly.");
    if (ItemId == TEXT("iron_sword")) return TEXT("A basic front-line weapon. Max HP +10.");
    if (ItemId == TEXT("crystal_staff")) return TEXT("A mana-focused staff for mages. Max Mana +20.");
    if (ItemId == TEXT("leather_helm")) return TEXT("Light head armor. Max HP +15.");
    if (ItemId == TEXT("chain_body")) return TEXT("Medium body armor. Max HP +30.");
    if (ItemId == TEXT("swift_boots")) return TEXT("Enchanted boots. Max Mana +10.");
    if (ItemId == TEXT("mana_ring")) return TEXT("A rare crystal ring. Max Mana +25, Max HP +10.");
    if (ItemId == TEXT("sparring_token")) return TEXT("A training token dropped by Sparring Minions.");

    if (ItemId.Contains(TEXT("potion"))) return TEXT("Restores health when consumed.");
    if (ItemId.Contains(TEXT("sword")))  return TEXT("A sharp blade for close combat.");
    if (ItemId.Contains(TEXT("shield"))) return TEXT("Provides additional defense.");
    if (ItemId.Contains(TEXT("mana")))   return TEXT("Restores mana when consumed.");
    return TEXT("A mysterious item.");
}

FString UMOBAMMOInventoryWidget::GetRarityLabel(const FString& ItemId)
{
    if (ItemId == TEXT("mana_ring")) return TEXT("RARE");
    if (ItemId == TEXT("crystal_staff") || ItemId == TEXT("chain_body") || ItemId == TEXT("swift_boots")) return TEXT("UNCOMMON");
    if (ItemId == TEXT("health_potion_small") || ItemId == TEXT("mana_potion_small") || ItemId == TEXT("iron_sword") || ItemId == TEXT("leather_helm") || ItemId == TEXT("sparring_token")) return TEXT("COMMON");
    if (ItemId.Contains(TEXT("legend")) || ItemId.Contains(TEXT("divine"))) return TEXT("LEGENDARY");
    if (ItemId.Contains(TEXT("epic")) || ItemId.Contains(TEXT("shadow")))   return TEXT("EPIC");
    if (ItemId.Contains(TEXT("rare")) || ItemId.Contains(TEXT("crystal"))) return TEXT("RARE");
    if (ItemId.Contains(TEXT("uncommon")) || ItemId.Contains(TEXT("iron"))) return TEXT("UNCOMMON");
    return TEXT("COMMON");
}
