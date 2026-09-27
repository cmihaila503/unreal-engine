// Builds the paper map for the pause menu: gathers the marks (known places, the job's place, him) and makes the widget.

#pragma once

#include "CoreMinimal.h"

class SPaperMap;
class UWorld;

namespace MurdarPaperMap
{
	/** Null when no map texture is set (the menu then hides its "Harta" row). */
	MURDAR_GAMEDEV_API TSharedPtr<SPaperMap> Build(UWorld* World, FSimpleDelegate OnBack);
	MURDAR_GAMEDEV_API bool IsAvailable();
}
