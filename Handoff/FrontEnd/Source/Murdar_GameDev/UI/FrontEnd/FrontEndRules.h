// Front end — the title screen, save slots, the loading screen's tips. Pure C++17, unit-tested (Handoff/FrontEnd/Tests).
//   Slots: three manual slots plus the autosave; "Continuă" is the newest existing one; every slot shows chapter,
//          game day and hour, and when it was saved.
//   New game over existing saves asks first (and never deletes them).
//   Tips: never the same tip twice in a row, and not one of the last few.

#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace MurdarFrontEnd
{
	struct FSlot
	{
		std::string Name;          // "auto", "slot1", ...
		bool bExists = false;
		int64_t SavedAt = 0;       // any monotonic unit (UTC ticks)
		std::string Chapter;       // display name
		int Day = 0;               // 0-based
		float Minutes = -1.f;      // game clock; < 0 = unknown
		float Money = 0.f;
	};

	/** The newest existing slot, or -1. */
	inline int ContinueSlot(const std::vector<FSlot>& Slots)
	{
		int Best = -1;
		for (int i = 0; i < int(Slots.size()); ++i)
		{
			if (Slots[i].bExists && (Best < 0 || Slots[i].SavedAt > Slots[Best].SavedAt)) { Best = i; }
		}
		return Best;
	}

	inline bool AnySave(const std::vector<FSlot>& Slots) { return ContinueSlot(Slots) >= 0; }

	/** "Ziua 3, 21:05" (game clock). Empty when unknown. */
	inline std::string Clock(int Day, float Minutes)
	{
		if (Minutes < 0.f) { return std::string(); }
		const int M = int(Minutes) % 1440;
		char Buf[32];
		std::snprintf(Buf, sizeof(Buf), "Ziua %d, %02d:%02d", Day + 1, M / 60, M % 60);
		return Buf;
	}

	/** One line per slot: "Slot 1 — Capitolul doi · Ziua 3, 21:05" / "Slot 2 — gol". */
	inline std::string Label(const FSlot& S, const std::string& Title)
	{
		if (!S.bExists) { return Title + " \xE2\x80\x94 gol"; }
		std::string L = Title + " \xE2\x80\x94 " + (S.Chapter.empty() ? std::string("?") : S.Chapter);
		const std::string C = Clock(S.Day, S.Minutes);
		if (!C.empty()) { L += " \xC2\xB7 " + C; }
		return L;
	}

	/** Overwriting an existing slot asks first; an empty one doesn't. */
	inline bool NeedsOverwriteConfirm(const FSlot& S) { return S.bExists; }

	/** Next tip index: random, never one of the Recent (most recent last); falls back gracefully with few tips. */
	inline int NextTip(int Count, const std::vector<int>& Recent, float Rand01, int AvoidLast = 3)
	{
		if (Count <= 0) { return -1; }
		std::vector<int> Ok;
		const int Avoid = std::min<int>(AvoidLast, std::max(0, Count - 1));
		for (int i = 0; i < Count; ++i)
		{
			bool bRecent = false;
			for (int k = 0; k < Avoid && k < int(Recent.size()); ++k) { bRecent |= Recent[Recent.size() - 1 - k] == i; }
			if (!bRecent) { Ok.push_back(i); }
		}
		if (Ok.empty()) { return 0; }
		return Ok[std::min(int(Ok.size()) - 1, int(std::clamp(Rand01, 0.f, 1.f) * Ok.size()))];
	}
}
