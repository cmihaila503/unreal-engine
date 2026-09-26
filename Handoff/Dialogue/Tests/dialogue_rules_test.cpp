// g++ -std=c++17 -Wall -Wextra -Wshadow -I../Source/Murdar_GameDev/Director/Dialogue dialogue_rules_test.cpp -o dt && ./dt
#include "DialogueRules.h"

#include <cmath>
#include <cstdio>
#include <set>

using namespace MurdarDialogue;
static int Failures = 0, Checks = 0;
#define CHECK(c) do { ++Checks; if (!(c)) { ++Failures; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static void Run(const char* N, const std::function<void()>& F) { const int B = Failures; F(); std::printf("%s %s\n", Failures == B ? "ok  " : "FAIL", N); }

struct FWorld
{
	std::set<std::string> Facts;
	std::map<std::string, float> Values;
	FQuery Q() const
	{
		return { [this](const std::string& T) { return Facts.count(T) > 0; },
		         [this](const std::string& T) { auto It = Values.find(T); return It == Values.end() ? 0.f : It->second; } };
	}
};

static FNode Line(const std::string& Id, const std::string& Text, const std::string& Next = "")
{
	FNode N; N.Id = Id; N.Speaker = "Gică"; N.Text = Text; N.Seconds = 2.f; N.Next = Next; return N;
}
static int Count(const std::vector<FStep>& Out, EStep K) { int C = 0; for (const FStep& S : Out) { C += S.Kind == K; } return C; }
static const FStep* Last(const std::vector<FStep>& Out, EStep K) { const FStep* R = nullptr; for (const FStep& S : Out) { if (S.Kind == K) { R = &S; } } return R; }

// The traffic stop as authored in the README: no line of our own (the police already said it), three choices.
static FDialogueSpec TrafficStop()
{
	FDialogueSpec S;
	S.Entries.push_back({ {}, "offer" });
	FNode N; N.Id = "offer"; N.ChoiceTimeout = 7.f; N.DefaultChoice = 2;
	FChoice Small; Small.Text = "Dă-i 200"; Small.Cond.Values.push_back({ "Stat.Money", 200.f });
	Small.Effects.push_back({ EEffect::PayPolice, "", 200.f });
	FChoice Big; Big.Text = "Dă-i 500"; Big.Cond.Values.push_back({ "Stat.Money", 500.f });
	Big.Effects.push_back({ EEffect::PayPolice, "", 500.f });
	FChoice Quiet; Quiet.Text = "Taci"; Quiet.Effects.push_back({ EEffect::SetFact, "Fact.Stop.Silent" });
	N.Choices = { Small, Big, Quiet };
	S.Nodes.push_back(N);
	return S;
}

