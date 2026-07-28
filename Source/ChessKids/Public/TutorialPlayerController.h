#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TutorialPlayerController.generated.h"

class ATutorialBoard;
class ATutorialManager;

UCLASS()
class CHESSKIDS_API ATutorialPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ATutorialPlayerController();
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	ATutorialBoard* Board = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	ATutorialManager* Manager = nullptr;

private:
	void OnClick();
	// Mouse-wheel board zoom, shared with the settings slider's persisted value.
	void ZoomIn();
	void ZoomOut();
	void HandleHover();

	FString HoveredSquare;
};
