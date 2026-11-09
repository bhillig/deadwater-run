// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DwPlaceholderHUD.generated.h"

/**
 * Text-only combat readout so a fight is playable before the CommonUI HUD exists. DW-107 (ui-engineer) replaces it
 * with WBP_CombatHud and removes this class from ADwGameMode. Reads only the interaction state and the snapshot.
 */
UCLASS()
class DEADWATERRUN_API ADwPlaceholderHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void Line(const FString& Text, const FLinearColor& Color, float& Y, float Scale = 1.f);

	struct FPanelLine
	{
		FString Text;
		FLinearColor Color;
		float Scale = 1.f;
	};
	TArray<FPanelLine> Panel;
	bool bCollecting = false;
};
