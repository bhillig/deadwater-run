// Copyright Brandon Hillig 2026

#include "Core/DwPlaceholderHUD.h"

#include "Combat/DwCombatInteraction.h"
#include "Combat/DwCombatSubsystem.h"
#include "Combat/DwCombatUnit.h"
#include "Core/DwPlayerController.h"
#include "Data/DwAbilityDefinition.h"
#include "Data/DwUnitDefinition.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace DwPlaceholderHUD
{
	const FLinearColor Text(0.95f, 0.92f, 0.85f);
	const FLinearColor Dim(0.6f, 0.6f, 0.6f);
	const FLinearColor Good(0.45f, 0.85f, 0.45f);
	const FLinearColor Bad(1.f, 0.45f, 0.35f);
	const FLinearColor Gold(1.f, 0.82f, 0.3f);

	FString OutcomeText(EDwCombatOutcome Outcome)
	{
		switch (Outcome)
		{
		case EDwCombatOutcome::VictorySunk:         return TEXT("VICTORY - the enemy ship is sunk");
		case EDwCombatOutcome::VictoryCrewDefeated: return TEXT("VICTORY - the enemy crew is defeated (plunder!)");
		case EDwCombatOutcome::VictorySurrender:    return TEXT("VICTORY - the enemy surrenders");
		case EDwCombatOutcome::VictoryBoss:         return TEXT("VICTORY - the Kraken is slain");
		case EDwCombatOutcome::DefeatCrewDead:      return TEXT("DEFEAT - your crew is lost");
		case EDwCombatOutcome::DefeatCaptainDead:   return TEXT("DEFEAT - your captain has fallen");
		case EDwCombatOutcome::DefeatShipSunk:      return TEXT("DEFEAT - your ship is sunk");
		default:                                    return FString();
		}
	}
}

void ADwPlaceholderHUD::Line(const FString& Text, const FLinearColor& Color, float& Y, float Scale)
{
	if (bCollecting)
	{
		Panel.Add({ Text, Color, Scale });
		return;
	}
	DrawText(Text, Color, 24.f, Y, GEngine->GetMediumFont(), Scale);
	Y += 22.f * Scale;
}

