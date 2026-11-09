// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDwEditor, Log, All);

/** Editor module for Deadwater Run. Empty in DW-003; DW-108 registers the Deadwater menu and validators here. */
class FDeadwaterRunEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
