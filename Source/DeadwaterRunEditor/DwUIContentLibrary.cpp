// Copyright Brandon Hillig 2026

#include "DwUIContentLibrary.h"

#include "Animation/WidgetAnimation.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "CommonInputBaseTypes.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Core/DwGameData.h"
#include "EditorAssetLibrary.h"
#include "Engine/DataTable.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UI/DwCombatHudWidgets.h"
#include "UI/DwFloatingText.h"
#include "UI/DwPrimaryLayoutWidget.h"
#include "UI/DwUIScreenSet.h"
#include "UI/DwUISoundMap.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

namespace DwUIContent
{
	const FString WidgetFolder = TEXT("/Game/DeadwaterRun/UI");
	const FString UIInput = TEXT("/Game/DeadwaterRun/UI/Input");
	const FString UIData = TEXT("/Game/DeadwaterRun/UI/Data");
	const FString Data = TEXT("/Game/DeadwaterRun/Data");

	// Placeholder style (art direction U6 pending): plain, high-contrast, engine Roboto.
	const FLinearColor Panel(0.02f, 0.03f, 0.05f, 0.82f);
	const FLinearColor Gold(1.f, 0.82f, 0.3f);
	const FLinearColor Light(0.95f, 0.92f, 0.85f);
	const FLinearColor Dim(0.7f, 0.7f, 0.72f);
	const FLinearColor Bad(1.f, 0.45f, 0.35f);
	const FLinearColor Mana(0.6f, 0.78f, 1.f);
	const FLinearColor Clear(0.f, 0.f, 0.f, 0.f);

	template <typename T>
	T* Load(const FString& Folder, const FString& Name)
	{
		return LoadObject<T>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), *Folder, *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	template <typename T>
	T* FindOrCreate(const FString& Folder, const FString& Name, TArray<FString>& Log, bool* bOutCreated = nullptr)
	{
		if (T* Existing = Load<T>(Folder, Name))
		{
			if (bOutCreated) { *bOutCreated = false; }
			return Existing;
		}
		UPackage* Package = CreatePackage(*(Folder / Name));
		T* Asset = NewObject<T>(Package, *Name, RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Asset);
		Log.Add(FString::Printf(TEXT("Created %s/%s"), *Folder, *Name));
		if (bOutCreated) { *bOutCreated = true; }
		return Asset;
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

	UBlueprint* FindOrCreateBlueprint(const FString& Folder, const FString& Name, UClass* Parent, UClass* BlueprintClass,
		UClass* GeneratedClass, TArray<FString>& Log)
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
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(Parent, Package, *Name, BPTYPE_Normal, BlueprintClass, GeneratedClass, NAME_None);
		FAssetRegistryModule::AssetCreated(Blueprint);
		Log.Add(FString::Printf(TEXT("Created %s/%s (parent %s)"), *Folder, *Name, *Parent->GetName()));
		return Blueprint;
	}

	/**
	 * Builds one WBP's widget tree. Begin() moves the previous tree out of the way (so names can be reused and nothing
	 * stale is saved), Finish() registers widget variable guids, compiles and reports BindWidget problems.
	 */
	struct FTreeBuilder
	{
		UWidgetBlueprint* Blueprint = nullptr;
		UWidgetTree* Tree = nullptr;
		TArray<FString>* Log = nullptr;

		explicit FTreeBuilder(UWidgetBlueprint& InBlueprint, TArray<FString>& InLog) : Blueprint(&InBlueprint), Tree(InBlueprint.WidgetTree), Log(&InLog)
		{
			TArray<UWidget*> Old;
			Tree->GetAllWidgets(Old);
			for (UWidget* Widget : Old)
			{
				Widget->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
				Widget->MarkAsGarbage();
			}
			Tree->RootWidget = nullptr;
		}

		template <typename T>
		T* Make(const TCHAR* Name = nullptr)
		{
			T* Widget = Tree->ConstructWidget<T>(T::StaticClass(), Name ? FName(Name) : NAME_None);
			Widget->bIsVariable = Name != nullptr;
			return Widget;
		}

