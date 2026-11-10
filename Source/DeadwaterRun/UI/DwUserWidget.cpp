// Copyright Brandon Hillig 2026

#include "UI/DwUserWidget.h"

#include "UI/DwUIJuice.h"
#include "UI/DwViewModelBase.h"

void UDwUserWidget::SetViewModel(UDwViewModelBase* InViewModel)
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

void UDwUserWidget::HandleViewModelChanged(UDwViewModelBase* /*Changed*/)
{
	NativeOnViewModelChanged();
	OnViewModelChanged();
}

bool UDwUserWidget::PlayJuice(FName AnimationName)
{
	return DwJuice::Play(*this, AnimationName);
}

void UDwUserWidget::PlayUISound(FGameplayTag SoundTag)
{
	DwJuice::PlaySound(*this, SoundTag);
}

void UDwUserWidget::NativeDestruct()
{
	if (ViewModel)
	{
		ViewModel->OnChanged.RemoveAll(this);
	}
	Super::NativeDestruct();
}
