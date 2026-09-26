# Cheats for UDirectorCheats: MurdarCivilians, MurdarCivMax, MurdarCivDensity (compiled out of shipping like the others)

Header (with the other `UFUNCTION(Exec)` declarations):

```cpp
/** Civilian population stats. */
UFUNCTION(Exec) void MurdarCivilians();

/** Overrides the civilian budget (negative clears the override). Separate from MurdarCivilians: an optional Exec
 *  argument isn't reliably defaulted by the console parser. */
UFUNCTION(Exec) void MurdarCivMax(int32 Max);

/** Scales every zone's civilian density (1 = as configured, 0 = empty streets). */
UFUNCTION(Exec) void MurdarCivDensity(float Scale);
```

Source (ADAPT the logging/printing helper the other cheats use, e.g. how `MurdarFactions` prints):

```cpp
#include "AI/Civilians/CivilianPopulationSubsystem.h"

void UDirectorCheats::MurdarCivMax(int32 Max)
{
	if (UCivilianPopulationSubsystem* Pop = GetWorld() ? GetWorld()->GetSubsystem<UCivilianPopulationSubsystem>() : nullptr)
	{
		Pop->SetMaxOverride(Max);
	}
	MurdarCivilians();
}

void UDirectorCheats::MurdarCivilians()
{
	UCivilianPopulationSubsystem* Pop = GetWorld() ? GetWorld()->GetSubsystem<UCivilianPopulationSubsystem>() : nullptr;
	if (!Pop)
	{
		return;
	}
	const FCivilianPopulationStats S = Pop->GetStats();
	const FString Line = FString::Printf(
		TEXT("Civilians: active %d/%d pinned %d pooled %d | new %d reused %d despawned %d | rejected vis %d nav %d crowd %d | failed %d | density x%.2f"),
		S.Active, S.Target, S.Pinned, S.Pooled, S.SpawnedNew, S.ReusedFromPool, S.Despawned,
		S.RejectedVisible, S.RejectedNav, S.RejectedCrowded, S.SpawnFailed, Pop->GetDensityScale());
	// ADAPT: same output as the other cheats (console + screen)
	UE_LOG(LogTemp, Display, TEXT("%s"), *Line);
}

void UDirectorCheats::MurdarCivDensity(float Scale)
{
	if (UCivilianPopulationSubsystem* Pop = GetWorld() ? GetWorld()->GetSubsystem<UCivilianPopulationSubsystem>() : nullptr)
	{
		Pop->SetDensityScale(Scale);
	}
}
```

Add both to the cheat list in `Docs/PROJECT_OVERVIEW.md` §8, and `Murdar.Civ.Debug` to the cvar list.
