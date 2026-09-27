#include "Director/Jobs/JobPoint.h"

#include "Components/BillboardComponent.h"

AJobPoint::AJobPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
#if WITH_EDITORONLY_DATA
	if (UBillboardComponent* Icon = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Icon"))) { Icon->SetupAttachment(RootComponent); }
#endif
}
