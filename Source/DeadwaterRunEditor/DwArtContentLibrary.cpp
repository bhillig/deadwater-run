// Copyright Brandon Hillig 2026

#include "DwArtContentLibrary.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Board/DwBoard.h"
#include "Board/DwDeckLayout.h"
#include "Combat/DwCombatUnit.h"
#include "Combat/DwShip.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/DwGameData.h"
#include "Data/DwEncounterDefinition.h"
#include "Data/DwUnitDefinition.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "GameFramework/Character.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "NiagaraSystem.h"
#include "Presentation/DwSeaEnvironment.h"
#include "UObject/Package.h"

namespace DwArtContent
{
	const FString Data = TEXT("/Game/DeadwaterRun/Data");
	const FString Boards = TEXT("/Game/DeadwaterRun/Data/Boards");
	const FString UnitData = TEXT("/Game/DeadwaterRun/Data/Units");
	const FString EncounterData = TEXT("/Game/DeadwaterRun/Data/Encounters");
	const FString MoodData = TEXT("/Game/DeadwaterRun/Data/Sea");
	const FString Materials = TEXT("/Game/DeadwaterRun/Materials");
	const FString Board = TEXT("/Game/DeadwaterRun/Board");
	const FString Ships = TEXT("/Game/DeadwaterRun/Ships");
	const FString Units = TEXT("/Game/DeadwaterRun/Units");
	const FString SeaLevel = TEXT("/Game/DeadwaterRun/Maps/L_Sea");

	const FString Pirate = TEXT("/Game/Synty/PolygonPirateRealm");
	const FString PirateMeshes = Pirate + TEXT("/Meshes");
	const FString Core = TEXT("/Game/Synty/PNB_Core");

	// ---- Tables: every number that places art. Units are world centimetres and degrees. ----

	/** Hull scale: the small ship's waist deck is 5.3 m wide; six 1 m tiles need a little more. */
	constexpr float HullScale = 1.18f;

	struct FShipSpec
	{
		const TCHAR* Blueprint;
		const TCHAR* Preset;      // Synty BP_Veh_Preset_*
		const TCHAR* Layout;      // deck layout this hull is aligned to
		FVector DeckCentre;       // where the grid's middle goes on the open waist deck, on the planks, in the preset's space
	};
	// The grid sits as far aft on the waist as it fits: the forecastle is on the camera's side and would hide the near row.
	const FShipSpec ShipSpecs[] = {
		{ TEXT("BP_Ship_Sloop_Player"), TEXT("BP_Veh_Preset_S_01"), TEXT("DA_Deck_PlayerShip"), FVector(0.f, 40.f, 147.f) },
		{ TEXT("BP_Ship_Sloop_Enemy"), TEXT("BP_Veh_Preset_S_02"), TEXT("DA_Deck_EnemyPlaceholder"), FVector(9.f, 98.f, 160.f) },
	};

	/** Room for both hull walls between the two decks (each hull is wider than its 6-tile grid). */
	constexpr float DeckGap = 190.f;

	struct FUnitSpec
	{
		const TCHAR* Blueprint;
		const TCHAR* SyntyCharacter; // BP_Chr_*: mesh and attachments are read from it
		const TCHAR* Weapon;         // SM_Wep_* in the right hand (Synty weapons point along their +Y)
		float WeaponPitch;           // degrees the weapon points below level, straight ahead, while the pirate idles
	};
	constexpr float SwordPitch = 35.f; // blade lowered, at rest
	constexpr float RiflePitch = 20.f;
	const FUnitSpec UnitSpecs[] = {
		{ TEXT("BP_Unit_Captain"), TEXT("BP_Chr_Captain_01"), TEXT("SM_Wep_Sword_02"), SwordPitch },
		{ TEXT("BP_Unit_Swordsman"), TEXT("BP_Chr_Male_01"), TEXT("SM_Wep_Sword_01"), SwordPitch },
		{ TEXT("BP_Unit_Sniper"), TEXT("BP_Chr_Female_01"), TEXT("SM_Wep_Rifle_01"), RiflePitch },
		{ TEXT("BP_Unit_Raider_Swordsman"), TEXT("BP_Chr_Male_03"), TEXT("SM_Wep_Sword_04"), SwordPitch },
		{ TEXT("BP_Unit_Raider_Sniper"), TEXT("BP_Chr_Female_04"), TEXT("SM_Wep_Rifle_03"), RiflePitch },
	};
	const FName WeaponBone = TEXT("hand_r");

