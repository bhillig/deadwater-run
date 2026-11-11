// Copyright Brandon Hillig 2026

#include "Core/DwHUD.h"

#include "Data/DwAbilityDefinition.h"
#include "Data/DwCaptainDefinition.h"
#include "Data/DwEncounterDefinition.h"
#include "Data/DwItemDefinition.h"
#include "Data/DwPerkDefinition.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "HAL/IConsoleManager.h"
#include "Run/DwRun.h"
#include "Run/DwRunSubsystem.h"

#if !UE_BUILD_SHIPPING
namespace DwHUD
{
	static TAutoConsoleVariable<bool> CVarRunReadout(TEXT("dw.Run.Readout"), false,
		TEXT("Development builds: draw the run readout (phase, crew, routes, pending rewards) over the run screens. Off by default since DW-211."));

	static FString NodeLabel(const FDwRunState& State, int32 Index)
	{
		const FDwMapNode& Node = State.Map.Nodes[Index];
		const FString Type = StaticEnum<EDwNodeType>()->GetNameStringByValue(static_cast<int64>(Node.Type));
		const FString What = Node.Encounter ? Node.Encounter->DisplayName.ToString() : FString(TEXT("-"));
		return FString::Printf(TEXT("%s: %s (layer %d, lane %d)"), *Type, *What, Node.Layer + 1, Node.Lane + 1);
	}
}
#endif

void ADwHUD::DrawHUD()
{
	Super::DrawHUD();
#if !UE_BUILD_SHIPPING
	if (DwHUD::CVarRunReadout.GetValueOnGameThread())
	{
		DrawRunReadout();
	}
#endif
}

