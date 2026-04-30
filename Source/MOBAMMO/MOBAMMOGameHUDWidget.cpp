#include "MOBAMMOGameHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "MOBAMMOAbilitySet.h"
#include "MOBAMMOBackendSubsystem.h"
#include "MOBAMMOGameState.h"
#include "MOBAMMOPlayerController.h"
#include "MOBAMMOPlayerState.h"
#include "Components/EditableTextBox.h"
#include "MOBAMMOTrainingMinionActor.h"
#include "MOBAMMOInventoryWidget.h"
#include "MOBAMMOCharacter.h"
#include "Components/BorderSlot.h"
#include "EngineUtils.h"
#include "MOBAMMOQuestCatalog.h"

// ─────────────────────────────────────────────────────────────
// Color palette
// ─────────────────────────────────────────────────────────────
namespace HUDColors
{
    // Glass panels
    static const FLinearColor PanelBg        (0.020f, 0.024f, 0.040f, 0.78f);
    static const FLinearColor PanelBorder    (0.180f, 0.220f, 0.320f, 0.45f);
    static const FLinearColor PanelHighlight (0.100f, 0.140f, 0.240f, 0.85f);

    // Health / Mana
    static const FLinearColor HealthFill     (0.780f, 0.180f, 0.200f, 1.0f);
    static const FLinearColor HealthBg       (0.140f, 0.060f, 0.060f, 1.0f);
    static const FLinearColor ManaFill       (0.160f, 0.440f, 0.820f, 1.0f);
    static const FLinearColor ManaBg         (0.040f, 0.060f, 0.140f, 1.0f);
    static const FLinearColor XPFill         (0.760f, 0.420f, 0.940f, 1.0f);
    static const FLinearColor XPBg           (0.090f, 0.040f, 0.140f, 1.0f);

    // Abilities
    static const FLinearColor AbilityReady   (0.220f, 0.580f, 0.920f, 1.0f);
    static const FLinearColor AbilityCooldown(0.640f, 0.520f, 0.180f, 1.0f);
    static const FLinearColor AbilityDead    (0.240f, 0.120f, 0.120f, 1.0f);
    static const FLinearColor AbilityDamage  (0.880f, 0.240f, 0.280f, 1.0f);
    static const FLinearColor AbilityHeal    (0.200f, 0.760f, 0.440f, 1.0f);
    static const FLinearColor AbilityCharged (0.340f, 0.780f, 1.000f, 1.0f);
    static const FLinearColor AbilityLocked  (0.180f, 0.180f, 0.220f, 1.0f);
    static const FLinearColor AbilityNoMana  (0.520f, 0.180f, 0.180f, 1.0f);
    static const FLinearColor AbilityNoTarget(0.420f, 0.200f, 0.160f, 1.0f);
    static const FLinearColor AbilityRespawn (0.220f, 0.720f, 0.460f, 1.0f);
    static const FLinearColor AbilityBarBg   (0.060f, 0.060f, 0.080f, 1.0f);

    // Text
    static const FLinearColor TextPrimary    (0.920f, 0.940f, 0.960f, 1.0f);
    static const FLinearColor TextSecondary  (0.600f, 0.660f, 0.740f, 1.0f);
    static const FLinearColor TextGold       (0.960f, 0.820f, 0.440f, 1.0f);
    static const FLinearColor TextDanger     (0.960f, 0.320f, 0.360f, 1.0f);
    static const FLinearColor TextSuccess    (0.340f, 0.920f, 0.560f, 1.0f);
    static const FLinearColor TextCyan       (0.500f, 0.840f, 1.000f, 1.0f);
    static const FLinearColor TextAlive      (0.340f, 0.920f, 0.560f, 1.0f);
    static const FLinearColor TextDead       (0.960f, 0.320f, 0.360f, 1.0f);
}

// ─────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────
void UMOBAMMOGameHUDWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    BuildLayout();

    if (UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
    {
        BindToSubsystem(BackendSubsystem);
    }
}

void UMOBAMMOGameHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();
    BindToReplicatedState();
    RefreshFromBackend();
}

void UMOBAMMOGameHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    CombatEventHighlightRemaining = FMath::Max(0.0f, CombatEventHighlightRemaining - InDeltaTime);
    LevelUpFlashRemaining          = FMath::Max(0.0f, LevelUpFlashRemaining - InDeltaTime);

    if (IsVisible())
    {
        EnsureAbilityBarVisible();
        UpdateTexts();
        UpdateMinimap();
        UpdateQuestPanel();
    }
}

void UMOBAMMOGameHUDWidget::RefreshFromBackend()
{
    UpdateTexts();
    SetVisibility(ShouldBeVisible() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    EnsureAbilityBarVisible();
}

bool UMOBAMMOGameHUDWidget::ShouldBeVisible() const
{
    const UWorld* World = GetWorld();
    const ENetMode NetMode = World ? World->GetNetMode() : NM_Standalone;
    if (NetMode != NM_Client && NetMode != NM_Standalone)
    {
        return false;
    }

    const UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem();
    if (BackendSubsystem)
    {
        const FString SessionStatus = BackendSubsystem->GetSessionStatus();
        if (SessionStatus != TEXT("Ready")
            && SessionStatus != TEXT("Active")
            && SessionStatus != TEXT("Reconnecting")
            && SessionStatus != TEXT("ReconnectRequired"))
        {
            return false;
        }
    }

    const APlayerController* PlayerController = GetOwningPlayer();
    const AMOBAMMOPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<AMOBAMMOPlayerState>() : nullptr;
    if (PlayerController
        && PlayerController->GetPawn()
        && PlayerState
        && !PlayerState->GetSessionId().TrimStartAndEnd().IsEmpty()
        && !PlayerState->GetCharacterId().TrimStartAndEnd().IsEmpty())
    {
        return true;
    }

    if (!BackendSubsystem)
    {
        return false;
    }

    const FString SessionStatus = BackendSubsystem->GetSessionStatus();
    return SessionStatus == TEXT("Ready") || SessionStatus == TEXT("Active") || SessionStatus == TEXT("Reconnecting") || SessionStatus == TEXT("ReconnectRequired");
}

bool UMOBAMMOGameHUDWidget::IsAbilityBarReady() const
{
    return AbilityBarBorder != nullptr
        && AbilitySlotBorders.Num() > 0
        && AbilityStateTexts.Num() == AbilitySlotBorders.Num()
        && AbilityCooldownBars.Num() == AbilitySlotBorders.Num();
}

UMOBAMMOBackendSubsystem* UMOBAMMOGameHUDWidget::GetBackendSubsystem() const
{
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        return GameInstance->GetSubsystem<UMOBAMMOBackendSubsystem>();
    }

    return nullptr;
}

// ─────────────────────────────────────────────────────────────
// Helper: glass panel
// ─────────────────────────────────────────────────────────────
UBorder* UMOBAMMOGameHUDWidget::MakeGlassPanel(const FLinearColor& Tint, float Alpha, float /*CornerRadius*/) const
{
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    FLinearColor BgColor = Tint;
    BgColor.A = Alpha;
    Panel->SetBrushColor(BgColor);
    return Panel;
}

UProgressBar* UMOBAMMOGameHUDWidget::MakeStyledBar(const FLinearColor& FillColor, const FLinearColor& BgColor, float Height) const
{
    UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
    Bar->SetFillColorAndOpacity(FillColor);
    Bar->SetPercent(1.0f);

    // Style the bar via WidgetStyle
    FProgressBarStyle BarStyle = Bar->GetWidgetStyle();
    FLinearColor BarBgColor = BgColor;
    BarBgColor.A = FMath::Max(BarBgColor.A, 0.6f);
    BarStyle.BackgroundImage.TintColor = FSlateColor(BarBgColor);
    Bar->SetWidgetStyle(BarStyle);

    return Bar;
}

// ─────────────────────────────────────────────────────────────
// Layout: Root
// ─────────────────────────────────────────────────────────────
void UMOBAMMOGameHUDWidget::BuildLayout()
{
    if (!WidgetTree)
    {
        return;
    }

    // Force clear any existing blueprint root widget so we can build the dynamic UI
    if (WidgetTree->RootWidget)
    {
        WidgetTree->RootWidget->RemoveFromParent();
        WidgetTree->RootWidget = nullptr;
    }

    RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
    WidgetTree->RootWidget = RootCanvas;

    BuildPlayerFrame(RootCanvas);
    BuildAbilityBar(RootCanvas);
    BuildTargetFrame(RootCanvas);
    BuildCombatLog(RootCanvas);
    BuildChatPanel(RootCanvas);
    BuildScoreBar(RootCanvas);
    BuildCenterNotifications(RootCanvas);
    BuildMinimap(RootCanvas);
    BuildQuestPanel(RootCanvas);

    InventoryWidget = WidgetTree->ConstructWidget<UMOBAMMOInventoryWidget>(UMOBAMMOInventoryWidget::StaticClass());
    if (InventoryWidget)
    {
        UCanvasPanelSlot* InvSlot = RootCanvas->AddChildToCanvas(InventoryWidget);
        if (InvSlot)
        {
            InvSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
            InvSlot->SetAlignment(FVector2D(0.5f, 0.5f));
            InvSlot->SetSize(FVector2D(490.0f, 420.0f));
            InvSlot->SetAutoSize(false);
        }
    }
}

void UMOBAMMOGameHUDWidget::ToggleInventory()
{
    if (InventoryWidget)
    {
        InventoryWidget->ToggleVisibility();
    }
}

// ─────────────────────────────────────────────────────────────
// Layout: Player Frame (bottom-left)
// ─────────────────────────────────────────────────────────────
bool UMOBAMMOGameHUDWidget::IsInventoryOpen() const
{
    return InventoryWidget && InventoryWidget->IsInventoryVisible();
}