	// ---- Helpers ----

	template <typename T>
	T* Load(const FString& Folder, const FString& Name)
	{
		return LoadObject<T>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), *Folder, *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	template <typename T>
	T* Require(const FString& Folder, const FString& Name, TArray<FString>& Log, bool& bOk)
	{
		T* Asset = Load<T>(Folder, Name);
		if (!Asset)
		{
			Log.Add(FString::Printf(TEXT("ERROR missing %s/%s"), *Folder, *Name));
			bOk = false;
		}
		return Asset;
	}

	template <typename T>
	T* FindOrCreate(const FString& Folder, const FString& Name, TArray<FString>& Log)
	{
		if (T* Existing = Load<T>(Folder, Name))
		{
			Log.Add(FString::Printf(TEXT("Updated %s/%s"), *Folder, *Name));
			return Existing;
		}
		UPackage* Package = CreatePackage(*(Folder / Name));
		T* Asset = NewObject<T>(Package, *Name, RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Asset);
		Log.Add(FString::Printf(TEXT("Created %s/%s"), *Folder, *Name));
		return Asset;
	}

	UBlueprint* FindOrCreateBlueprint(const FString& Folder, const FString& Name, UClass* Parent, TArray<FString>& Log)
	{
		if (UBlueprint* Existing = Load<UBlueprint>(Folder, Name))
		{
			if (Existing->ParentClass == Parent)
			{
				Log.Add(FString::Printf(TEXT("Updated %s/%s"), *Folder, *Name));
				return Existing;
			}
			Log.Add(FString::Printf(TEXT("ERROR %s/%s exists with parent %s, expected %s"), *Folder, *Name, *GetNameSafe(Existing->ParentClass), *Parent->GetName()));
			return nullptr;
		}
		UPackage* Package = CreatePackage(*(Folder / Name));
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(Parent, Package, *Name, BPTYPE_Normal,
			UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), NAME_None);
		FAssetRegistryModule::AssetCreated(Blueprint);
		Log.Add(FString::Printf(TEXT("Created %s/%s (parent %s)"), *Folder, *Name, *Parent->GetName()));
		return Blueprint;
	}

	template <typename T>
	T* Defaults(UBlueprint* Blueprint)
	{
		return Blueprint && Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject<T>() : nullptr;
	}

	bool Save(UObject* Asset, TArray<FString>& Log)
	{
		if (!Asset)
		{
			return false;
		}
		Asset->MarkPackageDirty();
		if (!UEditorAssetLibrary::SaveLoadedAsset(Asset, /*bOnlyIfIsDirty*/ false))
		{
			Log.Add(FString::Printf(TEXT("ERROR could not save %s"), *Asset->GetPathName()));
			return false;
		}
		return true;
	}

	FDwMeshPiece Piece(UStaticMesh* Mesh, const FTransform& Transform, FName Socket = NAME_None)
	{
		FDwMeshPiece Out;
		Out.Mesh = Mesh;
		Out.Transform = Transform;
		Out.Socket = Socket;
		return Out;
	}

	/** A component template's transform relative to the Blueprint's root (Synty nests wheels under cannons, etc.). */
	FTransform TransformInBlueprint(const USimpleConstructionScript& Script, USCS_Node& Node)
	{
		const USceneComponent* Template = Cast<USceneComponent>(Node.ComponentTemplate);
		FTransform Transform = Template ? Template->GetRelativeTransform() : FTransform::Identity;
		for (USCS_Node* Parent = Script.FindParentNode(&Node); Parent; Parent = Script.FindParentNode(Parent))
		{
			if (const USceneComponent* ParentTemplate = Cast<USceneComponent>(Parent->ComponentTemplate))
			{
				Transform *= ParentTemplate->GetRelativeTransform();
			}
		}
		return Transform;
	}

	/** Every static mesh component a Synty Blueprint adds, as pieces in the Blueprint's own space. */
	bool ReadStaticMeshes(const FString& Folder, const FString& Name, TArray<FDwMeshPiece>& OutPieces, TArray<FString>& Log)
	{
		const UBlueprint* Blueprint = Load<UBlueprint>(Folder, Name);
		if (!Blueprint || !Blueprint->SimpleConstructionScript)
		{
			Log.Add(FString::Printf(TEXT("ERROR missing %s/%s"), *Folder, *Name));
			return false;
		}
		for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
		{
			const UStaticMeshComponent* Template = Node ? Cast<UStaticMeshComponent>(Node->ComponentTemplate) : nullptr;
			if (Template && Template->GetStaticMesh())
			{
				OutPieces.Add(Piece(Template->GetStaticMesh(), TransformInBlueprint(*Blueprint->SimpleConstructionScript, *Node), Node->AttachToName));
			}
		}
		return true;
	}

	/**
	 * A translucent unlit material for flat overlays: emissive = "Color" rgb, opacity = OpacityCode(UV, Fill, EdgeOpacity,
	 * EdgeWidth) where Fill is "Color" alpha. Tiles and unit rings differ only in that shape code.
	 */
	UMaterial* MakeOverlayMaterial(const FString& Name, const TCHAR* OpacityCode, float EdgeWidth, TArray<FString>& Log)
	{
		UMaterial* Material = FindOrCreate<UMaterial>(Materials, Name, Log);
		Material->PreEditChange(nullptr);
		Material->GetExpressionCollection().Empty();
		Material->MaterialDomain = MD_Surface;
		Material->BlendMode = BLEND_Translucent;
		Material->SetShadingModel(MSM_Unlit);
		Material->TwoSided = false;
		// ADwBoard draws tiles as instances; without the usage flag the game shows the grey default material.
		Material->SetMaterialUsage(MATUSAGE_InstancedStaticMeshes);

		auto Add = [Material](UMaterialExpression* Expression, int32 X, int32 Y)
		{
			Expression->MaterialExpressionEditorX = X;
			Expression->MaterialExpressionEditorY = Y;
			Material->GetExpressionCollection().AddExpression(Expression);
		};
		UMaterialExpressionVectorParameter* Color = NewObject<UMaterialExpressionVectorParameter>(Material);
		Color->ParameterName = TEXT("Color");
		Color->DefaultValue = FLinearColor(1.f, 1.f, 1.f, 0.3f);
		Add(Color, -700, 0);
		UMaterialExpressionScalarParameter* EdgeOpacity = NewObject<UMaterialExpressionScalarParameter>(Material);
		EdgeOpacity->ParameterName = TEXT("EdgeOpacity");
		EdgeOpacity->DefaultValue = 0.9f;
		Add(EdgeOpacity, -700, 250);
		UMaterialExpressionScalarParameter* Width = NewObject<UMaterialExpressionScalarParameter>(Material);
		Width->ParameterName = TEXT("EdgeWidth");
		Width->DefaultValue = EdgeWidth;
		Add(Width, -700, 350);
		UMaterialExpressionTextureCoordinate* UV = NewObject<UMaterialExpressionTextureCoordinate>(Material);
		Add(UV, -700, 450);

		UMaterialExpressionCustom* Shape = NewObject<UMaterialExpressionCustom>(Material);
		Shape->Description = TEXT("OverlayOpacity");
		Shape->OutputType = CMOT_Float1;
		Shape->Code = OpacityCode;
		Shape->Inputs.SetNum(4);
		Shape->Inputs[0].InputName = TEXT("UV");
		Shape->Inputs[0].Input.Connect(0, UV);
		Shape->Inputs[1].InputName = TEXT("Fill");
		Shape->Inputs[1].Input.Connect(4, Color); // alpha
		Shape->Inputs[2].InputName = TEXT("EdgeOpacity");
		Shape->Inputs[2].Input.Connect(0, EdgeOpacity);
		Shape->Inputs[3].InputName = TEXT("EdgeWidth");
		Shape->Inputs[3].Input.Connect(0, Width);
		Add(Shape, -350, 300);

		UMaterialEditorOnlyData* Graph = Material->GetEditorOnlyData();
		Graph->EmissiveColor.Connect(0, Color);
		Graph->Opacity.Connect(0, Shape);
		Material->PostEditChange();
		return Material;
	}

	/** The props a sloop puts on its feature tiles (FDwDeckDressing, tile space). The mast comes from the preset. */
	bool MakeDressing(FDwDeckDressing& Out, TArray<FString>& Log)
	{
		bool bOk = true;
		const FString Props = PirateMeshes + TEXT("/Props");
		UStaticMesh* Crate = Require<UStaticMesh>(Props, TEXT("SM_Prop_Crate_01"), Log, bOk);
		UStaticMesh* Barrel = Require<UStaticMesh>(Props, TEXT("SM_Prop_Barrel_01"), Log, bOk);
		UStaticMesh* Ladder = Require<UStaticMesh>(Props, TEXT("SM_Prop_Ladder_01"), Log, bOk);
		UStaticMesh* Nest = Require<UStaticMesh>(Props, TEXT("SM_Prop_Crows_Nest_02"), Log, bOk);

		// Cannon: Synty's cannon (barrel along +Y, 2.9 m long) at half size, turned to fire along +X, at the back of
		// the tile so a gunner can stand on it.
		TArray<FDwMeshPiece> Cannon;
		bOk &= ReadStaticMeshes(Pirate + TEXT("/Blueprints/Weapons"), TEXT("BP_Wep_Cannon_01"), Cannon, Log);
		const FTransform CannonPlacement(FRotator(0.f, -90.f, 0.f), FVector(8.f, -30.f, 0.f), FVector(0.5f));
		for (FDwMeshPiece& Part : Cannon)
		{
			Part.Transform *= CannonPlacement;
			Part.Socket = NAME_None;
		}
		Out.Cannon = Cannon;

		// Low obstacle: a crate with a barrel beside it.
		Out.LowObstacle = {
			Piece(Crate, FTransform(FRotator(0.f, 18.f, 0.f), FVector(-14.f, -10.f, 0.f), FVector(1.05f))),
			Piece(Barrel, FTransform(FRotator(0.f, 0.f, 0.f), FVector(26.f, 24.f, 0.f), FVector(0.55f))) };

		// Ladder: 3.05 m tall, 0.66 m wide along its own +X. Stands at the tile's +X edge, facing the nest.
		Out.Ladder = { Piece(Ladder, FTransform(FRotator(0.f, 90.f, 0.f), FVector(44.f, -33.f, 0.f), FVector(1.f, 1.f, 0.68f))) };

		// Nest: three stacked crates up to the nest floor (one elevation = 200 above the deck), ringed by a crow's nest.
		const float CrateHeight = 200.f / 3.f;
		const FVector CrateScale(1.25f, 1.25f, CrateHeight / 68.f);
		Out.Nest = {
			Piece(Crate, FTransform(FRotator(0.f, 5.f, 0.f), FVector(0.f, 0.f, -200.f), CrateScale)),
			Piece(Crate, FTransform(FRotator(0.f, -12.f, 0.f), FVector(0.f, 0.f, -200.f + CrateHeight), CrateScale)),
			Piece(Crate, FTransform(FRotator(0.f, 9.f, 0.f), FVector(0.f, 0.f, -200.f + 2.f * CrateHeight), CrateScale)),
			Piece(Nest, FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, -6.f), FVector(0.38f))) };
		return bOk;
	}

	/** BP_Ship_*: the preset's hull as data, its mast (sail furled so it doesn't hide the deck) as hull-space dressing. */
	UBlueprint* MakeShip(const FShipSpec& Spec, const FDwDeckDressing& Common, TArray<FString>& Log, bool& bOk)
	{
		TArray<FDwMeshPiece> All;
		if (!ReadStaticMeshes(Pirate + TEXT("/Blueprints/Vehicles"), Spec.Preset, All, Log))
		{
			bOk = false;
			return nullptr;
		}
		UBlueprint* Blueprint = FindOrCreateBlueprint(Ships, Spec.Blueprint, ADwShip::StaticClass(), Log);
		ADwShip* Ship = Defaults<ADwShip>(Blueprint);
		if (!Ship)
		{
			bOk = false;
			return nullptr;
		}

		// The mast foot: the mast mesh itself (not its booms and sails).
		FVector Foot = FVector::ZeroVector;
		bool bHasMast = false;
		for (const FDwMeshPiece& Part : All)
		{
			const FString Name = Part.Mesh->GetName();
			if (Name.Contains(TEXT("_Mast_")) && !Name.Contains(TEXT("_Boom_")))
			{
				Foot = Part.Transform.GetLocation();
				bHasMast = true;
			}
		}
		if (!bHasMast)
		{
			Log.Add(FString::Printf(TEXT("ERROR %s has no mast"), Spec.Preset));
			bOk = false;
		}

		Ship->HullPieces.Reset();
		Ship->Dressing = Common;
		Ship->Dressing.Mast.Reset();
		for (FDwMeshPiece& Part : All)
		{
			Part.Socket = NAME_None;
			const FString Name = Part.Mesh->GetName();
			if (Name.Contains(TEXT("_Rigging_")))
			{
				continue; // the forestays run from the mast top to the bowsprit, straight through the combat camera's view
			}
			if (!Name.Contains(TEXT("_Mast_")))
			{
				Ship->HullPieces.Add(Part);
				continue;
			}
			// The square sail hangs between the camera and the deck: furl it. The fore-and-aft sail is edge-on; it stays.
			if (Name.Contains(TEXT("_Boom_01_Sail_Down_")))
			{
				const FString Furled = Name.Replace(TEXT("_Sail_Down_"), TEXT("_Sail_Tied_"));
				if (UStaticMesh* Tied = Require<UStaticMesh>(PirateMeshes + TEXT("/Vehicle"), Furled, Log, bOk))
				{
					Part.Mesh = Tied;
				}
			}
			Part.Transform.AddToTranslation(FVector(-Foot.X, -Foot.Y, 0.f));
			Ship->Dressing.Mast.Add(Part);
		}
		bOk &= Save(Blueprint, Log);

		// The hull's pivot relative to the deck centre: the waist deck's middle goes under the grid's middle.
		if (UDwDeckLayout* Layout = Require<UDwDeckLayout>(Boards, Spec.Layout, Log, bOk))
		{
			Layout->MeshAlignment = FTransform(FRotator::ZeroRotator, -Spec.DeckCentre * HullScale, FVector(HullScale));
			bOk &= Save(Layout, Log);
		}
		return Blueprint;
	}

	/** A bone's transform in the mesh's space at the start of an animation (the pose the pirate idles in). */
	bool BoneInMeshSpace(const UAnimSequence& Animation, FName Bone, FTransform& Out)
	{
		const USkeleton* Skeleton = Animation.GetSkeleton();
		if (!Skeleton)
		{
			return false;
		}
		const FReferenceSkeleton& Reference = Skeleton->GetReferenceSkeleton();
		int32 Index = Reference.FindBoneIndex(Bone);
		if (Index == INDEX_NONE)
		{
			return false;
		}
		Out = FTransform::Identity;
		for (; Index != INDEX_NONE; Index = Reference.GetParentIndex(Index))
		{
			FTransform Local;
			Animation.GetBoneTransform(Local, FSkeletonPoseBoneIndex(Index), 0.0, /*bUseRawData*/ true);
			Out *= Local;
		}
		return true;
	}

	/**
	 * The weapon relative to the hand bone so that, in the idle pose, it points straight ahead of the pirate and Pitch
	 * degrees down with its guard upright. Derived from the animation instead of eyeballed per character.
	 */
	FTransform MakeGrip(const UAnimSequence* Idle, float Pitch)
	{
		FTransform Hand;
		if (!Idle || !BoneInMeshSpace(*Idle, WeaponBone, Hand))
		{
			return FTransform::Identity;
		}
		// Mesh space: +Y is the pirate's forward, +Z up. Synty weapons point along their own +Y.
		const float Radians = FMath::DegreesToRadians(Pitch);
		const FVector Aim(0.f, FMath::Cos(Radians), -FMath::Sin(Radians));
		const FTransform Held(FRotationMatrix::MakeFromYZ(Aim, FVector::UpVector).ToQuat(), Hand.GetLocation());
		return Held.GetRelativeTransform(Hand);
	}

	/** BP_Unit_*: a Synty pirate (mesh and attachments read from its BP_Chr_*), a weapon, idle and run loops, side ring. */
	UBlueprint* MakeUnit(const FUnitSpec& Spec, UMaterialInterface* Ring, TArray<FString>& Log, bool& bOk)
	{
		const FString Characters = Pirate + TEXT("/Blueprints/Characters");
		const UBlueprint* Source = Require<UBlueprint>(Characters, Spec.SyntyCharacter, Log, bOk);
		const ACharacter* SourceDefaults = Source && Source->GeneratedClass ? Cast<ACharacter>(Source->GeneratedClass->GetDefaultObject()) : nullptr;
		USkeletalMesh* Mesh = SourceDefaults && SourceDefaults->GetMesh() ? SourceDefaults->GetMesh()->GetSkeletalMeshAsset() : nullptr;
		UStaticMesh* Weapon = Require<UStaticMesh>(PirateMeshes + TEXT("/Weapons"), Spec.Weapon, Log, bOk);
		UBlueprint* Blueprint = FindOrCreateBlueprint(Units, Spec.Blueprint, ADwCombatUnit::StaticClass(), Log);
		ADwCombatUnit* Unit = Defaults<ADwCombatUnit>(Blueprint);
		if (!Mesh || !Unit)
		{
			Log.Add(FString::Printf(TEXT("ERROR %s: no skeletal mesh in %s"), Spec.Blueprint, Spec.SyntyCharacter));
			bOk = false;
			return nullptr;
		}

		Unit->Character->SetSkeletalMeshAsset(Mesh);
		Unit->Attachments.Reset();
		bOk &= ReadStaticMeshes(Characters, Spec.SyntyCharacter, Unit->Attachments, Log); // hats, hair, beards on their sockets

		// The only loops the Synty pack ships for its mannequin skeleton.
		const FString Animations = TEXT("/Game/Synty/EpicContent/Mannequin/Animations");
		UAnimSequence* Idle = Require<UAnimSequence>(Animations, TEXT("ThirdPersonIdle"), Log, bOk);
		Unit->IdleAnimation = Idle;
		Unit->MoveAnimation = Require<UAnimSequence>(Animations, TEXT("ThirdPersonRun"), Log, bOk);
		Unit->Attachments.Add(Piece(Weapon, MakeGrip(Idle, Spec.WeaponPitch), WeaponBone));
		Unit->RingMaterial = Ring;
		bOk &= Save(Blueprint, Log);
		return Blueprint;
	}

	UDwSeaMood* MakeMood(const TCHAR* Name, const TCHAR* Display, TArray<FString>& Log)
	{
		UDwSeaMood* Mood = FindOrCreate<UDwSeaMood>(MoodData, Name, Log);
		Mood->DisplayName = FText::FromString(Display);
		return Mood;
	}

	/** One ADwSeaEnvironment in L_Sea, replacing the loose sun / sky / sky light actors of the grey-box level. */
	bool PrepareSeaLevel(UDwSeaMood* DefaultMood, TArray<FString>& Log)
	{
		UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(SeaLevel);
		if (!World)
		{
			Log.Add(TEXT("ERROR could not load L_Sea"));
			return false;
		}
		bool bOk = true;
		int32 Removed = 0;
		for (UClass* Class : { ADirectionalLight::StaticClass(), ASkyLight::StaticClass(), ASkyAtmosphere::StaticClass() })
		{
			TArray<AActor*> Old;
			for (TActorIterator<AActor> It(World, Class); It; ++It)
			{
				Old.Add(*It);
			}
			for (AActor* Actor : Old)
			{
				World->EditorDestroyActor(Actor, /*bShouldModifyLevel*/ true);
				++Removed;
			}
		}

		ADwSeaEnvironment* Sea = nullptr;
		for (TActorIterator<ADwSeaEnvironment> It(World); It; ++It)
		{
			Sea = *It;
			break;
		}
		const bool bCreated = Sea == nullptr;
		if (!Sea)
		{
			Sea = World->SpawnActor<ADwSeaEnvironment>();
			Sea->SetActorLabel(TEXT("SeaEnvironment"));
		}

		// Water: Synty's 50 m plane, big enough to reach the horizon from the combat camera.
		Sea->Water->SetStaticMesh(Require<UStaticMesh>(Core + TEXT("/Models"), TEXT("SM_Env_Water_Plane_01"), Log, bOk));
		Sea->Water->SetRelativeScale3D(FVector(400.f, 400.f, 1.f));
		Sea->SkyDome->SetStaticMesh(Require<UStaticMesh>(Core + TEXT("/Models"), TEXT("SM_Env_Skydome_01"), Log, bOk));
		Sea->SkyDome->SetRelativeScale3D(FVector(250.f));
		Sea->SkyDome->SetRelativeLocation(FVector(0.f, 0.f, -20000.f));
		// Deck plane is Z = 0; the hulls float with their waterline (preset Z = 0) at the surface.
		Sea->WaterHeight = -ShipSpecs[0].DeckCentre.Z * HullScale;
		Sea->DefaultMood = DefaultMood;
		Sea->RerunConstructionScripts();

		if (!UEditorLoadingAndSavingUtils::SaveMap(World, SeaLevel))
		{
			Log.Add(TEXT("ERROR could not save L_Sea"));
			return false;
		}
		Log.Add(FString::Printf(TEXT("L_Sea: %s SeaEnvironment, removed %d loose light/sky actors"), bCreated ? TEXT("added") : TEXT("updated"), Removed));
		return bOk;
	}
}

