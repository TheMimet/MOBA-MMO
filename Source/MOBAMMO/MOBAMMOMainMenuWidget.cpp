#include "MOBAMMOMainMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Dom/JsonObject.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WebBrowser.h"

namespace
{
    FString EscapeJson(const FString& Value)
    {
        return Value.ReplaceCharWithEscapedChar().Replace(TEXT("\n"), TEXT(" "));
    }
}

void UMOBAMMOMainMenuWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    BuildLayout();

    if (UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
    {
        BindToSubsystem(BackendSubsystem);
    }
}

void UMOBAMMOMainMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();
    RefreshFromBackend();
}

void UMOBAMMOMainMenuWidget::RefreshFromBackend()
{
    PushStateToWebUI();
    SetVisibility(ShouldBeVisible() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    UpdateNativeFallback();
}

bool UMOBAMMOMainMenuWidget::ShouldBeVisible() const
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

    if (FParse::Param(FCommandLine::Get(), TEXT("MOBAMMOAutoSession")))
    {
        return false;
    }

    const FString SessionStatus = BackendSubsystem->GetSessionStatus();
    if (SessionStatus == TEXT("Starting") || SessionStatus == TEXT("Traveling") || SessionStatus == TEXT("Ready") || SessionStatus == TEXT("Active") || SessionStatus == TEXT("Reconnecting"))
    {
        return false;
    }

    return BackendSubsystem->ShouldShowMainMenu();
}

UMOBAMMOBackendSubsystem* UMOBAMMOMainMenuWidget::GetBackendSubsystem() const
{
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        return GameInstance->GetSubsystem<UMOBAMMOBackendSubsystem>();
    }

    return nullptr;
}

void UMOBAMMOMainMenuWidget::BuildLayout()
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
    WebBrowserWidget->OnConsoleMessage.AddDynamic(this, &UMOBAMMOMainMenuWidget::HandleConsoleMessage);

    if (UOverlaySlot* BrowserSlot = RootOverlay->AddChildToOverlay(WebBrowserWidget))
    {
        BrowserSlot->SetHorizontalAlignment(HAlign_Fill);
        BrowserSlot->SetVerticalAlignment(VAlign_Fill);
    }

    WebBrowserWidget->LoadURL(WebUIBaseUrl + TEXT("/main-menu"));

    NativeFallbackPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    NativeFallbackPanel->SetBrushColor(FLinearColor(0.018f, 0.020f, 0.028f, 0.96f));
    NativeFallbackPanel->SetPadding(FMargin(30.0f, 26.0f));

    UVerticalBox* PanelContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    NativeFallbackPanel->SetContent(PanelContent);

    UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    TitleText->SetText(FText::FromString(TEXT("Arena Gate")));
    TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.94f, 0.88f, 0.70f, 1.0f)));
    TitleText->SetJustification(ETextJustify::Center);
    FSlateFontInfo TitleFont = TitleText->GetFont();
    TitleFont.Size = 28;
    TitleFont.TypefaceFontName = TEXT("Bold");
    TitleText->SetFont(TitleFont);
    PanelContent->AddChildToVerticalBox(TitleText);

    NativeFallbackStatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    NativeFallbackStatusText->SetText(FText::FromString(TEXT("Main menu WebUI is loading. Native menu is available.")));
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

    NativePlayButton = MakeFallbackButton(TEXT("Play"), FLinearColor(0.16f, 0.36f, 0.24f, 1.0f));
    NativeCharactersButton = MakeFallbackButton(TEXT("Characters"), FLinearColor(0.18f, 0.27f, 0.44f, 1.0f));
    UButton* NativeQuitButton = MakeFallbackButton(TEXT("Quit"), FLinearColor(0.34f, 0.12f, 0.13f, 1.0f));

    NativePlayButton->OnClicked.AddDynamic(this, &UMOBAMMOMainMenuWidget::HandleNativePlayClicked);
    NativeCharactersButton->OnClicked.AddDynamic(this, &UMOBAMMOMainMenuWidget::HandleNativeCharactersClicked);
    NativeQuitButton->OnClicked.AddDynamic(this, &UMOBAMMOMainMenuWidget::HandleNativeQuitClicked);

    USizeBox* PanelSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    PanelSizeBox->SetWidthOverride(420.0f);
    PanelSizeBox->SetContent(NativeFallbackPanel);
    if (UOverlaySlot* FallbackSlot = RootOverlay->AddChildToOverlay(PanelSizeBox))
    {
        FallbackSlot->SetHorizontalAlignment(HAlign_Center);
        FallbackSlot->SetVerticalAlignment(VAlign_Center);
        FallbackSlot->SetPadding(FMargin(24.0f));
    }

    UpdateNativeFallback();
}

