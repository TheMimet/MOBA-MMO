#include "MOBAMMOGameHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "MOBAMMOBackendSubsystem.h"
#include "MOBAMMOGameState.h"
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
    RefreshFromBackend();
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
    if (NetMode != NM_Client)
    {
        return false;
    }

    const UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem();
    return BackendSubsystem && BackendSubsystem->GetSessionStatus() != TEXT("Starting") && BackendSubsystem->GetSessionStatus() != TEXT("Traveling");
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

    RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    RootBorder->SetBrushColor(FLinearColor(0.01f, 0.04f, 0.07f, 0.78f));
    RootBorder->SetPadding(FMargin(10.0f));
    WidgetTree->RootWidget = RootBorder;

    StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    StatusText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    RootBorder->SetContent(StatusText);
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

void UMOBAMMOGameHUDWidget::UpdateTexts()
{
    FString CharacterName = TEXT("-");
    FString ClassId = TEXT("-");
    int32 CharacterLevel = 1;
    float Health = 0.0f;
    float MaxHealth = 0.0f;
    float Mana = 0.0f;
    float MaxMana = 0.0f;
    int32 ConnectedPlayers = 0;

    if (const APlayerController* PlayerController = GetOwningPlayer())
    {
        if (const AMOBAMMOPlayerState* PlayerState = PlayerController->GetPlayerState<AMOBAMMOPlayerState>())
        {
            CharacterName = PlayerState->GetCharacterName().IsEmpty() ? TEXT("-") : PlayerState->GetCharacterName();
            ClassId = PlayerState->GetClassId().IsEmpty() ? TEXT("-") : PlayerState->GetClassId();
            CharacterLevel = PlayerState->GetCharacterLevel();
            Health = PlayerState->GetCurrentHealth();
            MaxHealth = PlayerState->GetMaxHealth();
            Mana = PlayerState->GetCurrentMana();
            MaxMana = PlayerState->GetMaxMana();
        }
    }

    if (const UWorld* World = GetWorld())
    {
        if (const AMOBAMMOGameState* GameState = World->GetGameState<AMOBAMMOGameState>())
        {
            ConnectedPlayers = GameState->GetConnectedPlayers();
        }
    }

    if (StatusText)
    {
        StatusText->SetText(FText::FromString(FString::Printf(
            TEXT("Connected | %s [%s] Lv.%d | HP %.0f/%.0f | MP %.0f/%.0f | Players %d"),
            *CharacterName,
            *ClassId,
            CharacterLevel,
            Health,
            MaxHealth,
            Mana,
            MaxMana,
            ConnectedPlayers
        )));
    }
}

void UMOBAMMOGameHUDWidget::HandleBackendStateChanged()
{
    RefreshFromBackend();
}
