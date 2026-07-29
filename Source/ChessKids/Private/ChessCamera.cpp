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

	ApplyView();

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC) PC->SetViewTargetWithBlend(this, 0.f);

	// Re-frame until the board exists and PlayerColor is resolved. One deferred
	// tick isn't enough on World Partition maps (L_Field): the board can stream
	// in well after this camera's BeginPlay, which left the camera stuck at its
	// placed transform staring at nothing. Retry briefly, then stop.
	TWeakObjectPtr<AChessCamera> WeakThis(this);
	TSharedRef<FTimerHandle> Retry = MakeShared<FTimerHandle>();
	TSharedRef<int32> TriesLeft = MakeShared<int32>(24);   // 24 * 0.25s = 6s cap
	GetWorld()->GetTimerManager().SetTimer(*Retry,
		FTimerDelegate::CreateLambda([WeakThis, Retry, TriesLeft]()
		{
			AChessCamera* Self = WeakThis.Get();
			if (!Self) return;

			// ApplyView re-resolves the board and is side-aware + idempotent.
			Self->ApplyView();
			if (IsValid(Self->TargetBoard) || --(*TriesLeft) <= 0)
				Self->GetWorldTimerManager().ClearTimer(*Retry);
		}),
		0.25f, /*bLoop=*/true, /*FirstDelay=*/0.f);
}

void AChessCamera::ApplyView()
{
	if (!TargetBoard)
		TargetBoard = Cast<AChessBoard>(UGameplayStatics::GetActorOfClass(this, AChessBoard::StaticClass()));
	if (IsValid(TargetBoard))
		SetActorLocation(TargetBoard->GetBoardCenter());

	// Player's persisted zoom shrinks the arm toward MinZoomArmFraction (closest).
	float Zoom = 0.f;
	if (const UChessKidsGameInstance* GI = Cast<UChessKidsGameInstance>(GetGameInstance()))
		Zoom = GI->GetBoardZoom();
	const float Frac = FMath::Lerp(1.f, MinZoomArmFraction, Zoom);

	SpringArm->TargetArmLength = ArmLength * Frac;
	SpringArm->SetRelativeRotation(FRotator(-PitchAngle, Yaw, 0.f));

	// Mirror to the Black side of the board when the human plays Black. Done in
	// world space by spinning the already-correct default pose 180° around the
	// world vertical axis — a pure world-up rotation can never flip/invert a
	// valid view, it only orbits it, so level-authored tilt/framing is kept.
	// Because the relative rotation is reset above, re-running ApplyView (e.g.
	// on every zoom change) stays idempotent instead of flipping repeatedly.
	AChessManager* Manager = Cast<AChessManager>(
		UGameplayStatics::GetActorOfClass(this, AChessManager::StaticClass()));
	if (Manager && Manager->PlayerColor == EChessColor::Black)
	{
		const FQuat FlipYaw180(FVector::UpVector, PI);
		SpringArm->SetWorldRotation(FlipYaw180 * SpringArm->GetComponentQuat());
	}
}
