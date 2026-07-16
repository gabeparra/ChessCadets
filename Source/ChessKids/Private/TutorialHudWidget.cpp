#include "TutorialHudWidget.h"
#include "TutorialManager.h"
#include "ChessKidsGameInstance.h"
#include "ChessPieceSkinRegistry.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"

TSharedRef<SWidget> UTutorialHudWidget::RebuildWidget()
{
	if (WidgetTree && WidgetTree->RootWidget == nullptr)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = Root;

		// Guide speech panel — bottom center, where kids' eyes rest.
		GuidePanel = WidgetTree->ConstructWidget<UBorder>();
		GuidePanel->SetBrushColor(FLinearColor(0.f, 0.f, 0.05f, 0.8f));
		GuidePanel->SetPadding(FMargin(28.f, 14.f));
		GuidePanel->SetVisibility(ESlateVisibility::Hidden);
		if (UCanvasPanelSlot* GS = Root->AddChildToCanvas(GuidePanel))
		{
			GS->SetAnchors(FAnchors(0.5f, 1.f, 0.5f, 1.f));
			GS->SetAlignment(FVector2D(0.5f, 1.f));
			GS->SetPosition(FVector2D(0.f, -48.f));
			GS->SetAutoSize(true);
		}
		GuideText = WidgetTree->ConstructWidget<UTextBlock>();
		GuideText->SetJustification(ETextJustify::Center);
		// Fixed wrap width: the auto-sized panel otherwise grows as wide as the
		// longest line and stacks wrapped text upward over the board. This keeps
		// the speech bubble a consistent size at the bottom of the screen.
		GuideText->SetWrapTextAt(820.f);
		GuideText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 20));
		GuideText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.98f, 1.f)));
		GuidePanel->AddChild(GuideText);

		// MENU button — top right, always available.
		MenuButton = WidgetTree->ConstructWidget<UButton>();
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(FText::FromString(TEXT("MENU")));
		Label->SetJustification(ETextJustify::Center);
		Label->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 16));
		Label->SetColorAndOpacity(FSlateColor(FLinearColor(0.05f, 0.05f, 0.08f)));
		MenuButton->AddChild(Label);
		if (UCanvasPanelSlot* MS = Root->AddChildToCanvas(MenuButton))
		{
			MS->SetAnchors(FAnchors(1.f, 0.f, 1.f, 0.f));
			MS->SetAlignment(FVector2D(1.f, 0.f));
			MS->SetPosition(FVector2D(-24.f, 24.f));
			MS->SetAutoSize(true);
		}

		// Level-complete choice panel — centered, hidden until the boss falls.
		CompletePanel = WidgetTree->ConstructWidget<UBorder>();
		CompletePanel->SetBrushColor(FLinearColor(0.f, 0.f, 0.05f, 0.92f));
		CompletePanel->SetPadding(FMargin(48.f, 32.f));
		CompletePanel->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* CS = Root->AddChildToCanvas(CompletePanel))
		{
			CS->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
			CS->SetAlignment(FVector2D(0.5f, 0.5f));
			CS->SetPosition(FVector2D(0.f, 0.f));
			CS->SetAutoSize(true);
		}

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
		CompletePanel->AddChild(Column);

		UTextBlock* Headline = WidgetTree->ConstructWidget<UTextBlock>();
		Headline->SetText(FText::FromString(TEXT("LEVEL COMPLETE!")));
		Headline->SetJustification(ETextJustify::Center);
		Headline->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 30));
		Headline->SetColorAndOpacity(FSlateColor(FLinearColor(0.4f, 1.f, 0.8f)));
		if (UVerticalBoxSlot* HS = Column->AddChildToVerticalBox(Headline))
			HS->SetPadding(FMargin(0.f, 0.f, 0.f, 24.f));

		UHorizontalBox* ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>();
		Column->AddChildToVerticalBox(ButtonRow);

		auto MakeChoiceButton = [this](const TCHAR* Text) -> UButton*
		{
			UButton* Btn = WidgetTree->ConstructWidget<UButton>();
			UTextBlock* BtnLabel = WidgetTree->ConstructWidget<UTextBlock>();
			BtnLabel->SetText(FText::FromString(Text));
			BtnLabel->SetJustification(ETextJustify::Center);
			BtnLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 20));
			BtnLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.05f, 0.05f, 0.08f)));
			BtnLabel->SetMargin(FMargin(20.f, 10.f));
			Btn->AddChild(BtnLabel);
			return Btn;
		};

		ContinueButton = MakeChoiceButton(TEXT("CONTINUE"));
		if (UHorizontalBoxSlot* BS = ButtonRow->AddChildToHorizontalBox(ContinueButton))
			BS->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));

		CompleteMenuButton = MakeChoiceButton(TEXT("MAIN MENU"));
		ButtonRow->AddChildToHorizontalBox(CompleteMenuButton);

		// "New skin unlocked!" popup — shown ahead of CompletePanel when the boss
		// fight granted a skin. Gold accent to read as a reward, distinct from the
		// teal "LEVEL COMPLETE!" headline above.
		SkinUnlockPanel = WidgetTree->ConstructWidget<UBorder>();
		SkinUnlockPanel->SetBrushColor(FLinearColor(0.05f, 0.03f, 0.f, 0.95f));
		SkinUnlockPanel->SetPadding(FMargin(48.f, 32.f));
		SkinUnlockPanel->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* SUS = Root->AddChildToCanvas(SkinUnlockPanel))
		{
			SUS->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
			SUS->SetAlignment(FVector2D(0.5f, 0.5f));
			SUS->SetPosition(FVector2D(0.f, 0.f));
			SUS->SetAutoSize(true);
		}

		UVerticalBox* SkinColumn = WidgetTree->ConstructWidget<UVerticalBox>();
		SkinUnlockPanel->AddChild(SkinColumn);

		UTextBlock* SkinHeadline = WidgetTree->ConstructWidget<UTextBlock>();
		SkinHeadline->SetText(FText::FromString(TEXT("NEW SKIN UNLOCKED!")));
		SkinHeadline->SetJustification(ETextJustify::Center);
		SkinHeadline->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 26));
		SkinHeadline->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.82f, 0.2f)));
		if (UVerticalBoxSlot* SHS = SkinColumn->AddChildToVerticalBox(SkinHeadline))
			SHS->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

		SkinUnlockImage = WidgetTree->ConstructWidget<UImage>();
		if (UVerticalBoxSlot* SIS = SkinColumn->AddChildToVerticalBox(SkinUnlockImage))
		{
			SIS->SetHorizontalAlignment(HAlign_Center);
			SIS->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		}

		SkinUnlockNameText = WidgetTree->ConstructWidget<UTextBlock>();
		SkinUnlockNameText->SetJustification(ETextJustify::Center);
		SkinUnlockNameText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 20));
		SkinUnlockNameText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.98f, 1.f)));
		if (UVerticalBoxSlot* SNS = SkinColumn->AddChildToVerticalBox(SkinUnlockNameText))
			SNS->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));

		SkinUnlockOkButton = MakeChoiceButton(TEXT("NICE!"));
		if (UVerticalBoxSlot* SOS = SkinColumn->AddChildToVerticalBox(SkinUnlockOkButton))
			SOS->SetHorizontalAlignment(HAlign_Center);
	}

	return Super::RebuildWidget();
}

void UTutorialHudWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (MenuButton)
		MenuButton->OnClicked.AddUniqueDynamic(this, &UTutorialHudWidget::OnMenuClicked);
	if (ContinueButton)
		ContinueButton->OnClicked.AddUniqueDynamic(this, &UTutorialHudWidget::OnContinueClicked);
	if (CompleteMenuButton)
		CompleteMenuButton->OnClicked.AddUniqueDynamic(this, &UTutorialHudWidget::OnMenuClicked);
	if (SkinUnlockOkButton)
		SkinUnlockOkButton->OnClicked.AddUniqueDynamic(this, &UTutorialHudWidget::OnSkinUnlockOkClicked);

	Manager = Cast<ATutorialManager>(
		UGameplayStatics::GetActorOfClass(this, ATutorialManager::StaticClass()));
	if (Manager)
	{
		Manager->OnGuideMessage.AddUniqueDynamic(this, &UTutorialHudWidget::HandleGuideMessage);
		Manager->OnLevelComplete.AddUniqueDynamic(this, &UTutorialHudWidget::HandleLevelComplete);
		Manager->OnSkinUnlocked.AddUniqueDynamic(this, &UTutorialHudWidget::HandleSkinUnlocked);

		// Replay a message broadcast before this widget existed (e.g. the phase
		// intro, when the manager's BeginPlay ran before the controller's)
		const FString Missed = Manager->GetLastGuideMessage();
		if (!Missed.IsEmpty())
			HandleGuideMessage(Missed);
	}
}

void UTutorialHudWidget::HandleGuideMessage(const FString& Message)
{
	if (!GuideText || !GuidePanel) return;
	GuideText->SetText(FText::FromString(Message));
	GuidePanel->SetVisibility(ESlateVisibility::HitTestInvisible);

	// Linger long enough to read aloud, then tuck away.
	if (UWorld* World = GetWorld())
	{
		TWeakObjectPtr<UTutorialHudWidget> WeakThis(this);
		World->GetTimerManager().SetTimer(HideTimer, [WeakThis]()
		{
			if (UTutorialHudWidget* Self = WeakThis.Get())
				if (Self->GuidePanel)
					Self->GuidePanel->SetVisibility(ESlateVisibility::Hidden);
		}, 6.f, false);
	}
}

