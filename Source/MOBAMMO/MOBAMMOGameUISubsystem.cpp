#include "MOBAMMOGameUISubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "MOBAMMOBackendSubsystem.h"
#include "MOBAMMOGameHUDWidget.h"
#include "MOBAMMOLoginScreenWidget.h"
#include "MOBAMMOLoadingScreenWidget.h"
#include "MOBAMMOCharacterSelectWidget.h"
#include "UObject/SoftObjectPath.h"

void UMOBAMMOGameUISubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

#if !UE_BUILD_SHIPPING
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UMOBAMMOGameUISubsystem::Tick),
        0.25f
    );
#endif
}

void UMOBAMMOGameUISubsystem::Deinitialize()
{
#if !UE_BUILD_SHIPPING
    if (TickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
        TickHandle.Reset();
    }
#endif

    if (LoginWidget)
    {
        LoginWidget->RemoveFromParent();
        LoginWidget = nullptr;
    }

    if (CharacterSelectWidget)
    {
        CharacterSelectWidget->RemoveFromParent();
        CharacterSelectWidget = nullptr;
    }

    if (LoadingWidget)
    {
        LoadingWidget->RemoveFromParent();
        LoadingWidget = nullptr;
    }

    if (HUDWidget)
    {
        HUDWidget->RemoveFromParent();
        HUDWidget = nullptr;
    }

    Super::Deinitialize();
}

bool UMOBAMMOGameUISubsystem::Tick(float DeltaTime)
{
#if UE_BUILD_SHIPPING
    return false;
#else
    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance)
    {
        return true;
    }

    UWorld* World = GameInstance->GetWorld();
    if (!CanCreateWidgets(World))
    {
        return true;
    }

    ULocalPlayer* LocalPlayer = GameInstance->GetFirstGamePlayer();
    if (!LocalPlayer)
    {
        return true;
    }

    APlayerController* PlayerController = LocalPlayer->GetPlayerController(World);
    if (!PlayerController)
    {
        return true;
    }

    EnsureWidgets(PlayerController);
    ReconcileCommandLineAutoSession();
    ReconcileSessionState(PlayerController);
    UpdateWidgetVisibility(PlayerController);
    return true;
#endif
}

bool UMOBAMMOGameUISubsystem::CanCreateWidgets(UWorld* World) const
{
    if (!World)
    {
        return false;
    }

    if (!World->IsGameWorld())
    {
        return false;
    }

    return World->GetNetMode() != NM_DedicatedServer;
}

void UMOBAMMOGameUISubsystem::EnsureWidgets(APlayerController* PlayerController)
{
    auto ResetIfStale = [PlayerController](auto& Widget)
    {
        if (!Widget)
        {
            return;
        }

        if (Widget->GetOwningPlayer() != PlayerController || !Widget->IsInViewport())
        {
            Widget->RemoveFromParent();
            Widget = nullptr;
        }
    };

    ResetIfStale(LoginWidget);
    ResetIfStale(CharacterSelectWidget);
    ResetIfStale(LoadingWidget);

    if (HUDWidget && (HUDWidget->GetOwningPlayer() != PlayerController || !HUDWidget->IsInViewport() || !HUDWidget->IsAbilityBarReady()))
    {
        HUDWidget->RemoveFromParent();
        HUDWidget = nullptr;
    }

    if (!LoginWidget)
    {
        LoginWidget = CreateWidget<UMOBAMMOLoginScreenWidget>(PlayerController, ResolveLoginWidgetClass());
        if (LoginWidget)
        {
            LoginWidget->AddToViewport(7000);
        }
    }

    if (!CharacterSelectWidget)
    {
        CharacterSelectWidget = CreateWidget<UMOBAMMOCharacterSelectWidget>(PlayerController, ResolveCharacterSelectWidgetClass());
        if (CharacterSelectWidget)
        {
            CharacterSelectWidget->AddToViewport(7500);
        }
    }

    if (!LoadingWidget)
    {
        LoadingWidget = CreateWidget<UMOBAMMOLoadingScreenWidget>(PlayerController, ResolveLoadingWidgetClass());
        if (LoadingWidget)
        {
            LoadingWidget->AddToViewport(8500);
        }
    }

    if (!HUDWidget)
    {
        HUDWidget = CreateWidget<UMOBAMMOGameHUDWidget>(PlayerController, ResolveHUDWidgetClass());
        if (HUDWidget)
        {
            HUDWidget->AddToViewport(2000);
        }
    }
}