void UMOBAMMOMainMenuWidget::BindToSubsystem(UMOBAMMOBackendSubsystem* BackendSubsystem)
{
    if (!BackendSubsystem || bBoundToSubsystem)
    {
        return;
    }

    BackendSubsystem->OnDebugStateChanged.AddDynamic(this, &UMOBAMMOMainMenuWidget::HandleBackendStateChanged);
    BackendSubsystem->OnLoginSucceeded.AddDynamic(this, &UMOBAMMOMainMenuWidget::HandleLoginSucceeded);
    BackendSubsystem->OnCharactersLoaded.AddDynamic(this, &UMOBAMMOMainMenuWidget::HandleCharactersLoaded);
    BackendSubsystem->OnSessionStarted.AddDynamic(this, &UMOBAMMOMainMenuWidget::HandleSessionStarted);
    BackendSubsystem->OnCharactersLoadFailed.AddDynamic(this, &UMOBAMMOMainMenuWidget::HandleRequestFailed);
    BackendSubsystem->OnSessionStartFailed.AddDynamic(this, &UMOBAMMOMainMenuWidget::HandleRequestFailed);

    bBoundToSubsystem = true;
}

void UMOBAMMOMainMenuWidget::PushStateToWebUI()
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

    FString CharactersArrayJson = TEXT("[");
    const TArray<FBackendCharacterResult> Characters = BackendSubsystem->GetCachedCharacters();
    for (int32 Index = 0; Index < Characters.Num(); ++Index)
    {
        const FBackendCharacterResult& Character = Characters[Index];
        if (Index > 0)
        {
            CharactersArrayJson += TEXT(",");
        }

        CharactersArrayJson += FString::Printf(
            TEXT("{\"characterId\":\"%s\",\"name\":\"%s\",\"classId\":\"%s\",\"level\":%d}"),
            *EscapeJson(Character.CharacterId),
            *EscapeJson(Character.Name),
            *EscapeJson(Character.ClassId),
            Character.Level
        );
    }
    CharactersArrayJson += TEXT("]");

    const FString SessionStatus = BackendSubsystem->GetSessionStatus();
    const bool bIsBusy = SessionStatus == TEXT("Starting") || SessionStatus == TEXT("Traveling") || BackendSubsystem->GetCharacterListStatus() == TEXT("Loading");
    const bool bCanContinue = Characters.Num() > 0;

    const FString StateJson = FString::Printf(
        TEXT("{\"loginStatus\":\"%s\",\"characterListStatus\":\"%s\",\"sessionStatus\":\"%s\",\"lastError\":\"%s\",\"lastUsername\":\"%s\",\"selectedCharacterId\":\"%s\",\"characters\":%s,\"canContinue\":%s,\"isBusy\":%s}"),
        *EscapeJson(BackendSubsystem->GetLoginStatus()),
        *EscapeJson(BackendSubsystem->GetCharacterListStatus()),
        *EscapeJson(SessionStatus),
        *EscapeJson(BackendSubsystem->GetLastErrorMessage()),
        *EscapeJson(BackendSubsystem->GetLastUsername()),
        *EscapeJson(BackendSubsystem->GetSelectedCharacterId()),
        *CharactersArrayJson,
        bCanContinue ? TEXT("true") : TEXT("false"),
        bIsBusy ? TEXT("true") : TEXT("false")
    );

    const FString Script = FString::Printf(
        TEXT("(function(){var s=%s;window.__UE_STATE__=Object.assign(window.__UE_STATE__||{},s);window.dispatchEvent(new CustomEvent('ue-state',{detail:s}));})();"),
        *StateJson
    );

    WebBrowserWidget->ExecuteJavascript(Script);
}

