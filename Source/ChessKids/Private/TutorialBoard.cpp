#include "TutorialBoard.h"
#include "ChessPiece.h"
#include "ChessKidsGameInstance.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"

// Same contrast guarantee as the arena boards (ChessBoard.cpp): light squares are
// always bright and pale, dark squares always dim and saturated — the recognizable
// pattern cannot wash out no matter what colors reach the board.
static FLinearColor ClampTutorialBoardColor(const FLinearColor& In, bool bLight)
{
	FLinearColor HSV = In.LinearRGBToHSV();   // R = hue, G = sat, B = value
	if (bLight)
	{
		HSV.B = FMath::Max(HSV.B, 0.85f);
		HSV.G = FMath::Min(HSV.G, 0.50f);
	}
	else
	{
		HSV.B = FMath::Min(HSV.B, 0.28f);
		HSV.G = FMath::Max(HSV.G, 0.40f);
	}
	return HSV.HSVToLinearRGB();
}

ATutorialBoard::ATutorialBoard()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void ATutorialBoard::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildBoard();
}

void ATutorialBoard::BeginPlay()
{
	Super::BeginPlay();

	// Match the arena boards exactly: the player's saved custom hues win over any
	// per-instance colors baked into the map, so tutorials always show the same
	// board pattern the rest of the game does.
	if (const UChessKidsGameInstance* GI = Cast<UChessKidsGameInstance>(GetGameInstance()))
	{
		if (GI->bCustomBoardColors)
		{
			LightSquareColor = UChessKidsGameInstance::LightColorFromHue(GI->LightSquareHue);
			DarkSquareColor  = UChessKidsGameInstance::DarkColorFromHue(GI->DarkSquareHue);
		}
	}
	BuildBoard();   // rebuild with the (clamped) final colors
	UpdateHighlightContrastColor();
}

void ATutorialBoard::UpdateHighlightContrastColor()
{
	// Same "inverse" highlight system as the arena boards: the marker color is the
	// hue farthest from BOTH square hues (midpoint of the larger arc between them),
	// full saturation/value — guaranteed contrast against any board palette.
	if (LegalMoveMaterial && !LegalMoveDMI)
		LegalMoveDMI = UMaterialInstanceDynamic::Create(LegalMoveMaterial, this);
	if (SelectionMaterial && !SelectionDMI)
		SelectionDMI = UMaterialInstanceDynamic::Create(SelectionMaterial, this);
	if (!LegalMoveDMI && !SelectionDMI) return;

	const float HL = LightSquareColor.LinearRGBToHSV().R;
	const float HD = DarkSquareColor.LinearRGBToHSV().R;
	const float Lo = FMath::Min(HL, HD), Hi = FMath::Max(HL, HD);
	const float InnerArc = Hi - Lo;
	const float Hue = (InnerArc >= 360.f - InnerArc)
		? Lo + InnerArc * 0.5f
		: FMath::Fmod(Hi + (360.f - InnerArc) * 0.5f, 360.f);

	if (LegalMoveDMI)
		LegalMoveDMI->SetVectorParameterValue(TEXT("Color"),
			FLinearColor(Hue, 1.f, 1.f).HSVToLinearRGB());
	if (SelectionDMI)   // 45 degrees off the marker hue: related but distinguishable
		SelectionDMI->SetVectorParameterValue(TEXT("Color"),
			FLinearColor(FMath::Fmod(Hue + 45.f, 360.f), 1.f, 1.f).HSVToLinearRGB());
}

int32 ATutorialBoard::GridIndex(int32 File, int32 Rank) const
{
	if (File < 0 || File >= NumFiles || Rank < 0 || Rank >= NumRanks) return -1;
	return File + Rank * NumFiles;
}

bool ATutorialBoard::IsValidSquare(int32 File, int32 Rank) const
{
	return File >= 0 && File < NumFiles && Rank >= 0 && Rank < NumRanks;
}

