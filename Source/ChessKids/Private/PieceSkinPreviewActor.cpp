#include "PieceSkinPreviewActor.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Math/RotationMatrix.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

APieceSkinPreviewActor::APieceSkinPreviewActor()
{
	// Ticks just to keep retrying FrameCamera() until the freshly spawned piece's
	// bounds are valid (see Tick/FrameCamera) — the actual capture no longer needs
	// manual triggering at all (see CaptureComponent->bCaptureEveryFrame below).
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Root);
	Light->SetRelativeLocation(FVector(-40.f, -60.f, 160.f));
	// Needs to be this high for the piece to read correctly against auto-exposure —
	// found by live-tuning.
	Light->Intensity = 60000.f;
	Light->AttenuationRadius = 16384.f;
	Light->CastShadows = false;   // a single close-up piece doesn't need shadow cost

	CaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	CaptureComponent->SetupAttachment(Root);
	CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	// Continuous capture rather than manually calling CaptureScene() on demand — every
	// attempt to precisely time a one-shot capture (deferred ticks, forcing render-state
	// flushes, disabling Temporal AA, capturing twice) still left the UI showing a stale
	// frame one interaction behind. This sidesteps the question entirely: the render
	// target always reflects whatever the piece currently looks like, same as any live
	// camera-feed widget, with no "did the capture actually happen yet" state to track.
	CaptureComponent->bCaptureEveryFrame = true;
	CaptureComponent->bCaptureOnMovement = false;
	CaptureComponent->FOVAngle = FOVAngle;

	// Ignore the level's sky/atmosphere/fog entirely and pin exposure to a fixed value —
	// otherwise the capture's brightness shifts with whatever sky/exposure setup the
	// current level happens to have. (Lighting Channel isolation was tried here too, to
	// keep the level's own lights off the piece entirely, but the piece stopped being
	// lit by anything at all — including our own light — so it was dropped; the piece is
	// lit by whatever's actually nearby now, same as everything else in the level.)
	// GI/AO/reflections are left alone since stripping those made the piece far too dark.
	CaptureComponent->ShowFlags.SetAtmosphere(false);
	CaptureComponent->ShowFlags.SetFog(false);
	CaptureComponent->ShowFlags.SetSkyLighting(false);
	CaptureComponent->ShowFlags.SetDynamicShadows(false);
	CaptureComponent->ShowFlags.SetBloom(false);
	CaptureComponent->ShowFlags.SetVignette(false);
	CaptureComponent->ShowFlags.SetColorGrading(false);
	// Manual exposure with a hand-picked bias was tried here — tuned against a mostly
	// empty void, it left the piece looking like a flat black silhouette regardless of
	// how much light actually reached it (confirmed: still black even sitting right on
	// the lit board). Left at whatever the level's default auto-exposure is instead, now
	// that there's a real backdrop plane for it to calibrate against.

	// The background varied by level: the rig floats far above the playable area, but
	// that's still well within range to see each level's own skybox mesh / distant
	// terrain / backdrop geometry, which obviously differs per map. Clip the capture's
	// view distance down to just past the piece itself (FrameCamera tightens this
	// further once the piece's real size is known) so none of that far-away,
	// level-specific geometry can ever be seen at all — only our own Backdrop plane.
	CaptureComponent->MaxViewDistanceOverride = 1000.f;

	// Fallback pose in case FrameCamera ever runs before the piece has valid bounds —
	// a modest offset above/in-front of the root, looking back at it. FrameCamera()
	// replaces this with a properly fitted transform once the piece exists.
	const FVector FallbackCamLoc(150.f, 0.f, 90.f);
	CaptureComponent->SetRelativeLocation(FallbackCamLoc);
	CaptureComponent->SetRelativeRotation((-FallbackCamLoc).Rotation());

	// A plain white plane parked behind the piece — with the level's sky hidden and the
	// view distance clipped short above, there'd otherwise be nothing back there at all,
	// which renders as black. This guarantees a real, level-independent white backdrop.
	Backdrop = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Backdrop"));
	Backdrop->SetupAttachment(Root);
	Backdrop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Backdrop->CastShadow = false;
	if (UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")))
		Backdrop->SetStaticMesh(PlaneMesh);
	// A neutral mid-gray reads clearly against both white and black pieces — pure white
	// washes out against white pieces the same way pure black would against black ones.
	// Built from WhitePiece (same reliable, always-present material real board pieces
	// use, confirmed to accept a BaseColor tint) rather than deriving it from whatever
	// skin the piece currently has equipped, so the backdrop color is fixed and doesn't
	// shift depending on which skin is being previewed.
	if (UMaterialInterface* BackdropBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/MapTemplates/Materials/WhitePiece.WhitePiece")))
	{
		if (UMaterialInstanceDynamic* BackdropMID = UMaterialInstanceDynamic::Create(BackdropBase, this))
		{
			BackdropMID->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.5f, 0.5f, 0.5f));
			Backdrop->SetMaterial(0, BackdropMID);
		}
	}
}

