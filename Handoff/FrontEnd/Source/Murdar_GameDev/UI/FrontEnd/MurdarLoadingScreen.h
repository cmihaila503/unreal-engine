// The loading screen between maps: dark, the title, a tip that isn't the last one shown. MoviePlayer, so it keeps
// animating while the map loads. Installed once from the game module's StartupModule (README §Patches 2).

#pragma once

#include "CoreMinimal.h"

namespace MurdarLoadingScreen
{
	MURDAR_GAMEDEV_API void Install();
}
