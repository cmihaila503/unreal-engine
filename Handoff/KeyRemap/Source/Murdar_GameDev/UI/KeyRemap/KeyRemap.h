// Key remapping through Enhanced Input's user settings (UE 5.3+): the player-mappable mappings of the listed contexts,
// their current keys, rebinding with MurdarKeys::Rebind (conflicts in the same context are swapped), saved by the
// engine per user. The pause menu's settings page shows the rows (README §Patches).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "InputCoreTypes.h"
#include "UI/KeyRemap/KeyRemapRules.h"
#include "KeyRemap.generated.h"

class APlayerController;
class UInputMappingContext;

USTRUCT(BlueprintType)
struct FRemapContext
{
	GENERATED_BODY()
	/** „Pe jos”, „La volan” — also the context name for conflicts. */
	UPROPERTY(EditAnywhere, config, Category = "Keys") FText Name;
	UPROPERTY(EditAnywhere, config, Category = "Keys") TSoftObjectPtr<UInputMappingContext> Context;
};

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Murdar Key Remap"))
class MURDAR_GAMEDEV_API UKeyRemapSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	/** The contexts whose player-mappable keys appear in the menu. ADAPT: the GASP IMC(s) and the vehicle IMC. */
	UPROPERTY(EditAnywhere, config, Category = "Keys") TArray<FRemapContext> Contexts;
	/** Never rebindable (the pause keys). */
	UPROPERTY(EditAnywhere, config, Category = "Keys") TArray<FKey> Reserved = { EKeys::Escape, EKeys::Gamepad_Special_Right };
};

namespace MurdarKeyRemap
{
	struct FRow
	{
		FName MappingName;
		FText DisplayName;
		MurdarKeys::ESlot Slot = MurdarKeys::ESlot::KeyboardMouse;
		FKey Key;
		FText ContextName;
	};

	/** Registers the contexts with the user settings (once) and lists the mappable rows. */
	MURDAR_GAMEDEV_API TArray<FRow> Gather(APlayerController* PC);
	/** Rebind Rows[Index] to Key: the rules decide (swap on clash); the engine is told and saves. Returns the result. */
	MURDAR_GAMEDEV_API MurdarKeys::EResult Rebind(APlayerController* PC, const TArray<FRow>& Rows, int32 Index, const FKey& Key);
	MURDAR_GAMEDEV_API void ResetToDefaults(APlayerController* PC);
}
