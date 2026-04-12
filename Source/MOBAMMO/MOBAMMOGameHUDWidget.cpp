#include "MOBAMMOGameHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "MOBAMMOBackendSubsystem.h"

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
    const UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem();
    if (!BackendSubsystem)
    {
        return false;
    }

    const UWorld* World = GetWorld();
    const ENetMode NetMode = World ? World->GetNetMode() : NM_Standalone;
    if (NetMode != NM_Client)
    {
        return false;
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
    const UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem();
    const FString CharacterId = BackendSubsystem ? BackendSubsystem->GetLastCharacterId() : TEXT("-");
    const FString SessionStatus = BackendSubsystem ? BackendSubsystem->GetSessionStatus() : TEXT("-");
    const FString ConnectString = BackendSubsystem ? BackendSubsystem->GetLastSessionConnectString() : TEXT("-");

    if (StatusText)
    {
        StatusText->SetText(FText::FromString(FString::Printf(TEXT("Connected | Character: %s | Session: %s | Server: %s"), *CharacterId, *SessionStatus, *ConnectString)));
    }
}

void UMOBAMMOGameHUDWidget::HandleBackendStateChanged()
{
    RefreshFromBackend();
}