void ATutorialBoard::EnsureTintDMIs()
{
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Materials/BoardThemes/M_SquareTint.M_SquareTint"));
	if (!Base) { LightTintDMI = nullptr; DarkTintDMI = nullptr; return; }
	LightTintDMI = UMaterialInstanceDynamic::Create(Base, this);
	DarkTintDMI = UMaterialInstanceDynamic::Create(Base, this);
	LightTintDMI->SetVectorParameterValue(TEXT("Color"), ClampTutorialBoardColor(LightSquareColor, true));
	DarkTintDMI->SetVectorParameterValue(TEXT("Color"), ClampTutorialBoardColor(DarkSquareColor, false));
}

void ATutorialBoard::AutoLoadMaterials()
{
	if (!LegalMoveMaterial)
		LegalMoveMaterial = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/Materials/Board/M_LegalMove.M_LegalMove"));
	if (!SelectionMaterial)
		SelectionMaterial = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/Materials/Board/M_Selection.M_Selection"));
	if (!HoverMaterial)
		HoverMaterial = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/Materials/Board/M_Hover.M_Hover"));
}

void ATutorialBoard::BuildBoard()
{
	for (UStaticMeshComponent* M : SquareMeshes) if (IsValid(M)) M->DestroyComponent();
	for (UStaticMeshComponent* M : HighlightMeshes) if (IsValid(M)) M->DestroyComponent();
	SquareMeshes.Empty();
	HighlightMeshes.Empty();

	AutoLoadMaterials();

	UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(
		nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (!PlaneMesh) return;

	EnsureTintDMIs();
	UMaterialInterface* LightMat = LightTintDMI ? (UMaterialInterface*)LightTintDMI : nullptr;
	UMaterialInterface* DarkMat = DarkTintDMI ? (UMaterialInterface*)DarkTintDMI : nullptr;

	const float Scale = SquareSize / 100.f;
	const float HalfX = (NumFiles - 1) * 0.5f * SquareSize;
	const float HalfY = (NumRanks - 1) * 0.5f * SquareSize;

	for (int32 Rank = 0; Rank < NumRanks; ++Rank)
	{
		for (int32 File = 0; File < NumFiles; ++File)
		{
			const FVector LocalPos(File * SquareSize - HalfX, HalfY - Rank * SquareSize, 0.f);
			const bool bLight = ((File + Rank) % 2 != 0);

			UStaticMeshComponent* Sq = NewObject<UStaticMeshComponent>(this,
				*FString::Printf(TEXT("Sq_%d_%d"), File, Rank));
			Sq->SetupAttachment(GetRootComponent());
			Sq->RegisterComponent();
			Sq->SetStaticMesh(PlaneMesh);
			Sq->SetRelativeLocation(LocalPos);
			Sq->SetRelativeScale3D(FVector(Scale, Scale, 1.f));
			Sq->SetMaterial(0, bLight ? LightMat : DarkMat);
			Sq->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			SquareMeshes.Add(Sq);

			UStaticMeshComponent* Hl = NewObject<UStaticMeshComponent>(this,
				*FString::Printf(TEXT("Hl_%d_%d"), File, Rank));
			Hl->SetupAttachment(GetRootComponent());
			Hl->RegisterComponent();
			Hl->SetStaticMesh(PlaneMesh);
			Hl->SetRelativeLocation(LocalPos + FVector(0.f, 0.f, HighlightZOffset));
			Hl->SetRelativeScale3D(FVector(Scale * HighlightScaleFactor, Scale * HighlightScaleFactor, 5.f));
			Hl->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Hl->SetVisibility(false);
			HighlightMeshes.Add(Hl);
		}
	}
}

FVector ATutorialBoard::GridToWorldLocation(int32 File, int32 Rank, float ZOffset) const
{
	if (!IsValidSquare(File, Rank)) return FVector::ZeroVector;

	const float HalfX = (NumFiles - 1) * 0.5f * SquareSize;
	const float HalfY = (NumRanks - 1) * 0.5f * SquareSize;

	const FVector Local(File * SquareSize - HalfX, HalfY - Rank * SquareSize, ZOffset);
	return GetActorTransform().TransformPosition(Local);
}

bool ATutorialBoard::WorldToGrid(FVector WorldLoc, int32& OutFile, int32& OutRank) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLoc);
	const float HalfX = NumFiles * 0.5f * SquareSize;
	const float HalfY = NumRanks * 0.5f * SquareSize;

	OutFile = FMath::FloorToInt((Local.X + HalfX) / SquareSize);
	OutRank = FMath::FloorToInt((HalfY - Local.Y) / SquareSize);
	return IsValidSquare(OutFile, OutRank);
}

