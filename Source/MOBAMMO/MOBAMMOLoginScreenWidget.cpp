#include "MOBAMMOLoginScreenWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "WebBrowser.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// ─────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────
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
	const bool bVisible = ShouldBeVisible();
	SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bVisible)
	{
		LoadWebUIIfNeeded();
	}
	PushStateToWebUI();
	UpdateNativeFallback();
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

// ─────────────────────────────────────────────────────────────
// Layout
// ─────────────────────────────────────────────────────────────
void UMOBAMMOLoginScreenWidget::BuildLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = RootOverlay;

	UBorder* LoadingBackdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	LoadingBackdrop->SetBrushColor(FLinearColor(0.005f, 0.006f, 0.010f, 1.0f));
	if (UOverlaySlot* BackdropSlot = RootOverlay->AddChildToOverlay(LoadingBackdrop))
	{
		BackdropSlot->SetHorizontalAlignment(HAlign_Fill);
		BackdropSlot->SetVerticalAlignment(VAlign_Fill);
	}

	WebBrowserWidget = WidgetTree->ConstructWidget<UWebBrowser>(UWebBrowser::StaticClass());

	WebBrowserWidget->OnConsoleMessage.AddDynamic(this, &UMOBAMMOLoginScreenWidget::HandleConsoleMessage);

	if (UOverlaySlot* BrowserSlot = RootOverlay->AddChildToOverlay(WebBrowserWidget))
	{
		BrowserSlot->SetHorizontalAlignment(HAlign_Fill);
		BrowserSlot->SetVerticalAlignment(VAlign_Fill);
	}

	NativeFallbackPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	NativeFallbackPanel->SetBrushColor(FLinearColor(0.020f, 0.024f, 0.032f, 0.96f));
	NativeFallbackPanel->SetPadding(FMargin(28.0f, 24.0f));

	UVerticalBox* PanelContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	NativeFallbackPanel->SetContent(PanelContent);

	UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TitleText->SetText(FText::FromString(TEXT("MOBAMMO")));
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.86f, 0.92f, 1.0f, 1.0f)));
	TitleText->SetJustification(ETextJustify::Center);
	FSlateFontInfo TitleFont = TitleText->GetFont();
	TitleFont.Size = 30;
	TitleFont.TypefaceFontName = TEXT("Bold");
	TitleText->SetFont(TitleFont);
	PanelContent->AddChildToVerticalBox(TitleText);

	NativeFallbackStatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	NativeFallbackStatusText->SetText(FText::FromString(TEXT("Web login UI is loading. You can continue with the native fallback.")));
	NativeFallbackStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.70f, 0.76f, 0.86f, 1.0f)));
	NativeFallbackStatusText->SetAutoWrapText(true);
	NativeFallbackStatusText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* StatusSlot = PanelContent->AddChildToVerticalBox(NativeFallbackStatusText))
	{
		StatusSlot->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 18.0f));
	}

	NativeLoginButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	NativeLoginButton->SetBackgroundColor(FLinearColor(0.18f, 0.31f, 0.48f, 1.0f));
	UTextBlock* ButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ButtonText->SetText(FText::FromString(TEXT("Continue as mimet_bp_test")));
	ButtonText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	ButtonText->SetJustification(ETextJustify::Center);
	FSlateFontInfo ButtonFont = ButtonText->GetFont();
	ButtonFont.Size = 16;
	ButtonFont.TypefaceFontName = TEXT("Bold");
	ButtonText->SetFont(ButtonFont);
	NativeLoginButton->AddChild(ButtonText);
	NativeLoginButton->OnClicked.AddDynamic(this, &UMOBAMMOLoginScreenWidget::HandleNativeLoginClicked);
	if (UVerticalBoxSlot* ButtonSlot = PanelContent->AddChildToVerticalBox(NativeLoginButton))
	{
		ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	USizeBox* PanelSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PanelSizeBox->SetWidthOverride(420.0f);
	PanelSizeBox->SetContent(NativeFallbackPanel);
	PanelSizeBox->SetVisibility(ESlateVisibility::Collapsed);
	if (UOverlaySlot* FallbackSlot = RootOverlay->AddChildToOverlay(PanelSizeBox))
	{
		FallbackSlot->SetHorizontalAlignment(HAlign_Center);
		FallbackSlot->SetVerticalAlignment(VAlign_Center);
		FallbackSlot->SetPadding(FMargin(24.0f));
	}

	UpdateNativeFallback();
}

