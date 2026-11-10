// Copyright Brandon Hillig 2026

#include "Run/DwRunRandom.h"

#include "Misc/DateTime.h"
#include "Templates/TypeHash.h"

namespace DwRunRandom
{
	// Fixed per purpose. Changing one changes every seeded run for that purpose, so they never change.
	constexpr uint32 SaltMap = 0x4D415031;     // "MAP1"
	constexpr uint32 SaltNode = 0x4E4F4445;    // "NODE"
	constexpr uint32 SaltRewards = 0x52575244; // "RWRD"
	constexpr uint32 SaltCrew = 0x43524557;    // "CREW"

	/** 32-bit finaliser (MurmurHash3 fmix32): spreads nearby seeds and indices apart. */
	constexpr uint32 Mix(uint32 H)
	{
		H ^= H >> 16;
		H *= 0x85EBCA6Bu;
		H ^= H >> 13;
		H *= 0xC2B2AE35u;
		H ^= H >> 16;
		return H;
	}
}

int32 FDwRunRandom::Derive(int32 RunSeed, uint32 Salt, int32 Index)
{
	using namespace DwRunRandom;
	uint32 Hash = HashCombine(Mix(static_cast<uint32>(RunSeed)), Salt);
	Hash = Mix(HashCombine(Hash, static_cast<uint32>(Index)));
	return static_cast<int32>(Hash & 0x7FFFFFFF);
}

int32 FDwRunRandom::ForMap(int32 RunSeed, int32 Attempt)       { return Derive(RunSeed, DwRunRandom::SaltMap, Attempt); }
int32 FDwRunRandom::ForNode(int32 RunSeed, int32 NodeIndex)    { return Derive(RunSeed, DwRunRandom::SaltNode, NodeIndex); }
int32 FDwRunRandom::ForRewards(int32 RunSeed, int32 NodeIndex) { return Derive(RunSeed, DwRunRandom::SaltRewards, NodeIndex); }
int32 FDwRunRandom::ForCrew(int32 RunSeed)                     { return Derive(RunSeed, DwRunRandom::SaltCrew, 0); }

int32 FDwRunRandom::MakeRunSeed()
{
	return static_cast<int32>(DwRunRandom::Mix(static_cast<uint32>(FDateTime::UtcNow().GetTicks())) & 0x7FFFFFFF);
}
