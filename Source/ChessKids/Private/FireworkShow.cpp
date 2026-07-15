#include "FireworkShow.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"

AFireworkShow::AFireworkShow()
{
	PrimaryActorTick.bCanEverTick = true;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void AFireworkShow::BeginPlay()
{
	Super::BeginPlay();
	LaunchShell();   // first bang immediately — the win should feel instant
}

void AFireworkShow::LaunchShell()
{
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UMaterialInterface* TintBase = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Materials/BoardThemes/M_SquareTint.M_SquareTint"));
	if (!Sphere) return;

	FShell Shell;
	const float Ang = FMath::FRandRange(0.f, 2.f * PI);
	const float Dist = FMath::FRandRange(0.2f, 1.f) * SpreadRadius;
	Shell.Start = GetActorLocation() + FVector(FMath::Cos(Ang) * Dist, FMath::Sin(Ang) * Dist, 0.f);
	Shell.Apex = Shell.Start + FVector(0.f, 0.f, PopHeight * FMath::FRandRange(0.75f, 1.25f));
	Shell.Color = FLinearColor(FMath::FRandRange(0.f, 360.f), 0.85f, 1.f).HSVToLinearRGB();
	Shell.RiseTime = FMath::FRandRange(0.6f, 1.0f);

	Shell.Mesh = NewObject<UStaticMeshComponent>(this);
	Shell.Mesh->SetupAttachment(GetRootComponent());
	Shell.Mesh->RegisterComponent();
	Shell.Mesh->SetStaticMesh(Sphere);
	Shell.Mesh->SetWorldLocation(Shell.Start);
	Shell.Mesh->SetWorldScale3D(FVector(0.25f));
	Shell.Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (TintBase)
	{
		UMaterialInstanceDynamic* DMI = UMaterialInstanceDynamic::Create(TintBase, this);
		DMI->SetVectorParameterValue(TEXT("Color"), Shell.Color);
		DMI->SetScalarParameterValue(TEXT("EmissiveStrength"), 20.f);
		Shell.Mesh->SetMaterial(0, DMI);
	}

	Shell.Light = NewObject<UPointLightComponent>(this);
	Shell.Light->SetupAttachment(GetRootComponent());
	Shell.Light->RegisterComponent();
	Shell.Light->SetWorldLocation(Shell.Apex);
	Shell.Light->SetLightColor(Shell.Color);
	Shell.Light->SetIntensity(0.f);
	Shell.Light->SetAttenuationRadius(2500.f);
	Shell.Light->SetCastShadows(false);
	Shell.Light->SetMobility(EComponentMobility::Movable);

	Shells.Add(Shell);
}

void AFireworkShow::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	Elapsed += DeltaTime;
	SinceLaunch += DeltaTime;
	if (Elapsed < ShowDuration && SinceLaunch >= LaunchInterval)
	{
		SinceLaunch = 0.f;
		LaunchShell();
	}

	for (int32 i = Shells.Num() - 1; i >= 0; --i)
	{
		FShell& S = Shells[i];
		S.Age += DeltaTime;

		if (S.Age <= S.RiseTime)
		{
			// Rise: ease up toward the apex, shrinking slightly like a real shell.
			const float A = S.Age / S.RiseTime;
			if (IsValid(S.Mesh))
				S.Mesh->SetWorldLocation(FMath::Lerp(S.Start, S.Apex, FMath::InterpEaseOut(0.f, 1.f, A, 2.f)));
		}
		else if (S.Age <= S.RiseTime + S.FlashTime)
		{
			// Pop: hide the shell, flash the light hard then decay.
			const float A = (S.Age - S.RiseTime) / S.FlashTime;
			if (IsValid(S.Mesh) && S.Mesh->IsVisible())
				S.Mesh->SetVisibility(false);
			if (IsValid(S.Light))
				S.Light->SetIntensity(60000.f * FMath::Square(1.f - A));
		}
		else
		{
			if (IsValid(S.Mesh)) S.Mesh->DestroyComponent();
			if (IsValid(S.Light)) S.Light->DestroyComponent();
			Shells.RemoveAtSwap(i);
		}
	}

	if (Elapsed >= ShowDuration && Shells.Num() == 0)
		Destroy();
}
