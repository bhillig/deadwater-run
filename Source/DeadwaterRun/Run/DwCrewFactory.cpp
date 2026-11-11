// Copyright Brandon Hillig 2026

#include "Run/DwCrewFactory.h"

#include "Run/DwRunRandom.h"

FGuid FDwCrewFactory::MakeId(FDwRunState& State)
{
	return FGuid(static_cast<uint32>(FDwRunRandom::ForCrew(State.Seed)), 0x44575255u /* "DWRU" */, 0u, static_cast<uint32>(++State.LastCrewSerial));
}

FDwCrewMemberState& FDwCrewFactory::Create(FDwRunState& State, const UDwUnitDefinition& Unit, int32 Level, bool bIsCaptain)
{
	FDwCrewMemberState& Member = State.Crew.AddDefaulted_GetRef();
	Member.Id = MakeId(State);
	Member.Unit = &Unit;
	Member.bIsCaptain = bIsCaptain;
	Member.Level = FMath::Max(1, Level);
	Member.Health = Member.GetMaxHealth();
	return Member;
}
