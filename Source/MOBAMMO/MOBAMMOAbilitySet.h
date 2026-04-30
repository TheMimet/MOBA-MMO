#pragma once

#include "CoreMinimal.h"

enum class EMOBAMMOAbilityKind : uint8
{
    Damage,
    Heal,
    Utility,
    Respawn
};

struct FMOBAMMOAbilityDefinition
{
    int32 Slot = 0;
    FString KeyLabel;
    FString Name;
    FString Description;
    float ManaCost = 0.0f;
    float Cooldown = 0.0f;
    EMOBAMMOAbilityKind Kind = EMOBAMMOAbilityKind::Utility;
};

namespace MOBAMMOAbilitySet
{
    inline FMOBAMMOAbilityDefinition ArcBurst()
    {
        return {1, TEXT("1"), TEXT("Arc Burst"), TEXT("Single-target arc strike"), 10.0f, 2.0f, EMOBAMMOAbilityKind::Damage};
    }

    inline FMOBAMMOAbilityDefinition Renew()
    {
        return {2, TEXT("2"), TEXT("Renew"), TEXT("Self heal over a short cadence"), 8.0f, 4.0f, EMOBAMMOAbilityKind::Heal};
    }

    inline FMOBAMMOAbilityDefinition DrainPulse()
    {
        return {3, TEXT("3"), TEXT("Drain Pulse"), TEXT("Drain mana from your selected target"), 12.0f, 3.0f, EMOBAMMOAbilityKind::Utility};
    }

    inline FMOBAMMOAbilityDefinition ManaSurge()
    {
        return {4, TEXT("4"), TEXT("Mana Surge"), TEXT("Restore a burst of mana"), 0.0f, 6.0f, EMOBAMMOAbilityKind::Utility};
    }

    inline FMOBAMMOAbilityDefinition Reforge()
    {
        return {5, TEXT("5"), TEXT("Reforge"), TEXT("Respawn with full resources"), 0.0f, 0.0f, EMOBAMMOAbilityKind::Respawn};
    }

    inline FMOBAMMOAbilityDefinition WukongStaffSlam()
    {
        return {1, TEXT("1"), TEXT("Heavenly Slam"), TEXT("Leap into a heavy staff smash against the selected target"), 10.0f, 2.0f, EMOBAMMOAbilityKind::Damage};
    }

    inline FMOBAMMOAbilityDefinition WukongPrimalRenew()
    {
        return {2, TEXT("2"), TEXT("Primal Renewal"), TEXT("Recover health through Wukong's battle focus"), 8.0f, 4.0f, EMOBAMMOAbilityKind::Heal};
    }

    inline FMOBAMMOAbilityDefinition WukongSpiritDrain()
    {
        return {3, TEXT("3"), TEXT("Spirit Siphon"), TEXT("Steal spirit energy from the selected target"), 12.0f, 3.0f, EMOBAMMOAbilityKind::Utility};
    }

    inline FMOBAMMOAbilityDefinition WukongNimbusSurge()
    {
        return {4, TEXT("4"), TEXT("Nimbus Focus"), TEXT("Gather focus and restore a burst of mana"), 0.0f, 6.0f, EMOBAMMOAbilityKind::Utility};
    }

    inline FMOBAMMOAbilityDefinition WukongSageReforge()
    {
        return {5, TEXT("5"), TEXT("Sage Reforge"), TEXT("Return to battle with full resources"), 0.0f, 0.0f, EMOBAMMOAbilityKind::Respawn};
    }

    inline TArray<FMOBAMMOAbilityDefinition> ForClass(const FString& ClassId)
    {
        if (ClassId.Equals(TEXT("mage"), ESearchCase::IgnoreCase))
        {
            return {
                WukongStaffSlam(),
                WukongPrimalRenew(),
                WukongSpiritDrain(),
                WukongNimbusSurge(),
                WukongSageReforge()
            };
        }

        return {
            ArcBurst(),
            Renew(),
            DrainPulse(),
            ManaSurge(),
            Reforge()
        };
    }
}
