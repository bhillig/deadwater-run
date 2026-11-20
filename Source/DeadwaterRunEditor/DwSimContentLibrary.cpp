// Copyright Brandon Hillig 2026

#include "DwSimContentLibrary.h"

#include "AI/DwAiProfile.h"
#include "DwContentAssetUtils.h"
#include "Misc/DataValidation.h"
#include "Sim/DwSimPolicy.h"

TArray<FString> UDwSimContentLibrary::CreateSimContent()
{
	using namespace DwContentAssets;
	TArray<FString> Log;
	bool bOk = true;

	// DW-214 (ai-engineer) replaces this with DA_AiProfile_SimPlayer; prefer it as soon as it exists.
	UDwAiProfile* PlayerProfile = Load<UDwAiProfile>(TEXT("/Game/DeadwaterRun/Data/AI"), TEXT("DA_AiProfile_SimPlayer"));
	if (!PlayerProfile)
	{
		PlayerProfile = Load<UDwAiProfile>(TEXT("/Game/DeadwaterRun/Data/AI"), TEXT("DA_AiProfile_Default"));
	}
	if (!PlayerProfile)
	{
		Log.Add(TEXT("ERROR DA_AiProfile_Default is missing (run the AI content first)"));
		Log.Add(TEXT("CONTENT FAIL"));
		return Log;
	}

	// The default player: takes what comes on the route, repairs below 70% hull, keeps four crew, buys upgrades,
	// moves and items, learns offered moves. Estimate numbers are first guesses until QA playtests calibrate them.
	UDwSimPolicy* Policy = FindOrCreate<UDwSimPolicy>(TEXT("/Game/DeadwaterRun/Data/Sim"), TEXT("DA_SimPolicy_Default"), Log);
	Policy->DisplayName = FText::FromString(TEXT("Default simulated player"));
	Policy->Description = FText::FromString(TEXT("Plays the crew with the crew AI, sails any route, repairs when hurt and spends the rest."));
	Policy->PlayerProfile = PlayerProfile;
	Policy->MaxRoundsPerFight = 40;
	Policy->FightWeight = 1.f;
	Policy->ShopWeight = 1.f;
	Policy->EventWeight = 1.f;
	Policy->HurtShopBias = 3.f;
	Policy->RepairBelowHullPct = 0.7f;
	Policy->RecruitBelowCrew = 4;
	Policy->bBuyShipUpgrades = true;
	Policy->bBuyAbilities = true;
	Policy->bBuyItems = true;
	Policy->bTakeAbilities = true;
	Policy->SecondsPerAction = 8.f;
	Policy->SecondsPerEnemyAction = 2.f;
	Policy->SecondsPerNode = 30.f;
	FDataValidationContext Context;
	if (Policy->IsDataValid(Context) == EDataValidationResult::Invalid)
	{
		for (const FDataValidationContext::FIssue& Issue : Context.GetIssues())
		{
			Log.Add(TEXT("ERROR DA_SimPolicy_Default: ") + Issue.Message.ToString());
		}
		bOk = false;
	}
	bOk &= Save(Policy, Log);
	Log.Add(FString::Printf(TEXT("DA_SimPolicy_Default plays the crew with %s"), *PlayerProfile->GetName()));

	Log.Add(bOk ? TEXT("CONTENT PASS") : TEXT("CONTENT FAIL"));
	return Log;
}
