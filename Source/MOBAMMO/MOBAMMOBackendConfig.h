#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "MOBAMMOBackendConfig.generated.h"

UCLASS(Config=Game, DefaultConfig, BlueprintType)
class MOBAMMO_API UMOBAMMOBackendConfig : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Backend")
    FString BackendBaseUrl = TEXT("http://127.0.0.1:3000");
};
