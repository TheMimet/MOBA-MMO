#include "MOBAMMOGameInstance.h"

#include "Modules/ModuleManager.h"

void UMOBAMMOGameInstance::Init()
{
    PreloadGameplayModulesForIris();
    Super::Init();
}

void UMOBAMMOGameInstance::PreloadGameplayModulesForIris() const
{
    static const FName ModulesToPreload[] = {
        TEXT("MOBAMMOAbilities"),
        TEXT("MOBAMMOAI"),
        TEXT("GameplayTags"),
        TEXT("GameplayTasks"),
        TEXT("GameplayStateTreeModule"),
        TEXT("AutomationWorker"),
        TEXT("AutomationController"),
        TEXT("PerfCounters"),
    };

    FModuleManager& ModuleManager = FModuleManager::Get();
    for (const FName ModuleName : ModulesToPreload)
    {
        const FString ModuleNameString = ModuleName.ToString();
        if (ModuleManager.ModuleExists(*ModuleNameString) && !ModuleManager.IsModuleLoaded(ModuleName))
        {
            ModuleManager.LoadModule(ModuleName);
        }
    }
}