		UTextBlock* Text(const TCHAR* Name, const FString& Initial, int32 Size, const FLinearColor& Color, bool bBold = true)
		{
			UTextBlock* Block = Make<UTextBlock>(Name);
			Block->SetText(FText::FromString(Initial));
			FSlateFontInfo Font = Block->GetFont();
			Font.Size = Size;
			Font.TypefaceFontName = bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular"));
			Block->SetFont(Font);
			Block->SetColorAndOpacity(FSlateColor(Color));
			Block->SetShadowOffset(FVector2D(1.f, 1.f));
			Block->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f));
			Block->SetVisibility(ESlateVisibility::HitTestInvisible);
			return Block;
		}

		/** A progress bar of fixed height with its "12 / 30" label centred on it. */
		UOverlay* Bar(const TCHAR* BarName, const TCHAR* TextName, float Height, int32 FontSize, float Width = 0.f)
		{
			UOverlay* Overlay = Make<UOverlay>();
			USizeBox* Box = Make<USizeBox>();
			Box->SetHeightOverride(Height);
			if (Width > 0.f)
			{
				Box->SetWidthOverride(Width);
			}
			UProgressBar* Progress = Make<UProgressBar>(BarName);
			Progress->SetPercent(0.6f);
			Box->SetContent(Progress);
			UOverlaySlot* BarSlot = Overlay->AddChildToOverlay(Box);
			BarSlot->SetHorizontalAlignment(HAlign_Fill);
			BarSlot->SetVerticalAlignment(VAlign_Fill);
			UOverlaySlot* Label = Overlay->AddChildToOverlay(Text(TextName, TEXT("60 / 100"), FontSize, Light));
			Label->SetHorizontalAlignment(HAlign_Center);
			Label->SetVerticalAlignment(VAlign_Center);
			Overlay->SetVisibility(ESlateVisibility::HitTestInvisible);
			return Overlay;
		}

		bool Finish(UWidget* Root)
		{
			Tree->RootWidget = Root;
			TArray<UWidget*> Widgets;
			Tree->GetAllWidgets(Widgets);
			TSet<FName> Names;
			for (UWidget* Widget : Widgets)
			{
				Names.Add(Widget->GetFName());
				if (!Blueprint->WidgetVariableNameToGuidMap.Contains(Widget->GetFName()))
				{
					Blueprint->OnVariableAdded(Widget->GetFName());
				}
			}
			TArray<FName> Stale;
			for (const TPair<FName, FGuid>& Entry : Blueprint->WidgetVariableNameToGuidMap)
			{
				const bool bAnimation = Blueprint->Animations.ContainsByPredicate([&Entry](const UWidgetAnimation* Animation)
					{ return Animation && Animation->GetFName() == Entry.Key; });
				if (!Names.Contains(Entry.Key) && !bAnimation)
				{
					Stale.Add(Entry.Key);
				}
			}
			for (const FName& Name : Stale)
			{
				Blueprint->OnVariableRemoved(Name);
			}
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
			FKismetEditorUtilities::CompileBlueprint(Blueprint);
			if (Blueprint->Status == BS_Error)
			{
				Log->Add(FString::Printf(TEXT("ERROR %s failed to compile (missing or mistyped BindWidget?)"), *Blueprint->GetName()));
				return false;
			}
			return true;
		}
	};

	UCanvasPanelSlot* Place(UCanvasPanel& Canvas, UWidget* Widget, const FVector2D& Anchor, const FVector2D& Position, int32 ZOrder = 0)
	{
		UCanvasPanelSlot* Slot = Canvas.AddChildToCanvas(Widget);
		Slot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
		Slot->SetAlignment(Anchor);
		Slot->SetPosition(Position);
		Slot->SetAutoSize(true);
		Slot->SetZOrder(ZOrder);
		return Slot;
	}

	UVerticalBoxSlot* AddRow(UVerticalBox& Box, UWidget* Widget, const FMargin& Padding = FMargin(0.f), EHorizontalAlignment Align = HAlign_Fill)
	{
		UVerticalBoxSlot* Slot = Box.AddChildToVerticalBox(Widget);
		Slot->SetPadding(Padding);
		Slot->SetHorizontalAlignment(Align);
		return Slot;
	}

	UHorizontalBoxSlot* AddColumn(UHorizontalBox& Box, UWidget* Widget, const FMargin& Padding = FMargin(0.f), bool bFill = false,
		EVerticalAlignment Align = VAlign_Center)
	{
		UHorizontalBoxSlot* Slot = Box.AddChildToHorizontalBox(Widget);
		Slot->SetPadding(Padding);
		Slot->SetVerticalAlignment(Align);
		if (bFill)
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		return Slot;
	}

	// -----------------------------------------------------------------------------------------------------------------
	// Layouts

	bool BuildPrimaryLayout(UWidgetBlueprint& Blueprint, TArray<FString>& Log)
	{
		FTreeBuilder B(Blueprint, Log);
		UOverlay* Root = B.Make<UOverlay>(TEXT("Root"));
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		for (const TCHAR* Name : { TEXT("GameLayer"), TEXT("GameMenuLayer"), TEXT("MenuLayer"), TEXT("ModalLayer") }) // bottom to top
		{
			UCommonActivatableWidgetStack* Stack = B.Make<UCommonActivatableWidgetStack>(Name);
			Stack->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			UOverlaySlot* Slot = Root->AddChildToOverlay(Stack);
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}
		return B.Finish(Root);
	}

	bool BuildUnitCard(UWidgetBlueprint& Blueprint, TArray<FString>& Log)
	{
		FTreeBuilder B(Blueprint, Log);
		USizeBox* Root = B.Make<USizeBox>(TEXT("Root"));
		Root->SetWidthOverride(210.f);
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		UButton* Button = B.Make<UButton>(TEXT("CardButton"));
		Button->SetBackgroundColor(Clear); // the frame below draws the card; the button only takes clicks
		Root->SetContent(Button);
		// Frame = a 3 px outline (selected / hovered / acting colour), Body = the dark card behind the text.
		UBorder* Frame = B.Make<UBorder>(TEXT("Frame"));
		Frame->SetBrushColor(Panel);
		Frame->SetPadding(FMargin(3.f));
		Frame->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UButtonSlot* Slot = Cast<UButtonSlot>(Button->SetContent(Frame)))
		{
			Slot->SetPadding(FMargin(0.f));
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}
		UBorder* Body = B.Make<UBorder>();
		Body->SetBrushColor(Panel);
		Body->SetPadding(FMargin(8.f, 5.f));
		Frame->SetContent(Body);

		UVerticalBox* Rows = B.Make<UVerticalBox>();
		Body->SetContent(Rows);

		UTextBlock* Name = B.Text(TEXT("NameText"), TEXT("Swordsman"), 20, Light);
		Name->SetClipping(EWidgetClipping::ClipToBounds);
		Name->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis); // long names never push the card wider
		AddRow(*Rows, Name, FMargin(0.f, 0.f, 0.f, 4.f));

		AddRow(*Rows, B.Bar(TEXT("HealthBar"), TEXT("HealthText"), 22.f, 16), FMargin(0.f, 0.f, 0.f, 3.f));
		AddRow(*Rows, B.Bar(TEXT("ManaBar"), TEXT("ManaText"), 18.f, 14), FMargin(0.f, 0.f, 0.f, 4.f));

		UHorizontalBox* Footer = B.Make<UHorizontalBox>();
		AddColumn(*Footer, B.Text(TEXT("BudgetText"), TEXT("Move  Action"), 15, Dim, /*bBold*/ false), FMargin(0.f, 0.f, 8.f, 0.f), /*bFill*/ true);
		UHorizontalBox* FearRow = B.Make<UHorizontalBox>(TEXT("FearRow"));
		AddColumn(*FearRow, B.Text(nullptr, TEXT("Fear"), 15, Dim, false), FMargin(0.f, 0.f, 6.f, 0.f));
		AddColumn(*FearRow, B.Make<UHorizontalBox>(TEXT("FearPips")));
		AddColumn(*Footer, FearRow);
		AddRow(*Rows, Footer);
		// Captain / Acting / Down on its own line (collapsed when empty), so it never crowds the footer.
		AddRow(*Rows, B.Text(TEXT("RoleText"), TEXT("Captain"), 15, Gold), FMargin(0.f, 2.f, 0.f, 0.f));

		UWrapBox* Statuses = B.Make<UWrapBox>(TEXT("StatusList"));
		Statuses->SetInnerSlotPadding(FVector2D(4.f, 4.f));
		Statuses->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddRow(*Rows, Statuses, FMargin(0.f, 4.f, 0.f, 0.f));
		return B.Finish(Root);
	}

	bool BuildActionButton(UWidgetBlueprint& Blueprint, TArray<FString>& Log)
	{
		FTreeBuilder B(Blueprint, Log);
		USizeBox* Root = B.Make<USizeBox>(TEXT("Root"));
		Root->SetWidthOverride(180.f);
		Root->SetMinDesiredHeight(92.f);
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		UButton* Button = B.Make<UButton>(TEXT("ActionButton"));
		Root->SetContent(Button);
		UVerticalBox* Rows = B.Make<UVerticalBox>();
		if (UButtonSlot* Slot = Cast<UButtonSlot>(Button->SetContent(Rows)))
		{
			Slot->SetPadding(FMargin(8.f, 6.f));
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Top);
		}
		UHorizontalBox* Header = B.Make<UHorizontalBox>();
		AddColumn(*Header, B.Text(TEXT("HotkeyText"), TEXT("1"), 16, Gold), FMargin(0.f), /*bFill*/ true);
		AddColumn(*Header, B.Text(TEXT("CostText"), TEXT("2 MP"), 16, Mana));
		AddRow(*Rows, Header);
		UTextBlock* Name = B.Text(TEXT("NameText"), TEXT("Cutlass"), 19, Light);
		AddRow(*Rows, Name, FMargin(0.f, 2.f, 0.f, 2.f));
		UTextBlock* Reason = B.Text(TEXT("ReasonText"), TEXT("No target in reach"), 14, Bad, /*bBold*/ false);
		Reason->SetAutoWrapText(true);
		AddRow(*Rows, Reason);
		return B.Finish(Root);
	}

	bool BuildFloatingText(UWidgetBlueprint& Blueprint, TArray<FString>& Log)
	{
		FTreeBuilder B(Blueprint, Log);
		UTextBlock* Value = B.Text(TEXT("ValueText"), TEXT("-25"), 30, Light);
		Value->SetJustification(ETextJustify::Center);
		Value->SetShadowOffset(FVector2D(2.f, 2.f));
		return B.Finish(Value);
	}

	/** "YOUR SHIP" over a 360 px bar with "250 / 250" on it. */
	UVerticalBox* ShipPanel(FTreeBuilder& B, const TCHAR* PanelName, const TCHAR* Title, const TCHAR* BarName, const TCHAR* TextName,
		const FLinearColor& TitleColor, EHorizontalAlignment Align)
	{
		UVerticalBox* Box = B.Make<UVerticalBox>(PanelName);
		Box->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		AddRow(*Box, B.Text(nullptr, Title, 18, TitleColor), FMargin(0.f, 0.f, 0.f, 4.f), Align);
		AddRow(*Box, B.Bar(BarName, TextName, 28.f, 18, 360.f), FMargin(0.f), Align);
		return Box;
	}

	bool BuildCombatHud(UWidgetBlueprint& Blueprint, TArray<FString>& Log)
	{
		FTreeBuilder B(Blueprint, Log);
		UCanvasPanel* Root = B.Make<UCanvasPanel>(TEXT("Root"));
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible); // empty space clicks through to the board

		// Top: ships left and right, round and side in the middle.
		Place(*Root, ShipPanel(B, TEXT("PlayerShipPanel"), TEXT("YOUR SHIP"), TEXT("PlayerShipBar"), TEXT("PlayerShipText"),
			FLinearColor(0.45f, 0.7f, 1.f), HAlign_Left), FVector2D(0.f, 0.f), FVector2D(24.f, 16.f));
		Place(*Root, ShipPanel(B, TEXT("EnemyShipPanel"), TEXT("ENEMY SHIP"), TEXT("EnemyShipBar"), TEXT("EnemyShipText"),
			FLinearColor(1.f, 0.5f, 0.42f), HAlign_Right), FVector2D(1.f, 0.f), FVector2D(-24.f, 16.f));
		Place(*Root, B.Text(TEXT("TurnText"), TEXT("Round 1  -  YOUR TURN"), 26, Gold), FVector2D(0.5f, 0.f), FVector2D(0.f, 20.f));

		// Crew cards in a row under our ship, enemy bars under theirs: the board below stays uncovered
		// (placeholder placement, open question for Brandon). Filled at runtime from UnitCardClass.
		UHorizontalBox* Crew = B.Make<UHorizontalBox>(TEXT("CrewList"));
		Crew->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Place(*Root, Crew, FVector2D(0.f, 0.f), FVector2D(24.f, 92.f));
		UHorizontalBox* Enemies = B.Make<UHorizontalBox>(TEXT("EnemyList"));
		Enemies->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Place(*Root, Enemies, FVector2D(1.f, 0.f), FVector2D(-24.f, 92.f));

		// Bottom: hint / preview / message above the action bar and End Turn.
		UVerticalBox* Bottom = B.Make<UVerticalBox>(TEXT("BottomPanel"));
		Bottom->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		AddRow(*Bottom, B.Text(TEXT("HintText"), TEXT("Click one of your pirates."), 20, Gold), FMargin(0.f, 0.f, 0.f, 4.f), HAlign_Center);
		UVerticalBox* Preview = B.Make<UVerticalBox>(TEXT("PreviewList"));
		Preview->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddRow(*Bottom, Preview, FMargin(0.f, 0.f, 0.f, 4.f), HAlign_Center);
		AddRow(*Bottom, B.Text(TEXT("MessageText"), TEXT("Not a valid target"), 20, Bad), FMargin(0.f, 0.f, 0.f, 8.f), HAlign_Center);

		UHorizontalBox* Controls = B.Make<UHorizontalBox>();
		Controls->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UHorizontalBox* Actions = B.Make<UHorizontalBox>(TEXT("ActionBar"));
		Actions->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		AddColumn(*Controls, Actions, FMargin(0.f, 0.f, 24.f, 0.f), false, VAlign_Bottom);

		UVerticalBox* EndTurnColumn = B.Make<UVerticalBox>();
		EndTurnColumn->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UBorder* Prompt = B.Make<UBorder>(TEXT("EndTurnPrompt"));
		Prompt->SetBrushColor(Panel);
		Prompt->SetPadding(FMargin(10.f, 4.f));
		Prompt->SetContent(B.Text(nullptr, TEXT("End turn?"), 18, Gold));
		Prompt->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddRow(*EndTurnColumn, Prompt, FMargin(0.f, 0.f, 0.f, 6.f), HAlign_Center);
		USizeBox* EndTurnBox = B.Make<USizeBox>();
		EndTurnBox->SetWidthOverride(170.f);
		EndTurnBox->SetHeightOverride(92.f);
		UButton* EndTurn = B.Make<UButton>(TEXT("EndTurnButton"));
		EndTurnBox->SetContent(EndTurn);
		UVerticalBox* EndTurnLabels = B.Make<UVerticalBox>();
		if (UButtonSlot* Slot = Cast<UButtonSlot>(EndTurn->SetContent(EndTurnLabels)))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		AddRow(*EndTurnLabels, B.Text(TEXT("EndTurnText"), TEXT("END TURN"), 20, Light), FMargin(0.f), HAlign_Center);
		AddRow(*EndTurnLabels, B.Text(TEXT("EndTurnKeyText"), TEXT("Space"), 15, Gold, false), FMargin(0.f, 2.f, 0.f, 0.f), HAlign_Center);
		AddRow(*EndTurnColumn, EndTurnBox, FMargin(0.f), HAlign_Center);
		AddColumn(*Controls, EndTurnColumn, FMargin(0.f), false, VAlign_Bottom);
		AddRow(*Bottom, Controls, FMargin(0.f), HAlign_Center);
		Place(*Root, Bottom, FVector2D(0.5f, 1.f), FVector2D(0.f, -40.f));

		UTextBlock* Help = B.Text(TEXT("ControlsText"), TEXT("LMB select / move / attack"), 14, Dim, false);
		Place(*Root, Help, FVector2D(0.5f, 1.f), FVector2D(0.f, -12.f));

		// Banners in front of everything (never take clicks).
		UBorder* Banner = B.Make<UBorder>(TEXT("TurnBanner"));
		Banner->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.72f));
		Banner->SetPadding(FMargin(36.f, 12.f));
		Banner->SetVisibility(ESlateVisibility::HitTestInvisible);
		Banner->SetContent(B.Text(TEXT("TurnBannerText"), TEXT("Round 1 - Your turn"), 40, Gold));
		Place(*Root, Banner, FVector2D(0.5f, 0.28f), FVector2D::ZeroVector, 10);

		UBorder* Outcome = B.Make<UBorder>(TEXT("OutcomeBanner"));
		Outcome->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.78f));
		Outcome->SetPadding(FMargin(40.f, 16.f));
		Outcome->SetVisibility(ESlateVisibility::HitTestInvisible);
		Outcome->SetContent(B.Text(TEXT("OutcomeText"), TEXT("VICTORY"), 36, Gold));
		Place(*Root, Outcome, FVector2D(0.5f, 0.42f), FVector2D::ZeroVector, 11);

		return B.Finish(Root);
	}
}