FVector ATutorialBoard::GetBoardCenter() const
{
	return GetActorLocation();
}

UStaticMeshComponent* ATutorialBoard::GetHighlightMesh(int32 File, int32 Rank) const
{
	const int32 Idx = GridIndex(File, Rank);
	if (Idx < 0 || !HighlightMeshes.IsValidIndex(Idx)) return nullptr;
	return HighlightMeshes[Idx];
}

void ATutorialBoard::ClearHighlights()
{
	for (UStaticMeshComponent* Hl : HighlightMeshes)
		if (IsValid(Hl)) Hl->SetVisibility(false);
}

void ATutorialBoard::HighlightSquare(int32 File, int32 Rank, UMaterialInterface* Material)
{
	UStaticMeshComponent* Hl = GetHighlightMesh(File, Rank);
	if (!Hl) return;
	if (Material) Hl->SetMaterial(0, Material);
	Hl->SetVisibility(true);
}

void ATutorialBoard::SelectSquare(int32 File, int32 Rank)
{
	ClearHighlights();
	HighlightSquare(File, Rank, SelectionDMI ? static_cast<UMaterialInterface*>(SelectionDMI) : SelectionMaterial);
}

void ATutorialBoard::ShowLegalMoves(const TArray<FIntPoint>& Squares)
{
	for (const FIntPoint& Sq : Squares)
		HighlightSquare(Sq.X, Sq.Y, LegalMoveDMI ? static_cast<UMaterialInterface*>(LegalMoveDMI) : LegalMoveMaterial);
}

void ATutorialBoard::TintSquare(int32 File, int32 Rank, FLinearColor Color)
{
	const int32 Idx = GridIndex(File, Rank);
	if (Idx < 0 || !SquareMeshes.IsValidIndex(Idx)) return;

	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Materials/BoardThemes/M_SquareTint.M_SquareTint"));
	if (!Base) return;

	UMaterialInstanceDynamic* DMI = UMaterialInstanceDynamic::Create(Base, this);
	DMI->SetVectorParameterValue(TEXT("Color"), Color);
	SquareMeshes[Idx]->SetMaterial(0, DMI);
}

void ATutorialBoard::DisableSquare(int32 File, int32 Rank)
{
	const int32 Idx = GridIndex(File, Rank);
	if (Idx < 0 || !SquareMeshes.IsValidIndex(Idx)) return;
	if (DisabledSquareMaterial)
		SquareMeshes[Idx]->SetMaterial(0, DisabledSquareMaterial);
}

void ATutorialBoard::DisableSquares(const TArray<FIntPoint>& Squares)
{
	for (const FIntPoint& Sq : Squares)
		DisableSquare(Sq.X, Sq.Y);
}

void ATutorialBoard::GlideActorToSquare(AActor* ActorToMove, int32 File, int32 Rank, float ZOffset)
{
	if (!ActorToMove) return;
	const FVector Target = GridToWorldLocation(File, Rank, ZOffset);
	if (AChessPiece* Piece = Cast<AChessPiece>(ActorToMove))
		Piece->StartGlide(Target);
	else
		ActorToMove->SetActorLocation(Target, false, nullptr, ETeleportType::TeleportPhysics);
}
