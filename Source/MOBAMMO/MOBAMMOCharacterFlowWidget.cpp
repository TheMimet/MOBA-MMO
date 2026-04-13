#include "MOBAMMOCharacterFlowWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Dom/JsonObject.h"
#include "GameFramework/PlayerController.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WebBrowser.h"

namespace
{
	const TCHAR* CharacterFlowUrl = TEXT("http://127.0.0.1:3002/character-creator");
	const TCHAR* ActionPrefix = TEXT("UE_ACTION:");

	FString SerializeJsonObject(const TSharedRef<FJsonObject>& Object)
	{
		FString Output;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		FJsonSerializer::Serialize(Object, Writer);
		return Output;
	}
}

void UMOBAMMOCharacterFlowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildLayout();

	if (UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
	{
		BindToSubsystem(BackendSubsystem);
	}
}

void UMOBAMMOCharacterFlowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshFromBackend();
}

void UMOBAMMOCharacterFlowWidget::RefreshFromBackend()
{
	if (Browser && Browser->GetUrl().IsEmpty())
	{
		Browser->LoadURL(CharacterFlowUrl);
	}

	DispatchStateToBrowser();
	SetVisibility(ShouldBeVisible() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

bool UMOBAMMOCharacterFlowWidget::ShouldBeVisible() const
{
	if (const UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
	{
		return BackendSubsystem->IsWaitingForCharacterSelection();
	}

	return false;
}

UMOBAMMOBackendSubsystem* UMOBAMMOCharacterFlowWidget::GetBackendSubsystem() const
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<UMOBAMMOBackendSubsystem>();
	}

	return nullptr;
}

void UMOBAMMOCharacterFlowWidget::BuildLayout()
{
	if (WidgetTree->RootWidget)
	{
		return;
	}

	Browser = WidgetTree->ConstructWidget<UWebBrowser>(UWebBrowser::StaticClass(), TEXT("CharacterFlowBrowser"));
	if (!Browser)
	{
		return;
	}

	Browser->OnConsoleMessage.AddDynamic(this, &UMOBAMMOCharacterFlowWidget::HandleBrowserConsoleMessage);
	Browser->LoadURL(CharacterFlowUrl);
	WidgetTree->RootWidget = Browser;
}

void UMOBAMMOCharacterFlowWidget::BindToSubsystem(UMOBAMMOBackendSubsystem* BackendSubsystem)
{
	if (!BackendSubsystem || bBoundToSubsystem)
	{
		return;
	}

	BackendSubsystem->OnLoginSucceeded.AddDynamic(this, &UMOBAMMOCharacterFlowWidget::HandleLoginSucceeded);
	BackendSubsystem->OnDebugStateChanged.AddDynamic(this, &UMOBAMMOCharacterFlowWidget::HandleBackendStateChanged);
	BackendSubsystem->OnCharactersLoaded.AddDynamic(this, &UMOBAMMOCharacterFlowWidget::HandleCharactersLoaded);
	BackendSubsystem->OnCharacterCreated.AddDynamic(this, &UMOBAMMOCharacterFlowWidget::HandleCharacterCreated);
	BackendSubsystem->OnSessionStarted.AddDynamic(this, &UMOBAMMOCharacterFlowWidget::HandleSessionStarted);
	BackendSubsystem->OnLoginFailed.AddDynamic(this, &UMOBAMMOCharacterFlowWidget::HandleRequestFailed);
	BackendSubsystem->OnCharactersLoadFailed.AddDynamic(this, &UMOBAMMOCharacterFlowWidget::HandleRequestFailed);
	BackendSubsystem->OnCharacterCreateFailed.AddDynamic(this, &UMOBAMMOCharacterFlowWidget::HandleRequestFailed);
	BackendSubsystem->OnSessionStartFailed.AddDynamic(this, &UMOBAMMOCharacterFlowWidget::HandleRequestFailed);
	bBoundToSubsystem = true;
}

void UMOBAMMOCharacterFlowWidget::DispatchStateToBrowser()
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

FString UMOBAMMOCharacterFlowWidget::BuildStateJson() const
{
	const UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem();
	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();

	RootObject->SetStringField(TEXT("defaultCharacterName"), DefaultCharacterName);
	RootObject->SetStringField(TEXT("defaultClassId"), DefaultClassId);

	if (!BackendSubsystem)
	{
		return SerializeJsonObject(RootObject);
	}

	RootObject->SetStringField(TEXT("backendBaseUrl"), BackendSubsystem->GetBackendBaseUrl());
	RootObject->SetStringField(TEXT("loginStatus"), BackendSubsystem->GetLoginStatus());
	RootObject->SetStringField(TEXT("characterListStatus"), BackendSubsystem->GetCharacterListStatus());
	RootObject->SetStringField(TEXT("characterStatus"), BackendSubsystem->GetCharacterStatus());
	RootObject->SetStringField(TEXT("sessionStatus"), BackendSubsystem->GetSessionStatus());
	RootObject->SetStringField(TEXT("lastError"), BackendSubsystem->GetLastErrorMessage());
	RootObject->SetStringField(TEXT("accountId"), BackendSubsystem->GetLastAccountId());
	RootObject->SetStringField(TEXT("selectedCharacterId"), BackendSubsystem->GetSelectedCharacterId());

	TArray<TSharedPtr<FJsonValue>> CharacterValues;
	for (const FBackendCharacterResult& Character : BackendSubsystem->GetCachedCharacters())
	{
		const TSharedRef<FJsonObject> CharacterObject = MakeShared<FJsonObject>();
		CharacterObject->SetStringField(TEXT("characterId"), Character.CharacterId);
		CharacterObject->SetStringField(TEXT("name"), Character.Name);
		CharacterObject->SetStringField(TEXT("classId"), Character.ClassId);
		CharacterObject->SetNumberField(TEXT("level"), Character.Level);
		CharacterValues.Add(MakeShared<FJsonValueObject>(CharacterObject));
	}

	RootObject->SetArrayField(TEXT("characters"), CharacterValues);
	return SerializeJsonObject(RootObject);
}

void UMOBAMMOCharacterFlowWidget::HandleBrowserAction(const TSharedPtr<FJsonObject>& ActionObject)
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

	if (ActionType == TEXT("refreshCharacters"))
	{
		BackendSubsystem->ListCharacters(BackendSubsystem->GetLastAccountId());
		return;
	}

	if (ActionType == TEXT("selectCharacter"))
	{
		FString CharacterId;
		if (ActionObject->TryGetStringField(TEXT("characterId"), CharacterId))
		{
			BackendSubsystem->SelectCharacter(CharacterId);
		}
		return;
	}

	if (ActionType == TEXT("createCharacter"))
	{
		FString CharacterName = DefaultCharacterName;
		FString ClassId = DefaultClassId;
		int32 PresetId = 4;
		int32 ColorIndex = 0;
		int32 Shade = 58;
		int32 Transparent = 18;
		int32 TextureDetail = 88;
		double NumericValue = 0.0;
		bool bAutoEnter = false;

		ActionObject->TryGetStringField(TEXT("name"), CharacterName);
		ActionObject->TryGetStringField(TEXT("classId"), ClassId);
		ActionObject->TryGetBoolField(TEXT("autoEnter"), bAutoEnter);
		if (ActionObject->TryGetNumberField(TEXT("presetId"), NumericValue)) { PresetId = FMath::RoundToInt(NumericValue); }
		if (ActionObject->TryGetNumberField(TEXT("colorIndex"), NumericValue)) { ColorIndex = FMath::RoundToInt(NumericValue); }
		if (ActionObject->TryGetNumberField(TEXT("shade"), NumericValue)) { Shade = FMath::RoundToInt(NumericValue); }
		if (ActionObject->TryGetNumberField(TEXT("transparent"), NumericValue)) { Transparent = FMath::RoundToInt(NumericValue); }
		if (ActionObject->TryGetNumberField(TEXT("textureDetail"), NumericValue)) { TextureDetail = FMath::RoundToInt(NumericValue); }

		bAutoStartSessionAfterCharacterCreate = bAutoEnter;
		BackendSubsystem->CreateCharacterForCurrentAccount(CharacterName, ClassId, PresetId, ColorIndex, Shade, Transparent, TextureDetail);
		return;
	}

	if (ActionType == TEXT("startSession"))
	{
		FString CharacterId;
		if (ActionObject->TryGetStringField(TEXT("characterId"), CharacterId) && !CharacterId.IsEmpty())
		{
			BackendSubsystem->SelectCharacter(CharacterId);
		}

		BackendSubsystem->StartSessionForSelectedCharacter();
	}
}