TArray<FString> UDwUIContentLibrary::CreateUIContent()
{
	using namespace DwUIContent;
	TArray<FString> OutLog;
	bool bOk = true;
	auto Fail = [&OutLog]() { OutLog.Add(TEXT("CONTENT FAIL")); return OutLog; };

	UDwGameData* GameData = Load<UDwGameData>(Data, UDwGameData::AssetName.ToString());
	if (!GameData)
	{
		OutLog.Add(TEXT("ERROR DA_GameData is missing"));
		return Fail();
	}

	// 1. CommonUI input data (Config/DefaultGame.ini CommonInputSettings.InputData points at B_UIInputData).
	UScriptStruct* ActionRow = FindObject<UScriptStruct>(nullptr, TEXT("/Script/CommonUI.CommonInputActionDataBase"));
	UDataTable* Actions = FindOrCreate<UDataTable>(UIInput, TEXT("DT_UIInputActions"), OutLog);
	if (!ActionRow || !Actions)
	{
		OutLog.Add(TEXT("ERROR CommonInputActionDataBase not found (CommonUI plugin disabled?)"));
		return Fail();
	}
	Actions->EmptyTable();
	Actions->RowStruct = ActionRow;
	// The row struct is not exported by CommonUI, so rows are filled through reflection (DisplayName, keyboard key).
	const FTextProperty* DisplayName = FindFProperty<FTextProperty>(ActionRow, TEXT("DisplayName"));
	const FStructProperty* Keyboard = FindFProperty<FStructProperty>(ActionRow, TEXT("KeyboardInputTypeInfo"));
	const FStructProperty* KeyProperty = Keyboard ? FindFProperty<FStructProperty>(Keyboard->Struct, TEXT("Key")) : nullptr;
	if (!DisplayName || !KeyProperty)
	{
		OutLog.Add(TEXT("ERROR CommonInputActionDataBase layout changed (DisplayName / KeyboardInputTypeInfo.Key)"));
		return Fail();
	}
	auto AddAction = [&](const TCHAR* Row, const TCHAR* Display, const FKey& Key)
	{
		FStructOnScope Value(ActionRow);
		uint8* Memory = Value.GetStructMemory();
		DisplayName->SetPropertyValue_InContainer(Memory, FText::FromString(Display));
		*KeyProperty->ContainerPtrToValuePtr<FKey>(Keyboard->ContainerPtrToValuePtr<void>(Memory)) = Key;
		Actions->AddRow(Row, *reinterpret_cast<const FTableRowBase*>(Memory));
	};
	AddAction(TEXT("Confirm"), TEXT("Confirm"), EKeys::Enter);
	AddAction(TEXT("Back"), TEXT("Back"), EKeys::Escape);
	bOk &= Save(Actions, OutLog);

	UBlueprint* InputDataBp = FindOrCreateBlueprint(UIInput, TEXT("B_UIInputData"), UCommonUIInputData::StaticClass(),
		UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), OutLog);
	if (!InputDataBp)
	{
		return Fail();
	}
	FKismetEditorUtilities::CompileBlueprint(InputDataBp);
	if (UCommonUIInputData* InputData = InputDataBp->GeneratedClass ? InputDataBp->GeneratedClass->GetDefaultObject<UCommonUIInputData>() : nullptr)
	{
		InputData->DefaultClickAction.DataTable = Actions;
		InputData->DefaultClickAction.RowName = TEXT("Confirm");
		InputData->DefaultBackAction.DataTable = Actions;
		InputData->DefaultBackAction.RowName = TEXT("Back");
	}
	bOk &= Save(InputDataBp, OutLog);

	// 2. Widget blueprints (layout from code, structure and logic in the C++ parents).
	struct FWidgetSpec
	{
		const TCHAR* Name;
		UClass* Parent;
		bool (*Build)(UWidgetBlueprint&, TArray<FString>&);
	};
	const FWidgetSpec Specs[] = {
		{ TEXT("WBP_UnitCard"), UDwUnitCardWidget::StaticClass(), &BuildUnitCard },
		{ TEXT("WBP_ActionButton"), UDwActionButtonWidget::StaticClass(), &BuildActionButton },
		{ TEXT("WBP_FloatingText"), UDwFloatingTextWidget::StaticClass(), &BuildFloatingText },
		{ TEXT("WBP_PrimaryLayout"), UDwPrimaryLayoutWidget::StaticClass(), &BuildPrimaryLayout },
		{ TEXT("WBP_CombatHud"), UDwCombatHudScreen::StaticClass(), &BuildCombatHud } };
	TMap<FString, UWidgetBlueprint*> Built;
	for (const FWidgetSpec& Spec : Specs)
	{
		UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(FindOrCreateBlueprint(WidgetFolder, Spec.Name, Spec.Parent,
			UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass(), OutLog));
		if (!Blueprint || !Spec.Build(*Blueprint, OutLog))
		{
			return Fail();
		}
		Built.Add(Spec.Name, Blueprint);
	}
	auto ClassOf = [&Built](const TCHAR* Name) { return Built[Name]->GeneratedClass.Get(); };

	// Defaults the C++ parents read. Root user widgets must not take hits themselves, or they would swallow board clicks.
	for (const TPair<FString, UWidgetBlueprint*>& Entry : Built)
	{
		if (UUserWidget* Defaults = Entry.Value->GeneratedClass->GetDefaultObject<UUserWidget>())
		{
			Defaults->SetVisibility(Entry.Key == TEXT("WBP_FloatingText") ? ESlateVisibility::HitTestInvisible : ESlateVisibility::SelfHitTestInvisible);
		}
	}
	if (UDwCombatHudScreen* Hud = Built[TEXT("WBP_CombatHud")]->GeneratedClass->GetDefaultObject<UDwCombatHudScreen>())
	{
		Hud->UnitCardClass = ClassOf(TEXT("WBP_UnitCard"));
		Hud->ActionButtonClass = ClassOf(TEXT("WBP_ActionButton"));
	}
	for (const TPair<FString, UWidgetBlueprint*>& Entry : Built)
	{
		bOk &= Save(Entry.Value, OutLog);
		OutLog.Add(FString::Printf(TEXT("Built %s/%s"), *WidgetFolder, *Entry.Key));
	}

	// 3. DA_UIScreenSet and DA_UISoundMap (the sound map is created empty once; its contents belong to DW-402).
	UDwUIScreenSet* Screens = FindOrCreate<UDwUIScreenSet>(UIData, TEXT("DA_UIScreenSet"), OutLog);
	Screens->PrimaryLayout = TSoftClassPtr<UDwPrimaryLayoutWidget>(ClassOf(TEXT("WBP_PrimaryLayout")));
	Screens->CombatHud = TSoftClassPtr<UDwActivatableScreen>(ClassOf(TEXT("WBP_CombatHud")));
	Screens->FloatingText = TSoftClassPtr<UDwFloatingTextWidget>(ClassOf(TEXT("WBP_FloatingText")));
	bool bSoundMapCreated = false;
	UDwUISoundMap* SoundMap = FindOrCreate<UDwUISoundMap>(UIData, TEXT("DA_UISoundMap"), OutLog, &bSoundMapCreated);
	if (bSoundMapCreated)
	{
		bOk &= Save(SoundMap, OutLog);
	}
	Screens->SoundMap = SoundMap;
	bOk &= Save(Screens, OutLog);

	// 4. DA_GameData slot.
	GameData->UIScreenSet = Screens;
	bOk &= Save(GameData, OutLog);

	OutLog.Add(bOk ? TEXT("CONTENT PASS") : TEXT("CONTENT FAIL"));
	return OutLog;
}