void UMOBAMMOMainMenuWidget::UpdateNativeFallback()
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
    const FString SessionStatus = BackendSubsystem ? BackendSubsystem->GetSessionStatus() : TEXT("Idle");
    const bool bBusy = SessionStatus == TEXT("Starting") || SessionStatus == TEXT("Traveling")
        || (BackendSubsystem && BackendSubsystem->GetCharacterListStatus() == TEXT("Loading"));
    if (NativePlayButton)
    {
        NativePlayButton->SetIsEnabled(!bBusy);
    }
    if (NativeCharactersButton)
    {
        NativeCharactersButton->SetIsEnabled(!bBusy);
    }
    if (NativeFallbackStatusText)
    {
        const int32 CharacterCount = BackendSubsystem ? BackendSubsystem->GetCachedCharacters().Num() : 0;
        FString StatusLine = FString::Printf(TEXT("Web main menu is still loading. Characters: %d | Session: %s"), CharacterCount, *SessionStatus);
        if (BackendSubsystem && !BackendSubsystem->GetLastErrorMessage().IsEmpty())
        {
            StatusLine += FString::Printf(TEXT("\nLast error: %s"), *BackendSubsystem->GetLastErrorMessage());
        }
        NativeFallbackStatusText->SetText(FText::FromString(StatusLine));
    }
}

void UMOBAMMOMainMenuWidget::HandleConsoleMessage(const FString& Message, const FString& Source, int32 Line)
{
    if (Message.Contains(TEXT("UE_ACTION:")) && (Message.Contains(TEXT("\"ready\"")) || Message.Contains(TEXT("\"mainMenuReady\""))))
    {
        bPageLoaded = true;
        PushStateToWebUI();
        UpdateNativeFallback();
    }

    if (!Message.StartsWith(TEXT("UE_ACTION:")))
    {
        return;
    }

    const FString JsonPayload = Message.Mid(10);

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonPayload);
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("[MainMenuWidget] Failed to parse UE_ACTION JSON: %s"), *JsonPayload);
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

    if (ActionType == TEXT("play"))
    {
        BackendSubsystem->PlayFromMainMenu();
    }
    else if (ActionType == TEXT("openCharacters"))
    {
        BackendSubsystem->OpenMainMenuCharacters();
    }
    else if (ActionType == TEXT("quit"))
    {
        UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
    }
    else if (ActionType == TEXT("ready") || ActionType == TEXT("mainMenuReady"))
    {
        bPageLoaded = true;
        PushStateToWebUI();
        UpdateNativeFallback();
    }
}

void UMOBAMMOMainMenuWidget::HandleNativePlayClicked()
{
    if (UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
    {
        BackendSubsystem->PlayFromMainMenu();
    }

    UpdateNativeFallback();
}

void UMOBAMMOMainMenuWidget::HandleNativeCharactersClicked()
{
    if (UMOBAMMOBackendSubsystem* BackendSubsystem = GetBackendSubsystem())
    {
        BackendSubsystem->OpenMainMenuCharacters();
    }

    UpdateNativeFallback();
}

void UMOBAMMOMainMenuWidget::HandleNativeQuitClicked()
{
    UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UMOBAMMOMainMenuWidget::HandleBackendStateChanged()
{
    RefreshFromBackend();
}

void UMOBAMMOMainMenuWidget::HandleLoginSucceeded(const FBackendLoginResult& Result)
{
    RefreshFromBackend();
}

void UMOBAMMOMainMenuWidget::HandleCharactersLoaded(const FBackendCharacterListResult& Result)
{
    RefreshFromBackend();
}

void UMOBAMMOMainMenuWidget::HandleSessionStarted(const FBackendSessionResult& Result)
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

void UMOBAMMOMainMenuWidget::HandleRequestFailed(const FString& ErrorMessage)
{
    RefreshFromBackend();
}
