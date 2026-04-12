#include "MOBAMMOLoadingScreenWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
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

    RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    RootBorder->SetBrushColor(FLinearColor(0.01f, 0.02f, 0.05f, 0.92f));
    RootBorder->SetPadding(FMargin(40.0f));
    WidgetTree->RootWidget = RootBorder;

    UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    RootBorder->SetContent(Layout);

    TitleText = MakeLoadingText(WidgetTree, Layout, TEXT("Preparing Session"), 30, FLinearColor(0.92f, 0.96f, 1.0f, 1.0f));
    StatusText = MakeLoadingText(WidgetTree, Layout, TEXT("Please wait..."), 18, FLinearColor(0.78f, 0.85f, 0.92f, 1.0f));
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
}

void UMOBAMMOLoadingScreenWidget::HandleBackendStateChanged()
{
    RefreshFromBackend();
}