void UMOBAMMOGameHUDWidget::BuildPlayerFrame(UCanvasPanel* Canvas)
{
    UBorder* FrameBg = MakeGlassPanel(HUDColors::PanelBg, 0.82f);
    FrameBg->SetPadding(FMargin(16.0f, 10.0f, 16.0f, 12.0f));

    UCanvasPanelSlot* FrameSlot = Canvas->AddChildToCanvas(FrameBg);
    if (FrameSlot)
    {
        FrameSlot->SetAnchors(FAnchors(0.0f, 1.0f, 0.0f, 1.0f));      // bottom-left
        FrameSlot->SetAlignment(FVector2D(0.0f, 1.0f));
        FrameSlot->SetPosition(FVector2D(20.0f, -20.0f));
        FrameSlot->SetSize(FVector2D(320.0f, 170.0f));
        FrameSlot->SetAutoSize(false);
    }

    // Outer border glow
    UBorder* BorderGlow = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    BorderGlow->SetBrushColor(HUDColors::PanelBorder);
    BorderGlow->SetPadding(FMargin(1.0f));
    BorderGlow->SetContent(FrameBg);

    // Actually replace child with the glow wrapper
    if (FrameSlot)
    {
        FrameSlot->SetSize(FVector2D(322.0f, 172.0f));
    }
    Canvas->RemoveChild(FrameBg);
    UCanvasPanelSlot* GlowSlot = Canvas->AddChildToCanvas(BorderGlow);
    if (GlowSlot)
    {
        GlowSlot->SetAnchors(FAnchors(0.0f, 1.0f, 0.0f, 1.0f));
        GlowSlot->SetAlignment(FVector2D(0.0f, 1.0f));
        GlowSlot->SetPosition(FVector2D(20.0f, -20.0f));
        GlowSlot->SetSize(FVector2D(322.0f, 172.0f));
        GlowSlot->SetAutoSize(false);
    }

    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    FrameBg->SetContent(Content);

    // Row 1: Name + life state
    UHorizontalBox* NameRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    Content->AddChildToVerticalBox(NameRow);

    PlayerNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    PlayerNameText->SetColorAndOpacity(FSlateColor(HUDColors::TextPrimary));
    FSlateFontInfo NameFont = PlayerNameText->GetFont();
    NameFont.Size = 16;
    NameFont.TypefaceFontName = TEXT("Bold");
    PlayerNameText->SetFont(NameFont);
    if (UHorizontalBoxSlot* NameSlot = NameRow->AddChildToHorizontalBox(PlayerNameText))
    {
        NameSlot->SetSize(ESlateSizeRule::Fill);
        NameSlot->SetVerticalAlignment(VAlign_Center);
    }

    PlayerLifeStateText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    PlayerLifeStateText->SetColorAndOpacity(FSlateColor(HUDColors::TextAlive));
    FSlateFontInfo LifeFont = PlayerLifeStateText->GetFont();
    LifeFont.Size = 11;
    LifeFont.TypefaceFontName = TEXT("Bold");
    PlayerLifeStateText->SetFont(LifeFont);
    if (UHorizontalBoxSlot* LifeSlot = NameRow->AddChildToHorizontalBox(PlayerLifeStateText))
    {
        LifeSlot->SetVerticalAlignment(VAlign_Center);
        LifeSlot->SetPadding(FMargin(6.0f, 0.0f, 0.0f, 0.0f));
    }

    // Row 2: Class + Level (left) · Gold (right)
    UHorizontalBox* ClassRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    if (UVerticalBoxSlot* ClassRowSlot = Content->AddChildToVerticalBox(ClassRow))
    {
        ClassRowSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 8.0f));
    }

    PlayerClassLevelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    PlayerClassLevelText->SetColorAndOpacity(FSlateColor(HUDColors::TextSecondary));
    FSlateFontInfo ClassFont = PlayerClassLevelText->GetFont();
    ClassFont.Size = 11;
    PlayerClassLevelText->SetFont(ClassFont);
    if (UHorizontalBoxSlot* ClassTextSlot = ClassRow->AddChildToHorizontalBox(PlayerClassLevelText))
    {
        ClassTextSlot->SetSize(ESlateSizeRule::Fill);
        ClassTextSlot->SetVerticalAlignment(VAlign_Center);
    }

    GoldText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    GoldText->SetColorAndOpacity(FSlateColor(HUDColors::TextGold));
    FSlateFontInfo GoldFont = GoldText->GetFont();
    GoldFont.Size = 12;
    GoldFont.TypefaceFontName = TEXT("Bold");
    GoldText->SetFont(GoldFont);
    GoldText->SetText(FText::FromString(TEXT("0 g")));
    if (UHorizontalBoxSlot* GoldSlot = ClassRow->AddChildToHorizontalBox(GoldText))
    {
        GoldSlot->SetVerticalAlignment(VAlign_Center);
        GoldSlot->SetHorizontalAlignment(HAlign_Right);
        GoldSlot->SetPadding(FMargin(6.0f, 0.0f, 0.0f, 0.0f));
    }

    // Health bar with value overlay
    {
        UOverlay* HealthOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
        USizeBox* HealthSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        HealthSizeBox->SetHeightOverride(22.0f);
        HealthSizeBox->AddChild(HealthOverlay);

        HealthBar = MakeStyledBar(HUDColors::HealthFill, HUDColors::HealthBg, 22.0f);
        HealthOverlay->AddChildToOverlay(HealthBar);

        HealthValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        HealthValueText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        FSlateFontInfo HealthFont = HealthValueText->GetFont();
        HealthFont.Size = 11;
        HealthFont.TypefaceFontName = TEXT("Bold");
        HealthValueText->SetFont(HealthFont);
        if (UOverlaySlot* ValueSlot = HealthOverlay->AddChildToOverlay(HealthValueText))
        {
            ValueSlot->SetHorizontalAlignment(HAlign_Center);
            ValueSlot->SetVerticalAlignment(VAlign_Center);
        }

        if (UVerticalBoxSlot* HealthSlot = Content->AddChildToVerticalBox(HealthSizeBox))
        {
            HealthSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
        }
    }

    // Mana bar with value overlay
    {
        UOverlay* ManaOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
        USizeBox* ManaSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        ManaSizeBox->SetHeightOverride(18.0f);
        ManaSizeBox->AddChild(ManaOverlay);

        ManaBar = MakeStyledBar(HUDColors::ManaFill, HUDColors::ManaBg, 18.0f);
        ManaOverlay->AddChildToOverlay(ManaBar);

        ManaValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        ManaValueText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        FSlateFontInfo ManaFont = ManaValueText->GetFont();
        ManaFont.Size = 10;
        ManaFont.TypefaceFontName = TEXT("Bold");
        ManaValueText->SetFont(ManaFont);
        if (UOverlaySlot* ValueSlot = ManaOverlay->AddChildToOverlay(ManaValueText))
        {
            ValueSlot->SetHorizontalAlignment(HAlign_Center);
            ValueSlot->SetVerticalAlignment(VAlign_Center);
        }

        Content->AddChildToVerticalBox(ManaSizeBox);
    }

    // XP bar with overlaid level / progress text
    {
        UOverlay* XPOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
        USizeBox* XPSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        XPSizeBox->SetHeightOverride(14.0f);
        XPSizeBox->AddChild(XPOverlay);

        XPBar = MakeStyledBar(HUDColors::XPFill, HUDColors::XPBg, 14.0f);
        XPBar->SetPercent(0.0f);
        XPOverlay->AddChildToOverlay(XPBar);

        XPValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        XPValueText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        FSlateFontInfo XPFont = XPValueText->GetFont();
        XPFont.Size = 9;
        XPFont.TypefaceFontName = TEXT("Bold");
        XPValueText->SetFont(XPFont);
        if (UOverlaySlot* XPValueSlot = XPOverlay->AddChildToOverlay(XPValueText))
        {
            XPValueSlot->SetHorizontalAlignment(HAlign_Center);
            XPValueSlot->SetVerticalAlignment(VAlign_Center);
        }

        if (UVerticalBoxSlot* XPSlotV = Content->AddChildToVerticalBox(XPSizeBox))
        {
            XPSlotV->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
        }
    }

    // Status effect row — small text line showing active buffs/debuffs.
    StatusEffectText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    StatusEffectText->SetColorAndOpacity(FSlateColor(FLinearColor(0.80f, 0.80f, 0.80f, 1.0f)));
    {
        FSlateFontInfo StatusFont = StatusEffectText->GetFont();
        StatusFont.Size = 10;
        StatusFont.TypefaceFontName = TEXT("Regular");
        StatusEffectText->SetFont(StatusFont);
    }
    StatusEffectText->SetText(FText::GetEmpty());
    StatusEffectText->SetVisibility(ESlateVisibility::Collapsed);
    if (UVerticalBoxSlot* StatusSlot = Content->AddChildToVerticalBox(StatusEffectText))
    {
        StatusSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
    }
}

// ─────────────────────────────────────────────────────────────
// Layout: Ability Bar (bottom-center)
// ─────────────────────────────────────────────────────────────
void UMOBAMMOGameHUDWidget::BuildAbilityBar(UCanvasPanel* Canvas)
{
    const TArray<FMOBAMMOAbilityDefinition> Abilities = MOBAMMOAbilitySet::ForClass(TEXT(""));

    AbilityBarBorder = MakeGlassPanel(HUDColors::PanelBg, 0.88f);
    AbilityBarBorder->SetPadding(FMargin(10.0f, 8.0f));
    AbilityBarBorder->SetVisibility(ESlateVisibility::HitTestInvisible);

    UCanvasPanelSlot* BarSlot = Canvas->AddChildToCanvas(AbilityBarBorder);
    if (BarSlot)
    {
        BarSlot->SetAnchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f));         // bottom-center
        BarSlot->SetAlignment(FVector2D(0.5f, 1.0f));
        BarSlot->SetPosition(FVector2D(0.0f, -34.0f));
        BarSlot->SetSize(FVector2D(560.0f, 92.0f));
        BarSlot->SetAutoSize(false);
    }

    UHorizontalBox* AbilityRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    AbilityBarBorder->SetContent(AbilityRow);

    AbilitySlotBorders.Reset();
    AbilityKeyTexts.Reset();
    AbilityNameTexts.Reset();
    AbilityStateTexts.Reset();
    AbilityCooldownBars.Reset();

    for (int32 Index = 0; Index < Abilities.Num(); ++Index)
    {
        const FMOBAMMOAbilityDefinition& Def = Abilities[Index];

        // Each ability is a bordered card
        UBorder* SlotBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        SlotBorder->SetBrushColor(HUDColors::PanelHighlight);
        SlotBorder->SetPadding(FMargin(1.0f));

        UBorder* SlotBg = MakeGlassPanel(FLinearColor(0.030f, 0.035f, 0.060f), 0.92f);
        SlotBg->SetPadding(FMargin(8.0f, 6.0f));
        SlotBorder->SetContent(SlotBg);

        USizeBox* SlotSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        SlotSize->SetWidthOverride(100.0f);
        SlotSize->AddChild(SlotBorder);

        if (UHorizontalBoxSlot* HSlot = AbilityRow->AddChildToHorizontalBox(SlotSize))
        {
            HSlot->SetPadding(FMargin(Index > 0 ? 4.0f : 0.0f, 0.0f, 0.0f, 0.0f));
        }

        UVerticalBox* SlotContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
        SlotBg->SetContent(SlotContent);

        // Key label (centered)
        UTextBlock* KeyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        KeyText->SetColorAndOpacity(FSlateColor(HUDColors::TextGold));
        FSlateFontInfo KeyFont = KeyText->GetFont();
        KeyFont.Size = 14;
        KeyFont.TypefaceFontName = TEXT("Bold");
        KeyText->SetFont(KeyFont);
        KeyText->SetText(FText::FromString(Def.KeyLabel));
        KeyText->SetJustification(ETextJustify::Center);
        SlotContent->AddChildToVerticalBox(KeyText);

        // Ability name
        UTextBlock* NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        NameText->SetColorAndOpacity(FSlateColor(HUDColors::TextPrimary));
        FSlateFontInfo AbilityNameFont = NameText->GetFont();
        AbilityNameFont.Size = 9;
        NameText->SetFont(AbilityNameFont);
        NameText->SetText(FText::FromString(Def.Name));
        NameText->SetJustification(ETextJustify::Center);
        if (UVerticalBoxSlot* NameSlot = SlotContent->AddChildToVerticalBox(NameText))
        {
            NameSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 4.0f));
        }

        // Cooldown bar
        UProgressBar* CdBar = MakeStyledBar(HUDColors::AbilityReady, HUDColors::AbilityBarBg, 6.0f);
        USizeBox* CdSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        CdSizeBox->SetHeightOverride(6.0f);
        CdSizeBox->AddChild(CdBar);
        SlotContent->AddChildToVerticalBox(CdSizeBox);

        // State label
        UTextBlock* StateText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        StateText->SetColorAndOpacity(FSlateColor(HUDColors::TextSuccess));
        FSlateFontInfo StateFont = StateText->GetFont();
        StateFont.Size = 8;
        StateFont.TypefaceFontName = TEXT("Bold");
        StateText->SetFont(StateFont);
        StateText->SetText(FText::FromString(TEXT("READY")));
        StateText->SetJustification(ETextJustify::Center);
        if (UVerticalBoxSlot* StateSlot = SlotContent->AddChildToVerticalBox(StateText))
        {
            StateSlot->SetPadding(FMargin(0.0f, 3.0f, 0.0f, 0.0f));
        }

        AbilitySlotBorders.Add(SlotBorder);
        AbilityKeyTexts.Add(KeyText);
        AbilityNameTexts.Add(NameText);
        AbilityStateTexts.Add(StateText);
        AbilityCooldownBars.Add(CdBar);
    }

    EnsureAbilityBarVisible();
}

