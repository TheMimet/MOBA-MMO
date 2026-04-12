#include "MOBAMMOLoginScreenWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "MOBAMMOBackendSubsystem.h"

namespace
{
    UTextBlock* MakeLoginText(UWidgetTree* WidgetTree, UVerticalBox* Parent, const FString& Text, int32 FontSize = 20, const FLinearColor& Color = FLinearColor::White)
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
            Slot->SetPadding(FMargin(0.0f, 5.0f));
        }

        return Label;
    }

    UButton* MakeLoginButton(UWidgetTree* WidgetTree, UVerticalBox* Parent, const FString& Text)
    {
        UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
        UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Label->SetText(FText::FromString(Text));
        Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        Button->AddChild(Label);

        if (UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Button))
        {
            Slot->SetPadding(FMargin(0.0f, 10.0f));
        }

        return Button;
    }
}

void UMOBAMMOLoginScreenWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    BuildLayout();

    if (UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
    {
        BindToSubsystem(BackendSubsystem);
    }
}

void UMOBAMMOLoginScreenWidget::NativeConstruct()
{
    Super::NativeConstruct();
    RefreshFromBackend();
}

void UMOBAMMOLoginScreenWidget::RefreshFromBackend()
{
    UpdateTexts();
    SetVisibility(ShouldBeVisible() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

bool UMOBAMMOLoginScreenWidget::ShouldBeVisible() const
{
    const UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem();
    if (!BackendSubsystem)
    {
        return true;
    }

    const UWorld* World = GetWorld();
    const ENetMode NetMode = World ? World->GetNetMode() : NM_Standalone;
    if (NetMode == NM_Client)
    {
        return false;
    }

    if (BackendSubsystem->IsWaitingForCharacterSelection())
    {
        return false;
    }

    if (BackendSubsystem->GetSessionStatus() == TEXT("Starting") || BackendSubsystem->GetSessionStatus() == TEXT("Traveling"))
    {
        return false;
    }

    const FString LoginStatus = BackendSubsystem->GetLoginStatus();
    return LoginStatus.IsEmpty() || LoginStatus == TEXT("Idle") || LoginStatus == TEXT("LoggingIn") || LoginStatus == TEXT("Failed");
}

UMOBAMMOBackendSubsystem* UMOBAMMOLoginScreenWidget::GetBackendSubsystem() const
{
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        return GameInstance->GetSubsystem<UMOBAMMOBackendSubsystem>();
    }

    return nullptr;
}

void UMOBAMMOLoginScreenWidget::BuildLayout()
{
    if (!WidgetTree || WidgetTree->RootWidget)
    {
        return;
    }

    RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    RootBorder->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.06f, 0.94f));
    RootBorder->SetPadding(FMargin(40.0f));
    WidgetTree->RootWidget = RootBorder;

    UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    RootBorder->SetContent(Layout);

    TitleText = MakeLoginText(WidgetTree, Layout, TEXT("MOBA MMO"), 34, FLinearColor(0.90f, 0.96f, 1.0f, 1.0f));
    SubtitleText = MakeLoginText(WidgetTree, Layout, TEXT("Sign in to continue to character selection."), 18, FLinearColor(0.76f, 0.84f, 0.92f, 1.0f));
    StatusText = MakeLoginText(WidgetTree, Layout, TEXT("Status: Idle"), 16, FLinearColor(0.94f, 0.92f, 0.70f, 1.0f));
    ErrorText = MakeLoginText(WidgetTree, Layout, TEXT("Error: -"), 15, FLinearColor(1.0f, 0.65f, 0.65f, 1.0f));

    LoginButton = MakeLoginButton(WidgetTree, Layout, TEXT("Continue With Local Test Account"));
    LoginButton->OnClicked.AddDynamic(this, &UMOBAMMOLoginScreenWidget::HandleLoginClicked);
}

void UMOBAMMOLoginScreenWidget::BindToSubsystem(UMOBAMMOBackendSubsystem* BackendSubsystem)
{
    if (!BackendSubsystem || bBoundToSubsystem)
    {
        return;
    }

    BackendSubsystem->OnDebugStateChanged.AddDynamic(this, &UMOBAMMOLoginScreenWidget::HandleBackendStateChanged);
    BackendSubsystem->OnLoginSucceeded.AddDynamic(this, &UMOBAMMOLoginScreenWidget::HandleLoginSucceeded);
    BackendSubsystem->OnLoginFailed.AddDynamic(this, &UMOBAMMOLoginScreenWidget::HandleLoginFailed);
    bBoundToSubsystem = true;
}

void UMOBAMMOLoginScreenWidget::UpdateTexts()
{
    const UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem();
    const FString LoginStatus = BackendSubsystem ? BackendSubsystem->GetLoginStatus() : TEXT("Unavailable");
    const FString ErrorMessage = BackendSubsystem ? BackendSubsystem->GetLastErrorMessage() : FString();

    if (StatusText)
    {
        StatusText->SetText(FText::FromString(FString::Printf(TEXT("Status: %s"), *LoginStatus)));
    }

    if (ErrorText)
    {
        ErrorText->SetText(FText::FromString(FString::Printf(TEXT("Error: %s"), ErrorMessage.IsEmpty() ? TEXT("-") : *ErrorMessage)));
    }

    if (LoginButton)
    {
        LoginButton->SetIsEnabled(LoginStatus != TEXT("LoggingIn"));
    }
}

void UMOBAMMOLoginScreenWidget::HandleLoginClicked()
{
    if (UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
    {
        BackendSubsystem->MockLogin(DefaultUsername);
    }
}

void UMOBAMMOLoginScreenWidget::HandleBackendStateChanged()
{
    RefreshFromBackend();
}

void UMOBAMMOLoginScreenWidget::HandleLoginSucceeded(const FBackendLoginResult& Result)
{
    RefreshFromBackend();
}

void UMOBAMMOLoginScreenWidget::HandleLoginFailed(const FString& ErrorMessage)
{
    RefreshFromBackend();
}
