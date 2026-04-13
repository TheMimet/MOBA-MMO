#include "MOBAMMOLoginScreenWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WebBrowser.h"

namespace
{
	const TCHAR* LoginScreenUrl = TEXT("http://127.0.0.1:3002/login");
	const TCHAR* ActionPrefix = TEXT("UE_ACTION:");

	FString SerializeJsonObject(const TSharedRef<FJsonObject>& Object)
	{
		FString Output;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		FJsonSerializer::Serialize(Object, Writer);
		return Output;
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
	if (Browser && Browser->GetUrl().IsEmpty())
	{
		Browser->LoadURL(LoginScreenUrl);
	}

	DispatchStateToBrowser();
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
	if (WidgetTree->RootWidget)
	{
		return;
	}

	Browser = WidgetTree->ConstructWidget<UWebBrowser>(UWebBrowser::StaticClass(), TEXT("LoginScreenBrowser"));
	if (!Browser)
	{
		return;
	}

	Browser->OnConsoleMessage.AddDynamic(this, &UMOBAMMOLoginScreenWidget::HandleBrowserConsoleMessage);
	Browser->LoadURL(LoginScreenUrl);
	WidgetTree->RootWidget = Browser;
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

void UMOBAMMOLoginScreenWidget::DispatchStateToBrowser()
{
	if (!Browser)
	{
		return;
	}

	const FString StateJson = BuildStateJson();
	LastPushedStateJson = StateJson;

	const FString Script = FString::Printf(
		TEXT("window.__UE_STATE__=%s;window.dispatchEvent(new CustomEvent('ue-state',{detail:%s}));"),
		*StateJson,
		*StateJson
	);

	Browser->ExecuteJavascript(Script);
}

FString UMOBAMMOLoginScreenWidget::BuildStateJson() const
{
	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("defaultUsername"), DefaultUsername);

	const UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem();
	if (!BackendSubsystem)
	{
		return SerializeJsonObject(RootObject);
	}

	RootObject->SetStringField(TEXT("backendBaseUrl"), BackendSubsystem->GetBackendBaseUrl());
	RootObject->SetStringField(TEXT("loginStatus"), BackendSubsystem->GetLoginStatus());
	RootObject->SetStringField(TEXT("characterListStatus"), BackendSubsystem->GetCharacterListStatus());
	RootObject->SetStringField(TEXT("sessionStatus"), BackendSubsystem->GetSessionStatus());
	RootObject->SetStringField(TEXT("lastError"), BackendSubsystem->GetLastErrorMessage());
	RootObject->SetStringField(TEXT("lastUsername"), BackendSubsystem->GetLastUsername());
	return SerializeJsonObject(RootObject);
}

void UMOBAMMOLoginScreenWidget::HandleBrowserAction(const TSharedPtr<FJsonObject>& ActionObject)
{
	if (!ActionObject.IsValid())
	{
		return;
	}

	UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem();
	if (!BackendSubsystem)
	{
		return;
	}

	FString ActionType;
	if (!ActionObject->TryGetStringField(TEXT("type"), ActionType))
	{
		return;
	}

	if (ActionType == TEXT("ready"))
	{
		DispatchStateToBrowser();
		return;
	}

	if (ActionType == TEXT("mockLogin"))
	{
		FString Username = DefaultUsername;
		ActionObject->TryGetStringField(TEXT("username"), Username);
		BackendSubsystem->MockLogin(Username);
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

void UMOBAMMOLoginScreenWidget::HandleBrowserConsoleMessage(const FString& Message, const FString& Source, int32 Line)
{
	if (!Message.StartsWith(ActionPrefix))
	{
		return;
	}

	const FString Payload = Message.RightChop(FCString::Strlen(ActionPrefix));
	TSharedPtr<FJsonObject> ActionObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Payload);
	if (!FJsonSerializer::Deserialize(Reader, ActionObject) || !ActionObject.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LoginScreen] Failed to parse browser action payload: %s"), *Payload);
		return;
	}

	HandleBrowserAction(ActionObject);
}