void UMOBAMMOGameHUDWidget::EnsureAbilityBarVisible()
{
    if (AbilityBarBorder)
    {
        AbilityBarBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
    }

    for (TObjectPtr<UBorder>& SlotBorder : AbilitySlotBorders)
    {
        if (SlotBorder)
        {
            SlotBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
    }
}

// ─────────────────────────────────────────────────────────────
// Layout: Target Frame (top-right)
// ─────────────────────────────────────────────────────────────
void UMOBAMMOGameHUDWidget::BuildTargetFrame(UCanvasPanel* Canvas)
{
    TargetFrameBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    TargetFrameBorder->SetBrushColor(HUDColors::PanelBorder);
    TargetFrameBorder->SetPadding(FMargin(1.0f));
    TargetFrameBorder->SetVisibility(ESlateVisibility::Collapsed);

    UBorder* FrameBg = MakeGlassPanel(HUDColors::PanelBg, 0.82f);
    FrameBg->SetPadding(FMargin(14.0f, 10.0f, 14.0f, 12.0f));
    TargetFrameBorder->SetContent(FrameBg);

    UCanvasPanelSlot* TargetSlot = Canvas->AddChildToCanvas(TargetFrameBorder);
    if (TargetSlot)
    {
        TargetSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));     // top-right
        TargetSlot->SetAlignment(FVector2D(1.0f, 0.0f));
        TargetSlot->SetPosition(FVector2D(-20.0f, 20.0f));
        TargetSlot->SetSize(FVector2D(300.0f, 120.0f));
        TargetSlot->SetAutoSize(false);
    }

    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    FrameBg->SetContent(Content);

    // Name row
    UHorizontalBox* NameRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    Content->AddChildToVerticalBox(NameRow);

    TargetNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    TargetNameText->SetColorAndOpacity(FSlateColor(HUDColors::TextDanger));
    FSlateFontInfo TargetNameFont = TargetNameText->GetFont();
    TargetNameFont.Size = 14;
    TargetNameFont.TypefaceFontName = TEXT("Bold");
    TargetNameText->SetFont(TargetNameFont);
    if (UHorizontalBoxSlot* TNameSlot = NameRow->AddChildToHorizontalBox(TargetNameText))
    {
        TNameSlot->SetSize(ESlateSizeRule::Fill);
        TNameSlot->SetVerticalAlignment(VAlign_Center);
    }

    TargetRangeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    TargetRangeText->SetColorAndOpacity(FSlateColor(HUDColors::TextSecondary));
    FSlateFontInfo RangeFont = TargetRangeText->GetFont();
    RangeFont.Size = 9;
    RangeFont.TypefaceFontName = TEXT("Bold");
    TargetRangeText->SetFont(RangeFont);
    if (UHorizontalBoxSlot* RangeSlot = NameRow->AddChildToHorizontalBox(TargetRangeText))
    {
        RangeSlot->SetVerticalAlignment(VAlign_Center);
    }

    // Class text
    TargetClassText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    TargetClassText->SetColorAndOpacity(FSlateColor(HUDColors::TextSecondary));
    FSlateFontInfo TClassFont = TargetClassText->GetFont();
    TClassFont.Size = 10;
    TargetClassText->SetFont(TClassFont);
    if (UVerticalBoxSlot* CSlot = Content->AddChildToVerticalBox(TargetClassText))
    {
        CSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 6.0f));
    }

    // Target health bar
    {
        UOverlay* HealthOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
        USizeBox* HealthSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        HealthSizeBox->SetHeightOverride(18.0f);
        HealthSizeBox->AddChild(HealthOverlay);

        TargetHealthBar = MakeStyledBar(HUDColors::HealthFill, HUDColors::HealthBg, 18.0f);
        HealthOverlay->AddChildToOverlay(TargetHealthBar);

        TargetHealthValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        TargetHealthValueText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        FSlateFontInfo THFont = TargetHealthValueText->GetFont();
        THFont.Size = 9;
        THFont.TypefaceFontName = TEXT("Bold");
        TargetHealthValueText->SetFont(THFont);
        if (UOverlaySlot* ValSlot = HealthOverlay->AddChildToOverlay(TargetHealthValueText))
        {
            ValSlot->SetHorizontalAlignment(HAlign_Center);
            ValSlot->SetVerticalAlignment(VAlign_Center);
        }

        if (UVerticalBoxSlot* HSlot = Content->AddChildToVerticalBox(HealthSizeBox))
        {
            HSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 3.0f));
        }
    }

    // Target mana bar
    {
        UOverlay* ManaOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
        USizeBox* ManaSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        ManaSizeBox->SetHeightOverride(14.0f);
        ManaSizeBox->AddChild(ManaOverlay);

        TargetManaBar = MakeStyledBar(HUDColors::ManaFill, HUDColors::ManaBg, 14.0f);
        ManaOverlay->AddChildToOverlay(TargetManaBar);

        TargetManaValueText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        TargetManaValueText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        FSlateFontInfo TMFont = TargetManaValueText->GetFont();
        TMFont.Size = 8;
        TargetManaValueText->SetFont(TMFont);
        if (UOverlaySlot* ValSlot = ManaOverlay->AddChildToOverlay(TargetManaValueText))
        {
            ValSlot->SetHorizontalAlignment(HAlign_Center);
            ValSlot->SetVerticalAlignment(VAlign_Center);
        }

        Content->AddChildToVerticalBox(ManaSizeBox);
    }
}

// ─────────────────────────────────────────────────────────────
// Layout: Combat Log (left side)
// ─────────────────────────────────────────────────────────────
void UMOBAMMOGameHUDWidget::BuildCombatLog(UCanvasPanel* Canvas)
{
    UBorder* LogBg = MakeGlassPanel(HUDColors::PanelBg, 0.60f);
    LogBg->SetPadding(FMargin(12.0f, 8.0f));

    UCanvasPanelSlot* LogSlot = Canvas->AddChildToCanvas(LogBg);
    if (LogSlot)
    {
        LogSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));        // top-left
        LogSlot->SetAlignment(FVector2D(0.0f, 0.0f));
        LogSlot->SetPosition(FVector2D(20.0f, 20.0f));
        LogSlot->SetSize(FVector2D(340.0f, 180.0f));
        LogSlot->SetAutoSize(false);
    }

    CombatLogText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    CombatLogText->SetColorAndOpacity(FSlateColor(HUDColors::TextSecondary));
    FSlateFontInfo LogFont = CombatLogText->GetFont();
    LogFont.Size = 10;
    CombatLogText->SetFont(LogFont);
    CombatLogText->SetAutoWrapText(true);
    LogBg->SetContent(CombatLogText);
}

// ─────────────────────────────────────────────────────────────
// Layout: Chat Panel (bottom-left, above player frame)
// ─────────────────────────────────────────────────────────────
void UMOBAMMOGameHUDWidget::BuildChatPanel(UCanvasPanel* Canvas)
{
    // Outer glass panel
    UBorder* ChatBg = MakeGlassPanel(HUDColors::PanelBg, 0.60f);
    ChatBg->SetPadding(FMargin(10.0f, 6.0f));

    UCanvasPanelSlot* ChatSlot = Canvas->AddChildToCanvas(ChatBg);
    if (ChatSlot)
    {
        // Sits just above the player frame (player frame: 142h + 20 margin = 162px from bottom)
        ChatSlot->SetAnchors(FAnchors(0.0f, 1.0f, 0.0f, 1.0f));
        ChatSlot->SetAlignment(FVector2D(0.0f, 1.0f));
        ChatSlot->SetPosition(FVector2D(20.0f, -172.0f));
        ChatSlot->SetSize(FVector2D(340.0f, 126.0f));
        ChatSlot->SetAutoSize(false);
    }

    UVerticalBox* ChatVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    ChatBg->SetContent(ChatVBox);

    // ── Header ──
    UTextBlock* ChatHeader = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    ChatHeader->SetColorAndOpacity(FSlateColor(HUDColors::TextSecondary));
    FSlateFontInfo HeaderFont = ChatHeader->GetFont();
    HeaderFont.Size = 9;
    ChatHeader->SetFont(HeaderFont);
    ChatHeader->SetText(FText::FromString(TEXT("--- Zone Chat  [Enter] to type ---")));
    if (UVerticalBoxSlot* HSlot = ChatVBox->AddChildToVerticalBox(ChatHeader))
    {
        HSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
    }

    // ── Chat history ──
    ChatHistoryText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    ChatHistoryText->SetColorAndOpacity(FSlateColor(HUDColors::TextCyan));
    FSlateFontInfo ChatFont = ChatHistoryText->GetFont();
    ChatFont.Size = 10;
    ChatHistoryText->SetFont(ChatFont);
    ChatHistoryText->SetAutoWrapText(true);
    ChatHistoryText->SetText(FText::GetEmpty());
    if (UVerticalBoxSlot* HistSlot = ChatVBox->AddChildToVerticalBox(ChatHistoryText))
    {
        HistSlot->SetSize(ESlateSizeRule::Fill);
    }

    // ── Input row (hidden until Enter pressed) ──
    ChatInputBorder = MakeGlassPanel(HUDColors::PanelHighlight, 0.90f);
    ChatInputBorder->SetPadding(FMargin(6.0f, 3.0f));
    ChatInputBorder->SetVisibility(ESlateVisibility::Collapsed);
    ChatVBox->AddChildToVerticalBox(ChatInputBorder);

    ChatInputBox = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
    ChatInputBox->SetHintText(FText::FromString(TEXT("Type message... [Enter] send  [Esc] cancel")));
    // Style the hint and text color
    FEditableTextBoxStyle BoxStyle = ChatInputBox->GetWidgetStyle();
    BoxStyle.ForegroundColor = FSlateColor(HUDColors::TextPrimary);
    ChatInputBox->SetWidgetStyle(BoxStyle);
    ChatInputBox->OnTextCommitted.AddDynamic(this, &UMOBAMMOGameHUDWidget::HandleChatTextCommitted);
    ChatInputBorder->SetContent(ChatInputBox);
}

void UMOBAMMOGameHUDWidget::OpenChat()
{
    if (bChatOpen || !ChatInputBorder || !ChatInputBox)
    {
        return;
    }

    bChatOpen = true;
    ChatInputBox->SetText(FText::GetEmpty());
    ChatInputBorder->SetVisibility(ESlateVisibility::Visible);

    // Switch to UI-only input so keyboard goes to the text box
    if (APlayerController* PC = GetOwningPlayer())
    {
        FInputModeGameAndUI InputMode;
        InputMode.SetWidgetToFocus(ChatInputBox->TakeWidget());
        InputMode.SetHideCursorDuringCapture(false);
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PC->SetInputMode(InputMode);
        PC->SetShowMouseCursor(true);
    }
}

void UMOBAMMOGameHUDWidget::CloseChat()
{
    if (!bChatOpen)
    {
        return;
    }

    bChatOpen = false;

    if (ChatInputBorder)
    {
        ChatInputBorder->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (ChatInputBox)
    {
        ChatInputBox->SetText(FText::GetEmpty());
    }

    // Restore game + UI input mode
    if (APlayerController* PC = GetOwningPlayer())
    {
        FInputModeGameAndUI InputMode;
        InputMode.SetHideCursorDuringCapture(false);
        PC->SetInputMode(InputMode);
        PC->SetShowMouseCursor(true);
    }
}

void UMOBAMMOGameHUDWidget::HandleChatTextCommitted(const FText& Text, ETextCommit::Type CommitType)
{
    if (CommitType == ETextCommit::OnEnter)
    {
        const FString Message = Text.ToString().TrimStartAndEnd().Left(200);
        if (!Message.IsEmpty())
        {
            if (AMOBAMMOPlayerController* PC = Cast<AMOBAMMOPlayerController>(GetOwningPlayer()))
            {
                PC->ServerSendChatMessage(Message);
            }
        }
    }
    // Close on Enter or Escape
    CloseChat();
}

// ─────────────────────────────────────────────────────────────
// Layout: Score Bar (top-center)
// ─────────────────────────────────────────────────────────────
void UMOBAMMOGameHUDWidget::BuildScoreBar(UCanvasPanel* Canvas)
{
    UBorder* ScoreBg = MakeGlassPanel(HUDColors::PanelBg, 0.75f);
    ScoreBg->SetPadding(FMargin(20.0f, 6.0f));

    UCanvasPanelSlot* ScoreSlot = Canvas->AddChildToCanvas(ScoreBg);
    if (ScoreSlot)
    {
        ScoreSlot->SetAnchors(FAnchors(0.5f, 0.0f, 0.5f, 0.0f));      // top-center
        ScoreSlot->SetAlignment(FVector2D(0.5f, 0.0f));
        ScoreSlot->SetPosition(FVector2D(0.0f, 12.0f));
        ScoreSlot->SetAutoSize(true);
    }

    UHorizontalBox* ScoreRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    ScoreBg->SetContent(ScoreRow);

    KillDeathText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    KillDeathText->SetColorAndOpacity(FSlateColor(HUDColors::TextPrimary));
    FSlateFontInfo KDFont = KillDeathText->GetFont();
    KDFont.Size = 16;
    KDFont.TypefaceFontName = TEXT("Bold");
    KillDeathText->SetFont(KDFont);
    ScoreRow->AddChildToHorizontalBox(KillDeathText);

    // Separator
    UTextBlock* Sep = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Sep->SetColorAndOpacity(FSlateColor(HUDColors::TextSecondary));
    FSlateFontInfo SepFont = Sep->GetFont();
    SepFont.Size = 14;
    Sep->SetFont(SepFont);
    Sep->SetText(FText::FromString(TEXT("  |  ")));
    ScoreRow->AddChildToHorizontalBox(Sep);

    PlayersOnlineText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    PlayersOnlineText->SetColorAndOpacity(FSlateColor(HUDColors::TextCyan));
    FSlateFontInfo OnlineFont = PlayersOnlineText->GetFont();
    OnlineFont.Size = 12;
    PlayersOnlineText->SetFont(OnlineFont);
    if (UHorizontalBoxSlot* OnlineSlot = ScoreRow->AddChildToHorizontalBox(PlayersOnlineText))
    {
        OnlineSlot->SetVerticalAlignment(VAlign_Center);
    }

    UTextBlock* RosterSep = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    RosterSep->SetColorAndOpacity(FSlateColor(HUDColors::TextSecondary));
    FSlateFontInfo RosterSepFont = RosterSep->GetFont();
    RosterSepFont.Size = 12;
    RosterSep->SetFont(RosterSepFont);
    RosterSep->SetText(FText::FromString(TEXT("  |  Iris: ")));
    ScoreRow->AddChildToHorizontalBox(RosterSep);

    RosterText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    RosterText->SetColorAndOpacity(FSlateColor(HUDColors::TextSecondary));
    FSlateFontInfo RosterFont = RosterText->GetFont();
    RosterFont.Size = 10;
    RosterText->SetFont(RosterFont);
    if (UHorizontalBoxSlot* RosterSlot = ScoreRow->AddChildToHorizontalBox(RosterText))
    {
        RosterSlot->SetVerticalAlignment(VAlign_Center);
    }

    UTextBlock* SaveSep = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    SaveSep->SetColorAndOpacity(FSlateColor(HUDColors::TextSecondary));
    FSlateFontInfo SaveSepFont = SaveSep->GetFont();
    SaveSepFont.Size = 12;
    SaveSep->SetFont(SaveSepFont);
    SaveSep->SetText(FText::FromString(TEXT("  |  ")));
    ScoreRow->AddChildToHorizontalBox(SaveSep);

    SaveConnectionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    SaveConnectionText->SetColorAndOpacity(FSlateColor(HUDColors::TextSecondary));
    FSlateFontInfo SaveFont = SaveConnectionText->GetFont();
    SaveFont.Size = 11;
    SaveFont.TypefaceFontName = TEXT("Bold");
    SaveConnectionText->SetFont(SaveFont);
    SaveConnectionText->SetToolTipText(FText::FromString(TEXT("Backend save/session connection state")));
    if (UHorizontalBoxSlot* SaveSlot = ScoreRow->AddChildToHorizontalBox(SaveConnectionText))
    {
        SaveSlot->SetVerticalAlignment(VAlign_Center);
    }

    // Skill point indicator (hidden until SP > 0)
    UTextBlock* SpSep = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    SpSep->SetText(FText::FromString(TEXT("  |  ")));
    FSlateFontInfo SpSepF = SpSep->GetFont(); SpSepF.Size = 10; SpSep->SetFont(SpSepF);
    SpSep->SetColorAndOpacity(FSlateColor(HUDColors::TextSecondary));
    ScoreRow->AddChildToHorizontalBox(SpSep);

    SkillPointText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    SkillPointText->SetText(FText::FromString(TEXT("")));
    FSlateFontInfo SpF = SkillPointText->GetFont();
    SpF.Size = 11;
    SpF.TypefaceFontName = TEXT("Bold");
    SkillPointText->SetFont(SpF);
    SkillPointText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.75f, 0.10f, 1.0f)));
    SkillPointText->SetToolTipText(FText::FromString(TEXT("Skill points available — press F1-F4 to upgrade abilities")));
    if (UHorizontalBoxSlot* SpSlot = ScoreRow->AddChildToHorizontalBox(SkillPointText))
    {
        SpSlot->SetVerticalAlignment(VAlign_Center);
    }
}

