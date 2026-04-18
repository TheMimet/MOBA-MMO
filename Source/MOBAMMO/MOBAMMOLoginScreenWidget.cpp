#include "MOBAMMOLoginScreenWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

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
	const UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem();
	if (BackendSubsystem && ErrorText)
	{
	    const FString ErrorMessage = BackendSubsystem->GetLastErrorMessage();
	    if (!ErrorMessage.IsEmpty())
	    {
	        ErrorText->SetText(FText::FromString(ErrorMessage));
	        ErrorText->SetVisibility(ESlateVisibility::Visible);
	    }
	    else
	    {
	        ErrorText->SetVisibility(ESlateVisibility::Collapsed);
	    }
	}

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

	UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = RootOverlay;

	USizeBox* PanelSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PanelSizeBox->SetWidthOverride(400.0f);
	if (UOverlaySlot* PanelSlot = RootOverlay->AddChildToOverlay(PanelSizeBox))
	{
		PanelSlot->SetHorizontalAlignment(HAlign_Center);
		PanelSlot->SetVerticalAlignment(VAlign_Center);
	}

	RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	RootBorder->SetBrushColor(FLinearColor(0.02f, 0.04f, 0.08f, 0.85f));
	RootBorder->SetPadding(FMargin(24.0f));
	PanelSizeBox->SetContent(RootBorder);

	UVerticalBox* VerticalBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	RootBorder->SetContent(VerticalBox);

	UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TitleText->SetText(FText::FromString(TEXT("MOBA MMO LOGIN")));
	FSlateFontInfo FontInfo = TitleText->GetFont();
	FontInfo.Size = 24;
	TitleText->SetFont(FontInfo);
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.9f, 1.0f)));
	if (UVerticalBoxSlot* TitleSlot = VerticalBox->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 20.0f));
	}

	UsernameInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	UsernameInput->SetHintText(FText::FromString(TEXT("Username (e.g. hero123)")));
	UsernameInput->SetText(FText::FromString(DefaultUsername));
	if (UVerticalBoxSlot* InputSlot = VerticalBox->AddChildToVerticalBox(UsernameInput))
	{
		InputSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
	}

	LoginButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	LoginButton->SetBackgroundColor(FLinearColor(0.2f, 0.6f, 1.0f, 1.0f));
	UTextBlock* ButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ButtonText->SetText(FText::FromString(TEXT("CONNECT")));
	ButtonText->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
	LoginButton->AddChild(ButtonText);
	LoginButton->OnClicked.AddDynamic(this, &UMOBAMMOLoginScreenWidget::HandleLoginButtonClicked);

	if (UVerticalBoxSlot* BtnSlot = VerticalBox->AddChildToVerticalBox(LoginButton))
	{
		BtnSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	}

	ErrorText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ErrorText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.3f, 0.3f)));
	ErrorText->SetVisibility(ESlateVisibility::Collapsed);
	ErrorText->SetAutoWrapText(true);
	if (UVerticalBoxSlot* ErrSlot = VerticalBox->AddChildToVerticalBox(ErrorText))
	{
		ErrSlot->SetHorizontalAlignment(HAlign_Center);
	}
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

void UMOBAMMOLoginScreenWidget::HandleLoginButtonClicked()
{
	if (UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
	{
		if (UsernameInput)
		{
			BackendSubsystem->MockLogin(UsernameInput->GetText().ToString());
		}
	}
}
