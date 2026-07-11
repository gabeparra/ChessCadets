// ObjectSpawner.h
// Scatters objects (traffic cars, props, foliage) in a ring around the board.
// Design rule baked in: when spawning traffic/objects (SpawnActorClassOverride set),
// they are ALWAYS placed a fixed clearance UNDER the board and kept out of the board
// footprint — the board's play surface is never obstructed, on any level.
// (Renamed from AVegetationSpawner — a CoreRedirect keeps old placed actors working.)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/StaticMesh.h"
#include "ObjectSpawner.generated.h"

class UHierarchicalInstancedStaticMeshComponent;

USTRUCT(BlueprintType)
struct FObjectSpawnEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "500"))
	int32 Count = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ScaleMin = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ScaleMax = 1.3f;
};

UCLASS()
class CHESSKIDS_API AObjectSpawner : public AActor
{
	GENERATED_BODY()

public:
	AObjectSpawner();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Object Spawner")
	TArray<FObjectSpawnEntry> SpawnEntries;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Object Spawner", meta = (ClampMin = "100"))
	float SpawnRadius = 5000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Object Spawner", meta = (ClampMin = "0"))
	float ExclusionRadius = 2500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Object Spawner")
	bool bAlignToSurface = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Object Spawner")
	bool bRandomYaw = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Object Spawner", meta = (ClampMin = "1", ClampMax = "20"))
	int32 MaxPlacementAttempts = 10;

	// Spawn this actor class (e.g. a moving hover-car BP) instead of a plain static mesh.
	// When set, placement follows the "always under the board" rule below.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Object Spawner")
	TSubclassOf<AActor> SpawnActorClassOverride;

	// How far BELOW the board object-spawns are placed. The spawner finds the board's
	// height at runtime, so this works on any level regardless of how high the board
	// floats — objects can never end up over/on the board.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Object Spawner")
	float ClearanceBelowBoard = 350.f;

	// Ground vegetation only (no actor override): vertical offset above the traced ground.
	// Ignored for board-relative object spawns.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Object Spawner")
	float ZOffset = 0.f;

	UFUNCTION(CallInEditor, Category = "Object Spawner")
	void SpawnObjects();

	UFUNCTION(CallInEditor, Category = "Object Spawner")
	void ClearSpawnedObjects();

private:
	UPROPERTY()
	TArray<AActor*> SpawnedActors;

	UPROPERTY()
	TArray<UHierarchicalInstancedStaticMeshComponent*> SpawnedHISMs;

	bool TryGetGroundLocation(const FVector& Origin, float Radius, FVector& OutLocation, FRotator& OutRotation) const;

	// Height of the level's board (ChessBoard or TutorialBoard), found by class name so
	// this class stays decoupled from either board type. 0 if no board is present.
	float FindBoardZ() const;
};
