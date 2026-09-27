#include "UI/KeyRemap/KeyRemap.h"

#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "UserSettings/EnhancedInputUserSettings.h"   // ADAPT: 5.8 path of the user settings headers
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"

namespace
{
	UEnhancedInputUserSettings* UserSettings(APlayerController* PC)
	{
		const ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
		UEnhancedInputLocalPlayerSubsystem* Sub = LP ? LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
		// Needs Project Settings > Enhanced Input > "Enable User Settings" (README §Setup).
		return Sub ? Sub->GetUserSettings() : nullptr;
	}

	MurdarKeys::ESlot SlotOf(EPlayerMappableKeySlot S) { return S == EPlayerMappableKeySlot::Second ? MurdarKeys::ESlot::Gamepad : MurdarKeys::ESlot::KeyboardMouse; }
	EPlayerMappableKeySlot ToEngine(MurdarKeys::ESlot S) { return S == MurdarKeys::ESlot::Gamepad ? EPlayerMappableKeySlot::Second : EPlayerMappableKeySlot::First; }

	std::vector<MurdarKeys::FBinding> ToRules(const TArray<MurdarKeyRemap::FRow>& Rows)
	{
		std::vector<MurdarKeys::FBinding> Out;
		for (const MurdarKeyRemap::FRow& R : Rows)
		{
			Out.push_back({ TCHAR_TO_UTF8(*R.MappingName.ToString()), R.Slot, TCHAR_TO_UTF8(*R.Key.GetFName().ToString()), { TCHAR_TO_UTF8(*R.ContextName.ToString()) } });
		}
		return Out;
	}
}

TArray<MurdarKeyRemap::FRow> MurdarKeyRemap::Gather(APlayerController* PC)
{
	TArray<FRow> Out;
	UEnhancedInputUserSettings* Settings = UserSettings(PC);
	if (!Settings) { return Out; }
	UEnhancedPlayerMappableKeyProfile* Profile = Settings->GetActiveKeyProfile(); // ADAPT: GetCurrentKeyProfile() in 5.3
	for (const FRemapContext& C : GetDefault<UKeyRemapSettings>()->Contexts)
	{
		UInputMappingContext* IMC = C.Context.LoadSynchronous();
		if (!IMC || !Profile) { continue; }
		Settings->RegisterInputMappingContext(IMC);
		// The mappings of this context that are player-mappable, one row per slot (keyboard, pad).
		for (const FEnhancedActionKeyMapping& M : IMC->GetMappings())
		{
			if (!M.IsPlayerMappable()) { continue; }
			const FName Name = M.GetMappingName();
			if (const FKeyMappingRow* Row = Profile->FindKeyMappingRow(Name))
			{
				for (const FPlayerKeyMapping& P : Row->Mappings)
				{
					FRow R;
					R.MappingName = Name;
					R.DisplayName = P.GetDisplayName();
					R.Slot = SlotOf(P.GetSlot());
					R.Key = P.GetCurrentKey();
					R.ContextName = C.Name;
					if (!Out.ContainsByPredicate([&R](const FRow& X) { return X.MappingName == R.MappingName && X.Slot == R.Slot && X.ContextName.EqualTo(R.ContextName); })) { Out.Add(R); }
				}
			}
		}
	}
	return Out;
}

MurdarKeys::EResult MurdarKeyRemap::Rebind(APlayerController* PC, const TArray<FRow>& Rows, int32 Index, const FKey& Key)
{
	UEnhancedInputUserSettings* Settings = UserSettings(PC);
	if (!Settings || !Rows.IsValidIndex(Index)) { return MurdarKeys::EResult::WrongSlot; }
	std::vector<MurdarKeys::FBinding> B = ToRules(Rows);
	const std::vector<MurdarKeys::FBinding> Before = B;
	std::vector<std::string> Reserved;
	for (const FKey& K : GetDefault<UKeyRemapSettings>()->Reserved) { Reserved.push_back(TCHAR_TO_UTF8(*K.GetFName().ToString())); }
	const MurdarKeys::EResult R = MurdarKeys::Rebind(B, Index, TCHAR_TO_UTF8(*Key.GetFName().ToString()), Reserved);
	if (R != MurdarKeys::EResult::Ok && R != MurdarKeys::EResult::Swapped) { return R; }
	// Tell the engine about every row whose key changed (ours, and the swapped one).
	for (int32 i = 0; i < Rows.Num(); ++i)
	{
		if (B[i].Key == Before[i].Key) { continue; }
		FMapPlayerKeyArgs Args;
		Args.MappingName = Rows[i].MappingName;
		Args.Slot = ToEngine(Rows[i].Slot);
		Args.NewKey = FKey(FName(UTF8_TO_TCHAR(B[i].Key.c_str())));
		FGameplayTagContainer Failure;
		Settings->MapPlayerKey(Args, Failure);
	}
	Settings->ApplySettings();
	Settings->AsyncSaveSettings(); // ADAPT: SaveSettings() on older versions
	return R;
}

void MurdarKeyRemap::ResetToDefaults(APlayerController* PC)
{
	if (UEnhancedInputUserSettings* Settings = UserSettings(PC))
	{
		if (UEnhancedPlayerMappableKeyProfile* Profile = Settings->GetActiveKeyProfile())
		{
			FGameplayTagContainer Failure;
			Profile->ResetToDefault(); // ADAPT: the 5.8 reset call on the profile / settings
			Settings->ApplySettings();
			Settings->AsyncSaveSettings();
		}
	}
}
