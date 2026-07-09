#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ChessCamera.generated.h"

class UCameraComponent;
class USpringArmComponent;
class AChessBoard;

UCLASS()
class CHESSKIDS_API AChessCamera : public AActor
{
	GENERATED_BODY()

public:
	AChessCamera();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	USpringArmComponent* SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	UCameraComponent* Camera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	AChessBoard* TargetBoard = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float ArmLength = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float PitchAngle = 55.f;

	// Used when the human plays White (or as the editor-preview default before
	// PlayerColor is resolved at runtime). -90 = white side.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float Yaw = -90.f;

	// Used when the human plays Black — the camera orbits to their side of the board
	// so their pieces sit nearest the camera, same as physical chess.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float BlackSideYaw = 90.f;

private:
	void ApplyView();
};
