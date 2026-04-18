#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AbilityDataAsset.generated.h"

class UAbilityBase;

UCLASS(BlueprintType)
class MOBAMMOABILITIES_API UAbilityDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Abilities")
    TArray<TSubclassOf<UAbilityBase>> Abilities;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Config")
    int32 StartingMana = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Config")
    int32 MaxMana = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Config")
    float ManaRegenRate = 5.0f;
};
