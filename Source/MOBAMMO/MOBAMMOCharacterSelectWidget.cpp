#include "MOBAMMOCharacterSelectWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
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
	const bool bVisible = ShouldBeVisible();
	SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bVisible)
	{
		LoadWebUIIfNeeded();
	}
	PushStateToWebUI();
	UpdateNativeFallback();
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

	UBorder* LoadingBackdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	LoadingBackdrop->SetBrushColor(FLinearColor(0.005f, 0.006f, 0.010f, 1.0f));
	if (UOverlaySlot* BackdropSlot = RootOverlay->AddChildToOverlay(LoadingBackdrop))
	{
		BackdropSlot->SetHorizontalAlignment(HAlign_Fill);
		BackdropSlot->SetVerticalAlignment(VAlign_Fill);
	}

	WebBrowserWidget = WidgetTree->ConstructWidget<UWebBrowser>(UWebBrowser::StaticClass());

	WebBrowserWidget->OnConsoleMessage.AddDynamic(this, &UMOBAMMOCharacterSelectWidget::HandleConsoleMessage);

	if (UOverlaySlot* BrowserSlot = RootOverlay->AddChildToOverlay(WebBrowserWidget))
	{
		BrowserSlot->SetHorizontalAlignment(HAlign_Fill);
		BrowserSlot->SetVerticalAlignment(VAlign_Fill);
	}

	NativeFallbackPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	NativeFallbackPanel->SetBrushColor(FLinearColor(0.020f, 0.022f, 0.030f, 0.96f));
	NativeFallbackPanel->SetPadding(FMargin(30.0f, 26.0f));

	UVerticalBox* PanelContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	NativeFallbackPanel->SetContent(PanelContent);

	UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TitleText->SetText(FText::FromString(TEXT("Characters")));
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.90f, 0.86f, 0.70f, 1.0f)));
	TitleText->SetJustification(ETextJustify::Center);
	FSlateFontInfo TitleFont = TitleText->GetFont();
	TitleFont.Size = 28;
	TitleFont.TypefaceFontName = TEXT("Bold");
	TitleText->SetFont(TitleFont);
	PanelContent->AddChildToVerticalBox(TitleText);

	NativeFallbackStatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	NativeFallbackStatusText->SetText(FText::FromString(TEXT("Character WebUI is loading. Native fallback is available.")));
	NativeFallbackStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.78f, 0.88f, 1.0f)));
	NativeFallbackStatusText->SetAutoWrapText(true);
	NativeFallbackStatusText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* StatusSlot = PanelContent->AddChildToVerticalBox(NativeFallbackStatusText))
	{
		StatusSlot->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 18.0f));
	}

	auto MakeFallbackButton = [this, PanelContent](const FString& Label, const FLinearColor& Color) -> UButton*
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		Button->SetBackgroundColor(Color);
		UTextBlock* ButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		ButtonText->SetText(FText::FromString(Label));
		ButtonText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		ButtonText->SetJustification(ETextJustify::Center);
		FSlateFontInfo ButtonFont = ButtonText->GetFont();
		ButtonFont.Size = 16;
		ButtonFont.TypefaceFontName = TEXT("Bold");
		ButtonText->SetFont(ButtonFont);
		Button->AddChild(ButtonText);
		if (UVerticalBoxSlot* ButtonSlot = PanelContent->AddChildToVerticalBox(Button))
		{
			ButtonSlot->SetPadding(FMargin(0.0f, 5.0f));
			ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		return Button;
	};

	NativeStartButton = MakeFallbackButton(TEXT("Start Selected"), FLinearColor(0.16f, 0.36f, 0.24f, 1.0f));
	NativeCreateButton = MakeFallbackButton(TEXT("Create Test Hero"), FLinearColor(0.18f, 0.27f, 0.44f, 1.0f));
	UButton* NativeRefreshButton = MakeFallbackButton(TEXT("Refresh"), FLinearColor(0.26f, 0.21f, 0.13f, 1.0f));

	NativeStartButton->OnClicked.AddDynamic(this, &UMOBAMMOCharacterSelectWidget::HandleNativeStartClicked);
	NativeCreateButton->OnClicked.AddDynamic(this, &UMOBAMMOCharacterSelectWidget::HandleNativeCreateClicked);
	NativeRefreshButton->OnClicked.AddDynamic(this, &UMOBAMMOCharacterSelectWidget::HandleNativeRefreshClicked);

	USizeBox* PanelSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PanelSizeBox->SetWidthOverride(430.0f);
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
void UMOBAMMOCharacterSelectWidget::LoadWebUIIfNeeded()
{
	if (!WebBrowserWidget || bWebUILoadRequested)
	{
		return;
	}

	bWebUILoadRequested = true;
	bPageLoaded = false;
	WebBrowserWidget->LoadURL(WebUIBaseUrl + TEXT("/character-creator"));
}

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

