#include "UI/FrontEnd/SaveSlots.h"

#include "UI/FrontEnd/FrontEndSettings.h"
#include "Director/ChapterDefinition.h"
#include "Director/NarrativeState.h"
#include "Director/NarrativeStateSubsystem.h"

namespace
{
	MurdarFrontEnd::FSlot Read(FName Slot)
	{
		MurdarFrontEnd::FSlot S;
		S.Name = TCHAR_TO_UTF8(*Slot.ToString());
		FNarrativeState State;
		if (!UNarrativeStateSubsystem::PeekSave(Slot, State)) { return S; } // README §Patches 1
		S.bExists = true;
		S.SavedAt = State.SavedAt.GetTicks();
		const UChapterDefinition* Chapter = Cast<UChapterDefinition>(State.Chapter.TryLoad());
		S.Chapter = Chapter && !Chapter->DisplayName.IsEmpty() ? TCHAR_TO_UTF8(*Chapter->DisplayName.ToString()) : "";
		auto Value = [&State](const TCHAR* Name, float Default)
		{
			const float* V = State.Values.Find(FGameplayTag::RequestGameplayTag(FName(Name), false));
			return V ? *V : Default;
		};
		S.Day = FMath::RoundToInt(Value(TEXT("Stat.Day"), 0.f));
		S.Minutes = Value(TEXT("Stat.TimeOfDay"), -1.f);
		S.Money = Value(TEXT("Stat.Money"), 0.f);
		return S;
	}
}

TArray<MurdarSlots::FEntry> MurdarSlots::Gather(bool bIncludeAuto)
{
	const UFrontEndSettings* Settings = GetDefault<UFrontEndSettings>();
	TArray<FEntry> Out;
	for (int32 i = 0; i < Settings->Slots.Num(); ++i)
	{
		Out.Add({ Settings->Slots[i], FText::Format(NSLOCTEXT("MurdarFrontEnd", "Slot", "Slot {0}"), i + 1), Read(Settings->Slots[i]) });
	}
	if (bIncludeAuto) { Out.Add({ Settings->AutoSlot, NSLOCTEXT("MurdarFrontEnd", "Auto", "Automat"), Read(Settings->AutoSlot) }); }
	return Out;
}

FName MurdarSlots::ContinueSlot()
{
	const TArray<FEntry> All = Gather(true);
	std::vector<MurdarFrontEnd::FSlot> Infos;
	for (const FEntry& E : All) { Infos.push_back(E.Info); }
	const int I = MurdarFrontEnd::ContinueSlot(Infos);
	return I >= 0 ? All[I].Slot : NAME_None;
}