// ─────────────────────────────────────────────────────────────
// Layout: Center Notifications
// ─────────────────────────────────────────────────────────────
void UMOBAMMOGameHUDWidget::BuildCenterNotifications(UCanvasPanel* Canvas)
{
    // Combat event flash (center-top area)
    CombatEventText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    CombatEventText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.86f, 0.54f, 0.0f)));
    FSlateFontInfo EventFont = CombatEventText->GetFont();
    EventFont.Size = 18;
    EventFont.TypefaceFontName = TEXT("Bold");
    CombatEventText->SetFont(EventFont);
    CombatEventText->SetJustification(ETextJustify::Center);
    if (UCanvasPanelSlot* EventSlot = Canvas->AddChildToCanvas(CombatEventText))
    {
        EventSlot->SetAnchors(FAnchors(0.5f, 0.15f, 0.5f, 0.15f));
        EventSlot->SetAlignment(FVector2D(0.5f, 0.5f));
        EventSlot->SetAutoSize(true);
    }

    // LEVEL UP! flash (slightly above combat event so they don't overlap)
    LevelUpFlashText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    LevelUpFlashText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.84f, 0.40f, 0.0f)));
    FSlateFontInfo LvlFont = LevelUpFlashText->GetFont();
    LvlFont.Size = 32;
    LvlFont.TypefaceFontName = TEXT("Black");
    LevelUpFlashText->SetFont(LvlFont);
    LevelUpFlashText->SetJustification(ETextJustify::Center);
    LevelUpFlashText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f));
    LevelUpFlashText->SetShadowOffset(FVector2D(2.0f, 2.0f));
    if (UCanvasPanelSlot* LvlSlot = Canvas->AddChildToCanvas(LevelUpFlashText))
    {
        LvlSlot->SetAnchors(FAnchors(0.5f, 0.32f, 0.5f, 0.32f));
        LvlSlot->SetAlignment(FVector2D(0.5f, 0.5f));
        LvlSlot->SetAutoSize(true);
    }

    // Action hint (bottom-center, above ability bar)
    ActionHintText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    ActionHintText->SetColorAndOpacity(FSlateColor(HUDColors::TextGold));
    FSlateFontInfo HintFont = ActionHintText->GetFont();
    HintFont.Size = 12;
    ActionHintText->SetFont(HintFont);
    ActionHintText->SetJustification(ETextJustify::Center);
    ActionHintText->SetAutoWrapText(true);
    if (UCanvasPanelSlot* HintSlot = Canvas->AddChildToCanvas(ActionHintText))
    {
        HintSlot->SetAnchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f));
        HintSlot->SetAlignment(FVector2D(0.5f, 1.0f));
        HintSlot->SetPosition(FVector2D(0.0f, -140.0f));
        HintSlot->SetSize(FVector2D(600.0f, 40.0f));
        HintSlot->SetAutoSize(false);
    }

    // Arc charge indicator (below action hint)
    ArcChargeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    ArcChargeText->SetColorAndOpacity(FSlateColor(HUDColors::TextCyan));
    FSlateFontInfo ArcFont = ArcChargeText->GetFont();
    ArcFont.Size = 11;
    ArcFont.TypefaceFontName = TEXT("Bold");
    ArcChargeText->SetFont(ArcFont);
    ArcChargeText->SetJustification(ETextJustify::Center);
    if (UCanvasPanelSlot* ArcSlot = Canvas->AddChildToCanvas(ArcChargeText))
    {
        ArcSlot->SetAnchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f));
        ArcSlot->SetAlignment(FVector2D(0.5f, 1.0f));
        ArcSlot->SetPosition(FVector2D(0.0f, -118.0f));
        ArcSlot->SetAutoSize(true);
    }

    // Respawn hint (center screen)
    RespawnHintText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    RespawnHintText->SetColorAndOpacity(FSlateColor(HUDColors::TextDanger));
    FSlateFontInfo RespawnFont = RespawnHintText->GetFont();
    RespawnFont.Size = 22;
    RespawnFont.TypefaceFontName = TEXT("Bold");
    RespawnHintText->SetFont(RespawnFont);
    RespawnHintText->SetJustification(ETextJustify::Center);
    RespawnHintText->SetVisibility(ESlateVisibility::Collapsed);
    if (UCanvasPanelSlot* RespawnSlot = Canvas->AddChildToCanvas(RespawnHintText))
    {
        RespawnSlot->SetAnchors(FAnchors(0.5f, 0.4f, 0.5f, 0.4f));
        RespawnSlot->SetAlignment(FVector2D(0.5f, 0.5f));
        RespawnSlot->SetAutoSize(true);
    }

    // Floating damage/heal feedback
    FloatingFeedbackText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    FloatingFeedbackText->SetVisibility(ESlateVisibility::HitTestInvisible);
    FloatingFeedbackText->SetText(FText::GetEmpty());
    FloatingFeedbackText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.32f, 0.34f, 0.0f)));
    FSlateFontInfo FloatFont = FloatingFeedbackText->GetFont();
    FloatFont.Size = 18;
    FloatFont.TypefaceFontName = TEXT("Bold");
    FloatingFeedbackText->SetFont(FloatFont);
    if (UCanvasPanelSlot* FloatSlot = Canvas->AddChildToCanvas(FloatingFeedbackText))
    {
        FloatSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
        FloatSlot->SetPosition(FVector2D(0.0f, 0.0f));
        FloatSlot->SetAutoSize(true);
    }
}

// ─────────────────────────────────────────────────────────────
// Bind
// ─────────────────────────────────────────────────────────────
void UMOBAMMOGameHUDWidget::BindToSubsystem(UMOBAMMOBackendSubsystem* BackendSubsystem)
{
    if (!BackendSubsystem || bBoundToSubsystem)
    {
        return;
    }

    BackendSubsystem->OnDebugStateChanged.AddDynamic(this, &UMOBAMMOGameHUDWidget::HandleBackendStateChanged);
    bBoundToSubsystem = true;
}

void UMOBAMMOGameHUDWidget::BindToReplicatedState()
{
    if (!bBoundToPlayerState)
    {
        if (const APlayerController* PlayerController = GetOwningPlayer())
        {
            if (AMOBAMMOPlayerState* PlayerState = PlayerController->GetPlayerState<AMOBAMMOPlayerState>())
            {
                PlayerState->OnReplicatedStateUpdated.AddDynamic(this, &UMOBAMMOGameHUDWidget::HandleReplicatedStateChanged);
                bBoundToPlayerState = true;
            }
        }
    }

    if (!bBoundToGameState)
    {
        if (const UWorld* World = GetWorld())
        {
            if (AMOBAMMOGameState* GameState = World->GetGameState<AMOBAMMOGameState>())
            {
                GameState->OnReplicatedStateUpdated.AddDynamic(this, &UMOBAMMOGameHUDWidget::HandleReplicatedStateChanged);
                bBoundToGameState = true;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────
// Layout: Minimap
// ─────────────────────────────────────────────────────────────

// Minimap layout constants (all in canvas space with anchor top-right / align top-right)
namespace MMap
{
    // Background panel
    static constexpr float BgRight  = 20.0f;   // px from right edge (anchor offset X, negative means left of right)
    static constexpr float BgTop    = 150.0f;  // px below top edge
    static constexpr float BgW      = 200.0f;
    static constexpr float BgH      = 162.0f;

    // Map area inside background (6px padding L/R, 22px title, 8px bottom)
    static constexpr float PadL     = 6.0f;
    static constexpr float TitleH   = 22.0f;
    static constexpr float PadBot   = 8.0f;
    static constexpr float MapW     = BgW - PadL * 2.0f;        // 188
    static constexpr float MapH     = BgH - TitleH - PadBot;    // 132

    // Canvas offsets for the map top-left corner (relative to top-right anchor)
    // Bg right edge is at -BgRight from right. Bg left edge is at -(BgRight + BgW) from right.
    // Map left edge = -(BgRight + BgW - PadL) = -(20+200-6) = -214
    static constexpr float MapLeft  = -(BgRight + BgW - PadL);  // -214
    static constexpr float MapTopY  = BgTop + TitleH;            // 172

    // Dot size
    static constexpr float DotSize  = 8.0f;
    static constexpr float DotR     = DotSize * 0.5f;

    // Arena world bounds (match GameMode defaults)
    static constexpr float WMinX = -3500.0f;
    static constexpr float WMaxX =  3500.0f;
    static constexpr float WMinY = -2500.0f;
    static constexpr float WMaxY =  2500.0f;
}

// Helper: create a coloured dot border on the RootCanvas (top-right anchor)
static UBorder* MakeMinimapDot(UWidgetTree* Tree, UCanvasPanel* Canvas,
                                const FLinearColor& Color, float Size)
{
    UBorder* Dot = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Dot->SetBrushColor(Color);
    Dot->SetPadding(FMargin(0.0f));
    if (UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Dot))
    {
        Slot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
        Slot->SetAlignment(FVector2D(0.0f, 0.0f));
        Slot->SetSize(FVector2D(Size, Size));
        Slot->SetAutoSize(false);
        Slot->SetPosition(FVector2D(0.0f, 0.0f)); // will be updated in UpdateMinimap
    }
    return Dot;
}

void UMOBAMMOGameHUDWidget::BuildMinimap(UCanvasPanel* Canvas)
{
    // ── Background panel ──────────────────────────────────────
    MinimapBg = MakeGlassPanel(FLinearColor(0.012f, 0.015f, 0.030f), 0.88f);
    MinimapBg->SetPadding(FMargin(0.0f));
    if (UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(MinimapBg))
    {
        CanvasSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
        CanvasSlot->SetAlignment(FVector2D(1.0f, 0.0f));
        CanvasSlot->SetPosition(FVector2D(-MMap::BgRight, MMap::BgTop));
        CanvasSlot->SetSize(FVector2D(MMap::BgW, MMap::BgH));
        CanvasSlot->SetAutoSize(false);
    }

    // Inner layout: title bar
    {
        UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
        Cast<UBorderSlot>(MinimapBg->AddChild(VBox))->SetPadding(FMargin(0.0f));

        // Title bar row
        UBorder* TitleBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        TitleBar->SetBrushColor(FLinearColor(0.025f, 0.032f, 0.065f, 1.0f));
        TitleBar->SetPadding(FMargin(6.0f, 3.0f));
        if (UVerticalBoxSlot* VS = VBox->AddChildToVerticalBox(TitleBar))
        {
            VS->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        }

        UHorizontalBox* TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        TitleBar->AddChild(TitleRow);

        // "MAP" label
        UTextBlock* TitleLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        TitleLbl->SetText(FText::FromString(TEXT("MAP")));
        FSlateFontInfo F = TitleLbl->GetFont();
        F.Size = 9;
        F.TypefaceFontName = TEXT("Bold");
        TitleLbl->SetFont(F);
        TitleLbl->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.65f, 0.9f, 1.0f)));
        if (UHorizontalBoxSlot* HS = TitleRow->AddChildToHorizontalBox(TitleLbl))
        {
            HS->SetSize(ESlateSizeRule::Fill);
            HS->SetVerticalAlignment(VAlign_Center);
        }

        // Legend: green=you, yellow=others
        UTextBlock* LegendTxt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        LegendTxt->SetText(FText::FromString(TEXT("● You  ● Players")));
        FSlateFontInfo LF = LegendTxt->GetFont();
        LF.Size = 7;
        LegendTxt->SetFont(LF);
        LegendTxt->SetColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.45f, 0.5f, 1.0f)));
        if (UHorizontalBoxSlot* HS = TitleRow->AddChildToHorizontalBox(LegendTxt))
        {
            HS->SetVerticalAlignment(VAlign_Center);
        }
    }

    // ── Arena border line (thin line around the map area) ──────
    {
        UBorder* MapBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        MapBorder->SetBrushColor(FLinearColor(0.14f, 0.20f, 0.35f, 0.6f));
        MapBorder->SetPadding(FMargin(1.0f));
        if (UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(MapBorder))
        {
            CanvasSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
            CanvasSlot->SetAlignment(FVector2D(0.0f, 0.0f));
            // Map area starts at (MapLeft, MapTopY), size (MapW, MapH) — with 1px border
            CanvasSlot->SetPosition(FVector2D(MMap::MapLeft - 1.0f, MMap::MapTopY - 1.0f));
            CanvasSlot->SetSize(FVector2D(MMap::MapW + 2.0f, MMap::MapH + 2.0f));
            CanvasSlot->SetAutoSize(false);
        }
    }

    // ── NPC / static dots ──────────────────────────────────────
    // Training dummy: red
    MinimapDummyDot = MakeMinimapDot(WidgetTree, Canvas, FLinearColor(1.0f, 0.25f, 0.25f, 0.9f), MMap::DotSize);
    // Training minion: orange
    MinimapMinionDot = MakeMinimapDot(WidgetTree, Canvas, FLinearColor(1.0f, 0.55f, 0.10f, 0.9f), MMap::DotSize);
    // Vendor: cyan
    MinimapVendorDot = MakeMinimapDot(WidgetTree, Canvas, FLinearColor(0.20f, 0.80f, 1.0f, 0.9f), MMap::DotSize);

    // ── Other-player dots (pool of 8, yellow) ─────────────────
    MinimapOtherDots.Reset();
    for (int32 i = 0; i < 8; ++i)
    {
        UBorder* Dot = MakeMinimapDot(WidgetTree, Canvas, FLinearColor(1.0f, 0.88f, 0.12f, 0.9f), MMap::DotSize);
        if (Dot) { Dot->SetVisibility(ESlateVisibility::Collapsed); }
        MinimapOtherDots.Add(Dot);
    }

    // ── Self dot (green, slightly larger) ─────────────────────
    MinimapSelfDot = MakeMinimapDot(WidgetTree, Canvas, FLinearColor(0.22f, 1.0f, 0.45f, 1.0f), MMap::DotSize + 2.0f);
}

