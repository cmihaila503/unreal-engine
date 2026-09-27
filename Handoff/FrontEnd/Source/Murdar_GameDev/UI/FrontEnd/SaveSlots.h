// Save slots as the menus show them: what is in each file, read without loading it (UNarrativeStateSubsystem::PeekSave).

#pragma once

#include "CoreMinimal.h"
#include "UI/FrontEnd/FrontEndRules.h"

namespace MurdarSlots
{
	struct FEntry
	{
		FName Slot;
		FText Title;              // "Slot 1", "Automat"
		MurdarFrontEnd::FSlot Info;
		FText Label() const { return FText::FromString(UTF8_TO_TCHAR(MurdarFrontEnd::Label(Info, TCHAR_TO_UTF8(*Title.ToString())).c_str())); }
	};

	/** Manual slots first, then the autosave. */
	MURDAR_GAMEDEV_API TArray<FEntry> Gather(bool bIncludeAuto);
	/** The newest existing save, NAME_None if there is none. */
	MURDAR_GAMEDEV_API FName ContinueSlot();
}
