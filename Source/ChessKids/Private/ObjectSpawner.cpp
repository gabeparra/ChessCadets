// ObjectSpawner.cpp

#include "ObjectSpawner.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "EngineUtils.h"

AObjectSpawner::AObjectSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AObjectSpawner::BeginPlay()
{
	Super::BeginPlay();
	SpawnObjects();
}

float AObjectSpawner::FindBoardZ() const
{
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetClass()->GetName().Contains(TEXT("Board")))
				return It->GetActorLocation().Z;
		}
	}
	return 0.f;
}

void AObjectSpawner::SpawnObjects()
{
	UWorld* World = GetWorld();
	if (!World) return;

	ClearSpawnedObjects();

	USceneComponent* Root = GetRootComponent();
	if (!Root)
	{
		Root = NewObject<USceneComponent>(this, TEXT("SpawnerRoot"));
		Root->RegisterComponent();
		SetRootComponent(Root);
	}

	const FVector ActorOrigin = GetActorLocation();
	const FVector BoardCentre = FVector::ZeroVector; // board is at world origin

	// Object/traffic spawns are placed relative to the board so they always sit under it.
	// Vegetation (no actor override) keeps sitting on the traced ground.
	const bool bBoardRelative = (SpawnActorClassOverride != nullptr);
	const float ObjectZ = FindBoardZ() - ClearanceBelowBoard;

	for (const FObjectSpawnEntry& Entry : SpawnEntries)
	{
		UStaticMesh* LoadedMesh = Entry.Mesh.LoadSynchronous();
		if (!LoadedMesh) continue;

		UHierarchicalInstancedStaticMeshComponent* HISM = nullptr;
		if (!SpawnActorClassOverride)
		{
			HISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
			HISM->SetStaticMesh(LoadedMesh);
			HISM->SetMobility(Root->Mobility);
			HISM->SetupAttachment(Root);
			HISM->RegisterComponent();
			SpawnedHISMs.Add(HISM);
		}

		int32 Placed = 0;
		int32 TotalAttempts = 0;
		const int32 MaxTotalAttempts = Entry.Count * MaxPlacementAttempts;

		while (Placed < Entry.Count && TotalAttempts < MaxTotalAttempts)
		{
			TotalAttempts++;

			FVector SpawnLocation;
			FRotator SpawnRotation;
			if (!TryGetGroundLocation(ActorOrigin, SpawnRadius, SpawnLocation, SpawnRotation))
				continue;

			// Never spawn on the board footprint.
			if (FVector::Dist2D(SpawnLocation, BoardCentre) < ExclusionRadius)
				continue;

			if (bRandomYaw)
				SpawnRotation.Yaw = FMath::RandRange(0.f, 360.f);

			const float Scale = FMath::RandRange(Entry.ScaleMin, Entry.ScaleMax);

			// Objects: fixed height a set clearance UNDER the board (surface never obstructed).
			// Vegetation: on the ground plus its ZOffset.
			const FVector FinalLocation = bBoardRelative
				? FVector(SpawnLocation.X, SpawnLocation.Y, ObjectZ)
				: SpawnLocation + FVector(0.f, 0.f, ZOffset);

			if (HISM)
			{
				HISM->AddInstance(FTransform(SpawnRotation, FinalLocation, FVector(Scale)), /*bWorldSpace=*/true);
				Placed++;
			}
			else
			{
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				AActor* NewActor = World->SpawnActor<AActor>(SpawnActorClassOverride.Get(), FinalLocation, SpawnRotation, Params);
				if (NewActor)
				{
					if (UStaticMeshComponent* SMC = NewActor->FindComponentByClass<UStaticMeshComponent>())
						SMC->SetStaticMesh(LoadedMesh);
					NewActor->SetActorScale3D(FVector(Scale));
					SpawnedActors.Add(NewActor);
					Placed++;
				}
			}
		}
	}
}

void AObjectSpawner::ClearSpawnedObjects()
{
	for (AActor* Actor : SpawnedActors)
	{
		if (IsValid(Actor))
			Actor->Destroy();
	}
	SpawnedActors.Empty();

	for (UHierarchicalInstancedStaticMeshComponent* HISM : SpawnedHISMs)
	{
		if (IsValid(HISM))
		{
			HISM->ClearInstances();
			HISM->DestroyComponent();
		}
	}
	SpawnedHISMs.Empty();
}

bool AObjectSpawner::TryGetGroundLocation(const FVector& Origin, float Radius, FVector& OutLocation, FRotator& OutRotation) const
{
	UWorld* World = GetWorld();
	if (!World) return false;

	const float Angle = FMath::RandRange(0.f, 360.f) * (PI / 180.f);
	const float Dist  = FMath::RandRange(0.f, Radius);
	const FVector RandomOffset(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 0.f);
	const FVector TraceStart = Origin + RandomOffset + FVector(0.f, 0.f, 5000.f);
	const FVector TraceEnd   = Origin + RandomOffset - FVector(0.f, 0.f, 5000.f);

	FHitResult Hit;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
	{
		OutLocation = Hit.ImpactPoint;
		OutRotation = bAlignToSurface ? FRotationMatrix::MakeFromZ(Hit.ImpactNormal).Rotator() : FRotator::ZeroRotator;
		return true;
	}

	// No ground hit — fall back to the random XY at the spawner's height so object spawns
	// (which override Z anyway) still populate on levels with no ground under the ring.
	OutLocation = Origin + RandomOffset;
	OutRotation = FRotator::ZeroRotator;
	return true;
}
