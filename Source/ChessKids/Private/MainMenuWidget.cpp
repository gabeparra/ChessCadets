#include "MainMenuWidget.h"
#include "ChessKidsGameInstance.h"
#include "SettingsMenuWidget.h"
#include "PieceSkinsMenuWidget.h"
#include "ModeSelectWidget.h"
#include "TutorialSelectWidget.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Styling/CoreStyle.h"
#include "ChessKidsUiStyle.h"

void UMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Story: BP keeps its own navigation; C++ only adds the mode flag.
	if (StoryModeButton) StoryModeButton->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::OnStoryMode);

	// Chess mode: take the button over completely — Clear() drops the BP's direct
	// OpenLevel binding so the 1P/2P picker can be shown first.
	if (chessModeButton)
	{
		chessModeButton->OnClicked.Clear();
		chessModeButton->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::OnChessMode);
	}

	if (quitButton)      quitButton->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::OnQuit);

	// Duplicated from quitButton in the WBP, so it may have inherited Quit's own
	// click binding along with its style — Clear() first so it only ever opens Skins.
	if (skinsButton)
	{
		skinsButton->OnClicked.Clear();
		skinsButton->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::OnSkins);
	}

	InjectSettingsButton();
	InjectSkinsButton();
	InjectLearnButton();

	// Isabella's WBP menu is the real menu; keep these dev buttons hidden until K.
	SetDevMenuVisible(false);
	SetupDevInput();
}

void UMainMenuWidget::SetupDevInput()
{
	if (bDevInputBound) return;

	APlayerController* PC = GetOwningPlayer();
	if (!PC || !PC->InputComponent)
	{
		// Player controller / input not ready yet — retry next tick.
		if (UWorld* World = GetWorld())
		{
			TWeakObjectPtr<UMainMenuWidget> WeakThis(this);
			World->GetTimerManager().SetTimerForNextTick([WeakThis]()
			{
				if (UMainMenuWidget* Self = WeakThis.Get()) Self->SetupDevInput();
			});
		}
		return;
	}

	// Keys must reach the game input component, and the menu needs the cursor.
	PC->bShowMouseCursor = true;
	PC->SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false));

	// K toggles the dev overlay — bound on the controller so it works regardless of
	// which widget currently holds keyboard focus.
	PC->InputComponent->BindKey(EKeys::K, IE_Pressed, this, &UMainMenuWidget::ToggleDevMenu);
	bDevInputBound = true;
}

void UMainMenuWidget::ToggleDevMenu()
{
	SetDevMenuVisible(!bDevMenuShown);
}

void UMainMenuWidget::SetDevMenuVisible(bool bVisible)
{
	bDevMenuShown = bVisible;
	const ESlateVisibility V = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	if (InjectedSettingsButton) InjectedSettingsButton->SetVisibility(V);
	if (InjectedSkinsButton)    InjectedSkinsButton->SetVisibility(V);
	if (InjectedLearnButton)    InjectedLearnButton->SetVisibility(V);
}

void UMainMenuWidget::SetTwoPlayerMode(bool bTwoPlayer)
{
	if (UChessKidsGameInstance* GI = Cast<UChessKidsGameInstance>(GetGameInstance()))
		GI->bTwoPlayerMode = bTwoPlayer;
}

void UMainMenuWidget::OnStoryMode()
{
	SetTwoPlayerMode(false);   // story = you vs the robot; BP handles navigation
}

void UMainMenuWidget::OnChessMode()
{
	// Ask 1P-vs-CPU or 2-players on an overlay (menu background stays visible);
	// the picker sets the mode and opens the level.
	UClass* Cls = ModeSelectWidgetClass ? ModeSelectWidgetClass.Get() : UModeSelectWidget::StaticClass();
	if (!Cls) return;
	if (!ModeSelectInstance)
		ModeSelectInstance = CreateWidget<UUserWidget>(GetOwningPlayer(), Cls);
	if (ModeSelectInstance && !ModeSelectInstance->IsInViewport())
		ModeSelectInstance->AddToViewport(50);
}

void UMainMenuWidget::OnQuit()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UMainMenuWidget::OnSettings()
{
	UClass* Cls = SettingsWidgetClass ? SettingsWidgetClass.Get() : USettingsMenuWidget::StaticClass();
	if (!Cls) return;
	if (!SettingsInstance)
		SettingsInstance = CreateWidget<UUserWidget>(GetOwningPlayer(), Cls);
	if (SettingsInstance && !SettingsInstance->IsInViewport())
		SettingsInstance->AddToViewport(100);
}

