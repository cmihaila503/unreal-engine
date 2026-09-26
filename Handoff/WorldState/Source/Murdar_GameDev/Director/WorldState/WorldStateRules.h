// World state — what of the world a save brings back, beyond the story (facts, values, loadout). Pure C++17,
// unit-tested (Handoff/WorldState/Tests). The UE layer keeps the data (FMurdarWorldState, inside FNarrativeState so it
// is one file and one version); these are the decisions.
//
// Brought back: where he was (on foot or in his car), his car (where, how damaged), and placed actors that opted in
// (a door left open, a crate moved, a streetlight shot out and gone). Records are per map; a save made on another map
// changes nothing here.

#pragma once

#include <string>
#include <vector>

namespace MurdarWorld
{
	/** Only right after a load from file, and only on the map the save was made on. */
	inline bool ShouldApply(bool bPendingFromLoad, const std::string& SavedMap, const std::string& CurrentMap)
	{
		return bPendingFromLoad && !SavedMap.empty() && SavedMap == CurrentMap;
	}

	enum class EPlayerRestore { Checkpoint, OnFoot, InCar };

	/** A story chapter keeps its checkpoint placement; otherwise where he was, and in his car only if the car is back. */
	inline EPlayerRestore DecidePlayer(bool bStoryMode, bool bHasSavedPlayer, bool bSavedInCar, bool bCarRestored)
	{
		if (bStoryMode || !bHasSavedPlayer) { return EPlayerRestore::Checkpoint; }
		return (bSavedInCar && bCarRestored) ? EPlayerRestore::InCar : EPlayerRestore::OnFoot;
	}

	/** A saved position below the world's kill height (fell through the map) is not worth restoring. */
	inline bool IsPlaceable(float Z, float KillZ) { return Z > KillZ + 100.f; }

	struct FRecord
	{
		std::string Map, Id;
		bool bDestroyed = false;
		bool bHasTransform = false;
		bool bHasValue = false;
	};

	enum class EActorAction { Leave, Destroy, Restore };

	inline EActorAction DecideActor(const FRecord* R)
	{
		if (!R) { return EActorAction::Leave; }
		if (R->bDestroyed) { return EActorAction::Destroy; }
		return (R->bHasTransform || R->bHasValue) ? EActorAction::Restore : EActorAction::Leave;
	}

	/** Records keyed by (map, id). Destroyed is sticky: a later capture of the same id can't bring it back. */
	class FRecords
	{
	public:
		FRecord& Upsert(const std::string& Map, const std::string& Id)
		{
			if (FRecord* R = Find(Map, Id)) { return *R; }
			FRecord N; N.Map = Map; N.Id = Id;
			All.push_back(N);
			return All.back();
		}

		FRecord* Find(const std::string& Map, const std::string& Id)
		{
			for (FRecord& R : All) { if (R.Map == Map && R.Id == Id) { return &R; } }
			return nullptr;
		}

		void MarkDestroyed(const std::string& Map, const std::string& Id)
		{
			FRecord& R = Upsert(Map, Id);
			R.bDestroyed = true;
			R.bHasTransform = R.bHasValue = false;
		}

		/** A live capture; ignored for a destroyed record. */
		void Capture(const std::string& Map, const std::string& Id, bool bTransform, bool bValue)
		{
			FRecord& R = Upsert(Map, Id);
			if (R.bDestroyed) { return; }
			R.bHasTransform = R.bHasTransform || bTransform;
			R.bHasValue = R.bHasValue || bValue;
		}

		int Count(const std::string& Map) const
		{
			int N = 0;
			for (const FRecord& R : All) { N += R.Map == Map ? 1 : 0; }
			return N;
		}

		const std::vector<FRecord>& Get() const { return All; }

	private:
		std::vector<FRecord> All;
	};
}
