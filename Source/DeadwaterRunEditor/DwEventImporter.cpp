// Copyright Brandon Hillig 2026

#include "DwEventImporter.h"

#include "Core/DwGameData.h"
#include "Data/DwCaptainDefinition.h"
#include "Data/DwEventDefinition.h"
#include "Data/DwItemDefinition.h"
#include "Data/DwUnitDefinition.h"
#include "Dom/JsonObject.h"
#include "DwContentAssetUtils.h"
#include "Misc/DataValidation.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Run/Events/DwEventOutcome.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UObjectHash.h"

namespace DwEventImport
{
	const FString EventData = TEXT("/Game/DeadwaterRun/Data/Events");
	const FString ItemData = TEXT("/Game/DeadwaterRun/Data/Items");
	const FString UnitData = TEXT("/Game/DeadwaterRun/Data/Units");
	const FString CaptainData = TEXT("/Game/DeadwaterRun/Data/Captains");

	/** Reads one event; Where prefixes every problem ("DA_Event_X choices[1].outcomes[0]"). */
	struct FReader
	{
		UDwEventDefinition& Event;
		TArray<FString>& Log;
		bool bOk = true;

		void Error(const FString& Where, const FString& Problem)
		{
			Log.Add(FString::Printf(TEXT("ERROR %s %s: %s"), *Event.GetName(), *Where, *Problem));
			bOk = false;
		}

		FString String(const FJsonObject& Json, const TCHAR* Field, const FString& Where)
		{
			FString Value;
			if (!Json.TryGetStringField(Field, Value))
			{
				Error(Where, FString::Printf(TEXT("missing string \"%s\""), Field));
			}
			return Value;
		}

		int32 Int(const FJsonObject& Json, const TCHAR* Field, const FString& Where)
		{
			double Value = 0.0;
			if (!Json.TryGetNumberField(Field, Value))
			{
				Error(Where, FString::Printf(TEXT("missing number \"%s\""), Field));
			}
			return FMath::RoundToInt(Value);
		}

		template <typename T>
		T* Ref(const FString& Folder, const TCHAR* Prefix, const FString& Short, const FString& Where)
		{
			T* Found = DwContentAssets::Load<T>(Folder, FString::Printf(TEXT("%s%s"), Prefix, *Short));
			if (!Found)
			{
				Error(Where, FString::Printf(TEXT("no %s%s in %s"), Prefix, *Short, *Folder));
			}
			return Found;
		}

		EDwCrewSelector Selector(const FJsonObject& Json, const FString& Where)
		{
			const FString Name = String(Json, TEXT("selector"), Where);
			const int64 Value = StaticEnum<EDwCrewSelector>()->GetValueByNameString(Name);
			if (Value == INDEX_NONE)
			{
				Error(Where, FString::Printf(TEXT("unknown selector \"%s\" (All, Random, Captain, Chosen)"), *Name));
				return EDwCrewSelector::All;
			}
			return static_cast<EDwCrewSelector>(Value);
		}

