#include "PieceSkinsMenuWidget.h"
#include "PieceSkinSwatch.h"
#include "PieceSkinPreviewActor.h"
#include "ChessPieceSkinRegistry.h"
#include "ChessKidsGameInstance.h"
#include "ChessManager.h"
#include "ChessPiece.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInterface.h"
#include "InputCoreTypes.h"

TSharedRef<SWidget> UPieceSkinsMenuWidget::RebuildWidget()
{
	if (WidgetTree && WidgetTree->RootWidget == nullptr)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = Root;

		// Bigger centered panel — this screen now owns the whole view instead of
		// leaving the board visible behind it, since the live preview replaces the
		// board as the "what will this look like" reference.
		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
		Panel->SetBrushColor(FLinearColor(0.f, 0.f, 0.04f, 0.92f));
		Panel->SetPadding(FMargin(28.f, 20.f));
		if (UCanvasPanelSlot* PS = Root->AddChildToCanvas(Panel))
		{
			PS->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
			PS->SetAlignment(FVector2D(0.5f, 0.5f));
			PS->SetPosition(FVector2D::ZeroVector);
			PS->SetSize(FVector2D(940.f, 620.f));
		}

		UVerticalBox* MainBox = WidgetTree->ConstructWidget<UVerticalBox>();
		Panel->AddChild(MainBox);

		TitleText = WidgetTree->ConstructWidget<UTextBlock>();
		TitleText->SetText(FText::FromString(TEXT("PIECE SKINS")));
		TitleText->SetJustification(ETextJustify::Center);
		TitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 22));
		if (UVerticalBoxSlot* TS = MainBox->AddChildToVerticalBox(TitleText))
		{
			TS->SetPadding(FMargin(8.f, 0.f, 8.f, 12.f));
			TS->SetHorizontalAlignment(HAlign_Center);
		}

		UHorizontalBox* ContentBox = WidgetTree->ConstructWidget<UHorizontalBox>();
		if (UVerticalBoxSlot* CS = MainBox->AddChildToVerticalBox(ContentBox))
		{
			CS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			CS->SetHorizontalAlignment(HAlign_Fill);
		}

		// --- Left: live preview slot ---
		UVerticalBox* PreviewBox = WidgetTree->ConstructWidget<UVerticalBox>();
		USizeBox* PreviewColumn = WidgetTree->ConstructWidget<USizeBox>();
		PreviewColumn->SetWidthOverride(300.f);
		PreviewColumn->AddChild(PreviewBox);
		if (UHorizontalBoxSlot* PCS = ContentBox->AddChildToHorizontalBox(PreviewColumn))
		{
			PCS->SetPadding(FMargin(4.f, 4.f, 20.f, 4.f));
			PCS->SetVerticalAlignment(VAlign_Top);
		}

		USizeBox* PreviewImageBox = WidgetTree->ConstructWidget<USizeBox>();
		PreviewImageBox->SetWidthOverride(280.f);
		PreviewImageBox->SetHeightOverride(280.f);
		if (UVerticalBoxSlot* PIS = PreviewBox->AddChildToVerticalBox(PreviewImageBox))
		{
			PIS->SetHorizontalAlignment(HAlign_Center);
			PIS->SetPadding(FMargin(0.f, 4.f, 0.f, 10.f));
		}

		PreviewImage = WidgetTree->ConstructWidget<UImage>();
		PreviewImageBox->AddChild(PreviewImage);

		PreviewNameText = WidgetTree->ConstructWidget<UTextBlock>();
		PreviewNameText->SetJustification(ETextJustify::Center);
		PreviewNameText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 16));
		PreviewNameText->SetAutoWrapText(true);
		if (UVerticalBoxSlot* PNS = PreviewBox->AddChildToVerticalBox(PreviewNameText))
		{
			PNS->SetHorizontalAlignment(HAlign_Center);
			PNS->SetPadding(FMargin(4.f, 0.f, 4.f, 16.f));
		}

		EquipButton = WidgetTree->ConstructWidget<UButton>();
		UTextBlock* EquipLabel = WidgetTree->ConstructWidget<UTextBlock>();
		EquipLabel->SetText(FText::FromString(TEXT("Equip")));
		EquipLabel->SetJustification(ETextJustify::Center);
		EquipLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 15));
		EquipLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.05f, 0.05f, 0.08f)));
		EquipButton->AddChild(EquipLabel);
		if (UVerticalBoxSlot* EBS = PreviewBox->AddChildToVerticalBox(EquipButton))
		{
			EBS->SetHorizontalAlignment(HAlign_Fill);
			EBS->SetPadding(FMargin(20.f, 0.f));
		}

		// --- Right: Unlocked / Locked lists, stacked vertically ---
		UVerticalBox* ListsBox = WidgetTree->ConstructWidget<UVerticalBox>();
		if (UHorizontalBoxSlot* LBS = ContentBox->AddChildToHorizontalBox(ListsBox))
		{
			LBS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LBS->SetVerticalAlignment(VAlign_Fill);
		}

		UTextBlock* UnlockedHeader = WidgetTree->ConstructWidget<UTextBlock>();
		UnlockedHeader->SetText(FText::FromString(TEXT("UNLOCKED")));
		UnlockedHeader->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 14));
		UnlockedHeader->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.85f)));
		if (UVerticalBoxSlot* UHS = ListsBox->AddChildToVerticalBox(UnlockedHeader))
			UHS->SetPadding(FMargin(4.f, 0.f, 4.f, 4.f));

		USizeBox* UnlockedSizeBox = WidgetTree->ConstructWidget<USizeBox>();
		UnlockedSizeBox->SetHeightOverride(200.f);
		if (UVerticalBoxSlot* USB = ListsBox->AddChildToVerticalBox(UnlockedSizeBox))
			USB->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));

		UnlockedListBox = WidgetTree->ConstructWidget<UScrollBox>();
		UnlockedListBox->SetOrientation(Orient_Vertical);
		UnlockedSizeBox->AddChild(UnlockedListBox);

		UTextBlock* LockedHeader = WidgetTree->ConstructWidget<UTextBlock>();
		LockedHeader->SetText(FText::FromString(TEXT("LOCKED")));
		LockedHeader->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 14));
		LockedHeader->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.85f)));
		if (UVerticalBoxSlot* LHS = ListsBox->AddChildToVerticalBox(LockedHeader))
			LHS->SetPadding(FMargin(4.f, 0.f, 4.f, 4.f));

		USizeBox* LockedSizeBox = WidgetTree->ConstructWidget<USizeBox>();
		LockedSizeBox->SetHeightOverride(200.f);
		if (UVerticalBoxSlot* LSB = ListsBox->AddChildToVerticalBox(LockedSizeBox))
			LSB->SetVerticalAlignment(VAlign_Top);

		LockedListBox = WidgetTree->ConstructWidget<UScrollBox>();
		LockedListBox->SetOrientation(Orient_Vertical);
		LockedSizeBox->AddChild(LockedListBox);

		BackButton = WidgetTree->ConstructWidget<UButton>();
		UTextBlock* BackLabel = WidgetTree->ConstructWidget<UTextBlock>();
		BackLabel->SetText(FText::FromString(TEXT("Back")));
		BackLabel->SetJustification(ETextJustify::Center);
		BackLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 14));
		BackLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.05f, 0.05f, 0.08f)));
		BackButton->AddChild(BackLabel);
		if (UVerticalBoxSlot* S = MainBox->AddChildToVerticalBox(BackButton))
		{
			S->SetPadding(FMargin(48.f, 14.f, 48.f, 0.f));
			S->SetHorizontalAlignment(HAlign_Fill);
		}
	}

	return Super::RebuildWidget();
}

void UPieceSkinsMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// AddUniqueDynamic: this widget is cached and re-added by the pause/main menu each
	// time it opens, so NativeConstruct re-fires; plain AddDynamic would stack handlers.
	if (BackButton) BackButton->OnClicked.AddUniqueDynamic(this, &UPieceSkinsMenuWidget::OnBack);
	if (EquipButton) EquipButton->OnClicked.AddUniqueDynamic(this, &UPieceSkinsMenuWidget::OnEquipClicked);

	if (UChessKidsGameInstance* GI = Cast<UChessKidsGameInstance>(GetGameInstance()))
		PreviewedSkinId = GI->GetSelectedPieceSkinId();

	PopulateSkins();

	SetIsFocusable(true);
	SetKeyboardFocus();
}

FReply UPieceSkinsMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnBack();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UPieceSkinsMenuWidget::OnBack()
{
	RemoveFromParent();
}

void UPieceSkinsMenuWidget::PopulateSkins()
{
	if (!UnlockedListBox || !LockedListBox) return;
	UnlockedListBox->ClearChildren();
	LockedListBox->ClearChildren();

	UChessKidsGameInstance* GI = Cast<UChessKidsGameInstance>(GetGameInstance());
	if (!GI) return;

	const FName EquippedSkinId = GI->GetSelectedPieceSkinId();

	auto AddSwatch = [&](UScrollBox* TargetBox, FName SkinId, const FText& DisplayName, UTexture2D* Thumbnail, bool bUnlocked) -> UPieceSkinSwatch*
	{
		UPieceSkinSwatch* Swatch = CreateWidget<UPieceSkinSwatch>(this, UPieceSkinSwatch::StaticClass());
		if (!Swatch) return nullptr;
		Swatch->SkinId = SkinId;
		Swatch->DisplayName = DisplayName;
		Swatch->Thumbnail = Thumbnail;
		Swatch->bUnlocked = bUnlocked;
		Swatch->bEquipped = (SkinId == EquippedSkinId);
		Swatch->bPreviewed = (SkinId == PreviewedSkinId);
		if (bUnlocked)
			Swatch->OnSkinSwatchClicked.AddUniqueDynamic(this, &UPieceSkinsMenuWidget::OnSkinRowClicked);
		TargetBox->AddChild(Swatch);
		if (UScrollBoxSlot* SwatchSlot = Cast<UScrollBoxSlot>(Swatch->Slot))
		{
			SwatchSlot->SetPadding(FMargin(0.f, 2.f));
			SwatchSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		Swatch->RefreshAppearance();
		return Swatch;
	};

	// Always-unlocked entry that reverts pieces to their own default look. Its preview
	// is a flat swatch of the actual white/black tint for whichever side the player is
	// playing — there's no static image for the untextured look, so this is the closest
	// thing to a real thumbnail for it.
	AChessManager* Manager = Cast<AChessManager>(UGameplayStatics::GetActorOfClass(this, AChessManager::StaticClass()));
	// A live arena always has a real resolved PlayerColor — use it as-is. Without one
	// (the screen is only ever opened from the main menu before any arena exists, since
	// that's the only place with no AChessManager around), GI->PlayerColor is just
	// whatever stale/default session value happens to be sitting there, not a real
	// answer — Black is hardcoded instead of trusting it, matching the same fallback
	// already used for the Default swatch's material.
	const EChessColor PlayerColor = Manager ? Manager->PlayerColor : EChessColor::Black;

	if (UPieceSkinSwatch* DefaultSwatch = AddSwatch(UnlockedListBox, NAME_None, FText::FromString(TEXT("Default")), nullptr, true))
	{
		DefaultSwatch->bUseSwatchColor = true;
		DefaultSwatch->SwatchColor = AChessPiece::GetTintColor(PlayerColor);
		DefaultSwatch->RefreshAppearance();
	}

	if (UChessPieceSkinRegistry* Registry = GI->PieceSkinRegistry.LoadSynchronous())
	{
		for (const FChessPieceSkinEntry& Entry : Registry->Skins)
		{
			if (Entry.SkinId.IsNone()) continue;
			const bool bUnlocked = GI->IsPieceSkinUnlocked(Entry.SkinId);
			AddSwatch(bUnlocked ? UnlockedListBox : LockedListBox, Entry.SkinId, Entry.DisplayName, Entry.Thumbnail, bUnlocked);
		}
	}

	// Ensure the preview rig exists — RefreshPreview() below does the actual
	// class/color/skin setup in one shot (see APieceSkinPreviewActor::RefreshPreviewPiece).
	if (!IsValid(PreviewActor))
	{
		PreviewActor = Cast<APieceSkinPreviewActor>(UGameplayStatics::GetActorOfClass(this, APieceSkinPreviewActor::StaticClass()));
		if (!PreviewActor)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			// Above the playable area so it's never visible in the main game camera
			// regardless of which level is loaded, but not extremely far — many UE5
			// rendering systems (Lumen's Global Distance Field, light culling, etc.)
			// anchor their "active" range to wherever the main camera/gameplay actually
			// is, not to a scene capture camera, so sitting tens of thousands of units
			// away risked landing outside whatever range those systems consider
			// relevant even with our own dynamic light correctly set up. A typical
			// chess camera's arm length is on the order of ~1400 units, so 3000 is
			// comfortably out of view without being nearly as far removed.
			PreviewActor = GetWorld()->SpawnActor<APieceSkinPreviewActor>(FVector(0.f, 0.f, 3000.f), FRotator(0.f, 90.f, 0.f), Params);
		}
		BindPreviewImage();
	}

	RefreshPreview();
}

