// Copyright Brandon Hillig 2026

#include "UI/DwPrimaryLayoutWidget.h"

#include "Core/DwGameplayTags.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

UCommonActivatableWidgetStack* UDwPrimaryLayoutWidget::GetLayer(FGameplayTag LayerTag) const
{
	if (LayerTag == DwTags::UI_Layer_Game)     { return GameLayer; }
	if (LayerTag == DwTags::UI_Layer_GameMenu) { return GameMenuLayer; }
	if (LayerTag == DwTags::UI_Layer_Menu)     { return MenuLayer; }
	if (LayerTag == DwTags::UI_Layer_Modal)    { return ModalLayer; }
	return nullptr;
}
