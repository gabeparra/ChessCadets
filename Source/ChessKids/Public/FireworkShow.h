#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FireworkShow.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

// Asset-free celebration: for ShowDuration seconds, launches glowing "shells" that
// rise, pop into a bright point-light flash, and fade. Spawned by AChessManager when
// the player wins the AXIOM finale. Self-destroys when the show ends.
UCLASS()
class CHESSKIDS_API AFireworkShow : public AActor
{
	GENERATED_BODY()

public:
	AFireworkShow();
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fireworks")
	float ShowDuration = 14.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fireworks")
	float LaunchInterval = 0.55f;

	// How far around the actor shells appear, and how high above it they pop.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fireworks")
	float SpreadRadius = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fireworks")
	float PopHeight = 600.f;

protected:
	virtual void BeginPlay() override;

private:
	struct FShell
	{
		UStaticMeshComponent* Mesh = nullptr;
		UPointLightComponent* Light = nullptr;
		FVector Start = FVector::ZeroVector;
		FVector Apex = FVector::ZeroVector;
		FLinearColor Color = FLinearColor::White;
		float Age = 0.f;
		float RiseTime = 0.8f;
		float FlashTime = 0.9f;
	};

	TArray<FShell> Shells;
	float Elapsed = 0.f;
	float SinceLaunch = 0.f;

	void LaunchShell();
};
