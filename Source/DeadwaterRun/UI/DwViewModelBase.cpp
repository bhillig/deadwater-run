// Copyright Brandon Hillig 2026

#include "UI/DwViewModelBase.h"

void UDwViewModelBase::NotifyChanged()
{
	++ChangeCount;
	OnChanged.Broadcast(this);
}
