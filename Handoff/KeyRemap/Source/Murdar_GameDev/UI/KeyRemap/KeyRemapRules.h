// Key remapping — what may go where. Pure C++17, unit-tested (Handoff/KeyRemap/Tests).
//   A binding is an action, a slot (keyboard/mouse or gamepad) and a key, in a context (on foot, driving, menu).
//   The same key may serve two actions in contexts that never run together (E = interact on foot, E = something in
//   the car), but not twice in one context. A conflict is solved by swapping: the other action takes this action's old
//   key. Some keys can't be taken (the pause keys). Keyboard keys go in the keyboard slot, pad buttons in the pad slot.

#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace MurdarKeys
{
	enum class ESlot { KeyboardMouse, Gamepad };

	struct FBinding
	{
		std::string Action;
		ESlot Slot = ESlot::KeyboardMouse;
		std::string Key;
		std::vector<std::string> Contexts; // "OnFoot", "Vehicle"...
	};

	inline bool IsGamepadKey(const std::string& Key) { return Key.rfind("Gamepad_", 0) == 0; }

	inline bool SlotAccepts(ESlot Slot, const std::string& Key)
	{
		if (Key.empty()) { return false; }
		return Slot == ESlot::Gamepad ? IsGamepadKey(Key) : !IsGamepadKey(Key);
	}

	inline bool SharesContext(const FBinding& A, const FBinding& B)
	{
		for (const std::string& C : A.Contexts) { if (std::find(B.Contexts.begin(), B.Contexts.end(), C) != B.Contexts.end()) { return true; } }
		return false;
	}

	/** Indices of bindings that would clash if Bindings[Index] took Key. */
	inline std::vector<int> Conflicts(const std::vector<FBinding>& Bindings, int Index, const std::string& Key)
	{
		std::vector<int> Out;
		if (Index < 0 || Index >= int(Bindings.size())) { return Out; }
		const FBinding& Me = Bindings[Index];
		for (int i = 0; i < int(Bindings.size()); ++i)
		{
			if (i == Index) { continue; }
			const FBinding& B = Bindings[i];
			if (B.Slot == Me.Slot && B.Key == Key && SharesContext(B, Me)) { Out.push_back(i); }
		}
		return Out;
	}

	enum class EResult { Ok, Swapped, Reserved, WrongSlot, Same };

	/** Bind Key to Bindings[Index]; a clash is swapped (the other action gets our old key). */
	inline EResult Rebind(std::vector<FBinding>& Bindings, int Index, const std::string& Key, const std::vector<std::string>& Reserved)
	{
		if (Index < 0 || Index >= int(Bindings.size())) { return EResult::WrongSlot; }
		FBinding& Me = Bindings[Index];
		if (std::find(Reserved.begin(), Reserved.end(), Key) != Reserved.end()) { return EResult::Reserved; }
		if (!SlotAccepts(Me.Slot, Key)) { return EResult::WrongSlot; }
		if (Me.Key == Key) { return EResult::Same; }
		const std::vector<int> Clash = Conflicts(Bindings, Index, Key);
		const std::string Old = Me.Key;
		for (int c : Clash) { Bindings[c].Key = Old; }
		Me.Key = Key;
		return Clash.empty() ? EResult::Ok : EResult::Swapped;
	}

	/** Every clash left in a set (after loading someone's old config, say). */
	inline int CountClashes(const std::vector<FBinding>& Bindings)
	{
		int N = 0;
		for (int i = 0; i < int(Bindings.size()); ++i) { N += int(Conflicts(Bindings, i, Bindings[i].Key).size()); }
		return N / 2;
	}
}
