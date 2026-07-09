#include "TutorialHudWidget.h"
#include "TutorialManager.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
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

	Manager = Cast<ATutorialManager>(
		UGameplayStatics::GetActorOfClass(this, ATutorialManager::StaticClass()));
	if (Manager)
	{
		Manager->OnGuideMessage.AddUniqueDynamic(this, &UTutorialHudWidget::HandleGuideMessage);
		Manager->OnLevelComplete.AddUniqueDynamic(this, &UTutorialHudWidget::HandleLevelComplete);

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
	// Let the "you've mastered this piece!" message land first, then offer the choice.
	if (UWorld* World = GetWorld())
	{
		TWeakObjectPtr<UTutorialHudWidget> WeakThis(this);
		FTimerHandle ShowTimer;
		World->GetTimerManager().SetTimer(ShowTimer, [WeakThis]()
		{
			UTutorialHudWidget* Self = WeakThis.Get();
			if (!Self || !Self->CompletePanel) return;

			// No next level configured? Only offer the menu.
			if (Self->ContinueButton && Self->Manager &&
			    Self->Manager->GetNextLevelName() == NAME_None)
				Self->ContinueButton->SetVisibility(ESlateVisibility::Collapsed);

			Self->CompletePanel->SetVisibility(ESlateVisibility::Visible);
		}, 2.5f, false);
	}
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