void UMainMenuWidget::OnSkins()
{
	UClass* Cls = SkinsWidgetClass ? SkinsWidgetClass.Get() : UPieceSkinsMenuWidget::StaticClass();
	if (!Cls) return;
	if (!SkinsInstance)
		SkinsInstance = CreateWidget<UUserWidget>(GetOwningPlayer(), Cls);
	if (SkinsInstance && !SkinsInstance->IsInViewport())
		SkinsInstance->AddToViewport(100);
}

void UMainMenuWidget::OnLearn()
{
	SetTwoPlayerMode(false);
	// Lesson picker overlay — jump into any lesson, stars mark completed ones.
	if (UUserWidget* Picker = CreateWidget<UUserWidget>(GetOwningPlayer(), UTutorialSelectWidget::StaticClass()))
		Picker->AddToViewport(50);
}

void UMainMenuWidget::InjectLearnButton()
{
	if (InjectedLearnButton) return;

	UCanvasPanel* Canvas = WidgetTree ? Cast<UCanvasPanel>(WidgetTree->RootWidget) : nullptr;
	if (!Canvas) return;

	InjectedLearnButton = WidgetTree->ConstructWidget<UButton>();
	ChessKidsUi::StyleMenuButton(InjectedLearnButton);
	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
	ChessKidsUi::StyleButtonLabel(Label, FString(TEXT("LEARN CHESS")), 16);
	InjectedLearnButton->AddChild(Label);
	InjectedLearnButton->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::OnLearn);

	if (UCanvasPanelSlot* CS = Canvas->AddChildToCanvas(InjectedLearnButton))
	{
		CS->SetAnchors(FAnchors(0.f, 1.f, 0.f, 1.f));   // bottom-left corner
		CS->SetAlignment(FVector2D(0.f, 1.f));
		CS->SetPosition(FVector2D(24.f, -24.f));
		CS->SetAutoSize(true);
	}
}

void UMainMenuWidget::InjectSettingsButton()
{
	if (InjectedSettingsButton) return;   // once per widget lifetime

	UCanvasPanel* Canvas = WidgetTree ? Cast<UCanvasPanel>(WidgetTree->RootWidget) : nullptr;
	if (!Canvas) return;

	InjectedSettingsButton = WidgetTree->ConstructWidget<UButton>();
	ChessKidsUi::StyleMenuButton(InjectedSettingsButton);
	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
	ChessKidsUi::StyleButtonLabel(Label, FString(TEXT("Settings")), 24);
	InjectedSettingsButton->AddChild(Label);
	InjectedSettingsButton->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::OnSettings);

	if (UCanvasPanelSlot* CS = Canvas->AddChildToCanvas(InjectedSettingsButton))
	{
		CS->SetAnchors(FAnchors(1.f, 1.f, 1.f, 1.f));   // bottom-right corner
		CS->SetAlignment(FVector2D(1.f, 1.f));
		CS->SetPosition(FVector2D(-24.f, -24.f));
		CS->SetAutoSize(true);
	}
}

void UMainMenuWidget::InjectSkinsButton()
{
	if (InjectedSkinsButton) return;   // once per widget lifetime

	UCanvasPanel* Canvas = WidgetTree ? Cast<UCanvasPanel>(WidgetTree->RootWidget) : nullptr;
	if (!Canvas) return;

	InjectedSkinsButton = WidgetTree->ConstructWidget<UButton>();
	ChessKidsUi::StyleMenuButton(InjectedSkinsButton);
	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
	ChessKidsUi::StyleButtonLabel(Label, FString(TEXT("Piece Skins")), 24);
	InjectedSkinsButton->AddChild(Label);
	InjectedSkinsButton->OnClicked.AddUniqueDynamic(this, &UMainMenuWidget::OnSkins);

	if (UCanvasPanelSlot* CS = Canvas->AddChildToCanvas(InjectedSkinsButton))
	{
		CS->SetAnchors(FAnchors(1.f, 1.f, 1.f, 1.f));   // bottom-right corner, stacked above Settings
		CS->SetAlignment(FVector2D(1.f, 1.f));
		CS->SetPosition(FVector2D(-24.f, -66.f));
		CS->SetAutoSize(true);
	}
}
