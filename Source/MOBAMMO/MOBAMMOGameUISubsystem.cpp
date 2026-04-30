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
#include "MOBAMMOPauseMenuWidget.h"
#include "MOBAMMOLoadingScreenWidget.h"
#include "MOBAMMOMainMenuWidget.h"
#include "MOBAMMOCharacterSelectWidget.h"
#include "MOBAMMOVendorWidget.h"
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

    if (MainMenuWidget)
    {
        MainMenuWidget->RemoveFromParent();
        MainMenuWidget = nullptr;
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

    if (PauseMenuWidget)
    {
        PauseMenuWidget->RemoveFromParent();
        PauseMenuWidget = nullptr;
    }

    if (VendorWidget)
    {
        VendorWidget->RemoveFromParent();
        VendorWidget = nullptr;
    }

    Super::Deinitialize();
}

void UMOBAMMOGameUISubsystem::ToggleInventory()
{
    if (HUDWidget)
    {
        HUDWidget->ToggleInventory();
    }
}

void UMOBAMMOGameUISubsystem::OpenChat()
{
    if (HUDWidget)
    {
        HUDWidget->OpenChat();
    }
}

void UMOBAMMOGameUISubsystem::CloseChat()
{
    if (HUDWidget)
    {
        HUDWidget->CloseChat();
    }
}

bool UMOBAMMOGameUISubsystem::IsChatOpen() const
{
    return HUDWidget && HUDWidget->IsChatOpen();
}

void UMOBAMMOGameUISubsystem::TogglePauseMenu()
{
    if (!PauseMenuWidget)
    {
        APlayerController* PC = nullptr;
        if (const UGameInstance* GI = GetGameInstance())
        {
            if (const UWorld* World = GI->GetWorld())
            {
                PC = World->GetFirstPlayerController();
            }
        }
        if (!PC) return;

        PauseMenuWidget = CreateWidget<UMOBAMMOPauseMenuWidget>(PC, UMOBAMMOPauseMenuWidget::StaticClass());
        if (PauseMenuWidget)
        {
            PauseMenuWidget->AddToViewport(3000);
        }
    }

    if (PauseMenuWidget)
    {
        PauseMenuWidget->ToggleVisibility();
    }
}

bool UMOBAMMOGameUISubsystem::IsPauseMenuVisible() const
{
    return PauseMenuWidget && PauseMenuWidget->IsMenuVisible();
}

void UMOBAMMOGameUISubsystem::ToggleVendor()
{
    if (IsVendorOpen())
    {
        CloseVendor();
    }
    else
    {
        OpenVendor();
    }
}

void UMOBAMMOGameUISubsystem::OpenVendor()
{
    if (!VendorWidget)
    {
        APlayerController* PC = nullptr;
        if (const UGameInstance* GI = GetGameInstance())
        {
            if (const UWorld* World = GI->GetWorld())
            {
                PC = World->GetFirstPlayerController();
            }
        }
        if (!PC) { return; }

        VendorWidget = CreateWidget<UMOBAMMOVendorWidget>(PC, UMOBAMMOVendorWidget::StaticClass());
        if (VendorWidget)
        {
            VendorWidget->AddToViewport(2500);
        }
    }

    if (VendorWidget)
    {
        VendorWidget->SetVendorVisible(true);
    }
}

void UMOBAMMOGameUISubsystem::CloseVendor()
{
    if (VendorWidget)
    {
        VendorWidget->SetVendorVisible(false);
    }
}

bool UMOBAMMOGameUISubsystem::IsVendorOpen() const
{
    return VendorWidget && VendorWidget->IsVendorVisible();
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
    ResetIfStale(MainMenuWidget);
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

    if (!MainMenuWidget)
    {
        MainMenuWidget = CreateWidget<UMOBAMMOMainMenuWidget>(PlayerController, ResolveMainMenuWidgetClass());
        if (MainMenuWidget)
        {
            MainMenuWidget->AddToViewport(7600);
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
    if (!World || (World->GetNetMode() != NM_Client && World->GetNetMode() != NM_Standalone))
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
    const UWorld* World = PlayerController ? PlayerController->GetWorld() : nullptr;
    const bool bPlayableClientPawnReady = World
        && (World->GetNetMode() == NM_Client || World->GetNetMode() == NM_Standalone)
        && PlayerController
        && PlayerController->GetPawn();

    if (LoginWidget)
    {
        LoginWidget->RefreshFromBackend();
    }

    if (CharacterSelectWidget)
    {
        CharacterSelectWidget->RefreshFromBackend();
    }

    if (MainMenuWidget)
    {
        MainMenuWidget->RefreshFromBackend();
    }

    if (LoadingWidget)
    {
        LoadingWidget->RefreshFromBackend();
    }

    if (HUDWidget)
    {
        HUDWidget->RefreshFromBackend();
    }

    auto IsWidgetActuallyVisible = [](const UUserWidget* Widget)
    {
        return Widget
            && Widget->GetVisibility() != ESlateVisibility::Collapsed
            && Widget->GetVisibility() != ESlateVisibility::Hidden;
    };

    const bool bLoadingVisible = IsWidgetActuallyVisible(LoadingWidget);
    const bool bLoginVisible = IsWidgetActuallyVisible(LoginWidget);
    const bool bMainMenuVisible = IsWidgetActuallyVisible(MainMenuWidget);
    const bool bCharacterSelectVisible = IsWidgetActuallyVisible(CharacterSelectWidget);
    const bool bHUDVisible = IsWidgetActuallyVisible(HUDWidget);
    const bool bChatOpen = HUDWidget && HUDWidget->IsChatOpen();
    const bool bInventoryOpen = HUDWidget && HUDWidget->IsInventoryOpen();
    const bool bPauseMenuVisible = PauseMenuWidget && PauseMenuWidget->IsMenuVisible();
    const bool bVendorOpen = IsVendorOpen();

    const bool bNeedsMenuCursor = bLoginVisible || bLoadingVisible || bMainMenuVisible || bCharacterSelectVisible;
    const bool bNeedsGameplayCursor = bPlayableClientPawnReady && bHUDVisible;
    const bool bMenuOwnsInput = bLoginVisible || bLoadingVisible || bMainMenuVisible || bCharacterSelectVisible;
    const bool bBlockingUIOwnsInput = bMenuOwnsInput || bChatOpen || bInventoryOpen || bPauseMenuVisible || bVendorOpen;
    const bool bShouldShowCursor = bNeedsMenuCursor || bNeedsGameplayCursor || bChatOpen || bInventoryOpen || bPauseMenuVisible || bVendorOpen;
    PlayerController->SetShowMouseCursor(bShouldShowCursor);
    PlayerController->SetIgnoreMoveInput(bBlockingUIOwnsInput);
    PlayerController->SetIgnoreLookInput(bBlockingUIOwnsInput);

    if (bChatOpen)
    {
        // Chat owns keyboard focus while typing; do not stomp its focused input mode every tick.
        return;
    }

    if (bBlockingUIOwnsInput)
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

TSubclassOf<UMOBAMMOMainMenuWidget> UMOBAMMOGameUISubsystem::ResolveMainMenuWidgetClass() const
{
    return UMOBAMMOMainMenuWidget::StaticClass();
}
