// Car radio — stations that keep broadcasting whether you listen or not. Pure C++17, unit-tested (Handoff/Radio/Tests).
//   Program: each station's songs, DJ links, ads and news slots in a fixed order (seeded), looping. Tuning in
//            lands in the middle of whatever is on (GTA): the position comes from the world clock, not from you.
//   Streamer mode: licensed songs leave the program (Content ID), everything else stays.
//   News desk: what he did (a chase, an arrest, a stolen car) becomes a story; the next news slot reads the most
//              important one that is still fresh, or a generic bulletin.

#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace MurdarRadio
{
	enum class EKind { Song, Dj, Ad, News };

	struct FItem
	{
		EKind Kind = EKind::Song;
		int Index = 0;          // into the station's songs / DJ / ads (News: unused)
		float Duration = 0.f;
	};

	struct FStationContent
	{
		std::vector<float> Songs;          // durations
		std::vector<bool> SongLicensed;    // same size as Songs
		std::vector<float> Djs;
		std::vector<float> Ads;
		float NewsDuration = 30.f;
		int SongsPerDj = 2, SongsPerAd = 3, SongsPerNews = 6; // 0 = never
		unsigned Seed = 1;
	};

	inline unsigned NextRand(unsigned& S) { S = S * 1664525u + 1013904223u; return S >> 8; }

	/** The station's loop. Songs shuffled by the seed; links interleaved by the counts. */
	inline std::vector<FItem> BuildProgram(const FStationContent& C, bool bStreamerMode)
	{
		std::vector<int> Songs;
		for (int i = 0; i < int(C.Songs.size()); ++i)
		{
			const bool bLicensed = i < int(C.SongLicensed.size()) && C.SongLicensed[i];
			if (!(bStreamerMode && bLicensed) && C.Songs[i] > 0.f) { Songs.push_back(i); }
		}
		unsigned S = C.Seed ? C.Seed : 1u;
		for (int i = int(Songs.size()) - 1; i > 0; --i) { std::swap(Songs[i], Songs[int(NextRand(S) % unsigned(i + 1))]); }

		std::vector<FItem> P;
		int Dj = 0, Ad = 0;
		for (int n = 0; n < int(Songs.size()); ++n)
		{
			P.push_back({ EKind::Song, Songs[n], C.Songs[Songs[n]] });
			const int Played = n + 1;
			if (C.SongsPerNews > 0 && Played % C.SongsPerNews == 0) { P.push_back({ EKind::News, 0, C.NewsDuration }); continue; }
			if (C.SongsPerAd > 0 && !C.Ads.empty() && Played % C.SongsPerAd == 0) { P.push_back({ EKind::Ad, Ad, C.Ads[Ad] }); Ad = (Ad + 1) % int(C.Ads.size()); continue; }
			if (C.SongsPerDj > 0 && !C.Djs.empty() && Played % C.SongsPerDj == 0) { P.push_back({ EKind::Dj, Dj, C.Djs[Dj] }); Dj = (Dj + 1) % int(C.Djs.size()); }
		}
		if (P.empty() && !C.Djs.empty()) { P.push_back({ EKind::Dj, 0, C.Djs[0] }); } // a talk station
		return P;
	}

	inline float Length(const std::vector<FItem>& P)
	{
		float L = 0.f;
		for (const FItem& I : P) { L += std::max(0.f, I.Duration); }
		return L;
	}

	struct FPosition { int Item = -1; float Offset = 0.f; };

	/** What is on at world time T (seconds). Each station is offset by its own phase so they don't all start together. */
	inline FPosition At(const std::vector<FItem>& P, double T, float Phase)
	{
		FPosition Out;
		const float L = Length(P);
		if (P.empty() || L <= 0.f) { return Out; }
		double X = std::fmod(T + double(Phase), double(L));
		if (X < 0.0) { X += L; }
		for (int i = 0; i < int(P.size()); ++i)
		{
			const float D = std::max(0.f, P[i].Duration);
			if (X < D) { Out.Item = i; Out.Offset = float(X); return Out; }
			X -= D;
		}
		Out.Item = int(P.size()) - 1; Out.Offset = std::max(0.f, P.back().Duration - 0.01f);
		return Out;
	}

	/** Station dial: -1 = off. Next / previous wrap through off. */
	inline int Dial(int Current, int Count, int Dir)
	{
		if (Count <= 0) { return -1; }
		const int Slots = Count + 1; // stations + off
		const int Pos = Current < 0 ? Count : Current;
		const int NewPos = ((Pos + (Dir >= 0 ? 1 : -1)) % Slots + Slots) % Slots;
		return NewPos == Count ? -1 : NewPos;
	}

	struct FStory { std::string Tag; int Priority = 0; double Expires = 0.0; };

	class FNewsDesk
	{
	public:
		/** A story; the same tag again refreshes it (a second chase isn't a second story). */
		void Add(const std::string& Tag, int Priority, double Now, float Ttl)
		{
			for (FStory& S : Stories) { if (S.Tag == Tag) { S.Priority = std::max(S.Priority, Priority); S.Expires = Now + Ttl; return; } }
			Stories.push_back({ Tag, Priority, Now + Ttl });
		}

		/** The most important fresh story, removed; "" = read the generic bulletin. */
		std::string Take(double Now)
		{
			Stories.erase(std::remove_if(Stories.begin(), Stories.end(), [Now](const FStory& S) { return S.Expires <= Now; }), Stories.end());
			if (Stories.empty()) { return std::string(); }
			auto Best = std::max_element(Stories.begin(), Stories.end(), [](const FStory& A, const FStory& B) { return A.Priority < B.Priority; });
			const std::string Tag = Best->Tag;
			Stories.erase(Best);
			return Tag;
		}

		int Count() const { return int(Stories.size()); }

	private:
		std::vector<FStory> Stories;
	};
}
