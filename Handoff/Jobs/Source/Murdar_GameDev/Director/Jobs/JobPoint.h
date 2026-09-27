// A place jobs use, placed in the map: a pickup (a kiosk's back door, a warehouse), a drop (a garage, a yard), or a
// drop at the border (a ford on the Prut, a customs yard) for smuggling. PlaceName is what the contact says.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JobPoint.generated.h"

UCLASS()
class MURDAR_GAMEDEV_API AJobPoint : public AActor
{
	GENERATED_BODY()

public:
	AJobPoint();

	/** „garajul din spatele Gării”, „vadul de la Ungheni”. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Job") FText PlaceName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Job") bool bPickup = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Job") bool bDrop = true;
	/** A drop at the border: smuggling jobs go only here, other deliveries never. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Job") bool bBorder = false;
};
