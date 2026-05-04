#include "MOBAMMOSpawnPoint.h"

AMOBAMMOSpawnPoint::AMOBAMMOSpawnPoint()
{
    PrimaryActorTick.bCanEverTick = false;
    // Spawn points exist only on the server; clients never need to know about
    // them.  Keeping bReplicates = false saves bandwidth on every level load.
    bReplicates = false;

#if WITH_EDITORONLY_DATA
    // Show a visible sprite/arrow in the editor so designers can see them.
    bIsEditorOnlyActor = false;
#endif
}