void UMOBAMMOCharacterSelectWidget::UpdateNativeFallback()
{
	if (NativeFallbackPanel)
	{
		NativeFallbackPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (WebBrowserWidget)
	{
		WebBrowserWidget->SetVisibility(ESlateVisibility::Visible);
	}

	const UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem();
	const int32 CharacterCount = BackendSubsystem ? BackendSubsystem->GetCachedCharacters().Num() : 0;
	const FString CharacterStatus = BackendSubsystem ? BackendSubsystem->GetCharacterStatus() : TEXT("Idle");
	const FString SessionStatus = BackendSubsystem ? BackendSubsystem->GetSessionStatus() : TEXT("Idle");
	const bool bBusy = CharacterStatus == TEXT("Creating") || SessionStatus == TEXT("Starting") || SessionStatus == TEXT("Traveling")
		|| (BackendSubsystem && BackendSubsystem->GetCharacterListStatus() == TEXT("Loading"));

	if (NativeStartButton)
	{
		NativeStartButton->SetIsEnabled(false);
	}
	if (NativeCreateButton)
	{
		NativeCreateButton->SetIsEnabled(false);
	}
	if (NativeFallbackStatusText)
	{
		FString StatusLine = FString::Printf(TEXT("Web character UI is still loading. Characters: %d | Character: %s | Session: %s"), CharacterCount, *CharacterStatus, *SessionStatus);
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
void UMOBAMMOCharacterSelectWidget::HandleConsoleMessage(const FString& Message, const FString& Source, int32 Line)
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
		bool bArmed = false;
		JsonObject->TryGetBoolField(TEXT("armed"), bArmed);
		if (!bArmed)
		{
			UE_LOG(LogTemp, Warning, TEXT("[CharacterSelectWidget] Ignored unarmed createCharacter action."));
			return;
		}

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
		bool bAutoEnter = false;

		double TempVal;
		if (JsonObject->TryGetNumberField(TEXT("presetId"), TempVal)) PresetId = static_cast<int32>(TempVal);
		if (JsonObject->TryGetNumberField(TEXT("colorIndex"), TempVal)) ColorIndex = static_cast<int32>(TempVal);
		if (JsonObject->TryGetNumberField(TEXT("shade"), TempVal)) Shade = static_cast<int32>(TempVal);
		if (JsonObject->TryGetNumberField(TEXT("transparent"), TempVal)) Transparent = static_cast<int32>(TempVal);
		if (JsonObject->TryGetNumberField(TEXT("textureDetail"), TempVal)) TextureDetail = static_cast<int32>(TempVal);
		JsonObject->TryGetBoolField(TEXT("autoEnter"), bAutoEnter);

		bStartSessionAfterCharacterCreate = bAutoEnter;
		BackendSubsystem->CreateCharacterForCurrentAccount(Name, ClassId, PresetId, ColorIndex, Shade, Transparent, TextureDetail);
	}
	else if (ActionType == TEXT("startSession"))
	{
		bool bArmed = false;
		JsonObject->TryGetBoolField(TEXT("armed"), bArmed);
		if (!bArmed)
		{
			UE_LOG(LogTemp, Warning, TEXT("[CharacterSelectWidget] Ignored unarmed startSession action."));
			return;
		}

		// Optionally accept a specific characterId
		FString CharacterId;
		if (JsonObject->TryGetStringField(TEXT("characterId"), CharacterId) && !CharacterId.IsEmpty())
		{
			BackendSubsystem->SelectCharacter(CharacterId);
		}

		BackendSubsystem->StartSessionForSelectedCharacter();
	}
	else if (ActionType == TEXT("selectCharacter"))
	{
		FString CharacterId;
		if (JsonObject->TryGetStringField(TEXT("characterId"), CharacterId) && !CharacterId.IsEmpty())
		{
			BackendSubsystem->SelectCharacter(CharacterId);
			PushStateToWebUI();
		}
	}
	else if (ActionType == TEXT("refreshCharacters"))
	{
		BackendSubsystem->ListCharacters(BackendSubsystem->GetLastAccountId());
	}
	else if (ActionType == TEXT("ready"))
	{
		bPageLoaded = true;
		PushStateToWebUI();
		UpdateNativeFallback();
	}
}

void UMOBAMMOCharacterSelectWidget::HandleNativeStartClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("[CharacterSelectWidget] Native fallback start is disabled; use WebUI character flow."));
}

void UMOBAMMOCharacterSelectWidget::HandleNativeCreateClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("[CharacterSelectWidget] Native fallback create is disabled; use WebUI character flow."));
}

void UMOBAMMOCharacterSelectWidget::HandleNativeRefreshClicked()
{
	if (UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
	{
		BackendSubsystem->ListCharacters(BackendSubsystem->GetLastAccountId());
	}

	UpdateNativeFallback();
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
		if (bStartSessionAfterCharacterCreate)
		{
			bStartSessionAfterCharacterCreate = false;
			BackendSubsystem->StartSessionForSelectedCharacter();
		}
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
	bStartSessionAfterCharacterCreate = false;
	RefreshFromBackend();
}
