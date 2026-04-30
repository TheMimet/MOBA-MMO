#include "MOBAMMOPauseMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "MOBAMMOBackendSubsystem.h"
#include "MOBAMMOGameState.h"
#include "MOBAMMOPlayerState.h"
#include "MOBAMMOGameUISubsystem.h"

// ─────────────────────────────────────────────────────────────
// Colour palette  (matches HUD panel colours)
// ─────────────────────────────────────────────────────────────
namespace PauseColors
{
    static const FLinearColor Backdrop     (0.000f, 0.002f, 0.010f, 0.72f);
    static const FLinearColor CardBg       (0.016f, 0.020f, 0.034f, 0.97f);
    static const FLinearColor CardGlow     (0.140f, 0.180f, 0.280f, 0.55f);
    static const FLinearColor Divider      (0.120f, 0.150f, 0.220f, 0.40f);
    static const FLinearColor HeaderBg     (0.024f, 0.028f, 0.050f, 1.00f);
    static const FLinearColor BtnDefault   (0.030f, 0.038f, 0.068f, 1.00f);
    static const FLinearColor BtnHover     (0.060f, 0.080f, 0.140f, 1.00f);
    static const FLinearColor BtnDanger    (0.120f, 0.024f, 0.028f, 1.00f);
    static const FLinearColor BtnDisabled  (0.022f, 0.024f, 0.036f, 1.00f);
    static const FLinearColor LeaderBg     (0.020f, 0.024f, 0.040f, 0.80f);
    static const FLinearColor TextPrimary  (0.920f, 0.940f, 0.960f, 1.00f);
    static const FLinearColor TextSecondary(0.520f, 0.580f, 0.660f, 1.00f);
    static const FLinearColor TextGold     (0.960f, 0.820f, 0.440f, 1.00f);
    static const FLinearColor TextCyan     (0.460f, 0.820f, 1.000f, 1.00f);
    static const FLinearColor TextDanger   (0.960f, 0.320f, 0.360f, 1.00f);
    static const FLinearColor TextSuccess  (0.320f, 0.900f, 0.540f, 1.00f);
    static const FLinearColor TextDisabled (0.280f, 0.300f, 0.340f, 1.00f);
}

// ─────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────
static UTextBlock* PauseText(UWidgetTree* T, const FString& Str, int32 Size,
                              const FLinearColor& Col, bool bBold = false,
                              ETextJustify::Type Just = ETextJustify::Left)
{
    UTextBlock* W = T->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    W->SetText(FText::FromString(Str));
    W->SetColorAndOpacity(FSlateColor(Col));
    W->SetJustification(Just);
    FSlateFontInfo F = W->GetFont();
    F.Size = Size;
    if (bBold) F.TypefaceFontName = TEXT("Bold");
    W->SetFont(F);
    return W;
}

UBorder* UMOBAMMOPauseMenuWidget::MakeGlassPanel(const FLinearColor& Color, float Alpha) const
{
    UBorder* B = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    FLinearColor C = Color; C.A = Alpha;
    B->SetBrushColor(C);
    return B;
}

UBorder* UMOBAMMOPauseMenuWidget::MakeDivider() const
{
    UBorder* Sep = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Sep->SetBrushColor(PauseColors::Divider);
    USizeBox* SepBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    SepBox->SetHeightOverride(1.0f);
    SepBox->AddChild(Sep);
    // We return the border; caller wraps in size box manually
    return Sep;
}

UButton* UMOBAMMOPauseMenuWidget::MakeMenuButton(const FString& Label,
                                                   const FLinearColor& LabelColor,
                                                   const FLinearColor& BgColor,
                                                   int32 FontSize) const
{
    UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
    Btn->SetBackgroundColor(BgColor);

    UTextBlock* Lbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Lbl->SetText(FText::FromString(Label));
    Lbl->SetColorAndOpacity(FSlateColor(LabelColor));
    FSlateFontInfo F = Lbl->GetFont();
    F.Size = FontSize;
    F.TypefaceFontName = TEXT("Bold");
    Lbl->SetFont(F);
    Lbl->SetJustification(ETextJustify::Center);
    Btn->AddChild(Lbl);

    return Btn;
}

// ─────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────
void UMOBAMMOPauseMenuWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    BuildLayout();
}

void UMOBAMMOPauseMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetRenderOpacity(0.0f);
    SetVisibility(ESlateVisibility::Collapsed);
}

void UMOBAMMOPauseMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (FMath::Abs(FadeAlpha - FadeTarget) > 0.002f)
    {
        FadeAlpha = FMath::FInterpTo(FadeAlpha, FadeTarget, InDeltaTime, FadeSpeed);
        SetRenderOpacity(FadeAlpha);
        if (FadeAlpha < 0.01f && FadeTarget < 0.01f)
        {
            SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}

// ─────────────────────────────────────────────────────────────
// Show / Hide
// ─────────────────────────────────────────────────────────────
void UMOBAMMOPauseMenuWidget::ToggleVisibility()
{
    if (bIsVisible) HideMenu(); else ShowMenu();
}

void UMOBAMMOPauseMenuWidget::ShowMenu()
{
    if (bIsVisible) return;
    bIsVisible = true;
    RefreshContent();
    SetVisibility(ESlateVisibility::Visible);
    FadeTarget = 1.0f;

    if (APlayerController* PC = GetOwningPlayer())
    {
        FInputModeGameAndUI InputMode;
        InputMode.SetHideCursorDuringCapture(false);
        PC->SetInputMode(InputMode);
        PC->SetShowMouseCursor(true);
    }
}

void UMOBAMMOPauseMenuWidget::HideMenu()
{
    if (!bIsVisible) return;
    bIsVisible = false;
    FadeTarget = 0.0f;

    if (APlayerController* PC = GetOwningPlayer())
    {
        FInputModeGameAndUI InputMode;
        InputMode.SetHideCursorDuringCapture(false);
        PC->SetInputMode(InputMode);
        PC->SetShowMouseCursor(true);
    }
}

// ─────────────────────────────────────────────────────────────
// Content refresh (leaderboard + header)
// ─────────────────────────────────────────────────────────────
void UMOBAMMOPauseMenuWidget::RefreshContent()
{
    // ── Header ──
    const APlayerController* PC = GetOwningPlayer();
    const AMOBAMMOPlayerState* PS = PC ? PC->GetPlayerState<AMOBAMMOPlayerState>() : nullptr;
    if (PS)
    {
        if (HeaderNameText)
        {
            HeaderNameText->SetText(FText::FromString(
                PS->GetCharacterName().IsEmpty() ? PS->GetPlayerName() : PS->GetCharacterName()
            ));
        }
        if (HeaderClassText)
        {
            HeaderClassText->SetText(FText::FromString(
                FString::Printf(TEXT("%s  -  Level %d"),
                    *PS->GetClassId(),
                    PS->GetCharacterLevel())
            ));
        }
        if (HeaderKDText)
        {
            HeaderKDText->SetText(FText::FromString(
                FString::Printf(TEXT("K %d  /  D %d"), PS->GetKills(), PS->GetDeaths())
            ));
        }
    }

    // ── Leaderboard ──
    if (!LeaderboardText) return;

    const UWorld* World = GetWorld();
    const AMOBAMMOGameState* GS = World ? World->GetGameState<AMOBAMMOGameState>() : nullptr;
    if (!GS)
    {
        LeaderboardText->SetText(FText::FromString(TEXT("Waiting for server data...")));
        return;
    }

    // Collect and sort by kills
    TArray<const AMOBAMMOPlayerState*> Entries;
    for (APlayerState* RawPS : GS->PlayerArray)
    {
        if (const AMOBAMMOPlayerState* Entry = Cast<AMOBAMMOPlayerState>(RawPS))
        {
            Entries.Add(Entry);
        }
    }
    Entries.Sort([](const AMOBAMMOPlayerState& A, const AMOBAMMOPlayerState& B)
    {
        if (A.GetKills() != B.GetKills()) return A.GetKills() > B.GetKills();
        return A.GetDeaths() < B.GetDeaths();
    });

    FString Board;
    Board += FString::Printf(TEXT("%-2s  %-16s  %5s  %5s  %5s  %3s\n"),
        TEXT("#"), TEXT("Name"), TEXT("K"), TEXT("D"), TEXT("K/D"), TEXT("Lv"));
    Board += TEXT("--  ----------------  -----  -----  -----  ---\n");

    for (int32 i = 0; i < Entries.Num(); ++i)
    {
        const AMOBAMMOPlayerState* E = Entries[i];
        const FString Name = (E->GetCharacterName().IsEmpty() ? E->GetPlayerName() : E->GetCharacterName()).Left(16);
        const float KD = E->GetDeaths() > 0 ? (float)E->GetKills() / (float)E->GetDeaths() : (float)E->GetKills();
        const bool bSelf = (E == PS);
        const FString Prefix = bSelf ? TEXT("> ") : TEXT("  ");
        Board += FString::Printf(TEXT("%s%-2d  %-16s  %5d  %5d  %5.2f  %3d\n"),
            *Prefix, i + 1, *Name, E->GetKills(), E->GetDeaths(), KD, E->GetCharacterLevel());
    }

    if (Entries.IsEmpty())
    {
        Board = TEXT("No players found.");
    }

    LeaderboardText->SetText(FText::FromString(Board));
}

// ─────────────────────────────────────────────────────────────
// Build Layout
// ─────────────────────────────────────────────────────────────
void UMOBAMMOPauseMenuWidget::BuildLayout()
{
    if (!WidgetTree) return;

    if (WidgetTree->RootWidget)
    {
        WidgetTree->RootWidget->RemoveFromParent();
        WidgetTree->RootWidget = nullptr;
    }

    RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
    WidgetTree->RootWidget = RootCanvas;

    // ── Full-screen backdrop button (click to close) ──
    BackdropButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
    BackdropButton->SetBackgroundColor(PauseColors::Backdrop);
    BackdropButton->OnClicked.AddDynamic(this, &UMOBAMMOPauseMenuWidget::HandleBackdropClicked);
    if (UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(BackdropButton))
    {
        S->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
        S->SetOffsets(FMargin(0.0f));
    }

    // ── Card glow border ──
    CardGlow = MakeGlassPanel(PauseColors::CardGlow, 1.0f);
    CardGlow->SetPadding(FMargin(1.5f));
    if (UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(CardGlow))
    {
        S->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
        S->SetAlignment(FVector2D(0.5f, 0.5f));
        S->SetAutoSize(true);
        S->SetZOrder(10);
    }

    // ── Card background ──
    CardBg = MakeGlassPanel(PauseColors::CardBg, 1.0f);
    CardBg->SetPadding(FMargin(0.0f));
    CardGlow->SetContent(CardBg);

    // ── Card content vertical box ──
    CardContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

    // Fixed-width wrapper
    USizeBox* WidthBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    WidthBox->SetWidthOverride(420.0f);
    WidthBox->AddChild(CardContent);
    CardBg->SetContent(WidthBox);

    BuildHeader(CardContent);

    // Divider
    {
        UBorder* D = MakeGlassPanel(PauseColors::Divider, 1.0f);
        USizeBox* DB = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        DB->SetHeightOverride(1.0f);
        DB->AddChild(D);
        CardContent->AddChildToVerticalBox(DB);
    }

    BuildActionButtons(CardContent);

    // Divider
    {
        UBorder* D = MakeGlassPanel(PauseColors::Divider, 1.0f);
        USizeBox* DB = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        DB->SetHeightOverride(1.0f);
        DB->AddChild(D);
        CardContent->AddChildToVerticalBox(DB);
    }

    BuildLeaderboard(CardContent);

    // Divider
    {
        UBorder* D = MakeGlassPanel(PauseColors::Divider, 1.0f);
        USizeBox* DB = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        DB->SetHeightOverride(1.0f);
        DB->AddChild(D);
        CardContent->AddChildToVerticalBox(DB);
    }

    BuildDangerButtons(CardContent);
    BuildFooter(CardContent);
}

// ─────────────────────────────────────────────────────────────
// Header section
// ─────────────────────────────────────────────────────────────
void UMOBAMMOPauseMenuWidget::BuildHeader(UVerticalBox* Parent)
{
    UBorder* HeaderBg = MakeGlassPanel(PauseColors::HeaderBg, 1.0f);
    HeaderBg->SetPadding(FMargin(28.0f, 22.0f, 28.0f, 18.0f));
    Parent->AddChildToVerticalBox(HeaderBg);

    UVerticalBox* HContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    HeaderBg->SetContent(HContent);

    // Game title badge
    UTextBlock* Badge = PauseText(WidgetTree, TEXT("*  MOBA MMO"), 10,
        PauseColors::TextSecondary, false, ETextJustify::Center);
    if (UVerticalBoxSlot* S = HContent->AddChildToVerticalBox(Badge))
    {
        S->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
    }

    // Character name (big gold)
    HeaderNameText = PauseText(WidgetTree, TEXT("Hero"), 26,
        PauseColors::TextGold, true, ETextJustify::Center);
    if (UVerticalBoxSlot* S = HContent->AddChildToVerticalBox(HeaderNameText))
    {
        S->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
    }

    // Class · Level
    HeaderClassText = PauseText(WidgetTree, TEXT("Warrior  -  Level 1"), 12,
        PauseColors::TextSecondary, false, ETextJustify::Center);
    if (UVerticalBoxSlot* S = HContent->AddChildToVerticalBox(HeaderClassText))
    {
        S->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
    }

    // Thin internal divider
    {
        UBorder* D = MakeGlassPanel(PauseColors::Divider, 0.6f);
        USizeBox* DB = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        DB->SetHeightOverride(1.0f);
        DB->AddChild(D);
        if (UVerticalBoxSlot* S = HContent->AddChildToVerticalBox(DB))
        {
            S->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
        }
    }

    // K / D stat row
    UHorizontalBox* StatRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    HContent->AddChildToVerticalBox(StatRow);

    // Kills
    {
        UVerticalBox* KBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
        UTextBlock* KLabel = PauseText(WidgetTree, TEXT("KILLS"), 9,
            PauseColors::TextSecondary, false, ETextJustify::Center);
        HeaderKDText = PauseText(WidgetTree, TEXT("0  /  0"), 18,
            PauseColors::TextCyan, true, ETextJustify::Center);
        KBox->AddChildToVerticalBox(KLabel);
        KBox->AddChildToVerticalBox(HeaderKDText);
        if (UHorizontalBoxSlot* S = StatRow->AddChildToHorizontalBox(KBox))
        {
            S->SetSize(ESlateSizeRule::Fill);
            S->SetHorizontalAlignment(HAlign_Center);
        }
    }
}

// ─────────────────────────────────────────────────────────────
// Action buttons  (Resume, Inventory)
// ─────────────────────────────────────────────────────────────
void UMOBAMMOPauseMenuWidget::BuildActionButtons(UVerticalBox* Parent)
{
    UBorder* Section = MakeGlassPanel(PauseColors::CardBg, 1.0f);
    Section->SetPadding(FMargin(20.0f, 14.0f));
    if (UVerticalBoxSlot* S = Parent->AddChildToVerticalBox(Section))
    {
        S->SetPadding(FMargin(0.0f));
    }

    UVerticalBox* BtnList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Section->SetContent(BtnList);

    // ▶  RESUME
    {
        UButton* Btn = MakeMenuButton(TEXT(">   RESUME GAME"), PauseColors::TextSuccess,
            PauseColors::BtnDefault, 15);
        Btn->OnClicked.AddDynamic(this, &UMOBAMMOPauseMenuWidget::HandleResumeClicked);
        USizeBox* SB = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        SB->SetHeightOverride(50.0f);
        SB->AddChild(Btn);
        if (UVerticalBoxSlot* S = BtnList->AddChildToVerticalBox(SB))
        {
            S->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
        }
    }

    // 🎒  INVENTORY
    {
        UButton* Btn = MakeMenuButton(TEXT("[]  INVENTORY"), PauseColors::TextCyan,
            PauseColors::BtnDefault, 15);
        Btn->OnClicked.AddDynamic(this, &UMOBAMMOPauseMenuWidget::HandleInventoryClicked);
        USizeBox* SB = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        SB->SetHeightOverride(50.0f);
        SB->AddChild(Btn);
        BtnList->AddChildToVerticalBox(SB);
    }
}

// ─────────────────────────────────────────────────────────────
// Leaderboard section
// ─────────────────────────────────────────────────────────────
void UMOBAMMOPauseMenuWidget::BuildLeaderboard(UVerticalBox* Parent)
{
    UBorder* Section = MakeGlassPanel(PauseColors::LeaderBg, 1.0f);
    Section->SetPadding(FMargin(20.0f, 14.0f));
    if (UVerticalBoxSlot* S = Parent->AddChildToVerticalBox(Section))
    {
        S->SetPadding(FMargin(0.0f));
    }

    UVerticalBox* LBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Section->SetContent(LBox);

    // Section title
    UTextBlock* Title = PauseText(WidgetTree, TEXT("--- Zone Leaderboard ---"), 10,
        PauseColors::TextSecondary, false, ETextJustify::Center);
    if (UVerticalBoxSlot* S = LBox->AddChildToVerticalBox(Title))
    {
        S->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
    }

    // Leaderboard text (monospaced-style)
    LeaderboardText = PauseText(WidgetTree, TEXT("Loading..."), 10,
        PauseColors::TextPrimary, false, ETextJustify::Left);
    LeaderboardText->SetAutoWrapText(false);
    LBox->AddChildToVerticalBox(LeaderboardText);
}

// ─────────────────────────────────────────────────────────────
// Danger buttons (Disconnect)
// ─────────────────────────────────────────────────────────────
void UMOBAMMOPauseMenuWidget::BuildDangerButtons(UVerticalBox* Parent)
{
    UBorder* Section = MakeGlassPanel(PauseColors::CardBg, 1.0f);
    Section->SetPadding(FMargin(20.0f, 14.0f));
    Parent->AddChildToVerticalBox(Section);

    UVerticalBox* BtnList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Section->SetContent(BtnList);

    // ⚙  SETTINGS (disabled)
    {
        UButton* Btn = MakeMenuButton(TEXT("SETTINGS  (coming soon)"),
            PauseColors::TextDisabled, PauseColors::BtnDisabled, 13);
        Btn->SetIsEnabled(false);
        USizeBox* SB = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        SB->SetHeightOverride(44.0f);
        SB->AddChild(Btn);
        if (UVerticalBoxSlot* S = BtnList->AddChildToVerticalBox(SB))
        {
            S->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
        }
    }

    // ✕  DISCONNECT
    {
        UButton* Btn = MakeMenuButton(TEXT("X   DISCONNECT"), PauseColors::TextDanger,
            PauseColors::BtnDanger, 15);
        Btn->OnClicked.AddDynamic(this, &UMOBAMMOPauseMenuWidget::HandleDisconnectClicked);
        USizeBox* SB = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        SB->SetHeightOverride(50.0f);
        SB->AddChild(Btn);
        BtnList->AddChildToVerticalBox(SB);
    }
}

// ─────────────────────────────────────────────────────────────
// Footer
// ─────────────────────────────────────────────────────────────
void UMOBAMMOPauseMenuWidget::BuildFooter(UVerticalBox* Parent)
{
    UBorder* FooterBg = MakeGlassPanel(PauseColors::HeaderBg, 1.0f);
    FooterBg->SetPadding(FMargin(16.0f, 10.0f));
    Parent->AddChildToVerticalBox(FooterBg);

    UTextBlock* Hint = PauseText(WidgetTree,
        TEXT("[ESC]  or  click outside  to resume"),
        10, PauseColors::TextSecondary, false, ETextJustify::Center);
    FooterBg->SetContent(Hint);
}

// ─────────────────────────────────────────────────────────────
// Button handlers
// ─────────────────────────────────────────────────────────────
void UMOBAMMOPauseMenuWidget::HandleResumeClicked()
{
    HideMenu();
}

void UMOBAMMOPauseMenuWidget::HandleInventoryClicked()
{
    HideMenu();
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UMOBAMMOGameUISubsystem* UISub = GI->GetSubsystem<UMOBAMMOGameUISubsystem>())
        {
            UISub->ToggleInventory();
        }
    }
}

void UMOBAMMOPauseMenuWidget::HandleDisconnectClicked()
{
    HideMenu();

    // Fire the final save / session end before disconnecting
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UMOBAMMOBackendSubsystem* Backend = GI->GetSubsystem<UMOBAMMOBackendSubsystem>())
        {
            if (APlayerController* PC = GetOwningPlayer())
            {
                Backend->EndCurrentCharacterSession(PC);
                Backend->ReturnToCharacterSelectAfterDisconnect(PC);
            }
        }
    }
}

void UMOBAMMOPauseMenuWidget::HandleBackdropClicked()
{
    HideMenu();
}