TArray<FString> UDwArtContentLibrary::CreateArtContent()
{
	using namespace DwArtContent;
	TArray<FString> Log;
	bool bOk = true;
	auto Fail = [&Log]() { Log.Add(TEXT("CONTENT FAIL")); return Log; };

	UDwGameData* GameData = Load<UDwGameData>(Data, UDwGameData::AssetName.ToString());
	UDwEncounterDefinition* Encounter = Load<UDwEncounterDefinition>(EncounterData, TEXT("DA_Encounter_Placeholder"));
	if (!GameData || !Encounter)
	{
		Log.Add(TEXT("ERROR DA_GameData or DA_Encounter_Placeholder is missing (run the combat content first)"));
		return Fail();
	}

	// 1. Overlay materials. Tile: Fill inside, EdgeOpacity on a border. Ring: a soft-edged circle outline.
	UMaterial* TileMaterial = MakeOverlayMaterial(TEXT("M_TileOverlay"),
		TEXT("float2 D = abs(UV - 0.5) * 2.0;\nfloat Edge = step(1.0 - EdgeWidth, max(D.x, D.y));\nreturn lerp(Fill, EdgeOpacity, Edge);"), 0.07f, Log);
	UMaterial* RingMaterial = MakeOverlayMaterial(TEXT("M_UnitRing"),
		TEXT("float R = length(UV - 0.5) * 2.0;\nfloat Outer = 1.0 - smoothstep(0.94, 1.0, R);\nfloat Ring = smoothstep(1.0 - EdgeWidth - 0.05, 1.0 - EdgeWidth, R) * Outer;\nreturn max(Ring * EdgeOpacity, Outer * Fill);"), 0.2f, Log);
	bOk &= Save(TileMaterial, Log) && Save(RingMaterial, Log);

	// 2. BP_Board: translucent highlights (alpha = fill), a faint grid on every standable tile, no grey-box decks.
	UBlueprint* BoardBp = FindOrCreateBlueprint(Board, TEXT("BP_Board"), ADwBoard::StaticClass(), Log);
	if (ADwBoard* BoardDefaults = Defaults<ADwBoard>(BoardBp))
	{
		BoardDefaults->HighlightBaseMaterial = TileMaterial;
		BoardDefaults->bDrawPlaceholderDecks = false;
		BoardDefaults->bDrawGrid = true;
		BoardDefaults->LayerColors = {
			{ EDwHighlightLayer::Move, FLinearColor(0.1f, 0.45f, 1.f, 0.3f) },
			{ EDwHighlightLayer::Path, FLinearColor(1.f, 0.85f, 0.15f, 0.5f) },
			{ EDwHighlightLayer::Target, FLinearColor(1.f, 0.1f, 0.6f, 0.35f) },
			{ EDwHighlightLayer::AreaOfEffect, FLinearColor(1.f, 0.45f, 0.05f, 0.35f) },
			{ EDwHighlightLayer::Danger, FLinearColor(0.7f, 0.05f, 0.35f, 0.35f) },
			{ EDwHighlightLayer::Hover, FLinearColor(1.f, 1.f, 1.f, 0.2f) },
			{ EDwHighlightLayer::Preview, FLinearColor(0.3f, 0.8f, 0.3f, 0.3f) } };
		bOk &= Save(BoardBp, Log);
	}
	else
	{
		return Fail();
	}

	// 3. Ships and the alignment of their deck layouts.
	FDwDeckDressing Dressing;
	bOk &= MakeDressing(Dressing, Log);
	UBlueprint* PlayerShip = MakeShip(ShipSpecs[0], Dressing, Log, bOk);
	UBlueprint* EnemyShip = MakeShip(ShipSpecs[1], Dressing, Log, bOk);
	if (!PlayerShip || !EnemyShip)
	{
		return Fail();
	}

	// 4. Pirates.
	TMap<FString, UBlueprint*> UnitBps;
	for (const FUnitSpec& Spec : UnitSpecs)
	{
		UBlueprint* Unit = MakeUnit(Spec, RingMaterial, Log, bOk);
		if (!Unit)
		{
			return Fail();
		}
		UnitBps.Add(Spec.Blueprint, Unit);
	}
	auto UnitClass = [&UnitBps](const TCHAR* Name) { return TSoftClassPtr<ADwCombatUnit>(UnitBps[Name]->GeneratedClass.Get()); };
	auto SetUnitClass = [&](const TCHAR* Definition, const TCHAR* Blueprint)
	{
		if (UDwUnitDefinition* Unit = Require<UDwUnitDefinition>(UnitData, Definition, Log, bOk))
		{
			Unit->UnitClass = UnitClass(Blueprint);
			bOk &= Save(Unit, Log);
		}
	};
	SetUnitClass(TEXT("DA_Unit_Captain_TeamLeader"), TEXT("BP_Unit_Captain"));
	SetUnitClass(TEXT("DA_Unit_Swordsman"), TEXT("BP_Unit_Swordsman"));
	SetUnitClass(TEXT("DA_Unit_Sniper"), TEXT("BP_Unit_Sniper"));

	// 5. The placeholder raiders: their own sloop, and different pirates from your crew's.
	Encounter->ShipClass = TSoftClassPtr<ADwShip>(EnemyShip->GeneratedClass.Get());
	for (FDwEnemySlot& Slot : Encounter->Units)
	{
		const bool bSniper = Slot.UnitDefinition && Slot.UnitDefinition->GetName() == TEXT("DA_Unit_Sniper");
		Slot.UnitClassOverride = UnitClass(bSniper ? TEXT("BP_Unit_Raider_Sniper") : TEXT("BP_Unit_Raider_Swordsman"));
	}
	bOk &= Save(Encounter, Log);

	// 6. Sea moods. Day is the default look; the storm rolls now and then (weights below) or with -DwMood=Storm.
	const FString Env = Pirate + TEXT("/Materials/Env");
	UDwSeaMood* Day = MakeMood(TEXT("DA_SeaMood_Day"), TEXT("Day"), Log);
	Day->SkyMaterial = Require<UMaterialInterface>(Env, TEXT("MI_Skybox_Day_01"), Log, bOk);
	Day->WaterMaterial = Require<UMaterialInterface>(TEXT("/Game/Synty/PolygonGeneric/Materials/Base"), TEXT("MI_Generic_Water"), Log, bOk);
	Day->SunRotation = FRotator(-52.f, -125.f, 0.f);
	Day->SunIntensity = 7.f;
	Day->SunColor = FLinearColor(1.f, 0.96f, 0.88f);
	Day->SkyLightIntensity = 1.1f;
	Day->SkyLightColor = FLinearColor::White;
	Day->ExposureBias = 0.f;
	Day->FogDensity = 0.004f;
	Day->FogColor = FLinearColor(0.5f, 0.68f, 0.9f);
	Day->Weather = nullptr;
	bOk &= Save(Day, Log);

	UDwSeaMood* Storm = MakeMood(TEXT("DA_SeaMood_Storm"), TEXT("Storm"), Log);
	Storm->SkyMaterial = Require<UMaterialInterface>(Env, TEXT("MI_Skybox_Storm_01"), Log, bOk);
	// Synty's storm ocean materials blend in terrain through the vertex colours of its own ocean mesh; on a plain water
	// plane they render as grass. The storm keeps the generic water and gets its look from light, fog and rain.
	Storm->WaterMaterial = Day->WaterMaterial;
	Storm->SunRotation = FRotator(-38.f, -140.f, 0.f);
	Storm->SunIntensity = 2.2f;
	Storm->SunColor = FLinearColor(0.72f, 0.8f, 0.95f);
	Storm->SkyLightIntensity = 0.8f;
	Storm->SkyLightColor = FLinearColor(0.7f, 0.78f, 0.9f);
	Storm->ExposureBias = -1.1f; // auto exposure would otherwise brighten the dark scene and blow out the tile highlights
	Storm->FogDensity = 0.02f;
	Storm->FogColor = FLinearColor(0.2f, 0.26f, 0.33f);
	Storm->Weather = Require<UNiagaraSystem>(Pirate + TEXT("/FX"), TEXT("NS_FX_Rain_Heavy_01"), Log, bOk);
	bOk &= Save(Storm, Log);

	// 7. DA_GameData slots.
	GameData->BoardClass = TSoftClassPtr<ADwBoard>(BoardBp->GeneratedClass.Get());
	GameData->DeckGap = DeckGap;
	GameData->PlayerShipClass = TSoftClassPtr<ADwShip>(PlayerShip->GeneratedClass.Get());
	GameData->SeaMoods.Reset();
	GameData->SeaMoods.Add({ Day, 4.f });
	GameData->SeaMoods.Add({ Storm, 1.f });
	bOk &= Save(GameData, Log);

	// 8. The level.
	bOk &= PrepareSeaLevel(Day, Log);

	Log.Add(bOk ? TEXT("CONTENT PASS") : TEXT("CONTENT FAIL"));
	return Log;
}
