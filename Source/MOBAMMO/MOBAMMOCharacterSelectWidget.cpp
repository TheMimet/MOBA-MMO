#include "MOBAMMOCharacterSelectWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "GameFramework/PlayerController.h"
#include "WebBrowser.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// ─────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────
void UMOBAMMOCharacterSelectWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildLayout();

	if (UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
	{
		BindToSubsystem(BackendSubsystem);
	}
}

void UMOBAMMOCharacterSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshFromBackend();
}

void UMOBAMMOCharacterSelectWidget::RefreshFromBackend()
{
	PushStateToWebUI();
	SetVisibility(ShouldBeVisible() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

bool UMOBAMMOCharacterSelectWidget::ShouldBeVisible() const
{
	const UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem();
	if (!BackendSubsystem)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const ENetMode NetMode = World ? World->GetNetMode() : NM_Standalone;
	if (NetMode == NM_Client)
	{
		return false;
	}

	if (BackendSubsystem->IsWaitingForCharacterSelection())
	{
		return true;
	}

	return false;
}

UMOBAMMOBackendSubsystem* UMOBAMMOCharacterSelectWidget::GetBackendSubsystem() const
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
void UMOBAMMOCharacterSelectWidget::BuildLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = RootOverlay;

	WebBrowserWidget = WidgetTree->ConstructWidget<UWebBrowser>(UWebBrowser::StaticClass());
	WebBrowserWidget->LoadURL(WebUIBaseUrl + TEXT("/character-creator"));

	WebBrowserWidget->OnConsoleMessage.AddDynamic(this, &UMOBAMMOCharacterSelectWidget::HandleConsoleMessage);

	if (UOverlaySlot* BrowserSlot = RootOverlay->AddChildToOverlay(WebBrowserWidget))
	{
		BrowserSlot->SetHorizontalAlignment(HAlign_Fill);
		BrowserSlot->SetVerticalAlignment(VAlign_Fill);
	}
}

// ─────────────────────────────────────────────────────────────
// Subsystem Binding
// ─────────────────────────────────────────────────────────────
void UMOBAMMOCharacterSelectWidget::BindToSubsystem(UMOBAMMOBackendSubsystem* BackendSubsystem)
{
	if (!BackendSubsystem || bBoundToSubsystem)
	{
		return;
	}

	BackendSubsystem->OnDebugStateChanged.AddDynamic(this, &UMOBAMMOCharacterSelectWidget::HandleBackendStateChanged);
	BackendSubsystem->OnLoginSucceeded.AddDynamic(this, &UMOBAMMOCharacterSelectWidget::HandleLoginSucceeded);
	BackendSubsystem->OnCharactersLoaded.AddDynamic(this, &UMOBAMMOCharacterSelectWidget::HandleCharactersLoaded);
	BackendSubsystem->OnCharacterCreated.AddDynamic(this, &UMOBAMMOCharacterSelectWidget::HandleCharacterCreated);
	BackendSubsystem->OnSessionStarted.AddDynamic(this, &UMOBAMMOCharacterSelectWidget::HandleSessionStarted);
	BackendSubsystem->OnCharactersLoadFailed.AddDynamic(this, &UMOBAMMOCharacterSelectWidget::HandleRequestFailed);
	BackendSubsystem->OnCharacterCreateFailed.AddDynamic(this, &UMOBAMMOCharacterSelectWidget::HandleRequestFailed);
	BackendSubsystem->OnSessionStartFailed.AddDynamic(this, &UMOBAMMOCharacterSelectWidget::HandleRequestFailed);

	bBoundToSubsystem = true;
}

// ─────────────────────────────────────────────────────────────
// State Push to WebUI
// ─────────────────────────────────────────────────────────────
void UMOBAMMOCharacterSelectWidget::PushStateToWebUI()
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

	// Build characters array JSON
	FString CharactersArrayJson = TEXT("[");
	const TArray<FBackendCharacterResult>& Characters = BackendSubsystem->GetCachedCharacters();
	for (int32 Index = 0; Index < Characters.Num(); ++Index)
	{
		const FBackendCharacterResult& Char = Characters[Index];
		if (Index > 0) CharactersArrayJson += TEXT(",");
		CharactersArrayJson += FString::Printf(
			TEXT("{\"characterId\":\"%s\",\"name\":\"%s\",\"classId\":\"%s\",\"level\":%d}"),
			*Char.CharacterId.Replace(TEXT("\""), TEXT("\\\"")),
			*Char.Name.Replace(TEXT("\""), TEXT("\\\"")),
			*Char.ClassId.Replace(TEXT("\""), TEXT("\\\"")),
			Char.Level
		);
	}
	CharactersArrayJson += TEXT("]");

	FString StateJson = FString::Printf(
		TEXT("{\"loginStatus\":\"%s\",\"characterListStatus\":\"%s\",\"characterStatus\":\"%s\",\"sessionStatus\":\"%s\",\"lastError\":\"%s\",\"accountId\":\"%s\",\"selectedCharacterId\":\"%s\",\"characters\":%s}"),
		*BackendSubsystem->GetLoginStatus(),
		*BackendSubsystem->GetCharacterListStatus(),
		*BackendSubsystem->GetCharacterStatus(),
		*BackendSubsystem->GetSessionStatus(),
		*BackendSubsystem->GetLastErrorMessage().Replace(TEXT("\""), TEXT("\\\"")).Replace(TEXT("\n"), TEXT(" ")),
		*BackendSubsystem->GetLastAccountId(),
		*BackendSubsystem->GetSelectedCharacterId(),
		*CharactersArrayJson
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
void UMOBAMMOCharacterSelectWidget::HandleConsoleMessage(const FString& Message, const FString& Source, int32 Line)
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

	FString JsonPayload = Message.Mid(10);

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonPayload);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CharacterSelectWidget] Failed to parse UE_ACTION JSON: %s"), *JsonPayload);
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

	if (ActionType == TEXT("createCharacter"))
	{
		FString Name;
		JsonObject->TryGetStringField(TEXT("name"), Name);
		FString ClassId;
		JsonObject->TryGetStringField(TEXT("classId"), ClassId);
		if (ClassId.IsEmpty()) ClassId = TEXT("fighter");

		int32 PresetId = 4;
		int32 ColorIndex = 0;
		int32 Shade = 58;
		int32 Transparent = 18;
		int32 TextureDetail = 88;

		double TempVal;
		if (JsonObject->TryGetNumberField(TEXT("presetId"), TempVal)) PresetId = static_cast<int32>(TempVal);
		if (JsonObject->TryGetNumberField(TEXT("colorIndex"), TempVal)) ColorIndex = static_cast<int32>(TempVal);
		if (JsonObject->TryGetNumberField(TEXT("shade"), TempVal)) Shade = static_cast<int32>(TempVal);
		if (JsonObject->TryGetNumberField(TEXT("transparent"), TempVal)) Transparent = static_cast<int32>(TempVal);
		if (JsonObject->TryGetNumberField(TEXT("textureDetail"), TempVal)) TextureDetail = static_cast<int32>(TempVal);

		BackendSubsystem->CreateCharacterForCurrentAccount(Name, ClassId, PresetId, ColorIndex, Shade, Transparent, TextureDetail);
	}
	else if (ActionType == TEXT("startSession"))
	{
		// Optionally accept a specific characterId
		FString CharacterId;
		if (JsonObject->TryGetStringField(TEXT("characterId"), CharacterId) && !CharacterId.IsEmpty())
		{
			BackendSubsystem->SelectCharacter(CharacterId);
		}

		BackendSubsystem->StartSessionForSelectedCharacter();
	}
	else if (ActionType == TEXT("refreshCharacters"))
	{
		BackendSubsystem->ListCharacters(BackendSubsystem->GetLastAccountId());
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
void UMOBAMMOCharacterSelectWidget::HandleBackendStateChanged()
{
	RefreshFromBackend();
}

void UMOBAMMOCharacterSelectWidget::HandleLoginSucceeded(const FBackendLoginResult& Result)
{
	if (UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
	{
		BackendSubsystem->ListCharacters(Result.AccountId);
	}
}

void UMOBAMMOCharacterSelectWidget::HandleCharactersLoaded(const FBackendCharacterListResult& Result)
{
	RefreshFromBackend();
}

void UMOBAMMOCharacterSelectWidget::HandleCharacterCreated(const FBackendCharacterResult& Result)
{
	if (UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
	{
		BackendSubsystem->SelectCharacter(Result.CharacterId);
	}

	RefreshFromBackend();
}

void UMOBAMMOCharacterSelectWidget::HandleSessionStarted(const FBackendSessionResult& Result)
{
	if (UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
	{
		if (APlayerController* PlayerController = GetOwningPlayer())
		{
			BackendSubsystem->TravelToSession(PlayerController, Result.ConnectString);
		}
	}

	RefreshFromBackend();
}

void UMOBAMMOCharacterSelectWidget::HandleRequestFailed(const FString& ErrorMessage)
{
	RefreshFromBackend();
}
