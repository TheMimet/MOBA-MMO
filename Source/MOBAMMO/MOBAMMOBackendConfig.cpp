#include "MOBAMMOBackendConfig.h"

#include "HAL/PlatformMisc.h"

FString UMOBAMMOBackendConfig::ResolveSessionServerSecret() const
{
    FString EnvironmentSecret = FPlatformMisc::GetEnvironmentVariable(TEXT("MOBAMMO_SESSION_SERVER_SECRET")).TrimStartAndEnd();
    if (!EnvironmentSecret.IsEmpty())
    {
        return EnvironmentSecret;
    }

    EnvironmentSecret = FPlatformMisc::GetEnvironmentVariable(TEXT("SESSION_SERVER_SECRET")).TrimStartAndEnd();
    if (!EnvironmentSecret.IsEmpty())
    {
        return EnvironmentSecret;
    }

    return SessionServerSecret.TrimStartAndEnd();
}
