// Dialogue — which conversation, which line, which choices, what happens. Pure C++17, unit-tested
// (Handoff/Dialogue/Tests). No engine types: tags are strings, the world is two callbacks (fact? value?).
//
// A dialogue is a small graph of nodes. It has several entries, each with conditions; the most specific entry whose
// conditions hold wins (the "rules" approach: the same man says something else once you owe him money). A node says a
// line, may apply effects, may offer choices (filtered by conditions, e.g. "enough money"), and a choice left alone
// runs out: silence is an answer (at a traffic stop, it is the ticket).

#pragma once

#include <algorithm>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace MurdarDialogue
{
	struct FQuery
	{
		std::function<bool(const std::string&)> HasFact;
		std::function<float(const std::string&)> Value;
	};

	struct FValueCheck
	{
		std::string Tag;
		float Min = -1e30f;
		float Max = 1e30f;
	};

	struct FCond
	{
		std::vector<std::string> Required;
		std::vector<std::string> Forbidden;
		std::vector<FValueCheck> Values;
	};

	inline bool Passes(const FCond& C, const FQuery& Q)
	{
		for (const std::string& T : C.Required) { if (!Q.HasFact || !Q.HasFact(T)) { return false; } }
		for (const std::string& T : C.Forbidden) { if (Q.HasFact && Q.HasFact(T)) { return false; } }
		for (const FValueCheck& V : C.Values)
		{
			const float X = Q.Value ? Q.Value(V.Tag) : 0.f;
			if (X < V.Min || X > V.Max) { return false; }
		}
		return true;
	}

	/** More conditions = more specific = said in preference to the generic line. */
	inline int Specificity(const FCond& C) { return int(C.Required.size() + C.Forbidden.size() + C.Values.size()); }

	/** Same reading speed as AMurdarHUD::ShowSubtitle's auto duration, so the two stay in step. */
	inline float AutoSeconds(const std::string& Utf8Text)
	{
		int Chars = 0;
		for (unsigned char Ch : Utf8Text) { if ((Ch & 0xC0) != 0x80) { ++Chars; } } // count code points, not bytes (ă, ș, ț)
		return std::max(1.5f, Chars / 15.f + 0.6f);
	}

	enum class EEffect { SetFact, ClearFact, AddValue, Publish, PayPolice };

	struct FEffect
	{
		EEffect Kind = EEffect::SetFact;
		std::string Tag;
		float Amount = 0.f;
	};

	struct FChoice
	{
		std::string Text;
		FCond Cond;
		std::vector<FEffect> Effects;
		std::string Next; // "" = the conversation ends
	};

	struct FNode
	{
		std::string Id;
		std::string Speaker;
		std::string Text;         // "" = no line (e.g. choices after a line someone else already said)
		float Seconds = 0.f;      // 0 = AutoSeconds(Text); the UE layer puts the voice length here
		FCond Cond;               // fails = the node is skipped (straight to Next)
		std::vector<FEffect> Effects; // applied when the node is entered
		std::vector<FChoice> Choices;
		float ChoiceTimeout = 0.f;    // 0 = wait forever
		int DefaultChoice = -1;       // index into Choices taken on timeout; -1 (or hidden) = end
		std::string Next;             // after the line when there are no (visible) choices; "" = end
	};

	struct FEntry
	{
		FCond Cond;
		std::string Node;
	};

	struct FDialogueSpec
	{
		std::vector<FEntry> Entries;
		std::vector<FNode> Nodes;
	};

	/** Index of the most specific passing entry (first wins a tie), or -1. */
	inline int PickEntry(const FDialogueSpec& S, const FQuery& Q)
	{
		int Best = -1, BestSpec = -1;
		for (int i = 0; i < int(S.Entries.size()); ++i)
		{
			if (Passes(S.Entries[i].Cond, Q) && Specificity(S.Entries[i].Cond) > BestSpec)
			{
				Best = i;
				BestSpec = Specificity(S.Entries[i].Cond);
			}
		}
		return Best;
	}

	enum class EStep { Line, Choices, ChoicesClosed, Effect, End };

	struct FStep
	{
		EStep Kind = EStep::End;
		std::string Speaker, Text;
		float Seconds = 0.f;
		std::vector<std::string> ChoiceTexts; // visible choices, in order; Choose() takes an index into this list
		FEffect Effect;
		std::string Reason;                   // End: "done", "timeout", "aborted", "missing node X", "loop"
	};

	class FRuntime
	{
	public:
		enum class EState { Idle, Speaking, Choosing, Done };

		/** False (and no steps) when no entry passes. */
		bool Start(const FDialogueSpec& Spec, const FQuery& Q, float Now, std::vector<FStep>& Out)
		{
			const int E = PickEntry(Spec, Q);
			if (E < 0) { return false; }
			S = &Spec;
			Enter(Spec.Entries[E].Node, Q, Now, Out);
			return true;
		}

		void Tick(float Now, const FQuery& Q, std::vector<FStep>& Out)
		{
			if (State == EState::Speaking && Now >= Until)
			{
				AfterLine(Q, Now, Out);
			}
			else if (State == EState::Choosing && Deadline > 0.f && Now >= Deadline)
			{
				const FNode& N = *Current;
				int Pick = -1;
				for (int v = 0; v < int(Visible.size()); ++v) { if (Visible[v] == N.DefaultChoice) { Pick = v; } }
				if (Pick >= 0) { ChooseVisible(Pick, Q, Now, Out); }
				else { Out.push_back(Closed()); Finish("timeout", Out); }
			}
		}

		/** Index into the last Choices step's list. False when not choosing or out of range. */
		bool Choose(int VisibleIndex, float Now, const FQuery& Q, std::vector<FStep>& Out)
		{
			if (State != EState::Choosing || VisibleIndex < 0 || VisibleIndex >= int(Visible.size())) { return false; }
			ChooseVisible(VisibleIndex, Q, Now, Out);
			return true;
		}

		void Abort(std::vector<FStep>& Out)
		{
			if (State == EState::Idle || State == EState::Done) { return; }
			if (State == EState::Choosing) { Out.push_back(Closed()); }
			Finish("aborted", Out);
		}

		EState GetState() const { return State; }
		bool IsRunning() const { return State == EState::Speaking || State == EState::Choosing; }
		const std::string& CurrentNodeId() const { static const std::string None; return Current ? Current->Id : None; }

	private:
		const FDialogueSpec* S = nullptr;
		const FNode* Current = nullptr;
		EState State = EState::Idle;
		float Until = 0.f, Deadline = 0.f;
		std::vector<int> Visible;

		const FNode* Find(const std::string& Id) const
		{
			for (const FNode& N : S->Nodes) { if (N.Id == Id) { return &N; } }
			return nullptr;
		}

		static FStep Closed() { FStep X; X.Kind = EStep::ChoicesClosed; return X; }

		void Finish(const std::string& Reason, std::vector<FStep>& Out)
		{
			State = EState::Done;
			Current = nullptr;
			Visible.clear();
			FStep X; X.Kind = EStep::End; X.Reason = Reason; Out.push_back(X);
		}

		static void Effects(const std::vector<FEffect>& Es, std::vector<FStep>& Out)
		{
			for (const FEffect& E : Es) { FStep X; X.Kind = EStep::Effect; X.Effect = E; Out.push_back(X); }
		}

		/** Walks skipped nodes; stops at the first node that speaks or asks. */
		void Enter(std::string Id, const FQuery& Q, float Now, std::vector<FStep>& Out)
		{
			for (int Hops = 0; Hops < 64; ++Hops)
			{
				if (Id.empty()) { Finish("done", Out); return; }
				const FNode* N = Find(Id);
				if (!N) { Finish("missing node " + Id, Out); return; }
				if (!Passes(N->Cond, Q)) { Id = N->Next; continue; }
				Current = N;
				Effects(N->Effects, Out);
				if (!N->Text.empty())
				{
					FStep X; X.Kind = EStep::Line; X.Speaker = N->Speaker; X.Text = N->Text;
					X.Seconds = N->Seconds > 0.f ? N->Seconds : AutoSeconds(N->Text);
					Out.push_back(X);
					State = EState::Speaking;
					Until = Now + X.Seconds;
					return;
				}
				if (OpenChoices(Q, Now, Out)) { return; }
				Id = N->Next;
			}
			Finish("loop", Out);
		}

		void AfterLine(const FQuery& Q, float Now, std::vector<FStep>& Out)
		{
			if (OpenChoices(Q, Now, Out)) { return; }
			Enter(Current->Next, Q, Now, Out);
		}

		bool OpenChoices(const FQuery& Q, float Now, std::vector<FStep>& Out)
		{
			Visible.clear();
			FStep X; X.Kind = EStep::Choices;
			for (int i = 0; i < int(Current->Choices.size()); ++i)
			{
				if (Passes(Current->Choices[i].Cond, Q)) { Visible.push_back(i); X.ChoiceTexts.push_back(Current->Choices[i].Text); }
			}
			if (Visible.empty()) { return false; }
			Out.push_back(X);
			State = EState::Choosing;
			Deadline = Current->ChoiceTimeout > 0.f ? Now + Current->ChoiceTimeout : 0.f;
			return true;
		}

		void ChooseVisible(int V, const FQuery& Q, float Now, std::vector<FStep>& Out)
		{
			const FChoice& C = Current->Choices[Visible[V]];
			Out.push_back(Closed());
			Effects(C.Effects, Out);
			Visible.clear();
			Enter(C.Next, Q, Now, Out);
		}
	};

	/** Voiced barks (police lines, a passer-by's curse): never the same variant twice in a row, and a cooldown per key
	 *  so a unit repeating "Trage pe dreapta!" every second is heard once in a while, not every time. */
	class FBarkPicker
	{
	public:
		/** Variant index, or -1 while the key is cooling down / there are no variants. Rand01 in [0,1). */
		int Pick(const std::string& Key, int Count, float Now, float Cooldown, float Rand01)
		{
			if (Count <= 0) { return -1; }
			auto It = NextAllowed.find(Key);
			if (It != NextAllowed.end() && Now < It->second) { return -1; }
			int I = std::min(Count - 1, int(Rand01 * Count));
			auto L = Last.find(Key);
			if (Count > 1 && L != Last.end() && L->second == I) { I = (I + 1) % Count; }
			Last[Key] = I;
			NextAllowed[Key] = Now + Cooldown;
			return I;
		}

	private:
		std::map<std::string, int> Last;
		std::map<std::string, float> NextAllowed;
	};
}