#if !UE_BUILD_SHIPPING
void ADwHUD::DrawRunReadout()
{
	const UDwRunSubsystem* Runs = UDwRunSubsystem::Get(this);
	const UDwRun* Run = Runs ? Runs->GetRun() : nullptr;
	if (!Run || !Canvas || Run->GetPhase() == EDwRunPhase::Combat)
	{
		return;
	}
	const FDwRunState& S = Run->GetState();
	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("DEV RUN READOUT  (map screen: DW-211)")));
	Lines.Add(FString::Printf(TEXT("Captain %s   seed %d   phase %s"), S.Captain ? *S.Captain->DisplayName.ToString() : TEXT("-"), S.Seed,
		*DwRun::PhaseName(S.Phase)));
	const FDwMapNode* Current = Run->GetCurrentNode();
	Lines.Add(FString::Printf(TEXT("At %s   (%d of %d layers, then the boss)"), Current ? *DwHUD::NodeLabel(S, S.CurrentNode) : TEXT("the start"),
		Current ? Current->Layer + 1 : 0, S.Map.LayerCount));
	FString Items;
	for (const FDwItemStack& Stack : S.Inventory)
	{
		Items += FString::Printf(TEXT("%s%s x%d"), Items.IsEmpty() ? TEXT("") : TEXT(", "), Stack.Item ? *Stack.Item->DisplayName.ToString() : TEXT("?"), Stack.Count);
	}
	Lines.Add(FString::Printf(TEXT("Gold %d   Ship %.0f / %.0f   Items: %s"), S.Gold, S.Ship.Health, S.Ship.MaxHealth, Items.IsEmpty() ? TEXT("none") : *Items));
	for (int32 Index = 0; Index < S.Crew.Num(); ++Index)
	{
		const FDwCrewMemberState& Member = S.Crew[Index];
		if (Member.bAlive)
		{
			FString Extras;
			for (const UDwAbilityDefinition* Ability : Member.Abilities)
			{
				Extras += FString::Printf(TEXT("%s%s"), Extras.IsEmpty() ? TEXT("") : TEXT(", "), Ability ? *Ability->DisplayName.ToString() : TEXT("?"));
			}
			for (const UDwPerkDefinition* Perk : Member.Perks)
			{
				Extras += FString::Printf(TEXT("%s%s"), Extras.IsEmpty() ? TEXT("") : TEXT(", "), Perk ? *Perk->DisplayName.ToString() : TEXT("?"));
			}
			Lines.Add(FString::Printf(TEXT("  %d. %s%s   L%d (%d XP)   %.0f / %.0f HP   %s"), Index + 1, *Member.GetDisplayName().ToString(),
				Member.bIsCaptain ? TEXT(" (captain)") : TEXT(""), Member.Level, Member.Xp, Member.Health, Member.GetMaxHealth(), *Extras));
			continue;
		}
		const FDwCrewLoss* Loss = S.Stats.CrewLost.FindByPredicate([&Member](const FDwCrewLoss& L) { return L.CrewId == Member.Id; });
		Lines.Add(FString::Printf(TEXT("  %s   lost at node %d"), *Member.GetDisplayName().ToString(), Loss ? Loss->Node : INDEX_NONE));
	}
	Lines.Add(FString::Printf(TEXT("Cleared %d   sunk %d   plundered %d   surrendered %d   kills %d   rounds %d"), S.Stats.NodesCleared,
		S.Stats.FightsSunk, S.Stats.FightsPlundered, S.Stats.FightsSurrendered, S.Stats.Kills, S.Stats.Rounds));
	auto CrewName = [&S](const FGuid& Id)
	{
		const FDwCrewMemberState* Member = S.FindCrew(Id);
		return Member ? Member->GetDisplayName().ToString() : FString(TEXT("?"));
	};
	if (S.Phase == EDwRunPhase::Rewards)
	{
		const FDwRewardBundle& R = S.PendingRewards;
		FString Loot;
		for (const FDwItemStack& Stack : R.Items)
		{
			Loot += FString::Printf(TEXT(" + %s x%d"), Stack.Item ? *Stack.Item->DisplayName.ToString() : TEXT("?"), Stack.Count);
		}
		Lines.Add(FString::Printf(TEXT("REWARDS (%s): %d gold%s   (rewards screen: DW-211)"),
			*StaticEnum<EDwCombatOutcome>()->GetNameStringByValue(static_cast<int64>(R.Outcome)), R.Gold, *Loot));
		for (const FDwXpGain& Gain : R.Xp)
		{
			Lines.Add(FString::Printf(TEXT("  %s +%d XP%s"), *CrewName(Gain.CrewId), Gain.Xp,
				Gain.LevelsGained > 0 ? *FString::Printf(TEXT("   LEVEL UP x%d"), Gain.LevelsGained) : TEXT("")));
		}
		for (int32 Choice = 0; Choice < R.AbilityChoices.Num(); ++Choice)
		{
			const FDwAbilityOffer& Offer = R.AbilityChoices[Choice];
			FString Who;
			for (const FGuid& Id : Offer.EligibleCrew)
			{
				Who += FString::Printf(TEXT("%s%s"), Who.IsEmpty() ? TEXT("") : TEXT(", "), *CrewName(Id));
			}
			Lines.Add(FString::Printf(TEXT("  [Dw.Run.Claim %d <crew#>]  learn %s (%s)"), Choice + 1, Offer.Ability ? *Offer.Ability->DisplayName.ToString() : TEXT("?"), *Who));
		}
		Lines.Add(R.AbilityChoices.IsEmpty() ? FString(TEXT("  [Dw.Run.Claim]  take it (no new moves to learn yet)")) : FString(TEXT("  [Dw.Run.Claim 0]  skip the new move")));
	}
	else if (const FDwPendingLevelUp* LevelUp = S.Phase == EDwRunPhase::LevelUp ? Run->GetCurrentLevelUp() : nullptr)
	{
		Lines.Add(FString::Printf(TEXT("LEVEL UP: %s reaches level %d (+%d max HP)   %d more waiting   (level-up screen: DW-211)"), *CrewName(LevelUp->CrewId),
			LevelUp->NewLevel, LevelUp->StatGain.MaxHealth, S.PendingLevelUps.Num() - 1));
		for (int32 Choice = 0; Choice < LevelUp->PerkChoices.Num(); ++Choice)
		{
			const UDwPerkDefinition* Perk = LevelUp->PerkChoices[Choice];
			Lines.Add(FString::Printf(TEXT("  [Dw.Run.Perk %d]  %s: %s"), Choice + 1, Perk ? *Perk->DisplayName.ToString() : TEXT("?"),
				Perk ? *Perk->GetFormattedDescription().ToString() : TEXT("")));
		}
		if (LevelUp->PerkChoices.IsEmpty())
		{
			Lines.Add(TEXT("  [Dw.Run.Perk]  continue (no perks left to offer)"));
		}
	}
	if (S.IsOver())
	{
		Lines.Add(FString::Printf(TEXT("RUN OVER: %s  (%.0f s)   Dw.Run.Start to sail again"), *DwRun::EndReasonName(S.Stats.EndReason), S.Stats.ElapsedSeconds));
	}
	else
	{
		const TArray<int32> Choices = Run->GetChoices();
		for (int32 Index = 0; Index < Choices.Num(); ++Index)
		{
			Lines.Add(FString::Printf(TEXT("  [Dw.Run.Choose %d]  %s"), Index + 1, *DwHUD::NodeLabel(S, Choices[Index])));
		}
	}

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	const float Scale = FMath::Max(1.f, Canvas->ClipY / 900.f);
	const float LineHeight = 20.f * Scale;
	const float X = 40.f * Scale;
	float Y = 40.f * Scale;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), X - 12.f * Scale, Y - 10.f * Scale, 1000.f * Scale, LineHeight * Lines.Num() + 20.f * Scale);
	for (const FString& Line : Lines)
	{
		DrawText(Line, FLinearColor(0.95f, 0.9f, 0.75f), X, Y, Font, Scale);
		Y += LineHeight;
	}
}
#endif