		UDwEventOutcome* Outcome(const FJsonObject& Json, const FString& Where)
		{
			const FString Type = String(Json, TEXT("type"), Where);
			if (Type == TEXT("Nothing"))
			{
				return NewObject<UDwEventOutcome_Nothing>(&Event);
			}
			if (Type == TEXT("Gold"))
			{
				UDwEventOutcome_Gold* Out = NewObject<UDwEventOutcome_Gold>(&Event);
				Out->Delta = Int(Json, TEXT("delta"), Where);
				return Out;
			}
			if (Type == TEXT("ShipHealth"))
			{
				UDwEventOutcome_ShipHealth* Out = NewObject<UDwEventOutcome_ShipHealth>(&Event);
				Out->Delta = Int(Json, TEXT("delta"), Where);
				return Out;
			}
			if (Type == TEXT("CrewHealth"))
			{
				UDwEventOutcome_CrewHealth* Out = NewObject<UDwEventOutcome_CrewHealth>(&Event);
				Out->Delta = Int(Json, TEXT("delta"), Where);
				Out->Selector = Selector(Json, Where);
				return Out;
			}
			if (Type == TEXT("GiveItem"))
			{
				UDwEventOutcome_GiveItem* Out = NewObject<UDwEventOutcome_GiveItem>(&Event);
				Out->Item = Ref<UDwItemDefinition>(ItemData, TEXT("DA_Item_"), String(Json, TEXT("item"), Where), Where);
				Out->Count = Int(Json, TEXT("count"), Where);
				return Out;
			}
			if (Type == TEXT("AbilityChoice"))
			{
				UDwEventOutcome_AbilityChoice* Out = NewObject<UDwEventOutcome_AbilityChoice>(&Event);
				Out->Count = Int(Json, TEXT("count"), Where);
				return Out;
			}
			if (Type == TEXT("GrantXp"))
			{
				UDwEventOutcome_GrantXp* Out = NewObject<UDwEventOutcome_GrantXp>(&Event);
				Out->Amount = Int(Json, TEXT("amount"), Where);
				Out->Selector = Selector(Json, Where);
				return Out;
			}
			if (Type == TEXT("Recruit"))
			{
				UDwEventOutcome_Recruit* Out = NewObject<UDwEventOutcome_Recruit>(&Event);
				Out->Unit = Ref<UDwUnitDefinition>(UnitData, TEXT("DA_Unit_"), String(Json, TEXT("unit"), Where), Where);
				return Out;
			}
			if (Type == TEXT("LoseCrew"))
			{
				UDwEventOutcome_LoseCrew* Out = NewObject<UDwEventOutcome_LoseCrew>(&Event);
				Out->Selector = Selector(Json, Where);
				return Out;
			}
			if (Type == TEXT("RandomOf"))
			{
				UDwEventOutcome_RandomOf* Out = NewObject<UDwEventOutcome_RandomOf>(&Event);
				const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
				if (!Json.TryGetArrayField(TEXT("options"), Options))
				{
					Error(Where, TEXT("RandomOf needs \"options\""));
					return Out;
				}
				for (int32 Index = 0; Index < Options->Num(); ++Index)
				{
					const FString OptionWhere = FString::Printf(TEXT("%s.options[%d]"), *Where, Index);
					const TSharedPtr<FJsonObject>* OptionJson = nullptr;
					if (!(*Options)[Index]->TryGetObject(OptionJson))
					{
						Error(OptionWhere, TEXT("not an object"));
						continue;
					}
					FDwEventRandomOption& Option = Out->Options.AddDefaulted_GetRef();
					double Weight = 1.0;
					(*OptionJson)->TryGetNumberField(TEXT("weight"), Weight);
					Option.Weight = static_cast<float>(Weight);
					Option.Outcomes = Outcomes(**OptionJson, OptionWhere, /*bEmptyIsNothing*/ false);
				}
				return Out;
			}
			Error(Where, FString::Printf(TEXT("unknown outcome type \"%s\""), *Type));
			return nullptr;
		}

		TArray<TObjectPtr<UDwEventOutcome>> Outcomes(const FJsonObject& Json, const FString& Where, bool bEmptyIsNothing)
		{
			TArray<TObjectPtr<UDwEventOutcome>> Out;
			const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
			if (Json.TryGetArrayField(TEXT("outcomes"), List))
			{
				for (int32 Index = 0; Index < List->Num(); ++Index)
				{
					const TSharedPtr<FJsonObject>* Item = nullptr;
					const FString ItemWhere = FString::Printf(TEXT("%s.outcomes[%d]"), *Where, Index);
					if (!(*List)[Index]->TryGetObject(Item))
					{
						Error(ItemWhere, TEXT("not an object"));
					}
					else if (UDwEventOutcome* Outcome = this->Outcome(**Item, ItemWhere))
					{
						Out.Add(Outcome);
					}
				}
			}
			if (Out.IsEmpty() && bEmptyIsNothing)
			{
				Out.Add(NewObject<UDwEventOutcome_Nothing>(&Event));
			}
			return Out;
		}