void UMOBAMMOCharacterFlowWidget::HandleLoginSucceeded(const FBackendLoginResult& Result)
{
	RefreshFromBackend();
}

void UMOBAMMOCharacterFlowWidget::HandleBackendStateChanged()
{
	RefreshFromBackend();
}

void UMOBAMMOCharacterFlowWidget::HandleCharactersLoaded(const FBackendCharacterListResult& Result)
{
	RefreshFromBackend();
}

void UMOBAMMOCharacterFlowWidget::HandleCharacterCreated(const FBackendCharacterResult& Result)
{
	if (bAutoStartSessionAfterCharacterCreate)
	{
		bAutoStartSessionAfterCharacterCreate = false;
		if (UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
		{
			BackendSubsystem->StartSessionForSelectedCharacter();
		}
	}

	RefreshFromBackend();
}

void UMOBAMMOCharacterFlowWidget::HandleSessionStarted(const FBackendSessionResult& Result)
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

void UMOBAMMOCharacterFlowWidget::HandleRequestFailed(const FString& ErrorMessage)
{
	bAutoStartSessionAfterCharacterCreate = false;
	RefreshFromBackend();
}

void UMOBAMMOCharacterFlowWidget::HandleBrowserConsoleMessage(const FString& Message, const FString& Source, int32 Line)
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
		UE_LOG(LogTemp, Warning, TEXT("[CharacterFlow] Failed to parse browser action payload: %s"), *Payload);
		return;
	}

	HandleBrowserAction(ActionObject);
}