// ─────────────────────────────────────────────────────────────
// Subsystem Binding
// ─────────────────────────────────────────────────────────────
void UMOBAMMOLoginScreenWidget::LoadWebUIIfNeeded()
{
	if (!WebBrowserWidget || bWebUILoadRequested)
	{
		return;
	}

	bWebUILoadRequested = true;
	bPageLoaded = false;
	WebBrowserWidget->LoadURL(WebUIBaseUrl + TEXT("/login"));
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

// ─────────────────────────────────────────────────────────────
// State Push to WebUI
// ─────────────────────────────────────────────────────────────
void UMOBAMMOLoginScreenWidget::PushStateToWebUI()
{
	if (!WebBrowserWidget || !bPageLoaded)
	{
		return;
	}

	const UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem();
	if (!BackendSubsystem)
	{
		return;
	}

	// Build JSON state to push via CustomEvent
	FString StateJson = FString::Printf(
		TEXT("{\"backendBaseUrl\":\"%s\",\"loginStatus\":\"%s\",\"characterListStatus\":\"%s\",\"sessionStatus\":\"%s\",\"lastError\":\"%s\",\"lastUsername\":\"%s\",\"defaultUsername\":\"%s\"}"),
		*BackendSubsystem->GetBackendBaseUrl().Replace(TEXT("\""), TEXT("\\\"")),
		*BackendSubsystem->GetLoginStatus(),
		*BackendSubsystem->GetCharacterListStatus(),
		*BackendSubsystem->GetSessionStatus(),
		*BackendSubsystem->GetLastErrorMessage().Replace(TEXT("\""), TEXT("\\\"")).Replace(TEXT("\n"), TEXT(" ")),
		*BackendSubsystem->GetLastUsername(),
		*DefaultUsername
	);

	FString Script = FString::Printf(
		TEXT("(function(){var s=%s;window.__UE_STATE__=Object.assign(window.__UE_STATE__||{},s);window.dispatchEvent(new CustomEvent('ue-state',{detail:s}));})();"),
		*StateJson
	);

	WebBrowserWidget->ExecuteJavascript(Script);
}

void UMOBAMMOLoginScreenWidget::UpdateNativeFallback()
{
	if (NativeFallbackPanel)
	{
		NativeFallbackPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (WebBrowserWidget)
	{
		WebBrowserWidget->SetVisibility(ESlateVisibility::Visible);
	}

	UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem();
	const FString LoginStatus = BackendSubsystem ? BackendSubsystem->GetLoginStatus() : TEXT("Idle");
	if (NativeLoginButton)
	{
		NativeLoginButton->SetIsEnabled(false);
	}

	if (NativeFallbackStatusText)
	{
		FString StatusLine = FString::Printf(TEXT("Web login UI is still loading. Login status: %s"), *LoginStatus);
		if (BackendSubsystem && !BackendSubsystem->GetLastErrorMessage().IsEmpty())
		{
			StatusLine += FString::Printf(TEXT("\nLast error: %s"), *BackendSubsystem->GetLastErrorMessage());
		}
		NativeFallbackStatusText->SetText(FText::FromString(StatusLine));
	}
}

// ─────────────────────────────────────────────────────────────
// Console Message Handler (UE_ACTION bridge)
// ─────────────────────────────────────────────────────────────
void UMOBAMMOLoginScreenWidget::HandleConsoleMessage(const FString& Message, const FString& Source, int32 Line)
{
	// Detect page ready signal
	if (Message.Contains(TEXT("UE_ACTION:")) && Message.Contains(TEXT("\"ready\"")))
	{
		bPageLoaded = true;
		PushStateToWebUI();
		UpdateNativeFallback();
	}

	// Parse UE_ACTION messages
	if (!Message.StartsWith(TEXT("UE_ACTION:")))
	{
		return;
	}

	FString JsonPayload = Message.Mid(10); // strip "UE_ACTION:"

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonPayload);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LoginWidget] Failed to parse UE_ACTION JSON: %s"), *JsonPayload);
		return;
	}

	FString ActionType;
	if (!JsonObject->TryGetStringField(TEXT("type"), ActionType))
	{
		return;
	}

	UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem();
	if (!BackendSubsystem)
	{
		return;
	}

	if (ActionType == TEXT("mockLogin"))
	{
		bool bArmed = false;
		JsonObject->TryGetBoolField(TEXT("armed"), bArmed);
		if (!bArmed)
		{
			UE_LOG(LogTemp, Warning, TEXT("[LoginWidget] Ignored unarmed mockLogin action."));
			return;
		}

		FString Username;
		if (JsonObject->TryGetStringField(TEXT("username"), Username) && !Username.IsEmpty())
		{
			BackendSubsystem->LoginFromWebUI(Username);
		}
		else
		{
			BackendSubsystem->LoginFromWebUI(DefaultUsername);
		}
	}
	else if (ActionType == TEXT("ready"))
	{
		bPageLoaded = true;
		PushStateToWebUI();
		UpdateNativeFallback();
	}
}

void UMOBAMMOLoginScreenWidget::HandleNativeLoginClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("[LoginWidget] Native fallback login is disabled; use WebUI login."));
}

// ─────────────────────────────────────────────────────────────
// Event Handlers
// ─────────────────────────────────────────────────────────────
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
