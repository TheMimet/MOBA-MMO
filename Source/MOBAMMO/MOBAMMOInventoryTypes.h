#pragma once

#include "CoreMinimal.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "MOBAMMOInventoryTypes.generated.h"

USTRUCT(BlueprintType)
struct MOBAMMO_API FMOBAMMOInventoryItem : public FFastArraySerializerItem
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MOBAMMO|Inventory")
    FString ItemId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MOBAMMO|Inventory")
    int32 Quantity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MOBAMMO|Inventory")
    int32 SlotIndex;

    FMOBAMMOInventoryItem()
        : ItemId(TEXT(""))
        , Quantity(1)
        , SlotIndex(-1)
    {
    }

    FMOBAMMOInventoryItem(const FString& InItemId, int32 InQuantity, int32 InSlotIndex)
        : ItemId(InItemId)
        , Quantity(InQuantity)
        , SlotIndex(InSlotIndex)
    {
    }
};

USTRUCT(BlueprintType)
struct MOBAMMO_API FMOBAMMOInventoryItemArray : public FFastArraySerializer
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MOBAMMO|Inventory")
    TArray<FMOBAMMOInventoryItem> Items;

    bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
    {
        return FFastArraySerializer::FastArrayDeltaSerialize<FMOBAMMOInventoryItem, FMOBAMMOInventoryItemArray>(Items, DeltaParms, *this);
    }
};

template<>
struct TStructOpsTypeTraits<FMOBAMMOInventoryItemArray> : public TStructOpsTypeTraitsBase2<FMOBAMMOInventoryItemArray>
{
    enum
    {
        WithNetDeltaSerializer = true,
    };
};