void UMOBAMMOGameHUDWidget::PlaceMinimapDot(UBorder* Dot, float WorldX, float WorldY) const
{
    if (!Dot) { return; }
    UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Dot->Slot);
    if (!CanvasSlot) { return; }

    // Normalise world position to [0,1] within arena
    const float NormX = FMath::Clamp((WorldX - MMap::WMinX) / (MMap::WMaxX - MMap::WMinX), 0.0f, 1.0f);
    const float NormY = FMath::Clamp((MMap::WMaxY - WorldY) / (MMap::WMaxY - MMap::WMinY), 0.0f, 1.0f); // Y flipped

    // Map-relative pixel position
    const float RelX = NormX * MMap::MapW;
    const float RelY = NormY * MMap::MapH;

    // Canvas slot position (anchor = top-right, alignment = top-left of dot)
    const float SlotX = MMap::MapLeft + RelX - MMap::DotR;
    const float SlotY = MMap::MapTopY + RelY - MMap::DotR;
    CanvasSlot->SetPosition(FVector2D(SlotX, SlotY));
}

void UMOBAMMOGameHUDWidget::UpdateMinimap()
{
    if (!MinimapBg) { return; }
    const UWorld* World = GetWorld();
    if (!World) { return; }

    const APlayerController* PC = GetOwningPlayer();
    const AMOBAMMOGameState* GameState = World->GetGameState<AMOBAMMOGameState>();

    // ── Self dot ──────────────────────────────────────────────
    const APawn* LocalPawn = PC ? PC->GetPawn() : nullptr;
    if (LocalPawn && MinimapSelfDot)
    {
        const FVector Pos = LocalPawn->GetActorLocation();
        PlaceMinimapDot(MinimapSelfDot, Pos.X, Pos.Y);
        MinimapSelfDot->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    else if (MinimapSelfDot)
    {
        MinimapSelfDot->SetVisibility(ESlateVisibility::Collapsed);
    }

    // ── Other players (iterate replicated character pawns) ────
    int32 OtherIdx = 0;
    for (TActorIterator<AMOBAMMOCharacter> It(World); It; ++It)
    {
        if (It->GetController() == PC) { continue; }
        if (!MinimapOtherDots.IsValidIndex(OtherIdx)) { break; }

        UBorder* Dot = MinimapOtherDots[OtherIdx];
        if (Dot)
        {
            const FVector Pos = It->GetActorLocation();
            PlaceMinimapDot(Dot, Pos.X, Pos.Y);
            Dot->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        ++OtherIdx;
    }
    // Hide unused other-player dots
    for (int32 i = OtherIdx; i < MinimapOtherDots.Num(); ++i)
    {
        if (MinimapOtherDots[i])
        {
            MinimapOtherDots[i]->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // ── Training Dummy dot (fixed position, color changes when dead) ──
    if (MinimapDummyDot)
    {
        PlaceMinimapDot(MinimapDummyDot, 520.0f, 360.0f);
        const bool bAlive = GameState && GameState->IsTrainingDummyAlive();
        MinimapDummyDot->SetBrushColor(
            bAlive ? FLinearColor(1.0f, 0.25f, 0.25f, 0.9f)
                   : FLinearColor(0.35f, 0.35f, 0.35f, 0.45f));
        MinimapDummyDot->SetVisibility(ESlateVisibility::HitTestInvisible);
    }

    // ── Training Minion dot (actor position, hide if respawning) ──
    if (MinimapMinionDot)
    {
        bool bFound = false;
        for (TActorIterator<AMOBAMMOTrainingMinionActor> It(World); It; ++It)
        {
            const FVector Pos = It->GetActorLocation();
            PlaceMinimapDot(MinimapMinionDot, Pos.X, Pos.Y);
            bFound = true;
            break;
        }
        MinimapMinionDot->SetVisibility(
            bFound ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }

    // ── Vendor dot (fixed position) ──────────────────────────
    if (MinimapVendorDot)
    {
        PlaceMinimapDot(MinimapVendorDot, -520.0f, 360.0f);
        MinimapVendorDot->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
}

// ─────────────────────────────────────────────────────────────
// Update
// ─────────────────────────────────────────────────────────────
void UMOBAMMOGameHUDWidget::UpdateTexts()
{
    constexpr float RespawnBarReferenceDuration = 3.0f;

    FString CharacterName = TEXT("-");
    FString ClassId = TEXT("-");
    int32 CharacterLevel = 1;
    float Health = 0.0f;
    float MaxHealth = 0.0f;
    float Mana = 0.0f;
    float MaxMana = 0.0f;
    int32 CharacterExperience = 0;
    int32 XPInLevel = 0;
    int32 XPLevelSpan = 0;
    float XPFraction = 0.0f;
    bool bMaxLevel = false;
    int32 PlayerGold = 0;
    float DamageCooldownRemaining = 0.0f;
    float HealCooldownRemaining = 0.0f;
    float DrainCooldownRemaining = 0.0f;
    float ManaSurgeCooldownRemaining = 0.0f;
    float ArcChargeRemaining = 0.0f;
    float RespawnCooldownRemaining = 0.0f;
    int32 ConnectedPlayers = 0;
    int32 KillCount = 0;
    int32 DeathCount = 0;
    FString LifeState = TEXT("Dead");
    FString LastCombatLog = TEXT("No combat events yet.");
    FString TargetDisplayText = TEXT("No target");
    FString CombatFeedDisplay = TEXT("");
    FString RosterDisplay = TEXT("waiting");
    FString MinionThreatDisplay;
    float ServerWorldTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    FString SelectedTargetCharacterId;
    FString FloatingFeedbackStr;
    bool bFloatingFeedbackHealing = false;
    float FloatingFeedbackAlpha = 0.0f;
    FVector2D FloatingFeedbackScreenPosition(24.0f, 24.0f);
    float TargetHealth = 0.0f;
    float TargetMaxHealth = 0.0f;
    float TargetMana = 0.0f;
    float TargetMaxMana = 0.0f;
    bool bHasTarget = false;
    FString TargetRangeStr = TEXT("---");
    bool bTargetInRange = false;
    FString TargetName;
    FString TargetClass;
    int32 TargetLevel = 1;
    FString SaveStatus = TEXT("Idle");
    FString SaveErrorMessage;
    bool bSaveConnectionHealthy = true;

    if (const UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
    {
        SaveStatus = BackendSubsystem->GetSaveStatus();
        SaveErrorMessage = BackendSubsystem->GetLastSaveErrorMessage();
        bSaveConnectionHealthy = BackendSubsystem->IsSaveConnectionHealthy();
    }

    if (const UWorld* World = GetWorld())
    {
        if (const AGameStateBase* BaseGameState = World->GetGameState())
        {
            ServerWorldTimeSeconds = BaseGameState->GetServerWorldTimeSeconds();
        }
    }

    if (const AMOBAMMOPlayerController* PlayerController = Cast<AMOBAMMOPlayerController>(GetOwningPlayer()))
    {
        TargetDisplayText = PlayerController->GetSelectedTargetDisplayText();

        if (const AMOBAMMOPlayerState* PlayerState = PlayerController->GetPlayerState<AMOBAMMOPlayerState>())
        {
            CharacterName = PlayerState->GetCharacterName().IsEmpty() ? TEXT("Hero") : PlayerState->GetCharacterName();
            ClassId = PlayerState->GetClassId().IsEmpty() ? TEXT("-") : PlayerState->GetClassId();
            CharacterLevel = PlayerState->GetCharacterLevel();
            CharacterExperience = PlayerState->GetCharacterExperience();
            bMaxLevel = PlayerState->IsMaxLevel();
            XPFraction = PlayerState->GetExperienceProgressFraction();
            {
                const int32 LevelStart = PlayerState->GetTotalXPForLevel(CharacterLevel);
                const int32 LevelEnd   = PlayerState->GetTotalXPForLevel(CharacterLevel + 1);
                XPInLevel   = FMath::Max(0, CharacterExperience - LevelStart);
                XPLevelSpan = FMath::Max(0, LevelEnd - LevelStart);
            }
            Health = PlayerState->GetCurrentHealth();
            MaxHealth = PlayerState->GetMaxHealth();
            Mana = PlayerState->GetCurrentMana();
            MaxMana = PlayerState->GetMaxMana();
            DamageCooldownRemaining = PlayerState->GetDamageCooldownRemaining(ServerWorldTimeSeconds);
            HealCooldownRemaining = PlayerState->GetHealCooldownRemaining(ServerWorldTimeSeconds);
            DrainCooldownRemaining = PlayerState->GetDrainCooldownRemaining(ServerWorldTimeSeconds);
            ManaSurgeCooldownRemaining = PlayerState->GetManaSurgeCooldownRemaining(ServerWorldTimeSeconds);
            ArcChargeRemaining = PlayerState->GetArcChargeRemaining(ServerWorldTimeSeconds);
            RespawnCooldownRemaining = PlayerState->GetRespawnCooldownRemaining(ServerWorldTimeSeconds);
            KillCount = PlayerState->GetKills();
            DeathCount = PlayerState->GetDeaths();
            PlayerGold = PlayerState->GetGold();
            LifeState = PlayerState->IsAlive() ? TEXT("Alive") : TEXT("Dead");
            SelectedTargetCharacterId = PlayerState->GetSelectedTargetCharacterId();

            const FString ReplicatedSaveStatus = PlayerState->GetPersistenceStatus();
            if (!ReplicatedSaveStatus.IsEmpty())
            {
                SaveStatus = ReplicatedSaveStatus;
                SaveErrorMessage = PlayerState->GetPersistenceErrorMessage();
                bSaveConnectionHealthy = SaveStatus != TEXT("Failed") && SaveStatus != TEXT("SessionInvalid");
            }
        }
    }

    if (const UWorld* World = GetWorld())
    {
        if (const AMOBAMMOGameState* GameState = World->GetGameState<AMOBAMMOGameState>())
        {
            ConnectedPlayers = GameState->GetConnectedPlayers();
            if (!GameState->GetLastCombatLog().IsEmpty())
            {
                LastCombatLog = GameState->GetLastCombatLog();
                if (CachedCombatEvent != LastCombatLog)
                {
                    CachedCombatEvent = LastCombatLog;
                    CombatEventHighlightRemaining = 2.25f;
                }
            }

            const TArray<FString>& CombatFeed = GameState->GetCombatFeed();
            if (!CombatFeed.IsEmpty())
            {
                for (int32 FeedIndex = FMath::Max(0, CombatFeed.Num() - 6); FeedIndex < CombatFeed.Num(); ++FeedIndex)
                {
                    if (!CombatFeedDisplay.IsEmpty())
                    {
                        CombatFeedDisplay += TEXT("\n");
                    }
                    CombatFeedDisplay += CombatFeed[FeedIndex];
                }
            }

            RosterDisplay.Reset();
            int32 RosterCount = 0;
            for (APlayerState* IteratedPlayerState : GameState->PlayerArray)
            {
                const AMOBAMMOPlayerState* RosterPlayerState = Cast<AMOBAMMOPlayerState>(IteratedPlayerState);
                if (!RosterPlayerState)
                {
                    continue;
                }

                if (!RosterDisplay.IsEmpty())
                {
                    RosterDisplay += TEXT("  /  ");
                }

                const FString RosterName = RosterPlayerState->GetCharacterName().IsEmpty()
                    ? RosterPlayerState->GetPlayerName()
                    : RosterPlayerState->GetCharacterName();
                const FString RosterTarget = RosterPlayerState->GetSelectedTargetName().IsEmpty()
                    ? TEXT("none")
                    : RosterPlayerState->GetSelectedTargetName();
                RosterDisplay += FString::Printf(
                    TEXT("%s %.0fHP %.0fMP -> %s"),
                    *RosterName,
                    RosterPlayerState->GetCurrentHealth(),
                    RosterPlayerState->GetCurrentMana(),
                    *RosterTarget
                );
                ++RosterCount;

                if (RosterCount >= 3)
                {
                    if (ConnectedPlayers > RosterCount)
                    {
                        RosterDisplay += FString::Printf(TEXT("  +%d"), ConnectedPlayers - RosterCount);
                    }
                    break;
                }
            }

            if (RosterDisplay.IsEmpty())
            {
                RosterDisplay = TEXT("no replicated players yet");
            }

            const float MinionThreatAge = ServerWorldTimeSeconds - GameState->GetTrainingMinionLastStrikeServerTime();
            if (!GameState->GetTrainingMinionThreatName().IsEmpty() && MinionThreatAge >= 0.0f && MinionThreatAge <= 8.0f)
            {
                const float MinionAggroRemaining = FMath::Max(0.0f, GameState->GetTrainingMinionAggroEndServerTime() - ServerWorldTimeSeconds);
                MinionThreatDisplay = MinionAggroRemaining > KINDA_SMALL_NUMBER
                    ? FString::Printf(
                        TEXT("AI AGGRO: %s -> %s (%.1fs)"),
                        *GameState->GetTrainingMinionLastStrikeName(),
                        *GameState->GetTrainingMinionThreatName(),
                        MinionAggroRemaining
                    )
                    : FString::Printf(
                        TEXT("AI Threat: %s -> %s (%.1fs ago)"),
                        *GameState->GetTrainingMinionLastStrikeName(),
                        *GameState->GetTrainingMinionThreatName(),
                        MinionThreatAge
                    );
            }

            if (!SelectedTargetCharacterId.IsEmpty())
            {
                for (APlayerState* IteratedPlayerState : GameState->PlayerArray)
                {
                    const AMOBAMMOPlayerState* TargetPlayerState = Cast<AMOBAMMOPlayerState>(IteratedPlayerState);
                    if (!TargetPlayerState || TargetPlayerState->GetCharacterId() != SelectedTargetCharacterId)
                    {
                        continue;
                    }

                    TargetName = TargetPlayerState->GetCharacterName();
                    TargetClass = TargetPlayerState->GetClassId();
                    TargetLevel = TargetPlayerState->GetCharacterLevel();
                    TargetHealth = TargetPlayerState->GetCurrentHealth();
                    TargetMaxHealth = TargetPlayerState->GetMaxHealth();
                    TargetMana = TargetPlayerState->GetCurrentMana();
                    TargetMaxMana = TargetPlayerState->GetMaxMana();
                    bHasTarget = true;

                    if (const APlayerController* OwningController = GetOwningPlayer())
                    {
                        const APawn* LocalPawn = OwningController->GetPawn();
                        const APawn* TargetPawn = TargetPlayerState->GetPawn();
                        if (IsValid(LocalPawn) && IsValid(TargetPawn))
                        {
                            const float DistanceUnits = FVector::Dist(LocalPawn->GetActorLocation(), TargetPawn->GetActorLocation());
                            bTargetInRange = DistanceUnits <= 1800.0f;
                            TargetRangeStr = bTargetInRange
                                ? FString::Printf(TEXT("%.0fm IN RANGE"), DistanceUnits / 100.0f)
                                : FString::Printf(TEXT("%.0fm"), DistanceUnits / 100.0f);
                        }
                    }

                    const float FeedbackRemaining = FMath::Max(0.0f, TargetPlayerState->GetIncomingCombatFeedbackEndServerTime() - ServerWorldTimeSeconds);
                    if (FeedbackRemaining > KINDA_SMALL_NUMBER)
                    {
                        FloatingFeedbackStr = TargetPlayerState->GetIncomingCombatFeedbackText();
                        bFloatingFeedbackHealing = TargetPlayerState->IsIncomingCombatFeedbackHealing();
                        FloatingFeedbackAlpha = FMath::Clamp(FeedbackRemaining / 1.25f, 0.0f, 1.0f);

                        if (const APawn* TargetPawn = TargetPlayerState->GetPawn())
                        {
                            FVector WorldLocation = TargetPawn->GetActorLocation();
                            WorldLocation.Z += 120.0f;
                            FVector2D ProjectedScreenLocation;
                            if (const APlayerController* OwningController = GetOwningPlayer())
                            {
                                if (OwningController->ProjectWorldLocationToScreen(WorldLocation, ProjectedScreenLocation, true))
                                {
                                    FloatingFeedbackScreenPosition = ProjectedScreenLocation + FVector2D(-48.0f, -36.0f - (1.0f - FloatingFeedbackAlpha) * 18.0f);
                                }
                            }
                        }
                    }
                    break;
                }

                if (!bHasTarget && SelectedTargetCharacterId == AMOBAMMOTrainingMinionActor::GetTrainingMinionCharacterId())
                {
                    TargetName = AMOBAMMOTrainingMinionActor::GetTrainingMinionName();
                    TargetClass = TEXT("ai minion");
                    TargetLevel = 1;
                    TargetMana = 0.0f;
                    TargetMaxMana = 0.0f;
                    bHasTarget = true;
                    TargetRangeStr = TEXT("MINION LOST");

                    for (TActorIterator<AMOBAMMOTrainingMinionActor> It(World); It; ++It)
                    {
                        const AMOBAMMOTrainingMinionActor* MinionActor = *It;
                        if (!IsValid(MinionActor))
                        {
                            continue;
                        }

                        TargetHealth = MinionActor->GetCurrentHealth();
                        TargetMaxHealth = MinionActor->GetMaxHealth();
                        if (const APlayerController* OwningController = GetOwningPlayer())
                        {
                            const APawn* LocalPawn = OwningController->GetPawn();
                            if (IsValid(LocalPawn))
                            {
                                const float DistanceUnits = FVector::Dist(LocalPawn->GetActorLocation(), MinionActor->GetActorLocation());
                                bTargetInRange = DistanceUnits <= 1800.0f;
                                TargetRangeStr = bTargetInRange
                                    ? FString::Printf(TEXT("%.0fm IN RANGE"), DistanceUnits / 100.0f)
                                    : FString::Printf(TEXT("%.0fm"), DistanceUnits / 100.0f);
                            }
                        }
                        break;
                    }
                }

                if (!bHasTarget && SelectedTargetCharacterId == GameState->GetTrainingDummyCharacterId())
                {
                    TargetName = GameState->GetTrainingDummyName();
                    TargetClass = TEXT("training");
                    TargetLevel = 1;
                    TargetHealth = GameState->GetTrainingDummyHealth();
                    TargetMaxHealth = GameState->GetTrainingDummyMaxHealth();
                    TargetMana = GameState->GetTrainingDummyMana();
                    TargetMaxMana = GameState->GetTrainingDummyMaxMana();
                    bHasTarget = true;
                    bTargetInRange = true;
                    TargetRangeStr = TEXT("DUMMY LOCK");
                }
            }
        }
    }

    // ── Player Frame ──
    if (PlayerNameText)
    {
        PlayerNameText->SetText(FText::FromString(CharacterName));
    }

    if (PlayerClassLevelText)
    {
        PlayerClassLevelText->SetText(FText::FromString(FString::Printf(TEXT("%s  ·  Level %d"), *ClassId, CharacterLevel)));
    }

    if (GoldText)
    {
        // Compact formatting: 1234 -> "1,234 g".  Avoids breaking the right-aligned slot at 6+ digit values.
        FString GoldStr = FString::FromInt(PlayerGold);
        for (int32 InsertAt = GoldStr.Len() - 3; InsertAt > 0; InsertAt -= 3)
        {
            GoldStr.InsertAt(InsertAt, TEXT(","));
        }
        GoldText->SetText(FText::FromString(FString::Printf(TEXT("%s g"), *GoldStr)));
    }

    if (PlayerLifeStateText)
    {
        const bool bAlive = LifeState == TEXT("Alive");
        PlayerLifeStateText->SetText(FText::FromString(bAlive ? TEXT("● ALIVE") : TEXT("● DEAD")));
        PlayerLifeStateText->SetColorAndOpacity(FSlateColor(bAlive ? HUDColors::TextAlive : HUDColors::TextDead));
    }

    if (HealthBar)
    {
        HealthBar->SetPercent(MaxHealth > 0.0f ? Health / MaxHealth : 0.0f);
    }

    if (HealthValueText)
    {
        HealthValueText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), Health, MaxHealth)));
    }

    if (ManaBar)
    {
        ManaBar->SetPercent(MaxMana > 0.0f ? Mana / MaxMana : 0.0f);
    }

    // ── XP bar + level-up flash ──
    if (XPBar)
    {
        XPBar->SetPercent(bMaxLevel ? 1.0f : XPFraction);
        XPBar->SetFillColorAndOpacity(bMaxLevel ? HUDColors::TextGold : HUDColors::XPFill);
    }
    if (XPValueText)
    {
        if (bMaxLevel)
        {
            XPValueText->SetText(FText::FromString(TEXT("LEVEL MAX")));
        }
        else
        {
            XPValueText->SetText(FText::FromString(FString::Printf(TEXT("XP %d / %d"), XPInLevel, XPLevelSpan)));
        }
    }

    // Level-up detection: fire the flash when CharacterLevel increases.
    if (CachedCharacterLevel == 0)
    {
        // First read after spawn — seed without flashing.
        CachedCharacterLevel = CharacterLevel;
    }
    else if (CharacterLevel > CachedCharacterLevel)
    {
        LevelUpFlashRemaining = 3.0f;
        CachedCharacterLevel = CharacterLevel;
    }

    if (LevelUpFlashText)
    {
        const float FlashAlpha = LevelUpFlashRemaining > 0.0f
            ? FMath::Clamp(LevelUpFlashRemaining / 3.0f, 0.0f, 1.0f)
            : 0.0f;
        if (FlashAlpha > 0.0f)
        {
            LevelUpFlashText->SetText(FText::FromString(FString::Printf(TEXT("LEVEL UP!  ·  Level %d"), CharacterLevel)));
            LevelUpFlashText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.84f, 0.40f, FlashAlpha)));
        }
        else
        {
            LevelUpFlashText->SetText(FText::GetEmpty());
        }
    }

    if (ManaValueText)
    {
        ManaValueText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), Mana, MaxMana)));
    }

    // ── Status Effects ──
    if (StatusEffectText)
    {
        const AMOBAMMOPlayerState* PSForStatus = GetOwningPlayer()
            ? GetOwningPlayer()->GetPlayerState<AMOBAMMOPlayerState>() : nullptr;

        if (PSForStatus && PSForStatus->GetActiveStatusEffects().Num() > 0)
        {
            FString StatusLine;
            for (const FMOBAMMOStatusEffect& E : PSForStatus->GetActiveStatusEffects())
            {
                if (!StatusLine.IsEmpty())
                {
                    StatusLine += TEXT("  ");
                }
                switch (E.Type)
                {
                    case EMOBAMMOStatusEffectType::Shield:
                        StatusLine += FString::Printf(TEXT("🛡 Shield %.0f (%.0fs)"),
                            E.Magnitude, FMath::Max(0.0f, E.RemainingDuration));
                        break;
                    case EMOBAMMOStatusEffectType::Regeneration:
                        StatusLine += FString::Printf(TEXT("✦ Regen %.0f/s (%.0fs)"),
                            E.Magnitude, FMath::Max(0.0f, E.RemainingDuration));
                        break;
                    case EMOBAMMOStatusEffectType::Poison:
                        StatusLine += FString::Printf(TEXT("☠ Poison %.0f/s (%.0fs)"),
                            E.Magnitude, FMath::Max(0.0f, E.RemainingDuration));
                        break;
                    case EMOBAMMOStatusEffectType::Haste:
                        StatusLine += FString::Printf(TEXT("⚡ Haste (%.0fs)"),
                            FMath::Max(0.0f, E.RemainingDuration));
                        break;
                    case EMOBAMMOStatusEffectType::Silence:
                        StatusLine += FString::Printf(TEXT("🔇 Silence (%.0fs)"),
                            FMath::Max(0.0f, E.RemainingDuration));
                        break;
                }
            }
            StatusEffectText->SetText(FText::FromString(StatusLine));
            StatusEffectText->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        else
        {
            StatusEffectText->SetText(FText::GetEmpty());
            StatusEffectText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // ── Score Bar ──
    if (KillDeathText)
    {
        KillDeathText->SetText(FText::FromString(FString::Printf(TEXT("K %d  /  D %d"), KillCount, DeathCount)));
    }

    if (PlayersOnlineText)
    {
        PlayersOnlineText->SetText(FText::FromString(FString::Printf(TEXT("%d Online"), ConnectedPlayers)));
    }

    if (RosterText)
    {
        RosterText->SetText(FText::FromString(RosterDisplay));
    }

    if (SaveConnectionText)
    {
        FString SaveDisplay = TEXT("SAVE IDLE");
        FLinearColor SaveColor = HUDColors::TextSecondary;

        if (SaveStatus == TEXT("Saving"))
        {
            SaveDisplay = TEXT("SAVING");
            SaveColor = HUDColors::TextGold;
        }
        else if (SaveStatus == TEXT("Saved") || SaveStatus == TEXT("Ready"))
        {
            SaveDisplay = TEXT("SAVE OK");
            SaveColor = HUDColors::TextSuccess;
        }
        else if (SaveStatus == TEXT("Reconnecting"))
        {
            SaveDisplay = TEXT("RECONNECTING");
            SaveColor = HUDColors::TextGold;
        }
        else if (!bSaveConnectionHealthy || SaveStatus == TEXT("Failed") || SaveStatus == TEXT("SessionInvalid"))
        {
            SaveDisplay = SaveStatus == TEXT("SessionInvalid") ? TEXT("RECONNECT [R]") : TEXT("SAVE ISSUE");
            SaveColor = HUDColors::TextDanger;
        }

        SaveConnectionText->SetText(FText::FromString(SaveDisplay));
        SaveConnectionText->SetColorAndOpacity(FSlateColor(SaveColor));
        SaveConnectionText->SetToolTipText(FText::FromString(SaveErrorMessage.IsEmpty() ? TEXT("Backend save connection healthy.") : SaveErrorMessage));

        // Skill point indicator
        if (SkillPointText)
        {
            const AMOBAMMOPlayerState* PSLocal = GetOwningPlayer()
                ? GetOwningPlayer()->GetPlayerState<AMOBAMMOPlayerState>() : nullptr;
            const int32 SP = PSLocal ? PSLocal->GetSkillPoints() : 0;
            if (SP > 0)
            {
                SkillPointText->SetText(FText::FromString(
                    FString::Printf(TEXT("★ %d SP  [F1-F4]"), SP)));
                SkillPointText->SetVisibility(ESlateVisibility::Visible);
            }
            else
            {
                SkillPointText->SetVisibility(ESlateVisibility::Collapsed);
            }
        }
    }

    // ── Target Frame ──
    if (TargetFrameBorder)
    {
        TargetFrameBorder->SetVisibility(bHasTarget ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }

    if (bHasTarget)
    {
        if (TargetNameText)
        {
            TargetNameText->SetText(FText::FromString(TargetName));
        }

        if (TargetClassText)
        {
            const bool bSelectedMinion = SelectedTargetCharacterId == AMOBAMMOTrainingMinionActor::GetTrainingMinionCharacterId();
            const FString TargetClassDisplay = bSelectedMinion && !MinionThreatDisplay.IsEmpty()
                ? FString::Printf(TEXT("%s  -  %s"), *TargetClass, *MinionThreatDisplay)
                : FString::Printf(TEXT("%s  -  Lv.%d"), *TargetClass, TargetLevel);
            TargetClassText->SetText(FText::FromString(TargetClassDisplay));
        }

        if (TargetRangeText)
        {
            TargetRangeText->SetText(FText::FromString(TargetRangeStr));
            TargetRangeText->SetColorAndOpacity(FSlateColor(bTargetInRange ? HUDColors::TextSuccess : HUDColors::TextDanger));
        }

        if (TargetHealthBar)
        {
            TargetHealthBar->SetPercent(TargetMaxHealth > 0.0f ? TargetHealth / TargetMaxHealth : 0.0f);
        }

        if (TargetHealthValueText)
        {
            TargetHealthValueText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), TargetHealth, TargetMaxHealth)));
        }

        if (TargetManaBar)
        {
            TargetManaBar->SetPercent(TargetMaxMana > 0.0f ? TargetMana / TargetMaxMana : 0.0f);
        }

        if (TargetManaValueText)
        {
            TargetManaValueText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), TargetMana, TargetMaxMana)));
        }
    }

    // ── Combat Event ──
    if (CombatEventText)
    {
        const float EventAlpha = CombatEventHighlightRemaining > 0.0f
            ? FMath::Clamp(CombatEventHighlightRemaining / 2.25f, 0.0f, 1.0f)
            : 0.0f;
        CombatEventText->SetText(FText::FromString(EventAlpha > 0.0f ? LastCombatLog : TEXT("")));
        CombatEventText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.86f, 0.54f, EventAlpha)));
    }

    // ── Combat Log ──
    if (CombatLogText)
    {
        FString LogDisplay = TEXT("─── Combat Log ───");
        if (!CombatFeedDisplay.IsEmpty())
        {
            LogDisplay += TEXT("\n") + CombatFeedDisplay;
        }
        else
        {
            LogDisplay += TEXT("\nNo recent events.");
        }
        CombatLogText->SetText(FText::FromString(LogDisplay));
    }

    // ── Chat History ──
    if (ChatHistoryText)
    {
        if (const UWorld* World = GetWorld())
        {
            if (const AMOBAMMOGameState* GameState = World->GetGameState<AMOBAMMOGameState>())
            {
                const TArray<FString>& ChatMsgs = GameState->GetChatMessages();
                if (ChatMsgs.IsEmpty())
                {
                    ChatHistoryText->SetText(FText::GetEmpty());
                }
                else
                {
                    FString ChatDisplay;
                    // Show last 4 messages (array is newest-first)
                    for (int32 i = FMath::Min(ChatMsgs.Num(), 4) - 1; i >= 0; --i)
                    {
                        if (!ChatDisplay.IsEmpty())
                        {
                            ChatDisplay += TEXT("\n");
                        }
                        ChatDisplay += ChatMsgs[i];
                    }
                    ChatHistoryText->SetText(FText::FromString(ChatDisplay));
                }
            }
        }
    }

    // ── Action Hint ──
    const TArray<FMOBAMMOAbilityDefinition> AbilityDefinitions = MOBAMMOAbilitySet::ForClass(ClassId);
    const FMOBAMMOAbilityDefinition DamageAbility = AbilityDefinitions.IsValidIndex(0) ? AbilityDefinitions[0] : MOBAMMOAbilitySet::ArcBurst();
    const FMOBAMMOAbilityDefinition DrainAbility = AbilityDefinitions.IsValidIndex(2) ? AbilityDefinitions[2] : MOBAMMOAbilitySet::DrainPulse();
    const FMOBAMMOAbilityDefinition ManaSurgeAbility = AbilityDefinitions.IsValidIndex(3) ? AbilityDefinitions[3] : MOBAMMOAbilitySet::ManaSurge();
    const FMOBAMMOAbilityDefinition RespawnAbility = AbilityDefinitions.IsValidIndex(4) ? AbilityDefinitions[4] : MOBAMMOAbilitySet::Reforge();

    FString ActionHintDisplay;
    if (LifeState == TEXT("Dead"))
    {
        ActionHintDisplay = RespawnCooldownRemaining > KINDA_SMALL_NUMBER
            ? FString::Printf(TEXT("%s charging... %.1fs"), *RespawnAbility.Name, RespawnCooldownRemaining)
            : FString::Printf(TEXT("Press [5] to %s"), *RespawnAbility.Name);
    }
    else if (!bHasTarget)
    {
        ActionHintDisplay = MinionThreatDisplay.IsEmpty()
            ? FString::Printf(TEXT("Aim at Training Dummy and press [LMB]/[E], or press [6]. Then use [1] %s / [3] %s"), *DamageAbility.Name, *DrainAbility.Name)
            : MinionThreatDisplay;
    }
    else if (!bTargetInRange)
    {
        ActionHintDisplay = TEXT("Target out of range — move closer");
    }
    else if (DamageCooldownRemaining > KINDA_SMALL_NUMBER)
    {
        ActionHintDisplay = FString::Printf(TEXT("%s in %.1fs - use [2] or [3]"), *DamageAbility.Name, DamageCooldownRemaining);
    }
    else if (Mana < DamageAbility.ManaCost)
    {
        ActionHintDisplay = FString::Printf(TEXT("Low mana - use %s first"), *ManaSurgeAbility.Name);
    }
    else
    {
        ActionHintDisplay = FString::Printf(TEXT("Target exposed - %s ready!"), *DamageAbility.Name);
    }

    if (ActionHintText)
    {
        ActionHintText->SetText(FText::FromString(ActionHintDisplay));
    }

    // ── Arc Charge ──
    if (ArcChargeText)
    {
        if (ArcChargeRemaining > KINDA_SMALL_NUMBER)
        {
            const FString ChargeLabel = ClassId.Equals(TEXT("mage"), ESearchCase::IgnoreCase) ? TEXT("SAGE FOCUS") : TEXT("ARC CHARGE");
            ArcChargeText->SetText(FText::FromString(FString::Printf(TEXT("* %s  %.1fs"), *ChargeLabel, ArcChargeRemaining)));
            ArcChargeText->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            ArcChargeText->SetText(FText::GetEmpty());
            ArcChargeText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // ── Respawn Hint ──
    if (RespawnHintText)
    {
        const bool bShowRespawnHint = LifeState == TEXT("Dead");
        RespawnHintText->SetVisibility(bShowRespawnHint ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        RespawnHintText->SetText(FText::FromString(
            RespawnCooldownRemaining > KINDA_SMALL_NUMBER
                ? FString::Printf(TEXT("YOU ARE DOWN\n%s in %.1fs"), *RespawnAbility.Name, RespawnCooldownRemaining)
                : FString::Printf(TEXT("YOU ARE DOWN\nPress [5] to %s"), *RespawnAbility.Name)
        ));
    }

    // ── Floating Feedback ──
    if (FloatingFeedbackText)
    {
        const bool bShowFloating = !FloatingFeedbackStr.IsEmpty() && FloatingFeedbackAlpha > 0.0f;
        FloatingFeedbackText->SetText(FText::FromString(bShowFloating ? FloatingFeedbackStr : TEXT("")));
        FloatingFeedbackText->SetColorAndOpacity(FSlateColor(
            bFloatingFeedbackHealing
                ? FLinearColor(0.34f, 0.96f, 0.62f, FloatingFeedbackAlpha)
                : FLinearColor(0.98f, 0.34f, 0.38f, FloatingFeedbackAlpha)
        ));
        FloatingFeedbackText->SetRenderTranslation(FloatingFeedbackScreenPosition);
    }

    // ── Ability Bar ──
    for (int32 AbilityIndex = 0; AbilityIndex < AbilityDefinitions.Num(); ++AbilityIndex)
    {
        const FMOBAMMOAbilityDefinition& Def = AbilityDefinitions[AbilityIndex];
        const bool bHasCooldown = Def.Cooldown > KINDA_SMALL_NUMBER;
        float CooldownRemaining = 0.0f;

        if (Def.Slot == 1)        CooldownRemaining = DamageCooldownRemaining;
        else if (Def.Slot == 2)   CooldownRemaining = HealCooldownRemaining;
        else if (Def.Slot == 3)   CooldownRemaining = DrainCooldownRemaining;
        else if (Def.Slot == 4)   CooldownRemaining = ManaSurgeCooldownRemaining;

        // Determine state
        FString StateLabel = TEXT("READY");
        FLinearColor StateColor = HUDColors::TextSuccess;
        FLinearColor BarColor = HUDColors::AbilityReady;
        FLinearColor BorderColor = FLinearColor(0.18f, 0.50f, 0.82f, 0.60f);
        float BarPercent = 1.0f;

        if (LifeState == TEXT("Dead") && Def.Kind != EMOBAMMOAbilityKind::Respawn)
        {
            StateLabel = TEXT("DOWN");
            StateColor = HUDColors::TextDead;
            BarColor = HUDColors::AbilityDead;
            BorderColor = FLinearColor(0.24f, 0.10f, 0.10f, 0.50f);
            BarPercent = 0.0f;
        }
        else if (Def.Kind == EMOBAMMOAbilityKind::Respawn && LifeState != TEXT("Dead"))
        {
            StateLabel = TEXT("LOCKED");
            StateColor = HUDColors::TextSecondary;
            BarColor = HUDColors::AbilityLocked;
            BorderColor = FLinearColor(0.14f, 0.14f, 0.18f, 0.50f);
            BarPercent = 0.0f;
        }
        else if (Def.Kind == EMOBAMMOAbilityKind::Respawn && LifeState == TEXT("Dead"))
        {
            BarColor = HUDColors::AbilityRespawn;
            BorderColor = FLinearColor(0.16f, 0.64f, 0.40f, 0.70f);
            BarPercent = RespawnBarReferenceDuration > 0.0f
                ? FMath::Clamp(1.0f - (RespawnCooldownRemaining / RespawnBarReferenceDuration), 0.0f, 1.0f)
                : 1.0f;

            if (RespawnCooldownRemaining > KINDA_SMALL_NUMBER)
            {
                StateLabel = FString::Printf(TEXT("%.1fs"), RespawnCooldownRemaining);
                StateColor = HUDColors::TextGold;
            }
            else
            {
                StateLabel = TEXT("READY");
                StateColor = HUDColors::TextSuccess;
            }
        }
        else if (Def.Slot == 3 && !bHasTarget)
        {
            StateLabel = TEXT("NO TGT");
            StateColor = HUDColors::TextSecondary;
            BarColor = HUDColors::AbilityNoTarget;
            BorderColor = FLinearColor(0.32f, 0.16f, 0.12f, 0.50f);
        }
        else if (Def.Slot == 3 && !bTargetInRange)
        {
            StateLabel = TEXT("FAR");
            StateColor = HUDColors::TextDanger;
            BarColor = HUDColors::AbilityNoTarget;
            BorderColor = FLinearColor(0.32f, 0.16f, 0.12f, 0.50f);
        }
        else if (Def.Slot == 1 && ArcChargeRemaining > KINDA_SMALL_NUMBER)
        {
            StateLabel = FString::Printf(TEXT("* %.1fs"), ArcChargeRemaining);
            StateColor = HUDColors::TextCyan;
            BarColor = HUDColors::AbilityCharged;
            BorderColor = FLinearColor(0.30f, 0.70f, 1.00f, 0.80f);
        }
        else if (bHasCooldown && CooldownRemaining > KINDA_SMALL_NUMBER)
        {
            StateLabel = FString::Printf(TEXT("%.1fs"), CooldownRemaining);
            StateColor = HUDColors::TextGold;
            BarColor = HUDColors::AbilityCooldown;
            BorderColor = FLinearColor(0.52f, 0.42f, 0.14f, 0.55f);
            BarPercent = Def.Cooldown > 0.0f
                ? FMath::Clamp(1.0f - (CooldownRemaining / Def.Cooldown), 0.0f, 1.0f)
                : 1.0f;
        }
        else if (Def.ManaCost > 0.0f && Mana < Def.ManaCost)
        {
            StateLabel = TEXT("NO MP");
            StateColor = HUDColors::TextDanger;
            BarColor = HUDColors::AbilityNoMana;
            BorderColor = FLinearColor(0.42f, 0.14f, 0.14f, 0.55f);
        }
        else
        {
            if (Def.Kind == EMOBAMMOAbilityKind::Damage)
            {
                BarColor = HUDColors::AbilityDamage;
                BorderColor = FLinearColor(0.72f, 0.18f, 0.22f, 0.60f);
            }
            else if (Def.Kind == EMOBAMMOAbilityKind::Heal)
            {
                BarColor = HUDColors::AbilityHeal;
                BorderColor = FLinearColor(0.16f, 0.62f, 0.38f, 0.60f);
            }
        }

        if (AbilitySlotBorders.IsValidIndex(AbilityIndex) && AbilitySlotBorders[AbilityIndex])
        {
            AbilitySlotBorders[AbilityIndex]->SetBrushColor(BorderColor);
        }

        if (AbilityStateTexts.IsValidIndex(AbilityIndex) && AbilityStateTexts[AbilityIndex])
        {
            AbilityStateTexts[AbilityIndex]->SetText(FText::FromString(StateLabel));
            AbilityStateTexts[AbilityIndex]->SetColorAndOpacity(FSlateColor(StateColor));
        }

        if (AbilityNameTexts.IsValidIndex(AbilityIndex) && AbilityNameTexts[AbilityIndex])
        {
            // Show ability rank if above 1 (e.g. "Arc Burst Lv.3")
            FString AbilityLabel = Def.Name;
            const AMOBAMMOPlayerState* PSForAbility = GetOwningPlayer()
                ? GetOwningPlayer()->GetPlayerState<AMOBAMMOPlayerState>() : nullptr;
            if (PSForAbility && AbilityIndex < 4) // only player abilities 0-3 have ranks
            {
                const int32 Rank = PSForAbility->GetAbilityRank(AbilityIndex);
                if (Rank > 1)
                {
                    AbilityLabel = FString::Printf(TEXT("%s Lv.%d"), *Def.Name, Rank);
                }
                // Glow slot border gold if upgradeable (skill points available, not max rank)
                if (PSForAbility->CanUpgradeAbility(AbilityIndex) && AbilitySlotBorders.IsValidIndex(AbilityIndex))
                {
                    AbilitySlotBorders[AbilityIndex]->SetBrushColor(FLinearColor(0.85f, 0.65f, 0.05f, 0.75f));
                }
            }
            AbilityNameTexts[AbilityIndex]->SetText(FText::FromString(AbilityLabel));
        }

        if (AbilityCooldownBars.IsValidIndex(AbilityIndex) && AbilityCooldownBars[AbilityIndex])
        {
            AbilityCooldownBars[AbilityIndex]->SetPercent(BarPercent);
            AbilityCooldownBars[AbilityIndex]->SetFillColorAndOpacity(BarColor);
        }
    }
}

// ─────────────────────────────────────────────────────────────
// Layout: Quest / Objective Panel (top-left, below combat log)
// ─────────────────────────────────────────────────────────────
void UMOBAMMOGameHUDWidget::BuildQuestPanel(UCanvasPanel* Canvas)
{
    // Constants — panel sits just below the combat log (20,20,340,180)
    static constexpr float PanelX  = 20.0f;
    static constexpr float PanelY  = 210.0f;   // combat log bottom = 20+180 = 200, +10 gap
    static constexpr float PanelW  = 230.0f;
    static constexpr int32 MaxRows = 5;
    static constexpr float RowH    = 18.0f;
    static constexpr float TitleH  = 20.0f;
    static constexpr float PadV    = 6.0f;
    const float PanelH = TitleH + PadV + MaxRows * RowH + PadV;

    // Background panel
    QuestPanelBorder = MakeGlassPanel(FLinearColor(0.01f, 0.015f, 0.03f), 0.80f);
    QuestPanelBorder->SetPadding(FMargin(0.0f));

    if (UCanvasPanelSlot* QSlot = Canvas->AddChildToCanvas(QuestPanelBorder))
    {
        QSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
        QSlot->SetAlignment(FVector2D(0.0f, 0.0f));
        QSlot->SetPosition(FVector2D(PanelX, PanelY));
        QSlot->SetSize(FVector2D(PanelW, PanelH));
        QSlot->SetAutoSize(false);
    }

    // Inner vertical box
    UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    if (UBorderSlot* BS = Cast<UBorderSlot>(QuestPanelBorder->AddChild(VBox)))
    {
        BS->SetPadding(FMargin(0.0f));
    }

    // Title bar
    {
        UBorder* TitleBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        TitleBg->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.07f, 1.0f));
        TitleBg->SetPadding(FMargin(8.0f, 3.0f));
        if (UVerticalBoxSlot* VS = VBox->AddChildToVerticalBox(TitleBg))
        {
            VS->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        }
        UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Title->SetText(FText::FromString(TEXT("OBJECTIVES")));
        FSlateFontInfo TF = Title->GetFont();
        TF.Size = 9;
        TF.TypefaceFontName = TEXT("Bold");
        Title->SetFont(TF);
        Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.70f, 1.0f, 1.0f)));
        TitleBg->SetContent(Title);
    }

    // Spacer
    {
        USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
        Sp->SetSize(FVector2D(1.0f, PadV));
        if (UVerticalBoxSlot* VS = VBox->AddChildToVerticalBox(Sp))
        {
            VS->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        }
    }

    // Quest rows
    QuestRowTexts.Reset();
    const TArray<FMOBAMMOQuestDefinition>& AllQuests = UMOBAMMOQuestCatalog::GetAllQuests();
    for (int32 i = 0; i < MaxRows; ++i)
    {
        UTextBlock* Row = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        if (AllQuests.IsValidIndex(i))
        {
            Row->SetText(FText::FromString(FString::Printf(TEXT("○ %s"), *AllQuests[i].DisplayName)));
        }
        else
        {
            Row->SetText(FText::FromString(TEXT("")));
        }
        FSlateFontInfo RF = Row->GetFont();
        RF.Size = 9;
        Row->SetFont(RF);
        Row->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f)));

        UBorder* RowBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        RowBg->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
        RowBg->SetPadding(FMargin(8.0f, 2.0f));
        RowBg->SetContent(Row);

        if (UVerticalBoxSlot* VS = VBox->AddChildToVerticalBox(RowBg))
        {
            VS->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        }
        QuestRowTexts.Add(Row);
    }
}

