#include "MOBAMMOLoadingScreenWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "MOBAMMOBackendSubsystem.h"

namespace
{
    UTextBlock* MakeLoadingText(UWidgetTree* WidgetTree, UVerticalBox* Parent, const FString& Text, int32 FontSize = 20, const FLinearColor& Color = FLinearColor::White)
    {
        UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Label->SetText(FText::FromString(Text));
        Label->SetColorAndOpacity(FSlateColor(Color));
        Label->SetAutoWrapText(true);

        FSlateFontInfo FontInfo = Label->GetFont();
        FontInfo.Size = FontSize;
        Label->SetFont(FontInfo);

        if (UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Label))
        {
            Slot->SetPadding(FMargin(0.0f, 6.0f));
        }

        return Label;
    }
}

void UMOBAMMOLoadingScreenWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    BuildLayout();

    if (UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
    {
        BindToSubsystem(BackendSubsystem);
    }
}

void UMOBAMMOLoadingScreenWidget::NativeConstruct()
{
    Super::NativeConstruct();
    RefreshFromBackend();
}

void UMOBAMMOLoadingScreenWidget::RefreshFromBackend()
{
    UpdateTexts();
    SetVisibility(ShouldBeVisible() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

bool UMOBAMMOLoadingScreenWidget::ShouldBeVisible() const
{
    const UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem();
    if (!BackendSubsystem)
    {
        return false;
    }

    const FString SessionStatus = BackendSubsystem->GetSessionStatus();
    const UWorld* World = GetWorld();
    const ENetMode NetMode = World ? World->GetNetMode() : NM_Standalone;
    if (NetMode == NM_Client)
    {
        const APlayerController* PlayerController = GetOwningPlayer();
        if (PlayerController && PlayerController->GetPawn())
        {
            return false;
        }
    }

    return SessionStatus == TEXT("Starting") || SessionStatus == TEXT("Traveling");
}

UMOBAMMOBackendSubsystem* UMOBAMMOLoadingScreenWidget::GetBackendSubsystem() const
{
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        return GameInstance->GetSubsystem<UMOBAMMOBackendSubsystem>();
    }

    return nullptr;
}

void UMOBAMMOLoadingScreenWidget::BuildLayout()
{
    if (!WidgetTree || WidgetTree->RootWidget)
    {
        return;
    }

    UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
    WidgetTree->RootWidget = RootOverlay;

    UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Background->SetBrushColor(FLinearColor(0.01f, 0.02f, 0.05f, 0.94f));
    if (UOverlaySlot* BackgroundSlot = RootOverlay->AddChildToOverlay(Background))
    {
        BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
        BackgroundSlot->SetVerticalAlignment(VAlign_Fill);
    }

    USizeBox* PanelSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    PanelSizeBox->SetWidthOverride(600.0f);
    if (UOverlaySlot* PanelSlot = RootOverlay->AddChildToOverlay(PanelSizeBox))
    {
        PanelSlot->SetHorizontalAlignment(HAlign_Center);
        PanelSlot->SetVerticalAlignment(VAlign_Center);
        PanelSlot->SetPadding(FMargin(24.0f));
    }

    RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    RootBorder->SetBrushColor(FLinearColor(0.04f, 0.08f, 0.12f, 0.92f));
    RootBorder->SetPadding(FMargin(36.0f, 32.0f));
    PanelSizeBox->SetContent(RootBorder);

    UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    RootBorder->SetContent(Layout);

    MakeLoadingText(WidgetTree, Layout, TEXT("TRANSFER STATE"), 13, FLinearColor(0.44f, 0.82f, 0.73f, 1.0f));
    TitleText = MakeLoadingText(WidgetTree, Layout, TEXT("Preparing Session"), 34, FLinearColor(0.96f, 0.98f, 1.0f, 1.0f));
    StatusText = MakeLoadingText(WidgetTree, Layout, TEXT("Please wait..."), 18, FLinearColor(0.80f, 0.86f, 0.92f, 1.0f));
    HintText = MakeLoadingText(WidgetTree, Layout, TEXT("Backend session and dedicated server handoff are in progress."), 14, FLinearColor(0.60f, 0.69f, 0.76f, 1.0f));
}

void UMOBAMMOLoadingScreenWidget::BindToSubsystem(UMOBAMMOBackendSubsystem* BackendSubsystem)
{
    if (!BackendSubsystem || bBoundToSubsystem)
    {
        return;
    }

    BackendSubsystem->OnDebugStateChanged.AddDynamic(this, &UMOBAMMOLoadingScreenWidget::HandleBackendStateChanged);
    bBoundToSubsystem = true;
}

void UMOBAMMOLoadingScreenWidget::UpdateTexts()
{
    const UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem();
    const FString SessionStatus = BackendSubsystem ? BackendSubsystem->GetSessionStatus() : TEXT("Idle");

    FString DisplayText = TEXT("Please wait...");
    if (SessionStatus == TEXT("Starting"))
    {
        DisplayText = TEXT("Starting session and preparing your server slot...");
    }
    else if (SessionStatus == TEXT("Traveling"))
    {
        DisplayText = TEXT("Traveling to dedicated server...");
    }

    if (StatusText)
    {
        StatusText->SetText(FText::FromString(DisplayText));
    }

    if (HintText)
    {
        HintText->SetText(FText::FromString(FString::Printf(TEXT("Phase: %s"), *SessionStatus)));
    }
}

void UMOBAMMOLoadingScreenWidget::HandleBackendStateChanged()
{
    RefreshFromBackend();
}
