// Copyright Brandon Hillig 2026

// UI art pass: the icon studio's manifest (Tools/Content/ui_icon_renders.json) parses, rejects bad entries, and every
// mesh or Blueprint it names exists.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Dev/DwIconStudioSubsystem.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

BEGIN_DEFINE_SPEC(FDwIconStudioSpec, "DeadwaterRun.Dev.IconStudio",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	int32 Size = 0;
	float Key = 0.f;
	float Fill = 0.f;
	TArray<FDwIconRenderJob> Jobs;
	FString Error;

	bool Parse(const FString& Json)
	{
		Size = 512; Key = 6.f; Fill = 2.f;
		Jobs.Reset();
		Error.Reset();
		return UDwIconStudioSubsystem::ParseManifest(Json, Size, Key, Fill, Jobs, Error);
	}

END_DEFINE_SPEC(FDwIconStudioSpec)

void FDwIconStudioSpec::Define()
{
	It("reads sizes, lights and every job field", [this]()
	{
		const bool bOk = Parse(TEXT(R"({ "size": 128, "keyLight": 9, "fillLight": 3, "icons": [
			{ "name": "A", "mesh": "/Game/M", "view": "side", "roll": 30, "flip": true, "fill": 0.5 },
			{ "name": "B", "actor": "/Game/BP", "view": "three", "dir": [1, 0, 0], "focusBone": "head", "focusRadius": 40, "focusLift": 2 } ] })"));
		TestTrue(TEXT("parses"), bOk);
		TestEqual(TEXT("size"), Size, 128);
		TestEqual(TEXT("key light"), Key, 9.f);
		TestEqual(TEXT("fill light"), Fill, 3.f);
		if (!TestEqual(TEXT("two jobs"), Jobs.Num(), 2))
		{
			return;
		}
		TestEqual(TEXT("mesh"), Jobs[0].MeshPath, FString(TEXT("/Game/M")));
		TestEqual(TEXT("roll"), Jobs[0].Roll, 30.f);
		TestTrue(TEXT("flip"), Jobs[0].bFlip);
		TestEqual(TEXT("fill"), Jobs[0].Fill, 0.5f);
		TestEqual(TEXT("actor"), Jobs[1].ActorPath, FString(TEXT("/Game/BP")));
		TestEqual(TEXT("view"), Jobs[1].View, FString(TEXT("three")));
		TestEqual(TEXT("direction"), Jobs[1].Direction, FVector(1.0, 0.0, 0.0));
		TestEqual(TEXT("focus bone"), Jobs[1].FocusBone, FName(TEXT("head")));
		TestEqual(TEXT("focus radius"), Jobs[1].FocusRadius, 40.f);
	});

	It("rejects bad manifests with a reason", [this]()
	{
		TestFalse(TEXT("not JSON"), Parse(TEXT("{ nope")));
		TestFalse(TEXT("no icons"), Parse(TEXT(R"({ "size": 64 })")));
		TestFalse(TEXT("no name"), Parse(TEXT(R"({ "icons": [ { "mesh": "/Game/M" } ] })")));
		TestFalse(TEXT("neither mesh nor actor"), Parse(TEXT(R"({ "icons": [ { "name": "A" } ] })")));
		TestFalse(TEXT("both mesh and actor"), Parse(TEXT(R"({ "icons": [ { "name": "A", "mesh": "/Game/M", "actor": "/Game/BP" } ] })")));
		TestFalse(TEXT("unknown view"), Parse(TEXT(R"({ "icons": [ { "name": "A", "mesh": "/Game/M", "view": "top" } ] })")));
		TestFalse(TEXT("the last failure has a reason"), Error.IsEmpty());
	});

	It("the project manifest parses and every source asset exists", [this]()
	{
		FString Json;
		const FString File = FPaths::Combine(FPaths::ProjectDir(), TEXT("Tools/Content/ui_icon_renders.json"));
		if (!TestTrue(TEXT("manifest readable"), FFileHelper::LoadFileToString(Json, *File)) || !TestTrue(TEXT("parses: ") + Error, Parse(Json)))
		{
			return;
		}
		TSet<FString> Names;
		for (const FDwIconRenderJob& Job : Jobs)
		{
			bool bDuplicate = false;
			Names.Add(Job.Name, &bDuplicate);
			TestFalse(FString::Printf(TEXT("%s is unique"), *Job.Name), bDuplicate);
			if (!Job.MeshPath.IsEmpty())
			{
				TestNotNull(FString::Printf(TEXT("%s mesh %s"), *Job.Name, *Job.MeshPath), LoadObject<UStaticMesh>(nullptr, *Job.MeshPath));
			}
			else
			{
				const FString ClassPath = FString::Printf(TEXT("%s.%s_C"), *Job.ActorPath, *FPackageName::GetShortName(Job.ActorPath));
				TestNotNull(FString::Printf(TEXT("%s actor %s"), *Job.Name, *Job.ActorPath), LoadClass<AActor>(nullptr, *ClassPath));
			}
		}
	});
}

#endif
