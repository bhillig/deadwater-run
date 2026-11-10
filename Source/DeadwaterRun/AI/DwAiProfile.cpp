// Copyright Brandon Hillig 2026

#include "AI/DwAiProfile.h"

#include "Core/DwLog.h"

FDwAiTuningRow UDwAiProfile::GetTuningValues() const
{
	if (const FDwAiTuningRow* Row = GetTuningRow<FDwAiTuningRow>())
	{
		return *Row;
	}
	UE_LOG(LogDwAI, Error, TEXT("%s has no DT_AiTuning row ('%s'); the crew will not score anything."), *GetName(), *Tuning.RowName.ToString());
	return FDwAiTuningRow();
}

float FDwAiWeights::Dot(const FDwAiWeights& W, const FDwAiWeights& T)
{
	return W.Damage * T.Damage + W.Kill * T.Kill + W.Win * T.Win + W.ShipDamage * T.ShipDamage + W.ShipLowHull * T.ShipLowHull
		- W.FriendlyFire * T.FriendlyFire + W.Heal * T.Heal + W.Fear * T.Fear + W.LowHealthFocus * T.LowHealthFocus
		+ W.CaptainPressure * T.CaptainPressure - W.ManaCost * T.ManaCost
		- W.Danger * T.Danger - W.SinkingRisk * T.SinkingRisk - W.Approach * T.Approach
		+ W.HighGround * T.HighGround + W.Boarding * T.Boarding + W.CannonAffinity * T.CannonAffinity;
}

FString FDwAiWeights::ToString() const
{
	FString Out;
	auto Add = [&Out](const TCHAR* Name, float Value)
	{
		if (!FMath::IsNearlyZero(Value))
		{
			Out += FString::Printf(TEXT("%s%s=%.2f"), Out.IsEmpty() ? TEXT("") : TEXT(" "), Name, Value);
		}
	};
	Add(TEXT("Damage"), Damage);
	Add(TEXT("Kill"), Kill);
	Add(TEXT("Win"), Win);
	Add(TEXT("ShipDamage"), ShipDamage);
	Add(TEXT("ShipLowHull"), ShipLowHull);
	Add(TEXT("FriendlyFire"), FriendlyFire);
	Add(TEXT("Heal"), Heal);
	Add(TEXT("Fear"), Fear);
	Add(TEXT("LowHealthFocus"), LowHealthFocus);
	Add(TEXT("CaptainPressure"), CaptainPressure);
	Add(TEXT("ManaCost"), ManaCost);
	Add(TEXT("Danger"), Danger);
	Add(TEXT("SinkingRisk"), SinkingRisk);
	Add(TEXT("Approach"), Approach);
	Add(TEXT("HighGround"), HighGround);
	Add(TEXT("Boarding"), Boarding);
	Add(TEXT("CannonAffinity"), CannonAffinity);
	return Out.IsEmpty() ? TEXT("(none)") : Out;
}