void APieceSkinPreviewActor::BeginPlay()
{
	Super::BeginPlay();

	RenderTarget = NewObject<UTextureRenderTarget2D>(this);
	RenderTarget->RenderTargetFormat = RTF_RGBA8;
	RenderTarget->InitAutoFormat(RenderTargetSize, RenderTargetSize);
	RenderTarget->UpdateResourceImmediate(true);
	CaptureComponent->TextureTarget = RenderTarget;

	// Deliberately no piece spawn here — the widget always calls RefreshPreviewPiece()
	// with the real class/color/skin immediately after spawning this actor.
}

void APieceSkinPreviewActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Keeps retrying every frame until the piece's bounds are valid (e.g. right after a
	// fresh spawn, before the mesh is registered with the renderer) — with continuous
	// capture, a frame or two of the camera sitting at the fallback pose is harmless and
	// self-corrects the moment this succeeds, rather than needing to gate a one-shot
	// capture on it.
	if (bNeedsFraming)
		bNeedsFraming = !FrameCamera();
}

void APieceSkinPreviewActor::SpawnPreviewPiece()
{
	if (IsValid(PreviewPiece))
	{
		PreviewPiece->Destroy();
		PreviewPiece = nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = this;

	// Inherits the rig's own spawn rotation (e.g. the Yaw 90 set where this actor is
	// spawned) rather than a hardcoded zero — FrameCamera positions the camera/light in
	// absolute world space every frame, ignoring the rig's rotation entirely, so this
	// is the only thing that actually controls which face of the piece faces the camera.
	FVector SpawnLoc = GetActorLocation();
	FRotator SpawnRot = GetActorRotation();
	if (CurrentColor == EChessColor::Black)
	{
		// White and Black pieces face opposite directions on a real board —
		// SetupMeshAndMaterial (below, via Init) applies a 180 yaw to the mesh itself
		// specifically for White, so as-is White and Black end up facing opposite ways
		// in world space. Since the preview camera sits at one fixed angle, this
		// compensates on the Black side so both colors face the camera the same way.
		SpawnRot.Yaw += 180.f;
	}
	PreviewPiece = GetWorld()->SpawnActor<AChessPiece>(SpawnLoc, SpawnRot, Params);
	if (PreviewPiece)
	{
		// Dummy board coordinates — the preview piece was confirmed safe to spawn
		// standalone: Init()/SetupMeshAndMaterial()/ApplyPieceSkin() have no dependency
		// on a live AChessManager/AChessBoard. Real board pieces spawn exactly this way
		// too (AChessManager::SpawnPiece's PieceClass branch is unused by this project —
		// every configured piece has PieceClass=None) with the same nullptr MeshOverride,
		// so this is the identical mesh-loading path as actual gameplay, not a fallback.
		PreviewPiece->Init(EChessPieceType::Pawn, CurrentColor, 0, 0);

		// USkeletalMeshComponent defaults to only ticking its pose once it's been seen
		// by a primary (game camera) view — this rig is spawned far from the playable
		// area and is only ever seen by our SceneCaptureComponent2D, which doesn't count
		// as "being rendered" for that check. Left at the default, the mesh's pose/bounds
		// never initialize and it silently never appears in the capture (this is exactly
		// why it worked when temporarily spawned on the actual board, near the main camera).
		if (PreviewPiece->MeshComp)
			PreviewPiece->MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}

	// A newly spawned piece may not have valid bounds this exact frame — Tick() keeps
	// reframing every frame until it does.
	bNeedsFraming = true;
}

void APieceSkinPreviewActor::RefreshPreviewPiece(EChessColor Color, UMaterialInterface* SkinMaterial)
{
	CurrentSkinMaterial = SkinMaterial;

	// Only respawns the actor when the side actually changes (which changes the
	// rotation/tint) — the overwhelming majority of calls here are just browsing skins
	// with the same color, which only needs a material swap on the piece that's already
	// sitting there.
	const bool bNeedsRespawn = !IsValid(PreviewPiece) || CurrentColor != Color;
	CurrentColor = Color;

	if (bNeedsRespawn)
		SpawnPreviewPiece();

	if (IsValid(PreviewPiece))
		PreviewPiece->ApplyPieceSkin(SkinMaterial);
}

bool APieceSkinPreviewActor::FrameCamera()
{
	if (!IsValid(PreviewPiece) || !CaptureComponent) return false;

	// Bounds-fit rather than a fixed offset: a guessed distance either crops the piece
	// out of frame or (as happened when the camera was placed exactly at the piece's
	// own origin) puts the camera inside the mesh, where backface culling means nothing
	// renders at all. This works regardless of how tall/wide the pawn mesh really is.
	const FBox Bounds = PreviewPiece->GetComponentsBoundingBox(true);
	if (!Bounds.IsValid) return false;

	const FVector Center = Bounds.GetCenter();
	const float Radius = FMath::Max(Bounds.GetExtent().Size(), 1.f);
	const float HalfFOVRad = FMath::DegreesToRadians(FMath::Max(FOVAngle, 1.f) * 0.5f);
	// 1.3x margin so the piece doesn't touch the frame edges.
	const float FitDistance = (Radius / FMath::Sin(HalfFOVRad)) * 1.3f;

	// A pleasant elevated 3/4 viewing angle, scaled out to whatever distance fits.
	static const FVector ViewDir = FVector(150.f, 0.f, 90.f).GetSafeNormal();
	const FVector CamWorldLoc = Center + ViewDir * FitDistance;

	CaptureComponent->SetWorldLocation(CamWorldLoc);
	CaptureComponent->SetWorldRotation((Center - CamWorldLoc).Rotation());

	// Backdrop sits on the opposite side of the piece from the camera, facing back
	// toward it, scaled to comfortably fill the whole frame at that distance.
	const float BackdropDistance = FitDistance * 2.f;
	const float TotalDistance = FitDistance + BackdropDistance;
	if (Backdrop)
	{
		const FVector BackdropLoc = Center - ViewDir * BackdropDistance;
		Backdrop->SetWorldLocation(BackdropLoc);
		// Plane.Plane's face normal is local +Z by default — align that with ViewDir so
		// it faces back toward the camera, rather than a look-at rotation (which orients
		// local +X at the target and would leave the visible face sideways).
		Backdrop->SetWorldRotation(FRotationMatrix::MakeFromZ(ViewDir).Rotator());
		const float FrameSize = 2.f * TotalDistance * FMath::Tan(HalfFOVRad) * 1.5f;   // 1.5x margin
		Backdrop->SetWorldScale3D(FVector(FMath::Max(FrameSize / 100.f, 0.1f)));   // Plane.Plane is 100x100 units at scale 1
	}

	// Tight enough to fit the piece and backdrop, nowhere near far enough to reach any
	// level's skybox/terrain/backdrop — that's what was leaking through as a per-level
	// background before.
	CaptureComponent->MaxViewDistanceOverride = FMath::Max(FitDistance * 2.5f, TotalDistance * 1.2f);

	// Light rides exactly at the camera position (a "headlamp") instead of sitting at a
	// fixed spot or some Radius-scaled offset from it — those risked landing too close
	// to (or even inside) the mesh for a piece this size, which is most of why so little
	// light was actually reaching it. Co-locating with the camera guarantees a clear,
	// unobstructed line of sight to exactly whatever the camera is currently framing,
	// regardless of the piece's real scale. AttenuationRadius is left at its generous
	// constructor default (16384) rather than computed here — comfortably covers any
	// preview piece without needing to be right each time.
	if (Light)
		Light->SetWorldLocation(CamWorldLoc);

	return true;
}