void UMOBAMMOGameUISubsystem::ReconcileSessionState(APlayerController* PlayerController)
{
    if (!PlayerController)
    {
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UMOBAMMOBackendSubsystem* BackendSubsystem = GameInstance ? GameInstance->GetSubsystem<UMOBAMMOBackendSubsystem>() : nullptr;
    if (!BackendSubsystem)
    {
        return;
    }

    UWorld* World = PlayerController->GetWorld();
    if (!World || World->GetNetMode() != NM_Client)
    {
        return;
    }

    if (!PlayerController->GetPawn())
    {
        return;
    }

    BackendSubsystem->NotifyClientEnteredSessionWorld();
}

void UMOBAMMOGameUISubsystem::ReconcileCommandLineAutoSession()
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("MOBAMMOAutoSession")))
    {
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UMOBAMMOBackendSubsystem* BackendSubsystem = GameInstance ? GameInstance->GetSubsystem<UMOBAMMOBackendSubsystem>() : nullptr;
    if (!BackendSubsystem)
    {
        return;
    }

    UWorld* World = GameInstance->GetWorld();
    if (!World || World->GetNetMode() == NM_Client)
    {
        return;
    }

    FString AutoUsername = TEXT("mimet_bp_test");
    FParse::Value(FCommandLine::Get(), TEXT("MOBAMMOAutoUser="), AutoUsername);

    FString AutoCharacterName = TEXT("BPHero");
    FParse::Value(FCommandLine::Get(), TEXT("MOBAMMOAutoCharacter="), AutoCharacterName);

    if (!bAutoSessionLoginRequested && BackendSubsystem->GetLoginStatus() == TEXT("Idle"))
    {
        bAutoSessionLoginRequested = true;
        BackendSubsystem->MockLogin(AutoUsername);
        return;
    }

    if (bAutoSessionStartRequested || BackendSubsystem->GetCharacterListStatus() != TEXT("Loaded"))
    {
        return;
    }

    const TArray<FBackendCharacterResult> Characters = BackendSubsystem->GetCachedCharacters();
    const FBackendCharacterResult* SelectedCharacter = Characters.FindByPredicate([&AutoCharacterName](const FBackendCharacterResult& Character)
    {
        return Character.Name.Equals(AutoCharacterName, ESearchCase::IgnoreCase);
    });

    if (!SelectedCharacter && Characters.Num() > 0)
    {
        SelectedCharacter = &Characters[0];
    }

    if (!SelectedCharacter)
    {
        return;
    }

    bAutoSessionStartRequested = true;
    BackendSubsystem->SelectCharacter(SelectedCharacter->CharacterId);
    BackendSubsystem->StartSessionForSelectedCharacter();
}

void UMOBAMMOGameUISubsystem::UpdateWidgetVisibility(APlayerController* PlayerController)
{
    const bool bLoadingVisible = LoadingWidget && LoadingWidget->ShouldBeVisible();
    const bool bLoginVisible = LoginWidget && LoginWidget->ShouldBeVisible();
    const bool bCharacterSelectVisible = CharacterSelectWidget && CharacterSelectWidget->ShouldBeVisible();
    const bool bHUDVisible = HUDWidget && HUDWidget->ShouldBeVisible();

    if (LoginWidget)
    {
        LoginWidget->RefreshFromBackend();
    }

    if (CharacterSelectWidget)
    {
        CharacterSelectWidget->RefreshFromBackend();
    }

    if (LoadingWidget)
    {
        LoadingWidget->RefreshFromBackend();
    }

    if (HUDWidget)
    {
        HUDWidget->RefreshFromBackend();
    }

    const bool bNeedsCursor = bLoginVisible || bLoadingVisible || bCharacterSelectVisible;
    PlayerController->SetShowMouseCursor(bNeedsCursor);

    if (bNeedsCursor)
    {
        FInputModeGameAndUI InputMode;
        InputMode.SetHideCursorDuringCapture(false);
        PlayerController->SetInputMode(InputMode);
    }
    else
    {
        FInputModeGameOnly InputMode;
        PlayerController->SetInputMode(InputMode);
    }
}

TSubclassOf<UMOBAMMOLoginScreenWidget> UMOBAMMOGameUISubsystem::ResolveLoginWidgetClass() const
{
    // Force C++ Native UI execution because WBP_LoginScreen has old WebBrowser bindings
    return UMOBAMMOLoginScreenWidget::StaticClass();
}

TSubclassOf<UMOBAMMOLoadingScreenWidget> UMOBAMMOGameUISubsystem::ResolveLoadingWidgetClass() const
{
    // Force C++ Native UI execution
    return UMOBAMMOLoadingScreenWidget::StaticClass();
}

TSubclassOf<UMOBAMMOGameHUDWidget> UMOBAMMOGameUISubsystem::ResolveHUDWidgetClass() const
{
    // Force C++ Native UI execution because dynamically overriding BP WidgetTree breaks Slate rendering
    return UMOBAMMOGameHUDWidget::StaticClass();
}

TSubclassOf<UMOBAMMOCharacterSelectWidget> UMOBAMMOGameUISubsystem::ResolveCharacterSelectWidgetClass() const
{
    // Force C++ Native UI execution because WBP_CharacterSelect has old WebBrowser bindings
    return UMOBAMMOCharacterSelectWidget::StaticClass();
}
