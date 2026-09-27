#include "UI/PaperMap/PaperMapLibrary.h"

#include "UI/PaperMap/MapMarkerComponent.h"
#include "UI/PaperMap/PaperMapSettings.h"
#include "UI/PaperMap/SPaperMap.h"
#include "Director/Jobs/JobSubsystem.h"   // Handoff/Jobs
#include "Director/NarrativeStateSubsystem.h"

#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

namespace
{
	MurdarMap::FBounds Bounds()
	{
		const UPaperMapSettings* S = GetDefault<UPaperMapSettings>();
		return { { float(S->WorldAtUV0.X), float(S->WorldAtUV0.Y) }, { float(S->WorldAtUV1.X), float(S->WorldAtUV1.Y) } };
	}
	MurdarMap::FV2 UV(const FVector& W) { return MurdarMap::WorldToUV({ float(W.X), float(W.Y) }, Bounds()); }
}

bool MurdarPaperMap::IsAvailable() { return !GetDefault<UPaperMapSettings>()->MapTexture.IsNull(); }

TSharedPtr<SPaperMap> MurdarPaperMap::Build(UWorld* World, FSimpleDelegate OnBack)
{
	const UPaperMapSettings* S = GetDefault<UPaperMapSettings>();
	UTexture2D* Texture = S->MapTexture.LoadSynchronous();
	if (!World || !Texture) { return nullptr; }

	TArray<FPaperMapMark> Marks;
	const UNarrativeStateSubsystem* State = UNarrativeStateSubsystem::Get(World);
	for (TObjectIterator<UMapMarkerComponent> It; It; ++It)
	{
		const UMapMarkerComponent* M = *It;
		if (!M->GetOwner() || M->GetWorld() != World) { continue; }
		MurdarMap::FMarkerState St;
		St.bKnown = !M->KnownFact.IsValid() || (State && State->HasFact(M->KnownFact));
		if (!MurdarMap::ShowMarker(MurdarMap::EMarker::Custom, St)) { continue; }
		Marks.Add({ UV(M->GetOwner()->GetActorLocation()), M->Label, S->PencilColor, false, false });
	}
	// The job's place, circled in red (the contact said it; he marked it).
	if (const UJobSubsystem* Jobs = UJobSubsystem::Get(World))
	{
		FVector Where; FText Name;
		if (Jobs->GetTargetLocation(Where, Name)) { Marks.Add({ UV(Where), Name, S->JobColor, true, false }); }
	}
	MurdarMap::FV2 Open{ 0.5f, 0.5f };
	const APlayerController* PC = World->GetFirstPlayerController();
	if (PC && PC->GetPawn())
	{
		Open = UV(PC->GetPawn()->GetActorLocation());
		if (S->bShowPlayer) { Marks.Add({ Open, FText::GetEmpty(), S->PencilColor, false, true }); }
	}
	return SNew(SPaperMap).Texture(Texture).Marks(Marks).OpenAt(Open).MaxZoom(S->MaxZoom).OnBack(OnBack);
}
