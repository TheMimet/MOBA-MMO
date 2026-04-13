#include "MOBAMMODebugOverlaySubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "MOBAMMOCharacterFlowWidget.h"
#include "MOBAMMODebugLoginWidget.h"
#include "UObject/SoftObjectPath.h"

void UMOBAMMODebugOverlaySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

#if !UE_BUILD_SHIPPING
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UMOBAMMODebugOverlaySubsystem::Tick),
        0.25f
    );
#endif
}

void UMOBAMMODebugOverlaySubsystem::Deinitialize()
{
#if !UE_BUILD_SHIPPING
    if (TickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
        TickHandle.Reset();
    }
#endif

    if (DebugWidget)
    {
        DebugWidget->RemoveFromParent();
        DebugWidget = nullptr;
    }

    if (CharacterFlowWidget)
    {
        CharacterFlowWidget->RemoveFromParent();
        CharacterFlowWidget = nullptr;
    }

    Super::Deinitialize();
}

bool UMOBAMMODebugOverlaySubsystem::Tick(float DeltaTime)
{
#if UE_BUILD_SHIPPING
    return false;
#else
    if (DebugWidget && DebugWidget->IsInViewport() && ShouldShowDebugPanel())
    {
        return true;
    }

    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance)
    {
        return true;
    }

    UWorld* World = GameInstance->GetWorld();
    if (!CanCreateDebugWidget(World))
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

    EnsureCharacterFlowWidget(PlayerController);

    if (ShouldShowDebugPanel())
    {
        const TSubclassOf<UMOBAMMODebugLoginWidget> DebugWidgetClass = ResolveDebugWidgetClass();
        if (!DebugWidget)
        {
            DebugWidget = CreateWidget<UMOBAMMODebugLoginWidget>(PlayerController, DebugWidgetClass);
            if (DebugWidget)
            {
                DebugWidget->AddToViewport(10000);
                DebugWidget->SetPositionInViewport(FVector2D(24.0f, 24.0f), false);
            }
        }
    }
    else if (DebugWidget)
    {
        DebugWidget->SetVisibility(ESlateVisibility::Collapsed);
    }

    return true;
#endif
}

bool UMOBAMMODebugOverlaySubsystem::CanCreateDebugWidget(UWorld* World) const
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

bool UMOBAMMODebugOverlaySubsystem::ShouldShowDebugPanel() const
{
    return FParse::Param(FCommandLine::Get(), TEXT("ShowDebugPanel"));
}

TSubclassOf<UMOBAMMODebugLoginWidget> UMOBAMMODebugOverlaySubsystem::ResolveDebugWidgetClass() const
{
    const FSoftClassPath BlueprintWidgetClassPath(TEXT("/Game/WBP_DebugLogin.WBP_DebugLogin_C"));
    if (UClass* LoadedClass = BlueprintWidgetClassPath.TryLoadClass<UMOBAMMODebugLoginWidget>())
    {
        return LoadedClass;
    }

    return UMOBAMMODebugLoginWidget::StaticClass();
}

TSubclassOf<UMOBAMMOCharacterFlowWidget> UMOBAMMODebugOverlaySubsystem::ResolveCharacterFlowWidgetClass() const
{
    const FSoftClassPath BlueprintWidgetClassPath(TEXT("/Game/WBP_CharacterFlow.WBP_CharacterFlow_C"));
    if (UClass* LoadedClass = BlueprintWidgetClassPath.TryLoadClass<UMOBAMMOCharacterFlowWidget>())
    {
        return LoadedClass;
    }

    return UMOBAMMOCharacterFlowWidget::StaticClass();
}

bool UMOBAMMODebugOverlaySubsystem::EnsureCharacterFlowWidget(APlayerController* PlayerController)
{
    if (!PlayerController)
    {
        return false;
    }

    if (!CharacterFlowWidget)
    {
        CharacterFlowWidget = CreateWidget<UMOBAMMOCharacterFlowWidget>(PlayerController, ResolveCharacterFlowWidgetClass());
        if (CharacterFlowWidget)
        {
            CharacterFlowWidget->AddToViewport(9000);
        }
    }

    if (!CharacterFlowWidget)
    {
        return false;
    }

    CharacterFlowWidget->RefreshFromBackend();

    const bool bShouldBeVisible = CharacterFlowWidget->ShouldBeVisible();
    CharacterFlowWidget->SetVisibility(bShouldBeVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (DebugWidget)
    {
        DebugWidget->SetVisibility(bShouldBeVisible ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    }

    if (bShouldBeVisible && !bCharacterFlowInputApplied)
    {
        PlayerController->SetShowMouseCursor(true);
        FInputModeGameAndUI InputMode;
        InputMode.SetHideCursorDuringCapture(false);
        PlayerController->SetInputMode(InputMode);
        bCharacterFlowInputApplied = true;
    }
    else if (!bShouldBeVisible && bCharacterFlowInputApplied)
    {
        PlayerController->SetShowMouseCursor(false);
        FInputModeGameOnly InputMode;
        PlayerController->SetInputMode(InputMode);
        bCharacterFlowInputApplied = false;
    }

    return true;
}