void ADwPlaceholderHUD::DrawHUD()
{
	Super::DrawHUD();
	using namespace DwPlaceholderHUD;
	const UDwCombatSubsystem* Combat = GetWorld()->GetSubsystem<UDwCombatSubsystem>();
	const ADwPlayerController* PC = Cast<ADwPlayerController>(GetOwningPlayerController());
	const UDwCombatInteraction* Interaction = PC ? PC->GetInteraction() : nullptr;
	if (!Combat || !Combat->IsActive() || !Interaction)
	{
		return;
	}
	const FDwCombatSnapshot Snapshot = Combat->GetSnapshot();
	float Y = 20.f;

	const bool bPlayerTurn = Snapshot.ActiveSide == EDwSide::Player;
	Line(FString::Printf(TEXT("Round %d  -  %s"), Snapshot.Round, bPlayerTurn ? TEXT("YOUR TURN") : TEXT("ENEMY TURN")), bPlayerTurn ? Gold : Bad, Y, 1.4f);
	for (const FDwShipSnapshot& Ship : Snapshot.Ships)
	{
		Line(FString::Printf(TEXT("%s ship  %.0f / %.0f"), Ship.Side == EDwSide::Player ? TEXT("Your") : TEXT("Enemy"), Ship.Health, Ship.MaxHealth),
			Ship.Side == EDwSide::Player ? Text : Bad, Y);
	}

	// Everything about the selected pirate goes in a panel at the bottom-left, clear of the board.
	bCollecting = true;
	Panel.Reset();

	if (const FDwUnitSnapshot* Unit = Snapshot.FindUnit(Interaction->GetSelectedUnit()))
	{
		const ADwCombatUnit* Actor = Combat->FindUnit(Unit->Handle);
		const FString Name = Actor && Actor->GetDefinition() ? Actor->GetDefinition()->DisplayName.ToString() : TEXT("Pirate");
		Line(FString::Printf(TEXT("%s   HP %.0f/%.0f   MP %.0f/%.0f   Move %s   Actions %.0f"), *Name, Unit->Health, Unit->MaxHealth,
			Unit->Mana, Unit->MaxMana, Unit->MovePoints > 0.f ? TEXT("ready") : TEXT("used"), Unit->ActionPoints), Text, Y, 1.1f);
		const TArray<FDwActionSlot> Bar = Interaction->GetActionBar();
		for (int32 Index = 0; Index < Bar.Num() && Index < 6; ++Index)
		{
			const FDwActionSlot& Slot = Bar[Index];
			FString Entry = FString::Printf(TEXT("  [%d] %s"), Index + 1, *Slot.Label.ToString());
			if (Slot.Count > 0) { Entry += FString::Printf(TEXT(" x%d"), Slot.Count); }
			if (!Slot.bAvailable) { Entry += FString::Printf(TEXT("  - %s"), *Slot.Reason.ToString()); }
			const bool bArmed = Interaction->GetArmedAbility() == Slot.Ability && Interaction->GetArmedItem() == Slot.ItemId;
			Line(Entry, bArmed ? Gold : (Slot.bAvailable ? Text : Dim), Y);
		}
	}
	else if (bPlayerTurn)
	{
		Line(TEXT("Click one of your pirates."), Dim, Y);
	}

	if (Interaction->GetMode() == EDwInteractionMode::Targeting && Interaction->GetArmedAbility())
	{
		Line(FString::Printf(TEXT("Aiming %s - click a highlighted target (right-click to cancel)"), *Interaction->GetArmedAbility()->DisplayName.ToString()), Gold, Y);
	}
	const FDwAbilityPreview& Preview = Interaction->GetPreview();
	if (Preview.bValid)
	{
		for (const FDwTargetPreview& Hit : Preview.Targets)
		{
			FString What = Hit.Target.bShip ? TEXT("Ship") : FString::Printf(TEXT("Target %d"), Hit.Target.Unit.Id);
			if (const ADwCombatUnit* Victim = Combat->FindUnit(Hit.Target.Unit); Victim && Victim->GetDefinition())
			{
				What = Victim->GetDefinition()->DisplayName.ToString();
			}
			FString Effect;
			if (Hit.Damage > 0) { Effect += FString::Printf(TEXT(" -%d HP%s"), Hit.Damage * Preview.MinHits, Preview.MaxHits > Preview.MinHits ? *FString::Printf(TEXT(" to -%d"), Hit.Damage * Preview.MaxHits) : TEXT("")); }
			if (Hit.Heal > 0) { Effect += FString::Printf(TEXT(" +%d HP"), Hit.Heal); }
			if (Hit.Mana > 0) { Effect += FString::Printf(TEXT(" +%d MP"), Hit.Mana); }
			if (Hit.Fear > 0) { Effect += FString::Printf(TEXT(" +%d Fear"), Hit.Fear); }
			Line(FString::Printf(TEXT("Preview: %s%s%s"), *What, *Effect, Hit.bLethal ? TEXT("  LETHAL") : TEXT("")), Hit.bLethal ? Bad : Good, Y);
		}
	}
	else if (!Preview.Reason.IsEmpty())
	{
		Line(Preview.Reason.ToString(), Dim, Y);
	}
	if (!Interaction->GetMessage().IsEmpty())
	{
		Line(Interaction->GetMessage().ToString(), Bad, Y);
	}
	if (bPlayerTurn && Interaction->IsEndTurnSuggested())
	{
		Line(TEXT("Nobody has anything left to do. End turn? (Space)"), Gold, Y, 1.2f);
	}

	bCollecting = false;
	if (Canvas)
	{
		float Height = 0.f;
		for (const FPanelLine& Entry : Panel) { Height += 22.f * Entry.Scale; }
		float PanelY = Canvas->ClipY - 56.f - Height;
		for (const FPanelLine& Entry : Panel) { Line(Entry.Text, Entry.Color, PanelY, Entry.Scale); }
	}

	const FString Outcome = OutcomeText(Snapshot.Outcome);
	if (!Outcome.IsEmpty() && Canvas)
	{
		float Width = 0.f;
		float Height = 0.f;
		GetTextSize(Outcome, Width, Height, GEngine->GetLargeFont(), 2.f);
		DrawText(Outcome, DwCombat::IsVictory(Snapshot.Outcome) ? Gold : Bad, (Canvas->ClipX - Width) * 0.5f, Canvas->ClipY * 0.35f, GEngine->GetLargeFont(), 2.f);
	}

	if (Canvas)
	{
		DrawText(TEXT("LMB select / move / attack    1-6 actions    RMB cancel    Tab next pirate    Space end turn    WASD pan    Wheel zoom"),
			Dim, 24.f, Canvas->ClipY - 36.f, GEngine->GetMediumFont());
	}
}
