#include "MOBAMMOGameHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "MOBAMMOAbilitySet.h"
#include "MOBAMMOBackendSubsystem.h"
#include "MOBAMMOGameState.h"
#include "MOBAMMOPlayerController.h"
#include "MOBAMMOPlayerState.h"

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

    if (IsVisible())
    {
        UpdateTexts();
    }
}

void UMOBAMMOGameHUDWidget::RefreshFromBackend()
{
    UpdateTexts();
    SetVisibility(ShouldBeVisible() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
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
    if (!BackendSubsystem)
    {
        return NetMode == NM_Standalone;
    }

    return BackendSubsystem->GetSessionStatus() != TEXT("Starting") && BackendSubsystem->GetSessionStatus() != TEXT("Traveling");
}

UMOBAMMOBackendSubsystem* UMOBAMMOGameHUDWidget::GetBackendSubsystem() const
{
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        return GameInstance->GetSubsystem<UMOBAMMOBackendSubsystem>();
    }

    return nullptr;
}

void UMOBAMMOGameHUDWidget::BuildLayout()
{
    if (!WidgetTree || WidgetTree->RootWidget)
    {
        return;
    }

    RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
    WidgetTree->RootWidget = RootOverlay;

    RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    RootBorder->SetBrushColor(FLinearColor(0.02f, 0.05f, 0.08f, 0.82f));
    RootBorder->SetPadding(FMargin(14.0f, 12.0f));
    RootOverlay->AddChildToOverlay(RootBorder);

    UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    RootBorder->SetContent(Layout);

    FloatingTargetFeedbackText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    FloatingTargetFeedbackText->SetVisibility(ESlateVisibility::HitTestInvisible);
    FloatingTargetFeedbackText->SetText(FText::GetEmpty());
    FloatingTargetFeedbackText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.32f, 0.34f, 0.0f)));
    FSlateFontInfo FloatingFont = FloatingTargetFeedbackText->GetFont();
    FloatingFont.Size = 18;
    FloatingFont.TypefaceFontName = TEXT("Bold");
    FloatingTargetFeedbackText->SetFont(FloatingFont);
    if (UOverlaySlot* FloatingSlot = RootOverlay->AddChildToOverlay(FloatingTargetFeedbackText))
    {
        FloatingSlot->SetHorizontalAlignment(HAlign_Left);
        FloatingSlot->SetVerticalAlignment(VAlign_Top);
        FloatingSlot->SetPadding(FMargin(24.0f, 24.0f, 0.0f, 0.0f));
    }

    StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    StatusText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    FSlateFontInfo StatusFont = StatusText->GetFont();
    StatusFont.Size = 15;
    StatusText->SetFont(StatusFont);
    Layout->AddChildToVerticalBox(StatusText);

    DetailText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    DetailText->SetColorAndOpacity(FSlateColor(FLinearColor(0.66f, 0.78f, 0.84f, 1.0f)));
    FSlateFontInfo DetailFont = DetailText->GetFont();
    DetailFont.Size = 12;
    DetailText->SetFont(DetailFont);
    if (UVerticalBoxSlot* DetailSlot = Layout->AddChildToVerticalBox(DetailText))
    {
        DetailSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 8.0f));
    }

    HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
    HealthBar->SetFillColorAndOpacity(FLinearColor(0.83f, 0.30f, 0.34f, 1.0f));
    if (UVerticalBoxSlot* HealthSlot = Layout->AddChildToVerticalBox(HealthBar))
    {
        HealthSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
    }

    ManaBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
    ManaBar->SetFillColorAndOpacity(FLinearColor(0.25f, 0.60f, 0.88f, 1.0f));
    Layout->AddChildToVerticalBox(ManaBar);

    ControlsText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    ControlsText->SetColorAndOpacity(FSlateColor(FLinearColor(0.86f, 0.82f, 0.60f, 0.92f)));
    FSlateFontInfo ControlsFont = ControlsText->GetFont();
    ControlsFont.Size = 11;
    ControlsText->SetFont(ControlsFont);
    ControlsText->SetText(FText::FromString(TEXT("1 Damage | 2 Heal | 3 Spend Mana | 4 Restore Mana | 5 Respawn | 6 Cycle Target | 7 Clear Target")));
    if (UVerticalBoxSlot* ControlsSlot = Layout->AddChildToVerticalBox(ControlsText))
    {
        ControlsSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 6.0f));
    }

    ActionHintText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    ActionHintText->SetColorAndOpacity(FSlateColor(FLinearColor(0.98f, 0.84f, 0.56f, 0.96f)));
    FSlateFontInfo HintFont = ActionHintText->GetFont();
    HintFont.Size = 12;
    HintFont.TypefaceFontName = TEXT("Bold");
    ActionHintText->SetFont(HintFont);
    ActionHintText->SetAutoWrapText(true);
    if (UVerticalBoxSlot* HintSlot = Layout->AddChildToVerticalBox(ActionHintText))
    {
        HintSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
    }

    ArcChargeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    ArcChargeText->SetColorAndOpacity(FSlateColor(FLinearColor(0.60f, 0.86f, 1.0f, 0.96f)));
    FSlateFontInfo ArcChargeFont = ArcChargeText->GetFont();
    ArcChargeFont.Size = 11;
    ArcChargeFont.TypefaceFontName = TEXT("Bold");
    ArcChargeText->SetFont(ArcChargeFont);
    ArcChargeText->SetAutoWrapText(true);
    if (UVerticalBoxSlot* ArcChargeSlot = Layout->AddChildToVerticalBox(ArcChargeText))
    {
        ArcChargeSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
    }

    CombatEventText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    CombatEventText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.86f, 0.54f, 0.0f)));
    FSlateFontInfo EventFont = CombatEventText->GetFont();
    EventFont.Size = 16;
    EventFont.TypefaceFontName = TEXT("Bold");
    CombatEventText->SetFont(EventFont);
    CombatEventText->SetAutoWrapText(true);
    CombatEventText->SetJustification(ETextJustify::Center);
    if (UVerticalBoxSlot* CombatEventSlot = Layout->AddChildToVerticalBox(CombatEventText))
    {
        CombatEventSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
    }

    UHorizontalBox* AbilityBarRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    if (UVerticalBoxSlot* AbilityBarRowSlot = Layout->AddChildToVerticalBox(AbilityBarRow))
    {
        AbilityBarRowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
    }

    const TArray<FMOBAMMOAbilityDefinition> AbilityDefinitions = {
        MOBAMMOAbilitySet::ArcBurst(),
        MOBAMMOAbilitySet::Renew(),
        MOBAMMOAbilitySet::DrainPulse(),
        MOBAMMOAbilitySet::ManaSurge(),
        MOBAMMOAbilitySet::Reforge()
    };

    AbilityCooldownBars.Reset();
    AbilityCooldownTexts.Reset();
    for (const FMOBAMMOAbilityDefinition& AbilityDefinition : AbilityDefinitions)
    {
        UVerticalBox* AbilitySlot = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
        if (UHorizontalBoxSlot* AbilitySlotSlot = AbilityBarRow->AddChildToHorizontalBox(AbilitySlot))
        {
            AbilitySlotSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
            AbilitySlotSlot->SetSize(ESlateSizeRule::Fill);
        }

        UTextBlock* AbilityText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        AbilityText->SetColorAndOpacity(FSlateColor(FLinearColor(0.93f, 0.93f, 0.96f, 0.98f)));
        FSlateFontInfo AbilitySlotFont = AbilityText->GetFont();
        AbilitySlotFont.Size = 10;
        AbilityText->SetFont(AbilitySlotFont);
        AbilityText->SetAutoWrapText(true);
        AbilitySlot->AddChildToVerticalBox(AbilityText);

        UProgressBar* AbilityBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
        AbilityBar->SetPercent(1.0f);
        AbilityBar->SetFillColorAndOpacity(FLinearColor(0.82f, 0.68f, 0.28f, 1.0f));
        if (UVerticalBoxSlot* AbilityBarSlot = AbilitySlot->AddChildToVerticalBox(AbilityBar))
        {
            AbilityBarSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
        }

        AbilityCooldownTexts.Add(AbilityText);
        AbilityCooldownBars.Add(AbilityBar);
    }

    AbilityTrayText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    AbilityTrayText->SetColorAndOpacity(FSlateColor(FLinearColor(0.93f, 0.93f, 0.96f, 0.96f)));
    FSlateFontInfo AbilityFont = AbilityTrayText->GetFont();
    AbilityFont.Size = 11;
    AbilityTrayText->SetFont(AbilityFont);
    AbilityTrayText->SetAutoWrapText(true);
    if (UVerticalBoxSlot* AbilitySlot = Layout->AddChildToVerticalBox(AbilityTrayText))
    {
        AbilitySlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
    }

    TargetPanelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    TargetPanelText->SetColorAndOpacity(FSlateColor(FLinearColor(0.90f, 0.78f, 0.55f, 1.0f)));
    FSlateFontInfo TargetFont = TargetPanelText->GetFont();
    TargetFont.Size = 11;
    TargetFont.TypefaceFontName = TEXT("Bold");
    TargetPanelText->SetFont(TargetFont);
    TargetPanelText->SetAutoWrapText(true);
    if (UVerticalBoxSlot* TargetPanelSlot = Layout->AddChildToVerticalBox(TargetPanelText))
    {
        TargetPanelSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
    }

    TargetStatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    TargetStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.77f, 0.86f, 0.90f, 0.94f)));
    FSlateFontInfo TargetStatusFont = TargetStatusText->GetFont();
    TargetStatusFont.Size = 10;
    TargetStatusText->SetFont(TargetStatusFont);
    TargetStatusText->SetAutoWrapText(true);
    if (UVerticalBoxSlot* TargetStatusSlot = Layout->AddChildToVerticalBox(TargetStatusText))
    {
        TargetStatusSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
    }

    TargetHealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
    TargetHealthBar->SetFillColorAndOpacity(FLinearColor(0.88f, 0.34f, 0.36f, 1.0f));
    if (UVerticalBoxSlot* TargetHealthSlot = Layout->AddChildToVerticalBox(TargetHealthBar))
    {
        TargetHealthSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
    }

    TargetManaBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
    TargetManaBar->SetFillColorAndOpacity(FLinearColor(0.31f, 0.60f, 0.90f, 1.0f));
    if (UVerticalBoxSlot* TargetManaSlot = Layout->AddChildToVerticalBox(TargetManaBar))
    {
        TargetManaSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
    }

    RespawnHintText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    RespawnHintText->SetColorAndOpacity(FSlateColor(FLinearColor(0.98f, 0.76f, 0.42f, 1.0f)));
    FSlateFontInfo RespawnFont = RespawnHintText->GetFont();
    RespawnFont.Size = 12;
    RespawnFont.TypefaceFontName = TEXT("Bold");
    RespawnHintText->SetFont(RespawnFont);
    RespawnHintText->SetAutoWrapText(true);
    RespawnHintText->SetVisibility(ESlateVisibility::Collapsed);
    if (UVerticalBoxSlot* RespawnSlot = Layout->AddChildToVerticalBox(RespawnHintText))
    {
        RespawnSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
    }

    CombatLogText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    CombatLogText->SetColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.90f, 0.92f, 0.95f)));
    FSlateFontInfo CombatFont = CombatLogText->GetFont();
    CombatFont.Size = 11;
    CombatLogText->SetFont(CombatFont);
    CombatLogText->SetAutoWrapText(true);
    Layout->AddChildToVerticalBox(CombatLogText);
    CombatLogText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    RosterText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    RosterText->SetColorAndOpacity(FSlateColor(FLinearColor(0.74f, 0.84f, 0.90f, 0.95f)));
    FSlateFontInfo RosterFont = RosterText->GetFont();
    RosterFont.Size = 10;
    RosterText->SetFont(RosterFont);
    RosterText->SetAutoWrapText(true);
    if (UVerticalBoxSlot* RosterSlot = Layout->AddChildToVerticalBox(RosterText))
    {
        RosterSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
    }

    DetailText->SetAutoWrapText(true);
    StatusText->SetAutoWrapText(true);
}

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
    FString TargetPanelDisplay = TEXT("Target Panel: No target selected.");
    FString TargetStatusDisplay = TEXT("Target State: No target lock.");
    FString AbilityTrayDisplay;
    FString CombatFeedDisplay = TEXT("Combat Feed: No recent events.");
    FString RosterDisplay = TEXT("Combat Roster:\n- No active combatants.");
    FString ActionHintDisplay = TEXT("No target locked. Press 6 to cycle an enemy.");
    FString ArcChargeDisplay = TEXT("Arc Charge: Dormant");
    float ServerWorldTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    FString SelectedTargetCharacterId;
    FString FloatingFeedbackText;
    bool bFloatingFeedbackHealing = false;
    float FloatingFeedbackAlpha = 0.0f;
    FVector2D FloatingFeedbackScreenPosition(24.0f, 24.0f);
    float TargetHealth = 0.0f;
    float TargetMaxHealth = 0.0f;
    float TargetMana = 0.0f;
    float TargetMaxMana = 0.0f;
    bool bHasTarget = false;
    FString TargetRangeText = TEXT("Range unknown");

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
            CharacterName = PlayerState->GetCharacterName().IsEmpty() ? TEXT("-") : PlayerState->GetCharacterName();
            ClassId = PlayerState->GetClassId().IsEmpty() ? TEXT("-") : PlayerState->GetClassId();
            CharacterLevel = PlayerState->GetCharacterLevel();
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
            LifeState = PlayerState->IsAlive() ? TEXT("Alive") : TEXT("Dead");
            SelectedTargetCharacterId = PlayerState->GetSelectedTargetCharacterId();
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
                CombatFeedDisplay = TEXT("Combat Feed:");
                for (const FString& CombatEntry : CombatFeed)
                {
                    CombatFeedDisplay += FString::Printf(TEXT("\n- %s"), *CombatEntry);
                }
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

                    TargetPanelDisplay = FString::Printf(
                        TEXT("Target Panel: %s [%s] Lv.%d | HP %.0f/%.0f | MP %.0f/%.0f | %s"),
                        *TargetPlayerState->GetCharacterName(),
                        *TargetPlayerState->GetClassId(),
                        TargetPlayerState->GetCharacterLevel(),
                        TargetPlayerState->GetCurrentHealth(),
                        TargetPlayerState->GetMaxHealth(),
                        TargetPlayerState->GetCurrentMana(),
                        TargetPlayerState->GetMaxMana(),
                        TargetPlayerState->IsAlive() ? TEXT("Alive") : TEXT("Down")
                    );
                    TargetStatusDisplay = FString::Printf(
                        TEXT("Target State: K/D %d/%d | %s"),
                        TargetPlayerState->GetKills(),
                        TargetPlayerState->GetDeaths(),
                        TargetPlayerState->IsAlive() ? TEXT("Ready to engage") : TEXT("Target down")
                    );
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
                            TargetRangeText = FString::Printf(
                                TEXT("%s | %.0f units"),
                                DistanceUnits <= 1800.0f ? TEXT("IN RANGE") : TEXT("OUT OF RANGE"),
                                DistanceUnits
                            );
                        }
                    }

                    const float FeedbackRemaining = FMath::Max(0.0f, TargetPlayerState->GetIncomingCombatFeedbackEndServerTime() - ServerWorldTimeSeconds);
                    if (FeedbackRemaining > KINDA_SMALL_NUMBER)
                    {
                        FloatingFeedbackText = TargetPlayerState->GetIncomingCombatFeedbackText();
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
            }

            if (!GameState->PlayerArray.IsEmpty())
            {
                TArray<const AMOBAMMOPlayerState*> Combatants;
                for (APlayerState* IteratedPlayerState : GameState->PlayerArray)
                {
                    if (const AMOBAMMOPlayerState* Candidate = Cast<AMOBAMMOPlayerState>(IteratedPlayerState))
                    {
                        Combatants.Add(Candidate);
                    }
                }

                Combatants.Sort([](const AMOBAMMOPlayerState& Left, const AMOBAMMOPlayerState& Right)
                {
                    if (Left.GetKills() == Right.GetKills())
                    {
                        return Left.GetCharacterName() < Right.GetCharacterName();
                    }
                    return Left.GetKills() > Right.GetKills();
                });

                RosterDisplay = TEXT("Combat Roster:");
                const int32 MaxEntries = FMath::Min(Combatants.Num(), 4);
                for (int32 Index = 0; Index < MaxEntries; ++Index)
                {
                    const AMOBAMMOPlayerState* Combatant = Combatants[Index];
                    RosterDisplay += FString::Printf(
                        TEXT("\n%d. %s [%s]  K/D %d/%d  HP %.0f"),
                        Index + 1,
                        *Combatant->GetCharacterName(),
                        *Combatant->GetClassId(),
                        Combatant->GetKills(),
                        Combatant->GetDeaths(),
                        Combatant->GetCurrentHealth()
                    );
                }
            }
        }
    }

    if (StatusText)
    {
        StatusText->SetText(FText::FromString(FString::Printf(
            TEXT("%s [%s] | Level %d | %s"),
            *CharacterName,
            *ClassId,
            CharacterLevel,
            *LifeState
        )));
    }

    if (DetailText)
    {
        DetailText->SetText(FText::FromString(FString::Printf(
            TEXT("HP %.0f/%.0f | MP %.0f/%.0f | K/D %d/%d | Players %d | %s"),
            Health,
            MaxHealth,
            Mana,
            MaxMana,
            KillCount,
            DeathCount,
            ConnectedPlayers,
            *TargetDisplayText
        )));
    }

    if (HealthBar)
    {
        HealthBar->SetPercent(MaxHealth > 0.0f ? Health / MaxHealth : 0.0f);
    }

    if (ManaBar)
    {
        ManaBar->SetPercent(MaxMana > 0.0f ? Mana / MaxMana : 0.0f);
    }

    if (CombatEventText)
    {
        const float EventAlpha = CombatEventHighlightRemaining > 0.0f
            ? FMath::Clamp(CombatEventHighlightRemaining / 2.25f, 0.0f, 1.0f)
            : 0.0f;
        CombatEventText->SetText(FText::FromString(EventAlpha > 0.0f ? LastCombatLog : TEXT("")));
        CombatEventText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.86f, 0.54f, EventAlpha)));
    }

    const FMOBAMMOAbilityDefinition ArcBurst = MOBAMMOAbilitySet::ArcBurst();
    const FMOBAMMOAbilityDefinition Renew = MOBAMMOAbilitySet::Renew();
    const FMOBAMMOAbilityDefinition DrainPulse = MOBAMMOAbilitySet::DrainPulse();
    const FMOBAMMOAbilityDefinition ManaSurge = MOBAMMOAbilitySet::ManaSurge();
    const FMOBAMMOAbilityDefinition Reforge = MOBAMMOAbilitySet::Reforge();
    const bool bTargetInRange = TargetRangeText.StartsWith(TEXT("IN RANGE"));

    if (LifeState == TEXT("Dead"))
    {
        ActionHintDisplay = RespawnCooldownRemaining > KINDA_SMALL_NUMBER
            ? FString::Printf(TEXT("Reforge is charging. Return in %.1fs."), RespawnCooldownRemaining)
            : TEXT("Reforge is ready. Press 5 to return to the fight.");
    }
    else if (!bHasTarget)
    {
        ActionHintDisplay = TEXT("No target locked. Press 6 to cycle an enemy.");
    }
    else if (!bTargetInRange)
    {
        ActionHintDisplay = FString::Printf(TEXT("%s is out of range. Step in or switch targets."), *TargetDisplayText);
    }
    else if (DamageCooldownRemaining > KINDA_SMALL_NUMBER)
    {
        ActionHintDisplay = FString::Printf(TEXT("Arc Burst recovers in %.1fs. Renew or Drain Pulse are safer now."), DamageCooldownRemaining);
    }
    else if (Mana < ArcBurst.ManaCost)
    {
        ActionHintDisplay = TEXT("Low mana. Use Mana Surge or Drain Pulse before bursting.");
    }
    else
    {
        ActionHintDisplay = FString::Printf(TEXT("%s is exposed. Arc Burst is primed."), *TargetDisplayText);
    }

    if (ArcChargeRemaining > KINDA_SMALL_NUMBER)
    {
        ArcChargeDisplay = FString::Printf(
            TEXT("Arc Charge: Empowered for %.1fs | Arc Burst gains bonus damage and reduced mana cost."),
            ArcChargeRemaining
        );
    }

    AbilityTrayDisplay = FString::Printf(
        TEXT("[%s] %s %.1fs | [%s] %s %.1fs\n[%s] %s %.1fs | [%s] %s %.1fs | [%s] %s"),
        *ArcBurst.KeyLabel,
        *ArcBurst.Name,
        DamageCooldownRemaining,
        *Renew.KeyLabel,
        *Renew.Name,
        HealCooldownRemaining,
        *DrainPulse.KeyLabel,
        *DrainPulse.Name,
        DrainCooldownRemaining,
        *ManaSurge.KeyLabel,
        *ManaSurge.Name,
        ManaSurgeCooldownRemaining,
        *Reforge.KeyLabel,
        *Reforge.Name
    );

    if (AbilityTrayText)
    {
        AbilityTrayText->SetText(FText::FromString(AbilityTrayDisplay));
    }

    if (CombatLogText)
    {
        CombatLogText->SetText(FText::FromString(FString::Printf(
            TEXT("Cooldowns: Arc Burst %.1fs | Renew %.1fs | Drain Pulse %.1fs | Mana Surge %.1fs\nLatest: %s\n%s"),
            DamageCooldownRemaining,
            HealCooldownRemaining,
            DrainCooldownRemaining,
            ManaSurgeCooldownRemaining,
            *LastCombatLog,
            *CombatFeedDisplay
        )));
    }

    if (ActionHintText)
    {
        ActionHintText->SetText(FText::FromString(ActionHintDisplay));
    }

    if (ArcChargeText)
    {
        ArcChargeText->SetText(FText::FromString(ArcChargeDisplay));
    }

    if (TargetPanelText)
    {
        TargetPanelText->SetText(FText::FromString(TargetPanelDisplay));
    }

    if (TargetStatusText)
    {
        TargetStatusText->SetText(FText::FromString(FString::Printf(TEXT("%s | %s"), *TargetStatusDisplay, *TargetRangeText)));
    }

    if (FloatingTargetFeedbackText)
    {
        const bool bShowFloatingFeedback = !FloatingFeedbackText.IsEmpty() && FloatingFeedbackAlpha > 0.0f;
        FloatingTargetFeedbackText->SetText(FText::FromString(bShowFloatingFeedback ? FloatingFeedbackText : TEXT("")));
        FloatingTargetFeedbackText->SetColorAndOpacity(FSlateColor(
            bFloatingFeedbackHealing
                ? FLinearColor(0.34f, 0.96f, 0.62f, FloatingFeedbackAlpha)
                : FLinearColor(0.98f, 0.34f, 0.38f, FloatingFeedbackAlpha)
        ));
        FloatingTargetFeedbackText->SetRenderTranslation(FloatingFeedbackScreenPosition);
    }

    if (TargetHealthBar)
    {
        TargetHealthBar->SetPercent(bHasTarget && TargetMaxHealth > 0.0f ? TargetHealth / TargetMaxHealth : 0.0f);
        TargetHealthBar->SetVisibility(bHasTarget ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }

    if (TargetManaBar)
    {
        TargetManaBar->SetPercent(bHasTarget && TargetMaxMana > 0.0f ? TargetMana / TargetMaxMana : 0.0f);
        TargetManaBar->SetVisibility(bHasTarget ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }

    const TArray<FMOBAMMOAbilityDefinition> AbilityDefinitions = {
        ArcBurst,
        Renew,
        DrainPulse,
        ManaSurge,
        Reforge
    };

    for (int32 AbilityIndex = 0; AbilityIndex < AbilityDefinitions.Num(); ++AbilityIndex)
    {
        const FMOBAMMOAbilityDefinition& AbilityDefinition = AbilityDefinitions[AbilityIndex];
        const bool bHasCooldown = AbilityDefinition.Cooldown > KINDA_SMALL_NUMBER;
        float CooldownRemaining = 0.0f;
        if (AbilityDefinition.Name == ArcBurst.Name)
        {
            CooldownRemaining = DamageCooldownRemaining;
        }
        else if (AbilityDefinition.Name == Renew.Name)
        {
            CooldownRemaining = HealCooldownRemaining;
        }
        else if (AbilityDefinition.Name == DrainPulse.Name)
        {
            CooldownRemaining = DrainCooldownRemaining;
        }
        else if (AbilityDefinition.Name == ManaSurge.Name)
        {
            CooldownRemaining = ManaSurgeCooldownRemaining;
        }

        if (AbilityCooldownTexts.IsValidIndex(AbilityIndex) && AbilityCooldownTexts[AbilityIndex])
        {
            FString StateLabel = TEXT("READY");
            if (LifeState == TEXT("Dead") && AbilityDefinition.Kind != EMOBAMMOAbilityKind::Respawn)
            {
                StateLabel = TEXT("DOWN");
            }
            else if (AbilityDefinition.Kind == EMOBAMMOAbilityKind::Respawn && LifeState != TEXT("Dead"))
            {
                StateLabel = TEXT("LOCKED");
            }
            else if (AbilityDefinition.Name == DrainPulse.Name && !bHasTarget)
            {
                StateLabel = TEXT("NO TARGET");
            }
            else if (AbilityDefinition.Name == DrainPulse.Name && !TargetRangeText.StartsWith(TEXT("IN RANGE")))
            {
                StateLabel = TEXT("OUT OF RANGE");
            }
            else if (AbilityDefinition.Name == ArcBurst.Name && ArcChargeRemaining > KINDA_SMALL_NUMBER)
            {
                StateLabel = FString::Printf(TEXT("CHARGED %.1fs"), ArcChargeRemaining);
            }
            else if (bHasCooldown && CooldownRemaining > KINDA_SMALL_NUMBER)
            {
                StateLabel = FString::Printf(TEXT("CD %.1fs"), CooldownRemaining);
            }
            else if (AbilityDefinition.ManaCost > 0.0f && Mana < AbilityDefinition.ManaCost)
            {
                StateLabel = TEXT("NO MANA");
            }

            const FString CooldownLabel = FString::Printf(
                TEXT("[%s] %s\n%s"),
                *AbilityDefinition.KeyLabel,
                *AbilityDefinition.Name,
                *StateLabel
            );
            AbilityCooldownTexts[AbilityIndex]->SetText(FText::FromString(CooldownLabel));
        }

        if (AbilityCooldownBars.IsValidIndex(AbilityIndex) && AbilityCooldownBars[AbilityIndex])
        {
            float Percent = 1.0f;
            if (bHasCooldown)
            {
                Percent = AbilityDefinition.Cooldown > 0.0f
                    ? FMath::Clamp(1.0f - (CooldownRemaining / AbilityDefinition.Cooldown), 0.0f, 1.0f)
                    : 1.0f;
            }

            AbilityCooldownBars[AbilityIndex]->SetPercent(Percent);
            FLinearColor FillColor = FLinearColor(0.30f, 0.64f, 0.88f, 1.0f);
            if (LifeState == TEXT("Dead") && AbilityDefinition.Kind != EMOBAMMOAbilityKind::Respawn)
            {
                FillColor = FLinearColor(0.32f, 0.18f, 0.18f, 1.0f);
                Percent = 0.05f;
                AbilityCooldownBars[AbilityIndex]->SetPercent(Percent);
            }
            else if (AbilityDefinition.Kind == EMOBAMMOAbilityKind::Respawn)
            {
                FillColor = LifeState == TEXT("Dead")
                    ? FLinearColor(0.28f, 0.78f, 0.52f, 1.0f)
                    : FLinearColor(0.24f, 0.24f, 0.28f, 1.0f);
                const float RespawnPercent = RespawnBarReferenceDuration > 0.0f
                    ? FMath::Clamp(1.0f - (RespawnCooldownRemaining / RespawnBarReferenceDuration), 0.0f, 1.0f)
                    : 1.0f;
                AbilityCooldownBars[AbilityIndex]->SetPercent(LifeState == TEXT("Dead") ? RespawnPercent : 0.15f);
            }
            else if (bHasCooldown && CooldownRemaining > KINDA_SMALL_NUMBER)
            {
                FillColor = FLinearColor(0.82f, 0.68f, 0.28f, 1.0f);
            }
            else if (AbilityDefinition.Name == ArcBurst.Name && ArcChargeRemaining > KINDA_SMALL_NUMBER)
            {
                FillColor = FLinearColor(0.44f, 0.82f, 1.0f, 1.0f);
            }
            else if (AbilityDefinition.Name == DrainPulse.Name && (!bHasTarget || !TargetRangeText.StartsWith(TEXT("IN RANGE"))))
            {
                FillColor = FLinearColor(0.52f, 0.26f, 0.22f, 1.0f);
            }
            else if (AbilityDefinition.ManaCost > 0.0f && Mana < AbilityDefinition.ManaCost)
            {
                FillColor = FLinearColor(0.58f, 0.24f, 0.24f, 1.0f);
            }
            else if (AbilityDefinition.Kind == EMOBAMMOAbilityKind::Damage)
            {
                FillColor = FLinearColor(0.86f, 0.34f, 0.36f, 1.0f);
            }
            else if (AbilityDefinition.Kind == EMOBAMMOAbilityKind::Heal)
            {
                FillColor = FLinearColor(0.30f, 0.78f, 0.52f, 1.0f);
            }

            AbilityCooldownBars[AbilityIndex]->SetFillColorAndOpacity(FillColor);
        }
    }

    if (RespawnHintText)
    {
        const bool bShowRespawnHint = LifeState == TEXT("Dead");
        RespawnHintText->SetVisibility(bShowRespawnHint ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        RespawnHintText->SetText(FText::FromString(
            RespawnCooldownRemaining > KINDA_SMALL_NUMBER
                ? FString::Printf(TEXT("You are down. Reforge unlocks in %.1fs."), RespawnCooldownRemaining)
                : TEXT("You are down. Press 5 to Reforge and return to the fight.")
        ));
    }

    if (RosterText)
    {
        RosterText->SetText(FText::FromString(RosterDisplay));
    }
}

void UMOBAMMOGameHUDWidget::HandleBackendStateChanged()
{
    RefreshFromBackend();
}

void UMOBAMMOGameHUDWidget::HandleReplicatedStateChanged()
{
    RefreshFromBackend();
}