int main()
{
	Run("conditions: facts and value ranges", [&]
	{
		FWorld W; W.Facts = { "Fact.A" }; W.Values["Stat.Money"] = 300.f;
		FCond C; C.Required = { "Fact.A" }; CHECK(Passes(C, W.Q()));
		C.Forbidden = { "Fact.A" }; CHECK(!Passes(C, W.Q()));
		FCond V; V.Values.push_back({ "Stat.Money", 200.f, 400.f }); CHECK(Passes(V, W.Q()));
		W.Values["Stat.Money"] = 150.f; CHECK(!Passes(V, W.Q()));
		CHECK(Passes(FCond{}, FQuery{}));
	});

	Run("the most specific entry wins: he remembers you owe him", [&]
	{
		FDialogueSpec S;
		FEntry Generic{ {}, "hello" };
		FEntry Owes; Owes.Cond.Required = { "Fact.OwesGica" }; Owes.Node = "debt";
		FEntry Paid; Paid.Cond.Required = { "Fact.OwesGica", "Fact.PaidGica" }; Paid.Node = "thanks";
		S.Entries = { Generic, Owes, Paid };
		FWorld W; CHECK(PickEntry(S, W.Q()) == 0);
		W.Facts.insert("Fact.OwesGica"); CHECK(PickEntry(S, W.Q()) == 1);
		W.Facts.insert("Fact.PaidGica"); CHECK(PickEntry(S, W.Q()) == 2);
		FDialogueSpec None; FEntry Gated; Gated.Cond.Required = { "X" }; None.Entries = { Gated };
		FRuntime R; std::vector<FStep> Out; CHECK(!R.Start(None, W.Q(), 0.f, Out)); CHECK(Out.empty());
	});

	Run("lines play in order, each for its time, then the end", [&]
	{
		FDialogueSpec S; S.Entries.push_back({ {}, "a" });
		S.Nodes = { Line("a", "Salut.", "b"), Line("b", "Ce vrei?") };
		FWorld W; FRuntime R; std::vector<FStep> Out;
		CHECK(R.Start(S, W.Q(), 10.f, Out));
		CHECK(Out.size() == 1 && Out[0].Kind == EStep::Line && Out[0].Text == "Salut." && Out[0].Speaker == "Gică");
		Out.clear(); R.Tick(11.9f, W.Q(), Out); CHECK(Out.empty());
		R.Tick(12.f, W.Q(), Out); CHECK(Out.size() == 1 && Out[0].Text == "Ce vrei?");
		Out.clear(); R.Tick(14.f, W.Q(), Out); CHECK(Count(Out, EStep::End) == 1 && Last(Out, EStep::End)->Reason == "done");
		CHECK(!R.IsRunning());
	});

	Run("auto duration counts letters, not UTF-8 bytes", [&]
	{
		CHECK(std::fabs(AutoSeconds("ăștî") - AutoSeconds("astI")) < 1e-6f);
		CHECK(AutoSeconds("") == 1.5f);
		CHECK(std::fabs(AutoSeconds(std::string(150, 'a')) - 10.6f) < 1e-3f);
	});

	Run("traffic stop: choices you can't afford are not offered", [&]
	{
		FDialogueSpec S = TrafficStop();
		FWorld W; W.Values["Stat.Money"] = 250.f;
		FRuntime R; std::vector<FStep> Out;
		R.Start(S, W.Q(), 0.f, Out);
		const FStep* C = Last(Out, EStep::Choices);
		CHECK(C && C->ChoiceTexts.size() == 2 && C->ChoiceTexts[0] == "Dă-i 200" && C->ChoiceTexts[1] == "Taci");
		W.Values["Stat.Money"] = 50.f; FRuntime R2; Out.clear(); R2.Start(S, W.Q(), 0.f, Out);
		C = Last(Out, EStep::Choices); CHECK(C && C->ChoiceTexts.size() == 1);
	});

	Run("choosing applies the choice's effects, then ends", [&]
	{
		FDialogueSpec S = TrafficStop();
		FWorld W; W.Values["Stat.Money"] = 900.f;
		FRuntime R; std::vector<FStep> Out; R.Start(S, W.Q(), 0.f, Out); Out.clear();
		CHECK(!R.Choose(5, 1.f, W.Q(), Out));
		CHECK(R.Choose(1, 1.f, W.Q(), Out));
		CHECK(Out.size() == 3 && Out[0].Kind == EStep::ChoicesClosed && Out[1].Kind == EStep::Effect);
		CHECK(Out[1].Effect.Kind == EEffect::PayPolice && Out[1].Effect.Amount == 500.f && Out[2].Kind == EStep::End);
		CHECK(!R.Choose(0, 2.f, W.Q(), Out));
	});

	Run("silence is an answer: timeout takes the default choice", [&]
	{
		FDialogueSpec S = TrafficStop();
		FWorld W; W.Values["Stat.Money"] = 900.f;
		FRuntime R; std::vector<FStep> Out; R.Start(S, W.Q(), 0.f, Out); Out.clear();
		R.Tick(6.9f, W.Q(), Out); CHECK(Out.empty());
		R.Tick(7.f, W.Q(), Out);
		const FStep* E = Last(Out, EStep::Effect);
		CHECK(E && E->Effect.Kind == EEffect::SetFact && E->Effect.Tag == "Fact.Stop.Silent");
		CHECK(Last(Out, EStep::End) && Last(Out, EStep::End)->Reason == "done");
	});

	Run("timeout with a hidden default just closes and ends", [&]
	{
		FDialogueSpec S = TrafficStop();
		S.Nodes[0].DefaultChoice = 1; // the 500 one
		FWorld W; W.Values["Stat.Money"] = 250.f; // can't afford it: hidden
		FRuntime R; std::vector<FStep> Out; R.Start(S, W.Q(), 0.f, Out); Out.clear();
		R.Tick(7.f, W.Q(), Out);
		CHECK(Count(Out, EStep::Effect) == 0 && Count(Out, EStep::ChoicesClosed) == 1 && Last(Out, EStep::End)->Reason == "timeout");
	});

	Run("line, then choices, then a branch", [&]
	{
		FDialogueSpec S; S.Entries.push_back({ {}, "ask" });
		FNode Ask = Line("ask", "Ai marfa?");
		FChoice Yes; Yes.Text = "Da"; Yes.Next = "good";
		FChoice No; No.Text = "Nu"; No.Next = "bad";
		Ask.Choices = { Yes, No };
		FNode Good = Line("good", "Bravo."); Good.Effects.push_back({ EEffect::AddValue, "Stat.Money", 300.f });
		S.Nodes = { Ask, Good, Line("bad", "Păcat.") };
		FWorld W; FRuntime R; std::vector<FStep> Out;
		R.Start(S, W.Q(), 0.f, Out); CHECK(Count(Out, EStep::Choices) == 0);
		Out.clear(); R.Tick(2.f, W.Q(), Out); CHECK(Count(Out, EStep::Choices) == 1 && R.GetState() == FRuntime::EState::Choosing);
		Out.clear(); R.Choose(0, 3.f, W.Q(), Out);
		CHECK(Out.size() == 3 && Out[1].Kind == EStep::Effect && Out[1].Effect.Amount == 300.f && Out[2].Text == "Bravo.");
		CHECK(R.CurrentNodeId() == "good");
	});

	Run("a node whose conditions fail is skipped", [&]
	{
		FDialogueSpec S; S.Entries.push_back({ {}, "a" });
		FNode A = Line("a", "Iar tu?", "b"); A.Cond.Required = { "Fact.MetBefore" };
		S.Nodes = { A, Line("b", "Cine ești?") };
		FWorld W; FRuntime R; std::vector<FStep> Out;
		R.Start(S, W.Q(), 0.f, Out); CHECK(Out.size() == 1 && Out[0].Text == "Cine ești?");
	});

	Run("authoring errors end the talk instead of hanging", [&]
	{
		FDialogueSpec S; S.Entries.push_back({ {}, "a" });
		S.Nodes = { Line("a", "Hai.", "nowhere") };
		FWorld W; FRuntime R; std::vector<FStep> Out;
		R.Start(S, W.Q(), 0.f, Out); Out.clear(); R.Tick(2.f, W.Q(), Out);
		CHECK(Last(Out, EStep::End) && Last(Out, EStep::End)->Reason == "missing node nowhere");
		FDialogueSpec L; L.Entries.push_back({ {}, "x" });
		FNode X; X.Id = "x"; X.Next = "y"; FNode Y; Y.Id = "y"; Y.Next = "x"; L.Nodes = { X, Y };
		FRuntime R2; Out.clear(); R2.Start(L, W.Q(), 0.f, Out);
		CHECK(Last(Out, EStep::End) && Last(Out, EStep::End)->Reason == "loop");
	});

	Run("abort closes open choices", [&]
	{
		FDialogueSpec S = TrafficStop(); FWorld W;
		FRuntime R; std::vector<FStep> Out; R.Start(S, W.Q(), 0.f, Out); Out.clear();
		R.Abort(Out);
		CHECK(Out.size() == 2 && Out[0].Kind == EStep::ChoicesClosed && Out[1].Reason == "aborted");
		Out.clear(); R.Abort(Out); CHECK(Out.empty());
	});

	Run("barks: no immediate repeat, cooldown per key", [&]
	{
		FBarkPicker P;
		const int A = P.Pick("PullOver", 3, 0.f, 4.f, 0.5f);
		CHECK(A == 1);
		CHECK(P.Pick("PullOver", 3, 2.f, 4.f, 0.5f) == -1);        // cooling down
		CHECK(P.Pick("Demand", 3, 2.f, 4.f, 0.5f) == 1);           // other key is independent
		CHECK(P.Pick("PullOver", 3, 4.f, 4.f, 0.5f) == 2);         // same roll, but not the same variant twice
		CHECK(P.Pick("One", 1, 0.f, 0.f, 0.9f) == 0);
		CHECK(P.Pick("One", 1, 0.f, 0.f, 0.9f) == 0);              // a single variant may repeat
		CHECK(P.Pick("None", 0, 0.f, 0.f, 0.f) == -1);
		CHECK(P.Pick("Edge", 2, 0.f, 0.f, 0.99999f) == 1);
	});

	std::printf("\n%d checks, %d failed\n", Checks, Failures);
	return Failures == 0 ? 0 : 1;
}
