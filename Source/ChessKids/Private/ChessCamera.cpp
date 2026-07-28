// ChessCamera.cpp

#include "ChessCamera.h"
#include "ChessBoard.h"
#include "ChessManager.h"
#include "ChessKidsGameInstance.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Engine/World.h"

AChessCamera::AChessCamera()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(Root);
	SpringArm->bDoCollisionTest        = false;
	SpringArm->bUsePawnControlRotation = false;
	SpringArm->bInheritPitch           = false;
	SpringArm->bInheritYaw             = false;
	SpringArm->bInheritRoll            = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
}

void AChessCamera::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyView();
}

void AChessCamera::BeginPlay()
{
	Super::BeginPlay();

	if (!TargetBoard)
		TargetBoard = Cast<AChessBoard>(UGameplayStatics::GetActorOfClass(this, AChessBoard::StaticClass()));

	ApplyView();

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC) PC->SetViewTargetWithBlend(this, 0.f);

	// Orbit to the human's side once AChessManager::BeginPlay (elsewhere this frame)
	// has resolved PlayerColor — actor BeginPlay order isn't guaranteed, so defer.
	TWeakObjectPtr<AChessCamera> WeakThis(this);
	GetWorld()->GetTimerManager().SetTimerForNextTick([WeakThis]()
	{
		AChessCamera* Self = WeakThis.Get();
		if (!Self) return;

		AChessManager* Manager = Cast<AChessManager>(UGameplayStatics::GetActorOfClass(Self, AChessManager::StaticClass()));
		if (Manager && Manager->PlayerColor == EChessColor::Black)
		{
			// Mirror to the opposite side of the board by spinning the already-correct
			// default pose 180° around the world vertical axis. Done in world space
			// (not by negating the local Yaw property) so it's correct regardless of
			// whatever rotation this level's camera actor itself was placed with —
			// a pure world-up rotation can never flip/invert an existing valid view,
			// it only orbits it, so the level-authored tilt/framing is preserved.
			const FQuat FlipYaw180(FVector::UpVector, PI);
			Self->SpringArm->SetWorldRotation(FlipYaw180 * Self->SpringArm->GetComponentQuat());
		}
	});
}

void AChessCamera::ApplyView()
{
	if (IsValid(TargetBoard))
		SetActorLocation(TargetBoard->GetBoardCenter());

	// Player's persisted zoom shrinks the arm toward MinZoomArmFraction (closest).
	float Zoom = 0.f;
	if (const UChessKidsGameInstance* GI = Cast<UChessKidsGameInstance>(GetGameInstance()))
		Zoom = GI->GetBoardZoom();
	const float Frac = FMath::Lerp(1.f, MinZoomArmFraction, Zoom);

	SpringArm->TargetArmLength = ArmLength * Frac;
	SpringArm->SetRelativeRotation(FRotator(-PitchAngle, Yaw, 0.f));
}
