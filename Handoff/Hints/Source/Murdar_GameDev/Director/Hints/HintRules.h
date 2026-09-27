// Hints — the quiet tutorial. Pure C++17, unit-tested (Handoff/Hints/Tests).
// A hint is said once (a fact remembers it, saved), when its moment comes (a bus event), never while something is
// going on (chase, conversation, cutscene) — it waits a little for calm, then lapses — and never two close together.

#pragma once

#include <string>
#include <vector>

namespace MurdarHints
{
	struct FTuning
	{
		float MinGapSeconds = 25.f;   // between two hints
		float WaitForCalmSeconds = 20.f; // a hint whose moment came during a chase may still be said this long after
	};

	struct FPending { std::string Id; double Since = 0.0; };

	class FScheduler
	{
	public:
		/** Its moment came. Already shown or already waiting: ignored. */
		void Offer(const std::string& Id, bool bAlreadyShown, double Now)
		{
			if (bAlreadyShown) { return; }
			for (const FPending& P : Pending) { if (P.Id == Id) { return; } }
			Pending.push_back({ Id, Now });
		}

		/** The hint to show now, or "". Busy = chase / conversation / cutscene / menu. */
		std::string Poll(double Now, bool bBusy, const FTuning& T)
		{
			// Lapsed ones go.
			std::vector<FPending> Keep;
			for (const FPending& P : Pending) { if (Now - P.Since <= T.WaitForCalmSeconds) { Keep.push_back(P); } }
			Pending.swap(Keep);
			if (bBusy || Pending.empty() || Now - LastShown < T.MinGapSeconds) { return std::string(); }
			const std::string Id = Pending.front().Id;
			Pending.erase(Pending.begin());
			LastShown = Now;
			return Id;
		}

		int Waiting() const { return int(Pending.size()); }

	private:
		std::vector<FPending> Pending;
		double LastShown = -1e9;
	};
}
