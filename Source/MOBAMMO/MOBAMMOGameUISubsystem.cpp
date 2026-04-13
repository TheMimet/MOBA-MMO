#include "MOBAMMOGameUISubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "MOBAMMOBackendSubsystem.h"
#include "MOBAMMOGameHUDWidget.h"
#include "MOBAMMOLoginScreenWidget.h"
#include "MOBAMMOLoadingScreenWidget.h"
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
    if (!LoginWidget)
    {
        LoginWidget = CreateWidget<UMOBAMMOLoginScreenWidget>(PlayerController, ResolveLoginWidgetClass());
        if (LoginWidget)
        {
            LoginWidget->AddToViewport(7000);
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
            HUDWidget->SetPositionInViewport(FVector2D(24.0f, 24.0f), false);
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

void UMOBAMMOGameUISubsystem::UpdateWidgetVisibility(APlayerController* PlayerController)
{
    const bool bLoadingVisible = LoadingWidget && LoadingWidget->ShouldBeVisible();
    const bool bLoginVisible = LoginWidget && LoginWidget->ShouldBeVisible();
    const bool bHUDVisible = HUDWidget && HUDWidget->ShouldBeVisible();
    const UMOBAMMOBackendSubsystem* BackendSubsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMOBAMMOBackendSubsystem>() : nullptr;
    const bool bCharacterFlowVisible = BackendSubsystem && BackendSubsystem->IsWaitingForCharacterSelection();

    if (LoginWidget)
    {
        LoginWidget->RefreshFromBackend();
    }

    if (LoadingWidget)
    {
        LoadingWidget->RefreshFromBackend();
    }

    if (HUDWidget)
    {
        HUDWidget->RefreshFromBackend();
    }

    const bool bNeedsCursor = bLoginVisible || bLoadingVisible || bCharacterFlowVisible;
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
    const FSoftClassPath BlueprintWidgetClassPath(TEXT("/Game/WBP_LoginScreen.WBP_LoginScreen_C"));
    if (UClass* LoadedClass = BlueprintWidgetClassPath.TryLoadClass<UMOBAMMOLoginScreenWidget>())
    {
        return LoadedClass;
    }

    return UMOBAMMOLoginScreenWidget::StaticClass();
}

TSubclassOf<UMOBAMMOLoadingScreenWidget> UMOBAMMOGameUISubsystem::ResolveLoadingWidgetClass() const
{
    const FSoftClassPath BlueprintWidgetClassPath(TEXT("/Game/WBP_LoadingScreen.WBP_LoadingScreen_C"));
    if (UClass* LoadedClass = BlueprintWidgetClassPath.TryLoadClass<UMOBAMMOLoadingScreenWidget>())
    {
        return LoadedClass;
    }

    return UMOBAMMOLoadingScreenWidget::StaticClass();
}

TSubclassOf<UMOBAMMOGameHUDWidget> UMOBAMMOGameUISubsystem::ResolveHUDWidgetClass() const
{
    const FSoftClassPath BlueprintWidgetClassPath(TEXT("/Game/WBP_GameHUD.WBP_GameHUD_C"));
    if (UClass* LoadedClass = BlueprintWidgetClassPath.TryLoadClass<UMOBAMMOGameHUDWidget>())
    {
        return LoadedClass;
    }

    return UMOBAMMOGameHUDWidget::StaticClass();
}
