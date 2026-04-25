#include "MOBAMMOLoginScreenWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
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
	PushStateToWebUI();
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

	WebBrowserWidget = WidgetTree->ConstructWidget<UWebBrowser>(UWebBrowser::StaticClass());
	WebBrowserWidget->LoadURL(WebUIBaseUrl + TEXT("/login"));

	WebBrowserWidget->OnConsoleMessage.AddDynamic(this, &UMOBAMMOLoginScreenWidget::HandleConsoleMessage);

	if (UOverlaySlot* BrowserSlot = RootOverlay->AddChildToOverlay(WebBrowserWidget))
	{
		BrowserSlot->SetHorizontalAlignment(HAlign_Fill);
		BrowserSlot->SetVerticalAlignment(VAlign_Fill);
	}
}

// ─────────────────────────────────────────────────────────────
// Subsystem Binding
// ─────────────────────────────────────────────────────────────
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
		FString Username;
		if (JsonObject->TryGetStringField(TEXT("username"), Username) && !Username.IsEmpty())
		{
			BackendSubsystem->MockLogin(Username);
		}
		else
		{
			BackendSubsystem->MockLogin(DefaultUsername);
		}
	}
	else if (ActionType == TEXT("ready"))
	{
		bPageLoaded = true;
		PushStateToWebUI();
	}
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