void UPieceSkinsMenuWidget::RefreshPreview()
{
	UChessKidsGameInstance* GI = Cast<UChessKidsGameInstance>(GetGameInstance());
	if (!GI) return;

	AChessManager* Manager = Cast<AChessManager>(UGameplayStatics::GetActorOfClass(this, AChessManager::StaticClass()));
	// Same reasoning as PopulateSkins(): a real arena's PlayerColor is trustworthy, but
	// without one (main menu, no arena) there's no real side to match yet, so this is
	// hardcoded rather than reading a stale/default GI->PlayerColor.
	const EChessColor PlayerColor = Manager ? Manager->PlayerColor : EChessColor::Black;

	UMaterialInterface* PreviewMaterial = nullptr;
	FText PreviewName = FText::FromString(TEXT("Default"));

	if (!PreviewedSkinId.IsNone())
	{
		if (UChessPieceSkinRegistry* Registry = GI->PieceSkinRegistry.LoadSynchronous())
		{
			FChessPieceSkinEntry Entry;
			if (Registry->FindSkin(PreviewedSkinId, Entry))
			{
				PreviewMaterial = Entry.Material.LoadSynchronous();
				PreviewName = Entry.DisplayName;
			}
		}
	}
	else
	{
		// Default: show the piece's real look, not an unskinned/empty material. In a
		// live game that's whichever side the player is actually playing; outside a
		// game (e.g. opened from the main menu, no side resolved yet) it falls back to
		// Black (PlayerColor is already hardcoded to Black above for that case). Uses
		// the project's own /Game copies of these materials rather than /Engine so they
		// travel with the repo instead of depending on the engine install having them.
		PreviewMaterial = LoadObject<UMaterialInterface>(nullptr, (PlayerColor == EChessColor::White)
			? TEXT("/Game/Materials/Pieces/WhitePiece.WhitePiece")
			: TEXT("/Game/Materials/Pieces/BlackPiece.BlackPiece"));
	}

	if (PreviewNameText) PreviewNameText->SetText(PreviewName);

	if (IsValid(PreviewActor))
	{
		// PreviewImage's brush was already bound to PreviewActor's render target once
		// (see BindPreviewImage) — with bCaptureEveryFrame = true, it picks up whatever
		// this produces on its own, no need to touch the brush again here.
		PreviewActor->RefreshPreviewPiece(PlayerColor, PreviewMaterial);
	}
}

