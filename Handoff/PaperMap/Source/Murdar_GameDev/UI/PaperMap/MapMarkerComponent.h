// Put on anything the paper map should show once he knows it: a safehouse, a garage, a contact's bar, a payphone.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "MapMarkerComponent.generated.h"

UENUM(BlueprintType)
enum class EMapMarkerKind : uint8 { Safehouse, Garage, Payphone, Contact, Custom };

UCLASS(ClassGroup = (Murdar), meta = (BlueprintSpawnableComponent))
class MURDAR_GAMEDEV_API UMapMarkerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map") EMapMarkerKind Kind = EMapMarkerKind::Custom;
	/** Written next to the mark, as he would: „casa”, „Marian – service”, „telefon”. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map") FText Label;
	/** Shown only once this fact is set (found it, bought it, was told). None = always. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Map", meta = (Categories = "Fact")) FGameplayTag KnownFact;
};
