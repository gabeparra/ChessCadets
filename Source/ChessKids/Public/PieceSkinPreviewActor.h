#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChessPiece.h"
#include "PieceSkinPreviewActor.generated.h"

class UPointLightComponent;
class USceneCaptureComponent2D;
class UStaticMeshComponent;
class UTextureRenderTarget2D;
class UMaterialInterface;

// A self-contained "showroom" rig for the piece skins picker: a lit pawn piece captured
// into a render target, so the UI can show a real 3D preview of a skin instead of a
// flat icon. Spawned far from the playable area (see PieceSkinsMenuWidget) so it's
// never visible in the main game camera regardless of which level is loaded.
//
// CaptureComponent runs with bCaptureEveryFrame = true rather than being manually
// triggered on demand — every attempt to precisely time a one-shot CaptureScene() call
// (deferred ticks, SendAllEndOfFrameUpdates, FlushRenderingCommands, disabling Temporal
// AA, double-capturing) still left the UI showing a stale frame one interaction behind.
// Continuous capture sidesteps the question entirely: the render target always reflects
// whatever the piece currently looks like, the same way any live camera-feed widget
// works, with no "did the capture actually happen yet" state to get wrong.
UCLASS()
class CHESSKIDS_API APieceSkinPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	APieceSkinPreviewActor();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// Sets the side tint and skin material at once, then respawns the preview piece (if
	// the side actually changed). The capture itself just happens continuously — this
	// only needs to reframe the camera/backdrop/light for the (possibly new) piece.
	UFUNCTION(BlueprintCallable, Category = "Chess|Skins")
	void RefreshPreviewPiece(EChessColor Color, UMaterialInterface* SkinMaterial);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Chess|Skins")
	UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }

	// Tunable framing — adjustable in the editor without a code change, same idea as
	// AChessCamera's ArmLength/PitchAngle/Yaw. Narrower = more zoomed in. The actual
	// camera distance is derived from the piece's real bounds (see FrameCamera) so it
	// always fits regardless of the mesh's true size — this only controls the angle.
	UPROPERTY(EditAnywhere, Category = "Chess|Skins")
	float FOVAngle = 25.f;

	UPROPERTY(EditAnywhere, Category = "Chess|Skins")
	int32 RenderTargetSize = 512;

private:
	UPROPERTY(VisibleAnywhere, Category = "Chess|Skins")
	UPointLightComponent* Light = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Chess|Skins")
	USceneCaptureComponent2D* CaptureComponent = nullptr;

	// A plain white backdrop plane positioned behind the piece every frame — with the
	// level's sky/atmosphere hidden and the capture's view distance clipped short (see
	// FrameCamera), there's nothing else behind the piece for the camera to see, so
	// without this the background would just be empty/black.
	UPROPERTY(VisibleAnywhere, Category = "Chess|Skins")
	UStaticMeshComponent* Backdrop = nullptr;

	UPROPERTY()
	UTextureRenderTarget2D* RenderTarget = nullptr;

	UPROPERTY()
	AChessPiece* PreviewPiece = nullptr;

	UPROPERTY()
	UMaterialInterface* CurrentSkinMaterial = nullptr;

	EChessColor CurrentColor = EChessColor::White;

	// True until FrameCamera() first succeeds for the current PreviewPiece — Tick()
	// keeps retrying every frame until the piece's bounds are valid (e.g. right after a
	// fresh spawn, before the mesh is registered with the renderer), then stops.
	bool bNeedsFraming = true;

	void SpawnPreviewPiece();
	// Positions CaptureComponent (and the backdrop/light) outside the piece's actual
	// world-space bounds, aimed at it — a fixed relative transform can't work here since
	// it doesn't know how tall/wide the pawn mesh really is (a bad guess either crops it
	// out of frame or, worse, puts the camera inside the mesh where backface culling
	// hides everything). Returns false if the piece's bounds aren't valid yet.
	bool FrameCamera();
};
