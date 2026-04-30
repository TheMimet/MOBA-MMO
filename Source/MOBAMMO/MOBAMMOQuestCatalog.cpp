#include "MOBAMMOQuestCatalog.h"

// ─────────────────────────────────────────────────────────────
// 5 session quests covering the core gameplay loops.
// Initialized once via a static lambda.
// ─────────────────────────────────────────────────────────────
const TArray<FMOBAMMOQuestDefinition>& UMOBAMMOQuestCatalog::GetAllQuests()
{
    static const TArray<FMOBAMMOQuestDefinition> Catalog = []()
    {
        TArray<FMOBAMMOQuestDefinition> Out;

        auto Add = [&Out](const TCHAR* Id, const TCHAR* Name,
                          EMOBAMMOQuestType Type, int32 Target,
                          int32 XP, int32 Gold)
        {
            FMOBAMMOQuestDefinition Q;
            Q.QuestId     = Id;
            Q.DisplayName = Name;
            Q.Type        = Type;
            Q.TargetCount = Target;
            Q.RewardXP    = XP;
            Q.RewardGold  = Gold;
            Out.Add(MoveTemp(Q));
        };

        //             ID                    DisplayName              Type                               Target  XP   Gold
        Add(TEXT("first_strike"),  TEXT("First Strike"),   EMOBAMMOQuestType::KillTrainingDummy,    1,   25,   5);
        Add(TEXT("minion_hunter"), TEXT("Minion Hunter"),  EMOBAMMOQuestType::KillTrainingMinion,   3,   80,  15);
        Add(TEXT("war_path"),      TEXT("Warrior's Path"), EMOBAMMOQuestType::DealDamage,         300,   50,  10);
        Add(TEXT("merchant"),      TEXT("Merchant's Favor"),EMOBAMMOQuestType::BuyFromVendor,       1,   40,   0);
        Add(TEXT("spell_practice"),TEXT("Arcane Practice"),EMOBAMMOQuestType::UseAbility,          10,   60,  10);

        return Out;
    }();

    return Catalog;
}

bool UMOBAMMOQuestCatalog::FindQuest(const FString& QuestId, FMOBAMMOQuestDefinition& OutDef)
{
    for (const FMOBAMMOQuestDefinition& Q : GetAllQuests())
    {
        if (Q.QuestId == QuestId)
        {
            OutDef = Q;
            return true;
        }
    }
    return false;
}

TArray<FString> UMOBAMMOQuestCatalog::GetDefaultSessionQuestIds()
{
    TArray<FString> Ids;
    Ids.Reserve(GetAllQuests().Num());
    for (const FMOBAMMOQuestDefinition& Q : GetAllQuests())
    {
        Ids.Add(Q.QuestId);
    }
    return Ids;
}