void UPieceSkinsMenuWidget::BindPreviewImage()
{
	if (!PreviewImage || !IsValid(PreviewActor)) return;

	// UImage::SetBrushFromTexture only accepts UTexture2D, not UTextureRenderTarget2D,
	// so the brush is built by hand here. Only needs to run once (per PreviewActor) —
	// with bCaptureEveryFrame = true, the render target's live pixel content updates on
	// its own every frame from here on; there's no per-interaction "tell the UI a new
	// frame is ready" step to get wrong, which is what every earlier attempt at
	// precisely timing a one-shot capture kept getting wrong.
	FSlateBrush Brush;
	Brush.SetResourceObject(PreviewActor->GetRenderTarget());
	Brush.ImageSize = FVector2D(280.f, 280.f);
	Brush.DrawAs = ESlateBrushDrawType::Image;
	PreviewImage->SetBrush(Brush);
}

void UPieceSkinsMenuWidget::OnSkinRowClicked(FName SkinId)
{
	if (PreviewedSkinId == SkinId) return;
	PreviewedSkinId = SkinId;
	// Rebuilds the rows (to move the "previewed" highlight) and refreshes the preview
	// slot — deliberately does not touch GameInstance/AChessManager, so the actual
	// board pieces are untouched until Equip.
	PopulateSkins();
}

void UPieceSkinsMenuWidget::OnEquipClicked()
{
	UChessKidsGameInstance* GI = Cast<UChessKidsGameInstance>(GetGameInstance());
	if (!GI || !GI->SelectPieceSkin(PreviewedSkinId))
		return;

	if (AChessManager* Manager = Cast<AChessManager>(UGameplayStatics::GetActorOfClass(this, AChessManager::StaticClass())))
	{
		Manager->ApplyPieceSkin(GI->GetSelectedPieceMaterial());
	}

	// Move the equipped highlight to match.
	PopulateSkins();
}