void UMOBAMMOGameHUDWidget::UpdateQuestPanel()
{
    if (!QuestPanelBorder || QuestRowTexts.Num() == 0)
    {
        return;
    }

    const AMOBAMMOPlayerState* PS = nullptr;
    if (const APlayerController* PC = GetOwningPlayer())
    {
        PS = PC->GetPlayerState<AMOBAMMOPlayerState>();
    }

    const TArray<FMOBAMMOQuestDefinition>& AllQuests = UMOBAMMOQuestCatalog::GetAllQuests();

    for (int32 i = 0; i < QuestRowTexts.Num(); ++i)
    {
        UTextBlock* Row = QuestRowTexts[i];
        if (!Row || !AllQuests.IsValidIndex(i))
        {
            if (Row) { Row->SetText(FText::FromString(TEXT(""))); }
            continue;
        }

        const FMOBAMMOQuestDefinition& Def = AllQuests[i];

        // Find progress for this quest
        int32 Current   = 0;
        bool  bComplete = false;
        if (PS)
        {
            for (const FMOBAMMOQuestProgress& P : PS->GetQuestProgress())
            {
                if (P.QuestId == Def.QuestId)
                {
                    Current   = P.CurrentCount;
                    bComplete = P.bCompleted;
                    break;
                }
            }
        }

        FString Label;
        FLinearColor Color;

        if (bComplete)
        {
            Label = FString::Printf(TEXT("✓ %s"), *Def.DisplayName);
            Color = FLinearColor(0.25f, 1.0f, 0.40f, 1.0f);   // green
        }
        else
        {
            const int32 Target = Def.TargetCount;
            const FString Progress = (Target > 1)
                ? FString::Printf(TEXT(" (%d/%d)"), Current, Target)
                : FString();
            Label = FString::Printf(TEXT("○ %s%s"), *Def.DisplayName, *Progress);
            Color = FLinearColor(0.75f, 0.75f, 0.75f, 1.0f);   // gray-white
        }

        Row->SetText(FText::FromString(Label));
        Row->SetColorAndOpacity(FSlateColor(Color));
    }
}

// ─────────────────────────────────────────────────────────────
// Callbacks
// ─────────────────────────────────────────────────────────────
void UMOBAMMOGameHUDWidget::HandleBackendStateChanged()
{
    RefreshFromBackend();
}

void UMOBAMMOGameHUDWidget::HandleReplicatedStateChanged()
{
    RefreshFromBackend();
}
