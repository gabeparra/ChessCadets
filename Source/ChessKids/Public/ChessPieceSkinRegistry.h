#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ChessPiece.h"
#include "ChessPieceSkinRegistry.generated.h"

class UMaterialInterface;
class UTexture2D;

// One collectible piece skin. Authored in DA_PieceSkinRegistry, consumed by the
// piece skins menu UI and UChessKidsGameInstance's skin inventory.
USTRUCT(BlueprintType)
struct FChessPieceSkinEntry
{
	GENERATED_BODY()

	// Stable identifier used by UChessKidsGameInstance::SelectPieceSkin/UnlockPieceSkin.
	// Must be unique within the registry.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess|Skin")
	FName SkinId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess|Skin")
	FText DisplayName;

	// Assigned to the piece mesh's material slot when this skin is equipped.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess|Skin")
	TSoftObjectPtr<UMaterialInterface> Material;

	// Preview icon shown on the skin's swatch button in the picker UI.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess|Skin")
	TObjectPtr<UTexture2D> Thumbnail = nullptr;

	// Seeded into UnlockedPieceSkins the first time the game runs (no existing save).
	// Locked entries stay locked until a future store/achievement system unlocks them.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess|Skin")
	bool bUnlockedByDefault = false;

	// If set, completing TutorialUnlockPiece's tutorial (the boss phase) automatically
	// unlocks this skin and shows the "new skin!" popup. At most one skin should set
	// this per piece — UChessKidsGameInstance::UnlockSkinForTutorial grants the first
	// match it finds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess|Skin")
	bool bUnlockedByTutorial = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess|Skin", meta = (EditCondition = "bUnlockedByTutorial"))
	EChessPieceType TutorialUnlockPiece = EChessPieceType::Pawn;
};

// Data-driven catalog of collectible piece skins. Create one instance
// (DA_PieceSkinRegistry) under Content/Data and point UChessKidsGameInstance at it.
UCLASS(BlueprintType)
class CHESSKIDS_API UChessPieceSkinRegistry : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chess|Skins")
	TArray<FChessPieceSkinEntry> Skins;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Chess|Skins")
	bool FindSkin(FName SkinId, FChessPieceSkinEntry& OutEntry) const;
};
