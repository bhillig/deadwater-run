// Copyright Brandon Hillig 2026

#include "UI/DwActivatableScreen.h"

#include "UI/DwUIJuice.h"
#include "UI/DwViewModelBase.h"

void UDwActivatableScreen::SetViewModel(UDwViewModelBase* InViewModel)
{
	if (ViewModel)
	{
		ViewModel->OnChanged.RemoveAll(this);
	}
	ViewModel = InViewModel;
	if (ViewModel)
	{
		ViewModel->OnChanged.AddDynamic(this, &ThisClass::HandleViewModelChanged);
	}
	HandleViewModelChanged(ViewModel);
}

void UDwActivatableScreen::HandleViewModelChanged(UDwViewModelBase* /*Changed*/)
{
	NativeOnViewModelChanged();
	OnViewModelChanged();
}

bool UDwActivatableScreen::PlayJuice(FName AnimationName)
{
	return DwJuice::Play(*this, AnimationName);
}

void UDwActivatableScreen::PlayUISound(FGameplayTag SoundTag)
{
	DwJuice::PlaySound(*this, SoundTag);
}

TOptional<FUIInputConfig> UDwActivatableScreen::GetDesiredInputConfig() const
{
	return FUIInputConfig(InputMode, MouseCaptureMode, EMouseLockMode::DoNotLock, /*bHideCursorDuringViewportCapture*/ false);
}

void UDwActivatableScreen::NativeOnActivated()
{
	Super::NativeOnActivated();
	PlayJuice(DwJuice::Intro);
	OnJuiceIntro();
}

void UDwActivatableScreen::NativeOnDeactivated()
{
	PlayJuice(DwJuice::Outro);
	OnJuiceOutro();
	Super::NativeOnDeactivated();
}

void UDwActivatableScreen::NativeDestruct()
{
	if (ViewModel)
	{
		ViewModel->OnChanged.RemoveAll(this);
	}
	Super::NativeDestruct();
}