void UTutorialHudWidget::HandleLevelComplete()
{
	// Let the "you've mastered this piece!" message land first, then offer the choice
	// (or the skin-unlock popup first, if HandleSkinUnlocked already fired this frame).
	if (UWorld* World = GetWorld())
	{
		TWeakObjectPtr<UTutorialHudWidget> WeakThis(this);
		FTimerHandle ShowTimer;
		World->GetTimerManager().SetTimer(ShowTimer, [WeakThis]()
		{
			UTutorialHudWidget* Self = WeakThis.Get();
			if (!Self) return;

			if (!Self->PendingUnlockedSkinId.IsNone())
				Self->ShowSkinUnlockPopup();
			else
				Self->ShowCompletePanel();
		}, 2.5f, false);
	}
}

void UTutorialHudWidget::ShowCompletePanel()
{
	if (!CompletePanel) return;

	// No next level configured? Only offer the menu.
	if (ContinueButton && Manager && Manager->GetNextLevelName() == NAME_None)
		ContinueButton->SetVisibility(ESlateVisibility::Collapsed);

	CompletePanel->SetVisibility(ESlateVisibility::Visible);
}

void UTutorialHudWidget::HandleSkinUnlocked(FName SkinId)
{
	// OnLevelComplete/OnSkinUnlocked both broadcast synchronously from the same
	// OnPhaseComplete() call, well before HandleLevelComplete's 2.5s timer fires —
	// just remember it here for that timer to pick up.
	PendingUnlockedSkinId = SkinId;
}

void UTutorialHudWidget::ShowSkinUnlockPopup()
{
	if (!SkinUnlockPanel)
	{
		ShowCompletePanel();
		return;
	}

	FChessPieceSkinEntry Entry;
	bool bFound = false;
	if (const UChessKidsGameInstance* GI = Cast<UChessKidsGameInstance>(GetGameInstance()))
	{
		if (UChessPieceSkinRegistry* Registry = GI->PieceSkinRegistry.LoadSynchronous())
			bFound = Registry->FindSkin(PendingUnlockedSkinId, Entry);
	}

	if (SkinUnlockNameText)
		SkinUnlockNameText->SetText(bFound ? Entry.DisplayName : FText::FromName(PendingUnlockedSkinId));

	if (SkinUnlockImage)
	{
		if (bFound && Entry.Thumbnail)
		{
			SkinUnlockImage->SetBrushFromTexture(Entry.Thumbnail, false);
			SkinUnlockImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			SkinUnlockImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	SkinPopupAnimElapsed = 0.f;
	bSkinPopupAnimating = true;
	SkinUnlockPanel->SetRenderOpacity(0.f);
	SkinUnlockPanel->SetVisibility(ESlateVisibility::Visible);
}

void UTutorialHudWidget::OnSkinUnlockOkClicked()
{
	if (SkinUnlockPanel)
		SkinUnlockPanel->SetVisibility(ESlateVisibility::Collapsed);
	bSkinPopupAnimating = false;
	PendingUnlockedSkinId = NAME_None;
	ShowCompletePanel();
}

void UTutorialHudWidget::NativeTick(const FGeometry& MyGeometry, float DeltaTime)
{
	Super::NativeTick(MyGeometry, DeltaTime);

	if (!bSkinPopupAnimating || !SkinUnlockPanel)
		return;

	// Eased pop-in with a slight overshoot bounce, same manual-Tick-tween idiom this
	// project already uses for piece glides/hops (see AChessPiece::Tick) rather than
	// a UMG WidgetAnimation.
	SkinPopupAnimElapsed += DeltaTime;
	constexpr float PopDuration = 0.45f;
	const float Alpha = FMath::Clamp(SkinPopupAnimElapsed / PopDuration, 0.f, 1.f);

	const float Eased = FMath::InterpEaseOut(0.f, 1.f, Alpha, 3.f);
	const float Overshoot = 1.f + FMath::Sin(Alpha * PI) * 0.12f * (1.f - Alpha);
	const float Scale = FMath::Lerp(0.6f, 1.f, Eased) * Overshoot;

	SkinUnlockPanel->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	SkinUnlockPanel->SetRenderScale(FVector2D(Scale, Scale));
	SkinUnlockPanel->SetRenderOpacity(FMath::Clamp(SkinPopupAnimElapsed / 0.2f, 0.f, 1.f));

	if (Alpha >= 1.f)
		bSkinPopupAnimating = false;
}

void UTutorialHudWidget::OnContinueClicked()
{
	if (Manager && Manager->GetNextLevelName() != NAME_None)
		UGameplayStatics::OpenLevel(this, Manager->GetNextLevelName());
}

void UTutorialHudWidget::OnMenuClicked()
{
	UGameplayStatics::OpenLevel(this, TEXT("MainMenu"));
}
