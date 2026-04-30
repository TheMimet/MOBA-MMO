#pragma once

#include "CoreMinimal.h"
#include "MOBAMMOVendorCatalog.generated.h"

USTRUCT(BlueprintType)
struct MOBAMMO_API FMOBAMMOVendorItem
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="MOBAMMO|Vendor")
    FString ItemId;

    UPROPERTY(BlueprintReadOnly, Category="MOBAMMO|Vendor")
    FString DisplayName;

    UPROPERTY(BlueprintReadOnly, Category="MOBAMMO|Vendor")
    int32 Price = 0;
};

/** Static catalog of items sold by the in-world vendor. */
UCLASS()
class MOBAMMO_API UMOBAMMOVendorCatalog : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="MOBAMMO|Vendor")
    static const TArray<FMOBAMMOVendorItem>& GetCatalog();

    UFUNCTION(BlueprintPure, Category="MOBAMMO|Vendor")
    static bool FindItem(const FString& ItemId, FMOBAMMOVendorItem& OutItem);
};
