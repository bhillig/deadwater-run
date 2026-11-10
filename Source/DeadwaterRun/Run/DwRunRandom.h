// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"

/**
 * Seed derivation for a run (Docs/Architecture/09-Run-Map.md "Seeds"). Every purpose gets its own stream from the run
 * seed and a fixed salt, so what happens at one node (a fight that lasts longer, a different shop purchase) never
 * shifts the randomness anywhere else. Pure and stable across platforms: only integer hashing, no engine RNG state.
 */
struct DEADWATERRUN_API FDwRunRandom
{
	/** Map generation; Attempt > 0 when the validator rejected the previous graph (DW-202). */
	static int32 ForMap(int32 RunSeed, int32 Attempt);
	/** The fight, shop or event at a node. Also the combat seed (FDwEncounterSetup::Seed). */
	static int32 ForNode(int32 RunSeed, int32 NodeIndex);
	/** Rewards after the fight at a node (DW-203). */
	static int32 ForRewards(int32 RunSeed, int32 NodeIndex);
	/** Perk choices of the Ordinal-th level-up after the fight at a node (DW-203): one stream per level-up, rolled in order. */
	static int32 ForPerks(int32 RunSeed, int32 NodeIndex, int32 Ordinal);
	/** Roster ids (starting crew and recruits). */
	static int32 ForCrew(int32 RunSeed);

	/** A fresh run seed from the clock: Set Sail without -DwSeed. Never negative. */
	static int32 MakeRunSeed();

private:
	static int32 Derive(int32 RunSeed, uint32 Salt, int32 Index);
};
