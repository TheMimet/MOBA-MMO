#pragma once

#include "CoreMinimal.h"
#include "MOBAMMOQuestTypes.h"
#include "MOBAMMOQuestCatalog.generated.h"

// ─────────────────────────────────────────────────────────────
// Static catalog of all available quest definitions.
// Quests assigned to a new player session come from
// GetDefaultSessionQuestIds(). No instances needed — all methods
// are static BlueprintPure helpers.
// ─────────────────────────────────────────────────────────────
UCLASS()
class MOBAMMO_API UMOBAMMOQuestCatalog : public UObject
{
    GENERATED_BODY()

public:
    // Full list of all known quest definitions.
    UFUNCTION(BlueprintPure, Category="Quest")
    static const TArray<FMOBAMMOQuestDefinition>& GetAllQuests();

    // Looks up a single quest by ID. Returns false if not found.
    UFUNCTION(BlueprintPure, Category="Quest")
    static bool FindQuest(const FString& QuestId, FMOBAMMOQuestDefinition& OutDef);

    // IDs of the quests automatically assigned when a player joins a session.
    UFUNCTION(BlueprintPure, Category="Quest")
    static TArray<FString> GetDefaultSessionQuestIds();
};
