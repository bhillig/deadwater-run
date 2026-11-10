// Copyright Brandon Hillig 2026

#include "Core/DwHUD.h"

#include "Data/DwCaptainDefinition.h"
#include "Data/DwEncounterDefinition.h"
#include "Data/DwItemDefinition.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "HAL/IConsoleManager.h"
#include "Run/DwRun.h"
#include "Run/DwRunSubsystem.h"

#if !UE_BUILD_SHIPPING
namespace DwHUD
{
	static TAutoConsoleVariable<bool> CVarRunReadout(TEXT("dw.Run.Readout"), true,
		TEXT("Development builds: draw the run readout (phase, crew, routes) outside combat until the map screen exists (DW-211)."));

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
	for (const FDwCrewMemberState& Member : S.Crew)
	{
		if (Member.bAlive)
		{
			Lines.Add(FString::Printf(TEXT("  %s%s   L%d   %.0f / %.0f HP"), *Member.GetDisplayName().ToString(),
				Member.bIsCaptain ? TEXT(" (captain)") : TEXT(""), Member.Level, Member.Health, Member.GetMaxHealth()));
			continue;
		}
		const FDwCrewLoss* Loss = S.Stats.CrewLost.FindByPredicate([&Member](const FDwCrewLoss& L) { return L.CrewId == Member.Id; });
		Lines.Add(FString::Printf(TEXT("  %s   lost at node %d"), *Member.GetDisplayName().ToString(), Loss ? Loss->Node : INDEX_NONE));
	}
	Lines.Add(FString::Printf(TEXT("Cleared %d   sunk %d   plundered %d   surrendered %d   kills %d   rounds %d"), S.Stats.NodesCleared,
		S.Stats.FightsSunk, S.Stats.FightsPlundered, S.Stats.FightsSurrendered, S.Stats.Kills, S.Stats.Rounds));
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
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), X - 12.f * Scale, Y - 10.f * Scale, 760.f * Scale, LineHeight * Lines.Num() + 20.f * Scale);
	for (const FString& Line : Lines)
	{
		DrawText(Line, FLinearColor(0.95f, 0.9f, 0.75f), X, Y, Font, Scale);
		Y += LineHeight;
	}
}
#endif