		UDwEventRequirement* Requirement(const FJsonObject& Json, const FString& Where)
		{
			const FString Type = String(Json, TEXT("type"), Where);
			if (Type == TEXT("MinGold"))
			{
				UDwEventRequirement_MinGold* Out = NewObject<UDwEventRequirement_MinGold>(&Event);
				Out->Amount = Int(Json, TEXT("amount"), Where);
				return Out;
			}
			if (Type == TEXT("CrewCountAtLeast"))
			{
				UDwEventRequirement_CrewCountAtLeast* Out = NewObject<UDwEventRequirement_CrewCountAtLeast>(&Event);
				Out->Count = Int(Json, TEXT("count"), Where);
				return Out;
			}
			if (Type == TEXT("CrewCountAtMost"))
			{
				UDwEventRequirement_CrewCountAtMost* Out = NewObject<UDwEventRequirement_CrewCountAtMost>(&Event);
				Out->Count = Int(Json, TEXT("count"), Where);
				return Out;
			}
			if (Type == TEXT("HasItem"))
			{
				UDwEventRequirement_HasItem* Out = NewObject<UDwEventRequirement_HasItem>(&Event);
				Out->Item = Ref<UDwItemDefinition>(ItemData, TEXT("DA_Item_"), String(Json, TEXT("item"), Where), Where);
				double Count = 1.0;
				Json.TryGetNumberField(TEXT("count"), Count);
				Out->Count = FMath::Max(1, FMath::RoundToInt(Count));
				return Out;
			}
			if (Type == TEXT("CaptainIs"))
			{
				UDwEventRequirement_CaptainIs* Out = NewObject<UDwEventRequirement_CaptainIs>(&Event);
				Out->Captain = Ref<UDwCaptainDefinition>(CaptainData, TEXT("DA_Captain_"), String(Json, TEXT("captain"), Where), Where);
				return Out;
			}
			Error(Where, FString::Printf(TEXT("unknown requirement type \"%s\""), *Type));
			return nullptr;
		}

		void Read(const FJsonObject& Json)
		{
			Event.Title = FText::FromString(String(Json, TEXT("title"), TEXT("")));
			Event.DisplayName = Event.Title;
			Event.Body = FText::FromString(String(Json, TEXT("body"), TEXT("")));
			const FString Valence = String(Json, TEXT("valence"), TEXT(""));
			if (Valence == TEXT("Good") || Valence == TEXT("Bad"))
			{
				Event.Valence = Valence == TEXT("Good") ? EDwEventValence::Good : EDwEventValence::Bad;
			}
			else
			{
				Error(TEXT("valence"), FString::Printf(TEXT("\"%s\" is not Good or Bad"), *Valence));
			}
			double Weight = 1.0;
			Json.TryGetNumberField(TEXT("weight"), Weight);
			Event.Weight = static_cast<float>(Weight);
			// 1-based in the file (as the player counts nodes), 0-based like encounter layers in the game.
			Event.MinLayer = FMath::Max(0, Int(Json, TEXT("minLayer"), TEXT("")) - 1);
			Event.MaxLayer = FMath::Max(0, Int(Json, TEXT("maxLayer"), TEXT("")) - 1);

			Event.Choices.Reset();
			const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
			if (!Json.TryGetArrayField(TEXT("choices"), Choices))
			{
				Error(TEXT("choices"), TEXT("missing"));
				return;
			}
			for (int32 Index = 0; Index < Choices->Num(); ++Index)
			{
				const FString Where = FString::Printf(TEXT("choices[%d]"), Index);
				const TSharedPtr<FJsonObject>* ChoiceJson = nullptr;
				if (!(*Choices)[Index]->TryGetObject(ChoiceJson))
				{
					Error(Where, TEXT("not an object"));
					continue;
				}
				FDwEventChoice& Choice = Event.Choices.AddDefaulted_GetRef();
				Choice.Label = FText::FromString(String(**ChoiceJson, TEXT("label"), Where));
				Choice.ResultText = FText::FromString(String(**ChoiceJson, TEXT("resultText"), Where));
				Choice.Outcomes = Outcomes(**ChoiceJson, Where, /*bEmptyIsNothing*/ true);
				const TArray<TSharedPtr<FJsonValue>>* Requirements = nullptr;
				if ((*ChoiceJson)->TryGetArrayField(TEXT("requirements"), Requirements))
				{
					for (int32 R = 0; R < Requirements->Num(); ++R)
					{
						const FString ReqWhere = FString::Printf(TEXT("%s.requirements[%d]"), *Where, R);
						const TSharedPtr<FJsonObject>* ReqJson = nullptr;
						if (!(*Requirements)[R]->TryGetObject(ReqJson))
						{
							Error(ReqWhere, TEXT("not an object"));
						}
						else if (UDwEventRequirement* Requirement = this->Requirement(**ReqJson, ReqWhere))
						{
							Choice.Requirements.Add(Requirement);
						}
					}
				}
			}
		}
	};

