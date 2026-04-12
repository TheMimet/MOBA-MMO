#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "MOBAMMODebugOverlaySubsystem.generated.h"

class UMOBAMMODebugLoginWidget;
class UMOBAMMOCharacterFlowWidget;

UCLASS()
class MOBAMMO_API UMOBAMMODebugOverlaySubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

private:
    bool Tick(float DeltaTime);
    bool CanCreateDebugWidget(UWorld* World) const;
    bool EnsureCharacterFlowWidget(APlayerController* PlayerController);
    TSubclassOf<UMOBAMMOCharacterFlowWidget> ResolveCharacterFlowWidgetClass() const;
    TSubclassOf<UMOBAMMODebugLoginWidget> ResolveDebugWidgetClass() const;

    FTSTicker::FDelegateHandle TickHandle;

    UPROPERTY(Transient)
    TObjectPtr<UMOBAMMODebugLoginWidget> DebugWidget;

    UPROPERTY(Transient)
    TObjectPtr<UMOBAMMOCharacterFlowWidget> CharacterFlowWidget;

    bool bCharacterFlowInputApplied = false;
};
