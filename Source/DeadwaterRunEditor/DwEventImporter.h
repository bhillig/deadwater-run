// Copyright Brandon Hillig 2026

#pragma once

#include "CoreMinimal.h"

class UDwGameData;

/**
 * Tuning/Events.json -> DA_Event_* (Docs/Architecture/11-Events.md, DW-205). The JSON is the content pipeline's event
 * format ("id", "title", "body", "valence", "weight", "minLayer", "maxLayer", "choices": [{ "label", "requirements",
 * "outcomes", "resultText" }]), so drafts Brandon approves can be pasted in as they are. Layers in the JSON are 1-based
 * (layer 1 = the first node, as the player counts); definitions store them 0-based like encounters. An empty outcome
 * list becomes an explicit Nothing. Items, units and captains are named by their short name ("HealthRestore",
 * "Swordsman", "Fearmongerer").
 *
 * Every event in the file is created or rebuilt in /Game/DeadwaterRun/Data/Events and becomes DA_GameData.Events, in
 * file order. Events no longer in the file stay on disk but leave the pool (logged). One log line per asset or problem;
 * "ERROR ..." lines fail the content run.
 */
struct FDwEventImporter
{
	static bool Import(UDwGameData& GameData, TArray<FString>& Log);
};
