// Pooling — keeping people and cars to reuse instead of spawning and destroying them. Pure C++17, unit-tested
// (Handoff/Pooling/Tests). The population system spawns and despawns constantly as he moves; each spawn of a
// character (mesh, anim instance, AI controller, components) costs a frame hitch. A pool keeps a few parked out of
// sight, warmed up during quiet frames, and hands them back reset.

#pragma once

#include <algorithm>
#include <vector>

namespace MurdarPool
{
	struct FTuning
	{
		int Warm = 8;         // parked and ready, per kind
		int MaxParked = 16;   // beyond this, released ones are destroyed
		int PrewarmPerTick = 1; // spawns per pass while warming (never a burst)
	};

	enum class EAcquire { Reuse, Spawn };

	inline EAcquire Acquire(int Parked) { return Parked > 0 ? EAcquire::Reuse : EAcquire::Spawn; }

	enum class ERelease { Park, Destroy };

	inline ERelease Release(int Parked, const FTuning& T) { return Parked < T.MaxParked ? ERelease::Park : ERelease::Destroy; }

	/** How many to spawn this pass to reach Warm (bounded per pass). */
	inline int Prewarm(int Parked, bool bQuietFrame, const FTuning& T)
	{
		if (!bQuietFrame || Parked >= T.Warm) { return 0; }
		return std::min(T.PrewarmPerTick, T.Warm - Parked);
	}

	/** Parked ones to destroy when the budget shrinks (a smaller Warm, a map change): the oldest first. Returns count. */
	inline int Trim(int Parked, const FTuning& T) { return std::max(0, Parked - T.MaxParked); }

	/** A parked slot: when it went in (for "oldest first"). */
	struct FSlot { int Id = -1; double ParkedAt = 0.0; };

	/** Index of the slot to reuse: the most recently parked (warm caches, least likely to have been purged). */
	inline int PickReuse(const std::vector<FSlot>& Slots)
	{
		int Best = -1;
		for (int i = 0; i < int(Slots.size()); ++i) { if (Best < 0 || Slots[i].ParkedAt > Slots[Best].ParkedAt) { Best = i; } }
		return Best;
	}

	/** Index of the slot to destroy when trimming: the oldest. */
	inline int PickTrim(const std::vector<FSlot>& Slots)
	{
		int Best = -1;
		for (int i = 0; i < int(Slots.size()); ++i) { if (Best < 0 || Slots[i].ParkedAt < Slots[Best].ParkedAt) { Best = i; } }
		return Best;
	}
}