	/** Outcome and requirement objects left over from the previous import: out of the package, so they aren't saved. */
	void DiscardInstancedObjects(UDwEventDefinition& Event)
	{
		TArray<UObject*> Inner;
		GetObjectsWithOuter(&Event, Inner, EGetObjectsFlags::IncludeNestedObjects);
		for (UObject* Object : Inner)
		{
			if (Object->IsA<UDwEventOutcome>() || Object->IsA<UDwEventRequirement>())
			{
				Object->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
				Object->MarkAsGarbage();
			}
		}
	}
}

bool FDwEventImporter::Import(UDwGameData& GameData, TArray<FString>& Log)
{
	using namespace DwEventImport;
	const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Tuning"), TEXT("Events.json"));
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root)
	{
		Log.Add(FString::Printf(TEXT("ERROR %s is missing or not valid JSON"), *Path));
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (!Root->TryGetArrayField(TEXT("events"), List))
	{
		Log.Add(TEXT("ERROR Tuning/Events.json has no \"events\" array"));
		return false;
	}

	bool bOk = true;
	TArray<TObjectPtr<UDwEventDefinition>> Pool;
	int32 Good = 0, Bad = 0;
	for (int32 Index = 0; Index < List->Num(); ++Index)
	{
		const TSharedPtr<FJsonObject>* Json = nullptr;
		FString Id;
		if (!(*List)[Index]->TryGetObject(Json) || !(*Json)->TryGetStringField(TEXT("id"), Id) || !Id.StartsWith(TEXT("DA_Event_")))
		{
			Log.Add(FString::Printf(TEXT("ERROR Tuning/Events.json events[%d] needs an \"id\" starting with DA_Event_"), Index));
			bOk = false;
			continue;
		}
		UDwEventDefinition* Event = DwContentAssets::FindOrCreate<UDwEventDefinition>(EventData, Id, Log);
		DiscardInstancedObjects(*Event);
		FReader Reader{ *Event, Log };
		Reader.Read(**Json);
		bOk &= Reader.bOk;

		FDataValidationContext Validation;
		if (Event->IsDataValid(Validation) == EDataValidationResult::Invalid)
		{
			for (const FDataValidationContext::FIssue& Issue : Validation.GetIssues())
			{
				Log.Add(FString::Printf(TEXT("ERROR %s: %s"), *Id, *Issue.Message.ToString()));
			}
			bOk = false;
		}
		bOk &= DwContentAssets::Save(Event, Log);
		(Event->Valence == EDwEventValence::Good ? Good : Bad)++;
		Pool.Add(Event);
	}

	for (const UDwEventDefinition* Old : GameData.Events)
	{
		if (Old && !Pool.Contains(Old))
		{
			Log.Add(FString::Printf(TEXT("%s is no longer in Tuning/Events.json: left out of the pool"), *Old->GetName()));
		}
	}
	GameData.Events = Pool;
	if (Good == 0 || Bad == 0)
	{
		Log.Add(FString::Printf(TEXT("ERROR the event pool needs at least one Good and one Bad event (has %d Good, %d Bad)"), Good, Bad));
		bOk = false;
	}
	Log.Add(FString::Printf(TEXT("Events: %d in the pool (%d Good, %d Bad) from Tuning/Events.json"), Pool.Num(), Good, Bad));
	return bOk;
}
