#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"

#include "MOBAMMOGameInstance.generated.h"

UCLASS()
class MOBAMMO_API UMOBAMMOGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    virtual void Init() override;

private:
    void PreloadGameplayModulesForIris() const;
};
