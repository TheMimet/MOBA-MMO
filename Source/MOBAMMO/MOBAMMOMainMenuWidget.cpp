#include "MOBAMMOMainMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
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

    WebBrowserWidget = WidgetTree->ConstructWidget<UWebBrowser>(UWebBrowser::StaticClass());
    WebBrowserWidget->LoadURL(WebUIBaseUrl + TEXT("/main-menu"));
    WebBrowserWidget->OnConsoleMessage.AddDynamic(this, &UMOBAMMOMainMenuWidget::HandleConsoleMessage);

    if (UOverlaySlot* BrowserSlot = RootOverlay->AddChildToOverlay(WebBrowserWidget))
    {
        BrowserSlot->SetHorizontalAlignment(HAlign_Fill);
        BrowserSlot->SetVerticalAlignment(VAlign_Fill);
    }
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

void UMOBAMMOMainMenuWidget::HandleConsoleMessage(const FString& Message, const FString& Source, int32 Line)
{
    if (Message.Contains(TEXT("UE_ACTION:")) && (Message.Contains(TEXT("\"ready\"")) || Message.Contains(TEXT("\"mainMenuReady\""))))
    {
        bPageLoaded = true;
        PushStateToWebUI();
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
    }
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
