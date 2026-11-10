// Copyright Brandon Hillig 2026

#include "UI/DwUIManagerSubsystem.h"

#include "Combat/DwCombatSubsystem.h"
#include "Core/DwAssetManager.h"
#include "Core/DwGameData.h"
#include "Core/DwGameplayTags.h"
#include "Core/DwLog.h"
#include "Core/DwPlayerController.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "UI/DwActivatableScreen.h"
#include "UI/DwCombatViewModels.h"
#include "UI/DwFloatingText.h"
#include "UI/DwPrimaryLayoutWidget.h"
#include "UI/DwUIScreenSet.h"
#include "UI/DwUISoundMap.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

UDwUIManagerSubsystem* UDwUIManagerSubsystem::Get(const ULocalPlayer* LocalPlayer)
{
	return LocalPlayer ? LocalPlayer->GetSubsystem<UDwUIManagerSubsystem>() : nullptr;
}

const UDwUIScreenSet* UDwUIManagerSubsystem::GetScreenSet() const
{
	const UDwGameData* GameData = UDwAssetManager::Get().GetGameData();
	return GameData ? GameData->UIScreenSet.Get() : nullptr;
}

const UDwUISoundMap* UDwUIManagerSubsystem::GetSoundMap() const
{
	const UDwUIScreenSet* Screens = GetScreenSet();
	return Screens ? Screens->SoundMap.LoadSynchronous() : nullptr;
}

void UDwUIManagerSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);
	if (PlayerController.Get() == NewPlayerController)
	{
		return;
	}
	UnbindCombat();
	DestroyLayout();
	PlayerController = NewPlayerController;
	// Only real game viewports get UI (automation worlds and commandlets have none).
	if (NewPlayerController && NewPlayerController->GetWorld() && NewPlayerController->GetWorld()->GetGameViewport())
	{
		CreateLayout(*NewPlayerController);
		BindCombat(NewPlayerController->GetWorld());
	}
}

void UDwUIManagerSubsystem::Deinitialize()
{
	UnbindCombat();
	DestroyLayout();
	Super::Deinitialize();
}

void UDwUIManagerSubsystem::CreateLayout(APlayerController& InPlayerController)
{
	const UDwUIScreenSet* Screens = GetScreenSet();
	const TSubclassOf<UDwPrimaryLayoutWidget> LayoutClass = Screens ? Screens->PrimaryLayout.LoadSynchronous() : nullptr;
	if (!LayoutClass)
	{
		UE_LOG(LogDwUI, Error, TEXT("No UI: DA_GameData.UIScreenSet.PrimaryLayout is not set (run Tools/CreateUIContent.ps1)."));
		return;
	}
	Layout = CreateWidget<UDwPrimaryLayoutWidget>(&InPlayerController, LayoutClass);
	if (Layout)
	{
		Layout->AddToPlayerScreen(1000);
	}
}

void UDwUIManagerSubsystem::DestroyLayout()
{
	if (CombatHud)
	{
		CombatHud->Shutdown();
		CombatHud = nullptr;
	}
	if (FloatingText)
	{
		FloatingText->Shutdown();
		FloatingText = nullptr;
	}
	CombatHudScreen = nullptr;
	if (Layout)
	{
		Layout->RemoveFromParent();
		Layout = nullptr;
	}
}

UDwActivatableScreen* UDwUIManagerSubsystem::PushScreen(FGameplayTag LayerTag, TSubclassOf<UDwActivatableScreen> ScreenClass)
{
	UCommonActivatableWidgetStack* Layer = Layout ? Layout->GetLayer(LayerTag) : nullptr;
	if (!Layer || !ScreenClass)
	{
		UE_LOG(LogDwUI, Warning, TEXT("PushScreen(%s, %s): no layout or unknown layer."), *LayerTag.ToString(), *GetNameSafe(ScreenClass));
		return nullptr;
	}
	return Layer->AddWidget<UDwActivatableScreen>(ScreenClass);
}

void UDwUIManagerSubsystem::PopScreen(UCommonActivatableWidget* Screen)
{
	if (!Layout || !Screen)
	{
		return;
	}
	const FGameplayTag Layers[] = { DwTags::UI_Layer_Game, DwTags::UI_Layer_GameMenu, DwTags::UI_Layer_Menu, DwTags::UI_Layer_Modal };
	for (const FGameplayTag& Tag : Layers)
	{
		if (UCommonActivatableWidgetStack* Layer = Layout->GetLayer(Tag); Layer && Layer->GetWidgetList().Contains(Screen))
		{
			Layer->RemoveWidget(*Screen);
			return;
		}
	}
}

void UDwUIManagerSubsystem::BindCombat(UWorld* World)
{
	UDwCombatSubsystem* Fight = World ? World->GetSubsystem<UDwCombatSubsystem>() : nullptr;
	if (!Fight)
	{
		return;
	}
	Combat = Fight;
	Fight->OnCombatStarted.AddDynamic(this, &ThisClass::HandleCombatStarted);
	if (Fight->IsActive())
	{
		HandleCombatStarted();
	}
}

void UDwUIManagerSubsystem::UnbindCombat()
{
	if (UDwCombatSubsystem* Fight = Combat.Get())
	{
		Fight->OnCombatStarted.RemoveAll(this);
	}
	Combat = nullptr;
}

void UDwUIManagerSubsystem::HandleCombatStarted()
{
	// Stand-in for UDwRun::OnPhaseChanged(Combat) until DW-201 (see the class comment).
	ShowCombatHud();
}

void UDwUIManagerSubsystem::ShowCombatHud()
{
	UDwCombatSubsystem* Fight = Combat.Get();
	ADwPlayerController* PC = Cast<ADwPlayerController>(PlayerController.Get());
	const UDwUIScreenSet* Screens = GetScreenSet();
	if (!Fight || !Layout || !Screens)
	{
		return;
	}
	const UDwGameData* GameData = UDwAssetManager::Get().GetGameData();
	if (!CombatHud)
	{
		CombatHud = NewObject<UDwCombatHudViewModel>(this);
	}
	CombatHud->Initialize(*Fight, PC ? PC->GetInteraction() : nullptr, GameData ? FDwHudKeys::FromInput(GameData->Input) : FDwHudKeys());

	if (!CombatHudScreen)
	{
		CombatHudScreen = PushScreen(DwTags::UI_Layer_Game, Screens->CombatHud.LoadSynchronous());
	}
	if (CombatHudScreen)
	{
		CombatHudScreen->SetViewModel(CombatHud);
	}

	if (!FloatingText)
	{
		FloatingText = NewObject<UDwFloatingTextManager>(this);
	}
	FloatingText->Initialize(*Fight, PC, Screens->FloatingText.LoadSynchronous());
}
