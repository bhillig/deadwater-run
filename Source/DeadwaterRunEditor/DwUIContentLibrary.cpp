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
#include "AssetToolsModule.h"
#include "AssetImportTask.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Engine/Texture2D.h"
#include "Components/Image.h"
#include "Styling/SlateTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UI/DwCombatHudWidgets.h"
#include "UI/DwFloatingText.h"
#include "UI/DwPrimaryLayoutWidget.h"
#include "UI/DwRunScreens.h"
#include "UI/DwShellScreens.h"
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

	/**
	 * Art direction "Ship's Log" (Brandon, 2026-11-20; mockups in Saved/UIDesign, direction B): parchment, ink, brass and
	 * red wax, fonts Grenze (headings) and Texturina (body) from the Synty UI pack. Combat takes direction C's slim
	 * steel top bar, rosters and End Turn ("Iron Tide"). Every colour and brush lives here, nowhere else.
	 */
	namespace Ink
	{
		const FLinearColor Paper = FLinearColor::FromSRGBColor(FColor(236, 220, 180));
		const FLinearColor PaperDark = FLinearColor::FromSRGBColor(FColor(214, 192, 140));
		const FLinearColor Text = FLinearColor::FromSRGBColor(FColor(58, 38, 20));
		const FLinearColor Dim = FLinearColor::FromSRGBColor(FColor(107, 74, 42));
		const FLinearColor Red = FLinearColor::FromSRGBColor(FColor(154, 43, 29));
		const FLinearColor Blue = FLinearColor::FromSRGBColor(FColor(47, 93, 147));
		const FLinearColor Hp = FLinearColor::FromSRGBColor(FColor(168, 50, 42));
		const FLinearColor Mp = FLinearColor::FromSRGBColor(FColor(47, 93, 147));
		const FLinearColor Track = FLinearColor(0.23f, 0.15f, 0.08f, 0.18f);
		const FLinearColor Seal = FLinearColor::FromSRGBColor(FColor(160, 44, 28));
	}
	namespace Steel
	{
		const FLinearColor Panel = FLinearColor(0.012f, 0.022f, 0.032f, 0.9f);
		const FLinearColor Line = FLinearColor::FromSRGBColor(FColor(74, 93, 109));
		const FLinearColor Text = FLinearColor::FromSRGBColor(FColor(230, 237, 242));
		const FLinearColor Dim = FLinearColor::FromSRGBColor(FColor(147, 164, 178));
		const FLinearColor Sea = FLinearColor::FromSRGBColor(FColor(79, 214, 196));
		const FLinearColor SeaLight = FLinearColor::FromSRGBColor(FColor(159, 240, 228));
		const FLinearColor SeaInk = FLinearColor::FromSRGBColor(FColor(6, 34, 30));
		const FLinearColor Red = FLinearColor::FromSRGBColor(FColor(224, 73, 59));
		const FLinearColor RedText = FLinearColor::FromSRGBColor(FColor(255, 179, 168));
		const FLinearColor Mp = FLinearColor::FromSRGBColor(FColor(62, 143, 240));
		const FLinearColor Track = FLinearColor(1.f, 1.f, 1.f, 0.09f);
		const FLinearColor Gold = FLinearColor::FromSRGBColor(FColor(233, 183, 79));
	}

	// Fonts made from the pack's font faces (CreateUIFonts); set before any widget is built.
	UFont* HeadingFont = nullptr;
	UFont* BodyFont = nullptr;
	const FString FontFolder = TEXT("/Game/DeadwaterRun/UI/Fonts");
	const FString PackTextures = TEXT("/Game/Synty/UIFantasyWarriorHUD/Textures");

	UTexture2D* PackTexture(const FString& Relative)
	{
		const FString Path = PackTextures / Relative;
		return LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("%s.%s"), *Path, *FPackageName::GetShortName(Path)), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	/** A pack texture drawn as a 9-slice box (Margin = fraction kept unstretched at each edge), tinted. */
	FSlateBrush TextureBrush(const FString& Relative, const FLinearColor& Tint, float Margin = 0.25f, ESlateBrushDrawType::Type DrawAs = ESlateBrushDrawType::Box)
	{
		FSlateBrush Brush;
		if (UTexture2D* Texture = PackTexture(Relative))
		{
			Brush.SetResourceObject(Texture);
			Brush.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
		}
		Brush.DrawAs = DrawAs;
		Brush.Margin = FMargin(Margin);
		Brush.TintColor = FSlateColor(Tint);
		return Brush;
	}

	/** Flat fill with rounded corners and an optional outline (bars, tags, kbd boxes, steel panels). */
	FSlateBrush RoundedBrush(const FLinearColor& Fill, float Radius, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.f)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(Radius, Radius, Radius, Radius), FSlateColor(Outline), OutlineWidth);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		return Brush;
	}

	FSlateBrush ImageBrush(UTexture2D* Texture, const FVector2D& Size)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Texture);
		Brush.ImageSize = Size;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		return Brush;
	}

	/** A progress bar style: Track behind, a white fill the widget tints at runtime (SetFillColorAndOpacity). */
	FProgressBarStyle BarStyle(const FLinearColor& Track, float Radius, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.f)
	{
		FProgressBarStyle Style;
		Style.SetBackgroundImage(RoundedBrush(Track, Radius, Outline, OutlineWidth));
		Style.SetFillImage(RoundedBrush(FLinearColor::White, Radius));
		Style.SetMarqueeImage(RoundedBrush(FLinearColor::White, Radius));
		return Style;
	}

	FButtonStyle ButtonStyle(const FSlateBrush& Normal, float HoverBoost = 1.12f, float PressDim = 0.88f)
	{
		auto Scaled = [&Normal](float Factor)
		{
			FSlateBrush Brush = Normal;
			const FLinearColor Base = Normal.TintColor.GetSpecifiedColor();
			Brush.TintColor = FSlateColor(FLinearColor(Base.R * Factor, Base.G * Factor, Base.B * Factor, Base.A));
			return Brush;
		};
		FButtonStyle Style;
		Style.SetNormal(Normal);
		Style.SetHovered(Scaled(HoverBoost));
		Style.SetPressed(Scaled(PressDim));
		Style.SetDisabled(Scaled(0.7f));
		Style.SetNormalPadding(FMargin(0.f));
		Style.SetPressedPadding(FMargin(0.f, 2.f, 0.f, 0.f));
		return Style;
	}

	FSlateFontInfo Font(bool bHeading, int32 Size, float LetterSpacing = 0.f, bool bItalic = false)
	{
		const FName Typeface = bHeading ? FName(TEXT("Bold")) : (bItalic ? FName(TEXT("Italic")) : FName(TEXT("Regular")));
		FSlateFontInfo Info(bHeading ? static_cast<UObject*>(HeadingFont) : static_cast<UObject*>(BodyFont), Size, Typeface);
		Info.LetterSpacing = static_cast<int32>(LetterSpacing);
		return Info;
	}


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

		/** A child of another widget blueprint's class (e.g. WBP_StepButton inside WBP_SettingRow). */
		template <typename T>
		T* MakeOf(UClass* Class, const TCHAR* Name)
		{
			T* Widget = Tree->ConstructWidget<T>(Class, FName(Name));
			Widget->bIsVariable = true;
			return Widget;
		}

		UTextBlock* Text(const TCHAR* Name, const FString& Initial, int32 Size, const FLinearColor& Color, bool bBold = true)
		{
			UTextBlock* Block = Make<UTextBlock>(Name);
			Block->SetText(FText::FromString(Initial));
			if (HeadingFont && BodyFont)
			{
				Block->SetFont(DwUIContent::Font(bBold, Size));
			}
			else
			{
				FSlateFontInfo Font = Block->GetFont();
				Font.Size = Size;
				Font.TypefaceFontName = bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular"));
				Block->SetFont(Font);
			}
			Block->SetColorAndOpacity(FSlateColor(Color));
			// Light text gets a drop shadow over the 3D scene; ink on parchment gets none.
			const bool bLight = Color.GetLuminance() > 0.25f;
			Block->SetShadowOffset(bLight ? FVector2D(1.f, 1.f) : FVector2D::ZeroVector);
			Block->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, bLight ? 0.85f : 0.f));
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

	// -----------------------------------------------------------------------------------------------------------------
	// Shared pieces (art direction in DwUIContent::Ink / Steel)

	UBorder* Framed(FTreeBuilder& B, const TCHAR* Name, const FSlateBrush& Brush, const FMargin& Padding)
	{
		UBorder* Border = B.Make<UBorder>(Name);
		Border->SetBrush(Brush);
		Border->SetPadding(Padding);
		Border->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		return Border;
	}

	USizeBox* Sized(FTreeBuilder& B, UWidget* Content, float Width, float Height, const TCHAR* Name = nullptr)
	{
		USizeBox* Box = B.Make<USizeBox>(Name);
		if (Width > 0.f) { Box->SetWidthOverride(Width); }
		if (Height > 0.f) { Box->SetHeightOverride(Height); }
		Box->SetContent(Content);
		Box->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		return Box;
	}

	UImage* Picture(FTreeBuilder& B, const TCHAR* Name, UTexture2D* Texture, const FVector2D& Size)
	{
		UImage* Image = B.Make<UImage>(Name);
		Image->SetBrush(ImageBrush(Texture, Size));
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Image;
	}

	UTexture2D* Icon(const TCHAR* Name)
	{
		const FString Path = FString(TEXT("/Game/DeadwaterRun/UI/Icons/")) + Name;
		return LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("%s.%s"), *Path, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	FSlateBrush PaperBrush() { return TextureBrush(TEXT("FantasyWarrior/T_FantasyWarrior_Box_Parchment02"), Ink::Paper, 0.3f); }
	FSlateBrush PaperStripBrush() { return TextureBrush(TEXT("HUD/T_HUD_FantasyWarrior_Box_Medium_Parchment02"), Ink::Paper, 0.3f); }
	FSlateBrush PaperCardBrush() { return TextureBrush(TEXT("HUD/T_HUD_FantasyWarrior_Box_Small_Parchment02"), Ink::Paper, 0.3f); }
	FSlateBrush SteelPanelBrush() { return RoundedBrush(Steel::Panel, 3.f, Steel::Line, 1.f); }

	/** B: a rounded ink-outlined bar with its "12 / 30" label (white, shadowed) on it. */
	UOverlay* InkBar(FTreeBuilder& B, const TCHAR* BarName, const TCHAR* TextName, float Width, float Height, int32 FontSize)
	{
		UOverlay* Overlay = B.Make<UOverlay>();
		UProgressBar* Progress = B.Make<UProgressBar>(BarName);
		Progress->SetWidgetStyle(BarStyle(Ink::Track, Height * 0.5f, Ink::Dim, 2.f));
		Progress->SetPercent(0.7f);
		UOverlaySlot* BarSlot = Overlay->AddChildToOverlay(Sized(B, Progress, Width, Height));
		BarSlot->SetHorizontalAlignment(HAlign_Fill);
		UTextBlock* Label = B.Text(TextName, TEXT("140 / 140"), FontSize, Light, /*bBold*/ false);
		Label->SetShadowOffset(FVector2D(1.f, 1.f));
		Label->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f));
		UOverlaySlot* LabelSlot = Overlay->AddChildToOverlay(Label);
		LabelSlot->SetHorizontalAlignment(HAlign_Center);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		Overlay->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Overlay;
	}

	/** C: a slim bar, no label (the number sits beside it). */
	USizeBox* SteelBar(FTreeBuilder& B, const TCHAR* BarName, float Width, float Height)
	{
		UProgressBar* Progress = B.Make<UProgressBar>(BarName);
		Progress->SetWidgetStyle(BarStyle(Steel::Track, 1.f));
		Progress->SetPercent(0.7f);
		Progress->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Sized(B, Progress, Width, Height);
	}

	/** C: a key cap ("SPACE"). */
	UBorder* KeyCap(FTreeBuilder& B, const TCHAR* TextName, const TCHAR* Initial, const FLinearColor& Color)
	{
		UBorder* Cap = Framed(B, nullptr, RoundedBrush(FLinearColor(0.f, 0.f, 0.f, 0.12f), 3.f, Color, 1.f), FMargin(7.f, 1.f));
		UTextBlock* Text = B.Text(TextName, Initial, 13, Color, /*bBold*/ false);
		FSlateFontInfo Info = Text->GetFont();
		Info.LetterSpacing = 120;
		Text->SetFont(Info);
		Cap->SetContent(Text);
		Cap->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Cap;
	}

	UTextBlock* Caps(FTreeBuilder& B, const TCHAR* Name, const TCHAR* Initial, int32 Size, const FLinearColor& Color, int32 LetterSpacing = 120)
	{
		UTextBlock* Text = B.Text(Name, Initial, Size, Color);
		FSlateFontInfo Info = Text->GetFont();
		Info.LetterSpacing = LetterSpacing;
		Text->SetFont(Info);
		return Text;
	}

	// -----------------------------------------------------------------------------------------------------------------
	// Combat HUD pieces (hybrid: B parchment cards and plate, C steel rosters / top bar / End Turn)

	/** C roster row: portrait in a 2 px frame (selected / hovered / acting tint), name, slim HP bar, numbers, Fear diamonds. */
	bool BuildUnitCard(UWidgetBlueprint& Blueprint, TArray<FString>& Log)
	{
		FTreeBuilder B(Blueprint, Log);
		USizeBox* Root = B.Make<USizeBox>(TEXT("Root"));
		Root->SetWidthOverride(250.f);
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		UButton* Button = B.Make<UButton>(TEXT("CardButton"));
		FButtonStyle Style = ButtonStyle(RoundedBrush(FLinearColor(0.f, 0.f, 0.f, 0.f), 3.f));
		Style.SetHovered(RoundedBrush(FLinearColor(1.f, 1.f, 1.f, 0.06f), 3.f));
		Style.SetPressed(RoundedBrush(FLinearColor(1.f, 1.f, 1.f, 0.1f), 3.f));
		Button->SetStyle(Style);
		Root->SetContent(Button);

		UHorizontalBox* Row = B.Make<UHorizontalBox>();
		Row->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UButtonSlot* Slot = Cast<UButtonSlot>(Button->SetContent(Row)))
		{
			Slot->SetPadding(FMargin(2.f));
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}
		UBorder* Frame = Framed(B, TEXT("Frame"), RoundedBrush(FLinearColor::White, 2.f), FMargin(2.f));
		UBorder* PortraitBack = Framed(B, nullptr, RoundedBrush(FLinearColor::FromSRGBColor(FColor(36, 52, 66)), 1.f), FMargin(0.f));
		PortraitBack->SetContent(Picture(B, TEXT("Portrait"), Icon(TEXT("T_ICON_Portrait_Male_01")), FVector2D(60.f, 60.f)));
		Frame->SetContent(Sized(B, PortraitBack, 60.f, 60.f));
		AddColumn(*Row, Frame, FMargin(0.f, 0.f, 10.f, 0.f), false, VAlign_Top);

		UVerticalBox* Info = B.Make<UVerticalBox>();
		AddColumn(*Row, Info, FMargin(0.f), /*bFill*/ true, VAlign_Top);
		UHorizontalBox* NameRow = B.Make<UHorizontalBox>();
		UTextBlock* Name = B.Text(TEXT("NameText"), TEXT("Swordsman"), 17, Steel::Text, /*bBold*/ false);
		Name->SetClipping(EWidgetClipping::ClipToBounds);
		Name->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		AddColumn(*NameRow, Name, FMargin(0.f), /*bFill*/ true);
		AddRow(*Info, NameRow);
		AddRow(*Info, SteelBar(B, TEXT("HealthBar"), 170.f, 8.f), FMargin(0.f, 2.f, 0.f, 1.f));
		UHorizontalBox* Numbers = B.Make<UHorizontalBox>();
		AddColumn(*Numbers, B.Text(TEXT("HealthText"), TEXT("118 / 150"), 12, Steel::Text, /*bBold*/ false), FMargin(0.f), /*bFill*/ true);
		AddColumn(*Numbers, B.Text(TEXT("ManaText"), TEXT("20 / 20"), 12, Steel::Dim, /*bBold*/ false));
		UHorizontalBox* FearRow = B.Make<UHorizontalBox>(TEXT("FearRow"));
		AddColumn(*FearRow, B.Make<UHorizontalBox>(TEXT("FearPips")));
		AddColumn(*Numbers, FearRow);
		AddRow(*Info, Numbers);
		AddRow(*Info, SteelBar(B, TEXT("ManaBar"), 170.f, 4.f), FMargin(0.f, 1.f, 0.f, 1.f));
		UHorizontalBox* Footer = B.Make<UHorizontalBox>();
		AddColumn(*Footer, B.Text(TEXT("RoleText"), TEXT("Captain"), 12, Steel::Gold, /*bBold*/ false), FMargin(0.f, 0.f, 6.f, 0.f));
		AddColumn(*Footer, B.Text(TEXT("BudgetText"), TEXT("Move  Action"), 12, Steel::Dim, /*bBold*/ false), FMargin(0.f), /*bFill*/ true);
		AddRow(*Info, Footer);
		UWrapBox* Statuses = B.Make<UWrapBox>(TEXT("StatusList"));
		Statuses->SetInnerSlotPadding(FVector2D(3.f, 3.f));
		Statuses->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddRow(*Info, Statuses, FMargin(0.f, 2.f, 0.f, 0.f));
		return B.Finish(Root);
	}

	/** B card in the hand: hotkey and cost, the rendered icon, name, description or why it can't be played. */
	bool BuildActionButton(UWidgetBlueprint& Blueprint, TArray<FString>& Log)
	{
		FTreeBuilder B(Blueprint, Log);
		USizeBox* Root = B.Make<USizeBox>(TEXT("Root"));
		Root->SetWidthOverride(176.f);
		Root->SetHeightOverride(246.f);
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UOverlay* Layers = B.Make<UOverlay>();
		Layers->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Root->SetContent(Layers);

		UButton* Button = B.Make<UButton>(TEXT("ActionButton"));
		Button->SetStyle(ButtonStyle(PaperCardBrush(), 1.06f, 0.94f));
		UOverlaySlot* ButtonSlot = Layers->AddChildToOverlay(Button);
		ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
		ButtonSlot->SetVerticalAlignment(VAlign_Fill);
		UVerticalBox* Rows = B.Make<UVerticalBox>();
		Rows->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UButtonSlot* Slot = Cast<UButtonSlot>(Button->SetContent(Rows)))
		{
			Slot->SetPadding(FMargin(14.f, 12.f, 14.f, 10.f));
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}
		UHorizontalBox* Header = B.Make<UHorizontalBox>();
		AddColumn(*Header, B.Text(TEXT("HotkeyText"), TEXT("1"), 20, Ink::Text), FMargin(0.f), /*bFill*/ true);
		AddColumn(*Header, B.Text(TEXT("CostText"), TEXT("8 MP"), 16, Ink::Blue));
		AddRow(*Rows, Header);
		UImage* IconImage = Picture(B, TEXT("ActionIcon"), Icon(TEXT("T_ICON_Pirate_Rifle_01")), FVector2D(96.f, 96.f));
		AddRow(*Rows, IconImage, FMargin(0.f, -4.f, 0.f, 0.f), HAlign_Center);
		UTextBlock* Name = B.Text(TEXT("NameText"), TEXT("Musket Shot"), 20, Ink::Text);
		Name->SetJustification(ETextJustify::Center);
		Name->SetAutoWrapText(true);
		AddRow(*Rows, Name, FMargin(0.f, 0.f, 0.f, 2.f));
		UTextBlock* Description = B.Text(TEXT("DescText"), TEXT("24-30 damage. Range 6."), 12, Ink::Dim, /*bBold*/ false);
		Description->SetJustification(ETextJustify::Center);
		Description->SetAutoWrapText(true);
		AddRow(*Rows, Description);
		UTextBlock* Reason = B.Text(TEXT("ReasonText"), TEXT("Stand next to a loaded cannon"), 13, Ink::Red, /*bBold*/ false);
		Reason->SetJustification(ETextJustify::Center);
		Reason->SetAutoWrapText(true);
		AddRow(*Rows, Reason);

		// Armed (picked, waiting for a target): a red ink outline on top of the card.
		UBorder* Armed = Framed(B, TEXT("ArmedOutline"), RoundedBrush(FLinearColor::Transparent, 8.f, Ink::Red, 3.f), FMargin(0.f));
		Armed->SetVisibility(ESlateVisibility::HitTestInvisible);
		UOverlaySlot* ArmedSlot = Layers->AddChildToOverlay(Armed);
		ArmedSlot->SetHorizontalAlignment(HAlign_Fill);
		ArmedSlot->SetVerticalAlignment(VAlign_Fill);
		Root->SetClipping(EWidgetClipping::ClipToBounds); // long descriptions never spill past the card
		return B.Finish(Root);
	}

	bool BuildFloatingText(UWidgetBlueprint& Blueprint, TArray<FString>& Log)
	{
		FTreeBuilder B(Blueprint, Log);
		UTextBlock* Value = B.Text(TEXT("ValueText"), TEXT("-25"), 34, Light);
		Value->SetJustification(ETextJustify::Center);
		FSlateFontInfo Info = Value->GetFont();
		Info.OutlineSettings.OutlineSize = 2;
		Info.OutlineSettings.OutlineColor = FLinearColor(0.08f, 0.04f, 0.02f, 0.95f);
		Value->SetFont(Info);
		Value->SetShadowOffset(FVector2D(2.f, 2.f));
		return B.Finish(Value);
	}

	/** C top bar half: "YOUR SHIP   250 / 250" over a slim bar. */
	UVerticalBox* ShipPanel(FTreeBuilder& B, const TCHAR* PanelName, const TCHAR* Title, const TCHAR* BarName, const TCHAR* TextName,
		const FLinearColor& TitleColor, bool bMirror)
	{
		UVerticalBox* Box = B.Make<UVerticalBox>(PanelName);
		Box->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UHorizontalBox* Labels = B.Make<UHorizontalBox>();
		UTextBlock* Name = Caps(B, nullptr, Title, 15, TitleColor);
		UTextBlock* Numbers = B.Text(TextName, TEXT("250 / 250"), 15, Steel::Text, /*bBold*/ false);
		AddColumn(*Labels, bMirror ? static_cast<UWidget*>(Numbers) : Name, FMargin(0.f), /*bFill*/ true);
		AddColumn(*Labels, bMirror ? static_cast<UWidget*>(Name) : Numbers);
		AddRow(*Box, Labels, FMargin(0.f, 0.f, 0.f, 4.f));
		AddRow(*Box, SteelBar(B, BarName, 280.f, 10.f));
		return Box;
	}

	// -----------------------------------------------------------------------------------------------------------------
	// Run screens (DW-211)

	/** B row button: parchment strip, optional icon, ink title / body, red reason; red outline while selected. */
	bool BuildChoiceButton(UWidgetBlueprint& Blueprint, TArray<FString>& Log)
	{
		FTreeBuilder B(Blueprint, Log);
		USizeBox* Root = B.Make<USizeBox>(TEXT("Root"));
		Root->SetWidthOverride(300.f);
		Root->SetMinDesiredHeight(66.f);
		UOverlay* Layers = B.Make<UOverlay>();
		Root->SetContent(Layers);
		UButton* Button = B.Make<UButton>(TEXT("ChoiceButton"));
		Button->SetStyle(ButtonStyle(PaperStripBrush(), 1.06f, 0.92f));
		UOverlaySlot* ButtonSlot = Layers->AddChildToOverlay(Button);
		ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
		ButtonSlot->SetVerticalAlignment(VAlign_Fill);
		UHorizontalBox* Row = B.Make<UHorizontalBox>();
		Row->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UButtonSlot* Slot = Cast<UButtonSlot>(Button->SetContent(Row)))
		{
			Slot->SetPadding(FMargin(16.f, 10.f));
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		AddColumn(*Row, Picture(B, TEXT("ChoiceIcon"), Icon(TEXT("T_ICON_Pirate_Bottle_03")), FVector2D(54.f, 54.f)), FMargin(0.f, 0.f, 12.f, 0.f));
		UVerticalBox* Rows = B.Make<UVerticalBox>();
		AddColumn(*Row, Rows, FMargin(0.f), /*bFill*/ true);
		UTextBlock* Title = B.Text(TEXT("TitleText"), TEXT("Sea Legs"), 21, Ink::Text);
		Title->SetAutoWrapText(true); // long names (and "Teach <move>") wrap instead of leaving the card
		AddRow(*Rows, Title);
		UTextBlock* Body = B.Text(TEXT("BodyText"), TEXT("Moves 1 more tile each turn."), 15, Ink::Dim, /*bBold*/ false);
		Body->SetAutoWrapText(true);
		AddRow(*Rows, Body);
		UTextBlock* Reason = B.Text(TEXT("ReasonText"), TEXT("Pick who learns it."), 15, Ink::Red, /*bBold*/ false);
		Reason->SetAutoWrapText(true);
		AddRow(*Rows, Reason, FMargin(0.f, 2.f, 0.f, 0.f));
		UBorder* Outline = Framed(B, TEXT("SelectedOutline"), RoundedBrush(FLinearColor::Transparent, 6.f, Ink::Red, 3.f), FMargin(0.f));
		Outline->SetVisibility(ESlateVisibility::HitTestInvisible);
		UOverlaySlot* OutlineSlot = Layers->AddChildToOverlay(Outline);
		OutlineSlot->SetHorizontalAlignment(HAlign_Fill);
		OutlineSlot->SetVerticalAlignment(VAlign_Fill);
		return B.Finish(Root);
	}

	/** B tall card for picks (perks, captains, new moves): big icon or portrait, title, italic body. */
	bool BuildChoiceCard(UWidgetBlueprint& Blueprint, TArray<FString>& Log)
	{
		FTreeBuilder B(Blueprint, Log);
		USizeBox* Root = B.Make<USizeBox>(TEXT("Root"));
		Root->SetWidthOverride(280.f);
		Root->SetHeightOverride(380.f);
		Root->SetClipping(EWidgetClipping::ClipToBounds);
		UOverlay* Layers = B.Make<UOverlay>();
		Root->SetContent(Layers);
		UButton* Button = B.Make<UButton>(TEXT("ChoiceButton"));
		Button->SetStyle(ButtonStyle(PaperCardBrush(), 1.06f, 0.92f));
		UOverlaySlot* ButtonSlot = Layers->AddChildToOverlay(Button);
		ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
		ButtonSlot->SetVerticalAlignment(VAlign_Fill);
		UVerticalBox* Rows = B.Make<UVerticalBox>();
		Rows->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UButtonSlot* Slot = Cast<UButtonSlot>(Button->SetContent(Rows)))
		{
			Slot->SetPadding(FMargin(20.f, 22.f, 20.f, 14.f));
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}
		UImage* Picture = DwUIContent::Picture(B, TEXT("ChoiceIcon"), Icon(TEXT("T_ICON_Portrait_Captain_01")), FVector2D(150.f, 150.f));
		AddRow(*Rows, Picture, FMargin(0.f, 0.f, 0.f, 8.f), HAlign_Center);
		UTextBlock* Title = B.Text(TEXT("TitleText"), TEXT("Team Leader"), 28, Ink::Text);
		Title->SetJustification(ETextJustify::Center);
		Title->SetAutoWrapText(true);
		AddRow(*Rows, Title, FMargin(0.f, 0.f, 0.f, 4.f));
		UTextBlock* Body = B.Text(TEXT("BodyText"), TEXT("Rally Cry, Grapple Hook"), 16, Ink::Dim, /*bBold*/ false);
		Body->SetFont(Font(false, 16, 0.f, /*bItalic*/ true));
		Body->SetJustification(ETextJustify::Center);
		Body->SetAutoWrapText(true);
		AddRow(*Rows, Body);
		UTextBlock* Reason = B.Text(TEXT("ReasonText"), TEXT(""), 14, Ink::Red, /*bBold*/ false);
		Reason->SetJustification(ETextJustify::Center);
		Reason->SetAutoWrapText(true);
		AddRow(*Rows, Reason, FMargin(0.f, 4.f, 0.f, 0.f));
		UBorder* Outline = Framed(B, TEXT("SelectedOutline"), RoundedBrush(FLinearColor::Transparent, 10.f, Ink::Red, 4.f), FMargin(0.f));
		Outline->SetVisibility(ESlateVisibility::HitTestInvisible);
		UOverlaySlot* OutlineSlot = Layers->AddChildToOverlay(Outline);
		OutlineSlot->SetHorizontalAlignment(HAlign_Fill);
		OutlineSlot->SetVerticalAlignment(VAlign_Fill);
		return B.Finish(Root);
	}

	/** B chart stamp: an inked circle (state tint at runtime) with the node type's glyph. */
	bool BuildMapNode(UWidgetBlueprint& Blueprint, TArray<FString>& Log)
	{
		FTreeBuilder B(Blueprint, Log);
		UButton* Button = B.Make<UButton>(TEXT("NodeButton"));
		Button->SetStyle(ButtonStyle(RoundedBrush(FLinearColor::White, 40.f, Ink::Text, 3.f), 1.08f, 0.9f));
		UOverlay* Layers = B.Make<UOverlay>();
		Layers->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UButtonSlot* Slot = Cast<UButtonSlot>(Button->SetContent(Layers)))
		{
			Slot->SetPadding(FMargin(0.f));
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		UOverlaySlot* IconSlot = Layers->AddChildToOverlay(Picture(B, TEXT("NodeIcon"), PackTexture(TEXT("Icons_Map/T_ICON_FantasyWarrior_Map_Skull01_Clean")), FVector2D(44.f, 44.f)));
		IconSlot->SetHorizontalAlignment(HAlign_Center);
		IconSlot->SetVerticalAlignment(VAlign_Center);
		UTextBlock* Label = B.Text(TEXT("LabelText"), TEXT("Fight"), 17, Ink::Text);
		Label->SetJustification(ETextJustify::Center);
		UOverlaySlot* LabelSlot = Layers->AddChildToOverlay(Label);
		LabelSlot->SetHorizontalAlignment(HAlign_Center);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		return B.Finish(Button);
	}

	/** A run screen: the 3D sea dimmed warm behind a parchment page holding a centred column. */
	UVerticalBox* ScreenColumn(FTreeBuilder& B, UWidget*& OutRoot, float Width, float Opacity = 0.86f)
	{
		UBorder* Backdrop = B.Make<UBorder>(TEXT("Backdrop"));
		Backdrop->SetBrushColor(FLinearColor(0.045f, 0.025f, 0.012f, Opacity));
		Backdrop->SetPadding(FMargin(40.f, 28.f));
		Backdrop->SetHorizontalAlignment(HAlign_Center);
		Backdrop->SetVerticalAlignment(VAlign_Center);
		UBorder* Page = Framed(B, TEXT("Page"), PaperBrush(), FMargin(72.f, 60.f, 72.f, 64.f));
		Backdrop->SetContent(Page);
		USizeBox* Box = B.Make<USizeBox>();
		Box->SetWidthOverride(Width);
		Page->SetContent(Box);
		UVerticalBox* Column = B.Make<UVerticalBox>(TEXT("Column"));
		Box->SetContent(Column);
		OutRoot = Backdrop;
		return Column;
	}

	/** Choices that wrap at WrapWidth (an explicit width: a wrap box under a centred slot would otherwise wrap at its own desired width, one per row). */
	UWrapBox* ChoiceRow(FTreeBuilder& B, const TCHAR* Name, float WrapWidth, bool bCentre = true)
	{
		UWrapBox* Row = B.Make<UWrapBox>(Name);
		Row->SetInnerSlotPadding(FVector2D(14.f, 14.f));
		Row->SetExplicitWrapSize(true);
		Row->SetWrapSize(WrapWidth);
		Row->SetHorizontalAlignment(bCentre ? HAlign_Center : HAlign_Left);
		return Row;
	}

	/** An ink rule under a heading. */
	UWidget* InkRule(FTreeBuilder& B, float Width)
	{
		UImage* Rule = B.Make<UImage>();
		Rule->SetBrush(RoundedBrush(FLinearColor(Ink::Dim.R, Ink::Dim.G, Ink::Dim.B, 0.55f), 1.f));
		Rule->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Sized(B, Rule, Width, 3.f);
	}

	UTextBlock* Kicker(FTreeBuilder& B, const TCHAR* Text)
	{
		UTextBlock* Small = B.Text(nullptr, Text, 16, Ink::Dim, /*bBold*/ false);
		FSlateFontInfo Info = Small->GetFont();
		Info.LetterSpacing = 300;
		Small->SetFont(Info);
		return Small;
	}

	bool BuildMapScreen(UWidgetBlueprint& Blueprint, TArray<FString>& Log)
	{
		FTreeBuilder B(Blueprint, Log);
		UWidget* Root = nullptr;
		UVerticalBox* Column = ScreenColumn(B, Root, 1560.f, 0.7f);
		AddRow(*Column, Kicker(B, TEXT("CHART OF THE DEADWATER")));
		UHorizontalBox* Header = B.Make<UHorizontalBox>();
		AddColumn(*Header, B.Text(TEXT("TitleText"), TEXT("Sea chart - choose your first heading"), 44, Ink::Text), FMargin(0.f), /*bFill*/ true);
		AddColumn(*Header, Picture(B, nullptr, Icon(TEXT("T_ICON_Pirate_Coin_01")), FVector2D(40.f, 40.f)), FMargin(24.f, 0.f, 6.f, 0.f));
		AddColumn(*Header, B.Text(TEXT("GoldText"), TEXT("100 gold"), 28, Ink::Text));
		AddColumn(*Header, B.Text(TEXT("ShipText"), TEXT("Hull 250 / 250"), 24, Ink::Dim, /*bBold*/ false), FMargin(28.f, 0.f, 0.f, 0.f));
		AddRow(*Column, Header, FMargin(0.f, 0.f, 0.f, 6.f));
		UWrapBox* Crew = B.Make<UWrapBox>(TEXT("CrewList"));
		Crew->SetInnerSlotPadding(FVector2D(36.f, 2.f));
		AddRow(*Column, Crew, FMargin(0.f, 0.f, 0.f, 8.f));
		AddRow(*Column, InkRule(B, 1560.f), FMargin(0.f, 0.f, 0.f, 24.f));
		USizeBox* ChartBox = B.Make<USizeBox>();
		ChartBox->SetWidthOverride(1500.f);
		ChartBox->SetHeightOverride(340.f);
		ChartBox->SetContent(B.Make<UCanvasPanel>(TEXT("Chart")));
		AddRow(*Column, ChartBox, FMargin(0.f, 0.f, 0.f, 20.f), HAlign_Center);
		AddRow(*Column, B.Text(TEXT("HintText"), TEXT(""), 18, Ink::Red), FMargin(0.f, 0.f, 0.f, 6.f), HAlign_Center);
		UTextBlock* Legend = B.Text(nullptr, TEXT("Skull: a fight     Blade: a shop     ?: fortune or folly     Red ink: routes open to us     Black ink: our wake"), 16, Ink::Dim, false);
		Legend->SetFont(Font(false, 16, 0.f, /*bItalic*/ true));
		AddRow(*Column, Legend, FMargin(0.f), HAlign_Center);
		return B.Finish(Root);
	}

	bool BuildRewardsScreen(UWidgetBlueprint& Blueprint, TArray<FString>& Log)
	{
		FTreeBuilder B(Blueprint, Log);
		UWidget* Root = nullptr;
		UVerticalBox* Column = ScreenColumn(B, Root, 1000.f);
		AddRow(*Column, Kicker(B, TEXT("THE TALLY")), FMargin(0.f), HAlign_Center);
		AddRow(*Column, B.Text(TEXT("HeadlineText"), TEXT("Their crew is beaten - plunder!"), 52, Ink::Text), FMargin(0.f, 0.f, 0.f, 10.f), HAlign_Center);
		AddRow(*Column, InkRule(B, 600.f), FMargin(0.f, 0.f, 0.f, 18.f), HAlign_Center);
		UVerticalBox* Loot = B.Make<UVerticalBox>(TEXT("LootList"));
		AddRow(*Column, Loot, FMargin(0.f, 0.f, 0.f, 22.f), HAlign_Center);
		AddRow(*Column, B.Text(TEXT("OfferHintText"), TEXT("Learn a new move? (optional)"), 24, Ink::Text), FMargin(0.f, 0.f, 0.f, 12.f), HAlign_Center);
		AddRow(*Column, ChoiceRow(B, TEXT("OfferList"), 1000.f), FMargin(0.f, 0.f, 0.f, 12.f), HAlign_Center);
		AddRow(*Column, ChoiceRow(B, TEXT("LearnerList"), 1000.f), FMargin(0.f, 0.f, 0.f, 12.f), HAlign_Center);
		AddRow(*Column, ChoiceRow(B, TEXT("ForgetList"), 1000.f), FMargin(0.f, 0.f, 0.f, 12.f), HAlign_Center);
		AddRow(*Column, B.Make<UHorizontalBox>(TEXT("ClaimSlot")), FMargin(0.f, 12.f, 0.f, 0.f), HAlign_Center);
		return B.Finish(Root);
	}

	bool BuildLevelUpScreen(UWidgetBlueprint& Blueprint, TArray<FString>& Log)
	{
		FTreeBuilder B(Blueprint, Log);
		UWidget* Root = nullptr;
		UVerticalBox* Column = ScreenColumn(B, Root, 1100.f);
		AddRow(*Column, Kicker(B, TEXT("PROMOTION")), FMargin(0.f), HAlign_Center);
		AddRow(*Column, B.Text(TEXT("HeadlineText"), TEXT("Swordsman reaches level 2!"), 52, Ink::Text), FMargin(0.f, 0.f, 0.f, 6.f), HAlign_Center);
		UTextBlock* Stat = B.Text(TEXT("StatText"), TEXT("+10 max HP"), 24, Ink::Text, /*bBold*/ false);
		Stat->SetFont(Font(false, 24, 0.f, /*bItalic*/ true));
		AddRow(*Column, Stat, FMargin(0.f, 0.f, 0.f, 4.f), HAlign_Center);
		AddRow(*Column, B.Text(TEXT("QueueText"), TEXT("1 more level-up after this one"), 17, Ink::Dim, false), FMargin(0.f, 0.f, 0.f, 18.f), HAlign_Center);
		AddRow(*Column, InkRule(B, 600.f), FMargin(0.f, 0.f, 0.f, 18.f), HAlign_Center);
		AddRow(*Column, B.Text(nullptr, TEXT("Mark one trait in the log"), 26, Ink::Text), FMargin(0.f, 0.f, 0.f, 14.f), HAlign_Center);
		AddRow(*Column, ChoiceRow(B, TEXT("CardList"), 1100.f), FMargin(0.f), HAlign_Center);
		return B.Finish(Root);
	}

	/** A titled shop section: label, then its cards in a wrap row (both named, so the screen can collapse them). */
	void ShopSection(FTreeBuilder& B, UVerticalBox& Column, const TCHAR* LabelName, const TCHAR* Label, const TCHAR* ListName)
	{
		AddRow(Column, B.Text(LabelName, Label, 24, Ink::Text), FMargin(0.f, 0.f, 0.f, 8.f));
		AddRow(Column, ChoiceRow(B, ListName, 960.f, /*bCentre*/ false), FMargin(0.f, 0.f, 0.f, 16.f));
	}

	bool BuildShopScreen(UWidgetBlueprint& Blueprint, TArray<FString>& Log)
	{
		FTreeBuilder B(Blueprint, Log);
		UWidget* Root = nullptr;
		UVerticalBox* Column = ScreenColumn(B, Root, 1700.f);

		// Header: the ledger's title, gold, hull, hold; then the crew.
		UHorizontalBox* Header = B.Make<UHorizontalBox>();
		UVerticalBox* Title = B.Make<UVerticalBox>();
		AddRow(*Title, B.Text(nullptr, TEXT("The Black Lantern"), 42, Ink::Text));
		AddColumn(*Header, Title, FMargin(0.f), /*bFill*/ true);
		AddColumn(*Header, Picture(B, nullptr, Icon(TEXT("T_ICON_Pirate_Coin_01")), FVector2D(40.f, 40.f)), FMargin(28.f, 0.f, 6.f, 0.f));
		AddColumn(*Header, B.Text(TEXT("GoldText"), TEXT("240 gold"), 30, Ink::Text));
		AddColumn(*Header, B.Text(TEXT("ShipText"), TEXT("Hull 180 / 250"), 22, Ink::Dim, /*bBold*/ false), FMargin(28.f, 0.f));
		AddColumn(*Header, B.Text(TEXT("HoldText"), TEXT("Hold 1 / 6"), 22, Ink::Dim, /*bBold*/ false));
		AddRow(*Column, Header, FMargin(0.f, 0.f, 0.f, 4.f));
		UWrapBox* Crew = B.Make<UWrapBox>(TEXT("CrewList"));
		Crew->SetInnerSlotPadding(FVector2D(36.f, 2.f));
		AddRow(*Column, Crew, FMargin(0.f, 0.f, 0.f, 8.f));
		AddRow(*Column, InkRule(B, 1700.f), FMargin(0.f, 0.f, 0.f, 18.f));

		// Two pages: things bought on a click (left), new moves with their pick (right).
		UHorizontalBox* Body = B.Make<UHorizontalBox>();
		UVerticalBox* Left = B.Make<UVerticalBox>();
		ShopSection(B, *Left, TEXT("SupplyLabel"), TEXT("Repairs & Supplies"), TEXT("SupplyList"));
		ShopSection(B, *Left, TEXT("RecruitLabel"), TEXT("Crew for Hire"), TEXT("RecruitList"));
		ShopSection(B, *Left, TEXT("UpgradeLabel"), TEXT("Ship Upgrades"), TEXT("UpgradeList"));
		USizeBox* LeftBox = B.Make<USizeBox>();
		LeftBox->SetWidthOverride(960.f);
		LeftBox->SetContent(Left);
		AddColumn(*Body, LeftBox, FMargin(0.f, 0.f, 30.f, 0.f), /*bFill*/ false, VAlign_Top);

		UVerticalBox* Right = B.Make<UVerticalBox>();
		AddRow(*Right, B.Text(nullptr, TEXT("New Moves"), 24, Ink::Text), FMargin(0.f, 0.f, 0.f, 2.f));
		UTextBlock* Hint = B.Text(TEXT("MoveHintText"), TEXT("Pick a move, then who learns it."), 17, Ink::Dim, /*bBold*/ false);
		Hint->SetFont(Font(false, 17, 0.f, /*bItalic*/ true));
		Hint->SetAutoWrapText(true);
		AddRow(*Right, Hint, FMargin(0.f, 0.f, 0.f, 8.f));
		AddRow(*Right, ChoiceRow(B, TEXT("MoveList"), 700.f, /*bCentre*/ false), FMargin(0.f, 0.f, 0.f, 12.f));
		AddRow(*Right, ChoiceRow(B, TEXT("LearnerList"), 700.f, /*bCentre*/ false), FMargin(0.f, 0.f, 0.f, 12.f));
		AddRow(*Right, ChoiceRow(B, TEXT("ForgetList"), 700.f, /*bCentre*/ false), FMargin(0.f, 0.f, 0.f, 12.f));
		AddRow(*Right, B.Make<UHorizontalBox>(TEXT("BuyMoveSlot")), FMargin(0.f, 0.f, 0.f, 18.f));
		ShopSection(B, *Right, TEXT("DisbandLabel"), TEXT("Disband Crew"), TEXT("DisbandList"));
		USizeBox* RightBox = B.Make<USizeBox>();
		RightBox->SetWidthOverride(700.f);
		RightBox->SetContent(Right);
		AddColumn(*Body, RightBox, FMargin(0.f), /*bFill*/ false, VAlign_Top);
		AddRow(*Column, Body, FMargin(0.f, 0.f, 0.f, 10.f));

		AddRow(*Column, B.Make<UHorizontalBox>(TEXT("LeaveSlot")), FMargin(0.f, 6.f, 0.f, 0.f), HAlign_Center);
		return B.Finish(Root);
	}

	bool BuildNodeShellScreen(UWidgetBlueprint& Blueprint, TArray<FString>& Log)
	{
		FTreeBuilder B(Blueprint, Log);
		UWidget* Root = nullptr;
		UVerticalBox* Column = ScreenColumn(B, Root, 980.f);
		AddRow(*Column, Kicker(B, TEXT("FROM THE LOG")), FMargin(0.f), HAlign_Center);
		AddRow(*Column, B.Text(TEXT("HeadlineText"), TEXT("Calm water"), 54, Ink::Text), FMargin(0.f, 0.f, 0.f, 10.f), HAlign_Center);
		AddRow(*Column, InkRule(B, 500.f), FMargin(0.f, 0.f, 0.f, 20.f), HAlign_Center);
		UTextBlock* Body = B.Text(TEXT("BodyText"), TEXT("The sea is quiet."), 23, Ink::Text, false);
		Body->SetFont(Font(false, 23, 0.f, /*bItalic*/ true));
		Body->SetAutoWrapText(true);
		Body->SetJustification(ETextJustify::Center);
		AddRow(*Column, Body, FMargin(0.f, 0.f, 0.f, 26.f), HAlign_Center);
		AddRow(*Column, B.Text(nullptr, TEXT("What say you, Captain?"), 24, Ink::Text), FMargin(0.f, 0.f, 0.f, 12.f), HAlign_Center);
		AddRow(*Column, B.Make<UHorizontalBox>(TEXT("ActionSlot")), FMargin(0.f), HAlign_Center);
		return B.Finish(Root);
	}

	// -----------------------------------------------------------------------------------------------------------------
	// Shell screens (DW-210)

	bool BuildStepButton(UWidgetBlueprint& Blueprint, TArray<FString>& Log)
	{
		FTreeBuilder B(Blueprint, Log);
		USizeBox* Root = B.Make<USizeBox>(TEXT("Root"));
		Root->SetWidthOverride(52.f);
		Root->SetHeightOverride(44.f);
		UButton* Button = B.Make<UButton>(TEXT("ChoiceButton"));
		Button->SetStyle(ButtonStyle(PaperStripBrush(), 1.06f, 0.92f));
		Root->SetContent(Button);
		UVerticalBox* Rows = B.Make<UVerticalBox>();
		if (UButtonSlot* Slot = Cast<UButtonSlot>(Button->SetContent(Rows)))
		{
			Slot->SetPadding(FMargin(0.f));
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		AddRow(*Rows, B.Text(TEXT("TitleText"), TEXT("<"), 22, Ink::Text), FMargin(0.f), HAlign_Center);
		AddRow(*Rows, B.Text(TEXT("BodyText"), TEXT(""), 12, Ink::Dim, false));
		AddRow(*Rows, B.Text(TEXT("ReasonText"), TEXT(""), 12, Ink::Red, false));
		return B.Finish(Root);
	}

	/** Main menu, pause, confirm and the run summary: one log page (title, lines, buttons) with a wax seal. */
	bool BuildMenuScreen(UWidgetBlueprint& Blueprint, TArray<FString>& Log)
	{
		FTreeBuilder B(Blueprint, Log);
		UWidget* Root = nullptr;
		UVerticalBox* Column = ScreenColumn(B, Root, 720.f, /*Opacity*/ 0.9f); // pause and confirm stack over other screens
		AddRow(*Column, Kicker(B, TEXT("THE LOG OF THE")), FMargin(0.f), HAlign_Center);
		UTextBlock* Title = B.Text(TEXT("TitleText"), TEXT("DEADWATER RUN"), 64, Ink::Text);
		Title->SetJustification(ETextJustify::Center);
		Title->SetAutoWrapText(true);
		Title->SetLineHeightPercentage(0.82f); // Grenze's line box is tall: two-line titles (the victory page) stay together
		AddRow(*Column, Title, FMargin(0.f, 0.f, 0.f, 10.f), HAlign_Center);
		AddRow(*Column, InkRule(B, 420.f), FMargin(0.f, 0.f, 0.f, 16.f), HAlign_Center);
		AddRow(*Column, B.Make<UVerticalBox>(TEXT("LineList")), FMargin(0.f, 0.f, 0.f, 24.f), HAlign_Center);
		UVerticalBox* Buttons = B.Make<UVerticalBox>(TEXT("ButtonList"));
		AddRow(*Column, Buttons, FMargin(0.f), HAlign_Center);
		return B.Finish(Root);
	}

	bool BuildCaptainSelectScreen(UWidgetBlueprint& Blueprint, TArray<FString>& Log)
	{
		FTreeBuilder B(Blueprint, Log);
		UWidget* Root = nullptr;
		UVerticalBox* Column = ScreenColumn(B, Root, 1100.f);
		AddRow(*Column, Kicker(B, TEXT("WANTED - DEAD OR ALIVE")), FMargin(0.f), HAlign_Center);
		AddRow(*Column, B.Text(nullptr, TEXT("Who commands the Gull?"), 52, Ink::Text), FMargin(0.f, 0.f, 0.f, 18.f), HAlign_Center);
		AddRow(*Column, ChoiceRow(B, TEXT("CaptainList"), 1100.f), FMargin(0.f, 0.f, 0.f, 20.f), HAlign_Center);
		AddRow(*Column, B.Text(TEXT("DetailTitleText"), TEXT("Team Leader"), 32, Ink::Text), FMargin(0.f, 0.f, 0.f, 6.f));
		USizeBox* DetailBox = B.Make<USizeBox>();
		DetailBox->SetMinDesiredHeight(150.f);
		DetailBox->SetContent(B.Make<UVerticalBox>(TEXT("DetailList")));
		AddRow(*Column, DetailBox, FMargin(0.f, 0.f, 0.f, 18.f));
		AddRow(*Column, B.Make<UHorizontalBox>(TEXT("ButtonList")), FMargin(0.f), HAlign_Center);
		return B.Finish(Root);
	}

	bool BuildSettingRow(UWidgetBlueprint& Blueprint, TArray<FString>& Log)
	{
		UWidgetBlueprint* Step = Load<UWidgetBlueprint>(WidgetFolder, TEXT("WBP_StepButton"));
		if (!Step || !Step->GeneratedClass)
		{
			Log.Add(TEXT("ERROR WBP_StepButton must be built before WBP_SettingRow"));
			return false;
		}
		FTreeBuilder B(Blueprint, Log);
		USizeBox* Root = B.Make<USizeBox>(TEXT("Root"));
		Root->SetWidthOverride(900.f);
		UHorizontalBox* Row = B.Make<UHorizontalBox>();
		Root->SetContent(Row);
		USizeBox* LabelBox = B.Make<USizeBox>();
		LabelBox->SetWidthOverride(240.f);
		LabelBox->SetContent(B.Text(TEXT("LabelText"), TEXT("Game speed"), 22, Ink::Text, /*bBold*/ false));
		AddColumn(*Row, LabelBox);
		AddColumn(*Row, B.MakeOf<UDwChoiceButtonWidget>(Step->GeneratedClass, TEXT("DecreaseButton")), FMargin(0.f, 3.f, 8.f, 3.f));
		USizeBox* ValueBox = B.Make<USizeBox>();
		ValueBox->SetWidthOverride(220.f);
		UTextBlock* Value = B.Text(TEXT("ValueText"), TEXT("1.5x"), 22, Ink::Red);
		Value->SetJustification(ETextJustify::Center);
		ValueBox->SetContent(Value);
		AddColumn(*Row, ValueBox);
		AddColumn(*Row, B.MakeOf<UDwChoiceButtonWidget>(Step->GeneratedClass, TEXT("IncreaseButton")), FMargin(8.f, 3.f, 16.f, 3.f));
		UTextBlock* Note = B.Text(TEXT("NoteText"), TEXT("Animations only."), 15, Ink::Dim, false);
		Note->SetFont(Font(false, 15, 0.f, /*bItalic*/ true));
		Note->SetAutoWrapText(true);
		AddColumn(*Row, Note, FMargin(0.f), /*bFill*/ true);
		return B.Finish(Root);
	}

	bool BuildSettingsScreen(UWidgetBlueprint& Blueprint, TArray<FString>& Log)
	{
		FTreeBuilder B(Blueprint, Log);
		UWidget* Root = nullptr;
		UVerticalBox* Column = ScreenColumn(B, Root, 920.f, 0.9f);
		AddRow(*Column, Kicker(B, TEXT("SHIP'S ORDERS")), FMargin(0.f), HAlign_Center);
		AddRow(*Column, B.Text(nullptr, TEXT("Settings"), 52, Ink::Text), FMargin(0.f, 0.f, 0.f, 10.f), HAlign_Center);
		AddRow(*Column, InkRule(B, 420.f), FMargin(0.f, 0.f, 0.f, 20.f), HAlign_Center);
		AddRow(*Column, B.Make<UVerticalBox>(TEXT("RowList")), FMargin(0.f, 0.f, 0.f, 24.f));
		AddRow(*Column, B.Make<UHorizontalBox>(TEXT("ButtonList")), FMargin(0.f), HAlign_Center);
		return B.Finish(Root);
	}

	// -----------------------------------------------------------------------------------------------------------------
	// Kraken HUD (DW-303)

	/** "1  Tentacle Slam" over "Swordsman's tile (step off to dodge)" and "50 damage", on a dark red card. */
	/** C: one telegraphed Kraken move under the enemy roster (DW-303, B6); frame tint set at runtime. */
	bool BuildIntent(UWidgetBlueprint& Blueprint, TArray<FString>& Log)
	{
		FTreeBuilder B(Blueprint, Log);
		USizeBox* Root = B.Make<USizeBox>(TEXT("Root"));
		Root->SetWidthOverride(360.f);
		Root->SetVisibility(ESlateVisibility::HitTestInvisible);
		UBorder* Frame = Framed(B, TEXT("Frame"), RoundedBrush(FLinearColor::White, 3.f), FMargin(3.f, 0.f, 0.f, 0.f));
		Root->SetContent(Frame);
		UBorder* Body = Framed(B, nullptr, RoundedBrush(Steel::Panel, 2.f), FMargin(10.f, 6.f));
		Frame->SetContent(Body);
		UHorizontalBox* Columns = B.Make<UHorizontalBox>();
		Body->SetContent(Columns);
		AddColumn(*Columns, B.Text(TEXT("OrderText"), TEXT("1"), 24, Steel::Gold), FMargin(0.f, 0.f, 10.f, 0.f), false, VAlign_Top);
		UVerticalBox* Rows = B.Make<UVerticalBox>();
		AddColumn(*Columns, Rows, FMargin(0.f), /*bFill*/ true, VAlign_Top);
		AddRow(*Rows, Caps(B, TEXT("MoveText"), TEXT("Tentacle Slam"), 17, Steel::Text, 60));
		UTextBlock* Target = B.Text(TEXT("TargetText"), TEXT("Swordsman's tile  (step off to dodge)"), 14, Steel::Dim, /*bBold*/ false);
		Target->SetAutoWrapText(true);
		AddRow(*Rows, Target, FMargin(0.f, 1.f, 0.f, 0.f));
		UTextBlock* Effect = B.Text(TEXT("EffectText"), TEXT("50 damage"), 14, Steel::RedText, /*bBold*/ false);
		Effect->SetAutoWrapText(true);
		AddRow(*Rows, Effect, FMargin(0.f, 1.f, 0.f, 0.f));
		return B.Finish(Root);
	}

	/** The intent that floats above the Kraken's body: a C steel tag. */
	bool BuildIntentMarker(UWidgetBlueprint& Blueprint, TArray<FString>& Log)
	{
		FTreeBuilder B(Blueprint, Log);
		UBorder* Root = Framed(B, TEXT("Root"), SteelPanelBrush(), FMargin(12.f, 5.f));
		Root->SetVisibility(ESlateVisibility::HitTestInvisible);
		UTextBlock* Text = B.Text(TEXT("MarkerText"), TEXT("Tentacle Slam: Swordsman's tile"), 18, Steel::Gold);
		Text->SetJustification(ETextJustify::Center);
		Root->SetContent(Text);
		return B.Finish(Root);
	}

	bool BuildCombatHud(UWidgetBlueprint& Blueprint, TArray<FString>& Log)
	{
		FTreeBuilder B(Blueprint, Log);
		UCanvasPanel* Root = B.Make<UCanvasPanel>(TEXT("Root"));
		Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible); // empty space clicks through to the board

		// C top bar: our hull | ROUND n / YOUR TURN | their hull, or the Kraken's bar in its place (DW-303).
		UBorder* TopBar = Framed(B, nullptr, SteelPanelBrush(), FMargin(20.f, 10.f));
		UHorizontalBox* Top = B.Make<UHorizontalBox>();
		Top->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		TopBar->SetContent(Top);
		AddColumn(*Top, ShipPanel(B, TEXT("PlayerShipPanel"), TEXT("YOUR SHIP"), TEXT("PlayerShipBar"), TEXT("PlayerShipText"), Steel::Text, false));
		UVerticalBox* Turn = B.Make<UVerticalBox>();
		Turn->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		AddRow(*Turn, Caps(B, TEXT("RoundText"), TEXT("ROUND 1"), 13, Steel::Dim, 300), FMargin(0.f), HAlign_Center);
		AddRow(*Turn, Caps(B, TEXT("TurnText"), TEXT("YOUR TURN"), 26, Steel::SeaLight, 80), FMargin(0.f), HAlign_Center);
		AddColumn(*Top, Sized(B, Turn, 220.f, 0.f), FMargin(16.f, 0.f));
		UOverlay* TheirSide = B.Make<UOverlay>();
		TheirSide->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		TheirSide->AddChildToOverlay(ShipPanel(B, TEXT("EnemyShipPanel"), TEXT("ENEMY SHIP"), TEXT("EnemyShipBar"), TEXT("EnemyShipText"), Steel::RedText, true));
		UVerticalBox* BossBox = B.Make<UVerticalBox>(TEXT("BossPanel"));
		BossBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UHorizontalBox* BossTitle = B.Make<UHorizontalBox>();
		AddColumn(*BossTitle, Caps(B, TEXT("BossPhaseText"), TEXT("ENRAGED"), 15, Steel::Red), FMargin(0.f, 0.f, 8.f, 0.f));
		AddColumn(*BossTitle, B.Text(TEXT("BossTentacleText"), TEXT("Tentacles 3/4  -10%"), 13, Steel::Dim, /*bBold*/ false), FMargin(0.f), /*bFill*/ true);
		AddColumn(*BossTitle, B.Text(TEXT("BossHealthText"), TEXT("476 / 500"), 15, Steel::Text, /*bBold*/ false), FMargin(8.f, 0.f));
		AddColumn(*BossTitle, Caps(B, TEXT("BossNameText"), TEXT("THE KRAKEN"), 15, Steel::RedText));
		AddRow(*BossBox, BossTitle, FMargin(0.f, 0.f, 0.f, 4.f));
		AddRow(*BossBox, SteelBar(B, TEXT("BossHealthBar"), 340.f, 10.f));
		TheirSide->AddChildToOverlay(BossBox);
		AddColumn(*Top, TheirSide);
		Place(*Root, TopBar, FVector2D(0.5f, 0.f), FVector2D(0.f, 12.f));
		Place(*Root, B.Text(TEXT("ControlsText"), TEXT("LMB select / move / attack"), 13, Steel::Dim, /*bBold*/ false), FVector2D(0.5f, 0.f), FVector2D(0.f, 118.f));

		// C rosters down the screen edges (the decks stay clear). Filled at runtime from UnitCardClass.
		UVerticalBox* Crew = B.Make<UVerticalBox>(TEXT("CrewList"));
		Crew->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Place(*Root, Crew, FVector2D(0.f, 0.f), FVector2D(18.f, 118.f));
		UVerticalBox* Enemies = B.Make<UVerticalBox>(TEXT("EnemyList"));
		Enemies->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Place(*Root, Enemies, FVector2D(1.f, 0.f), FVector2D(-18.f, 118.f));

		// Kraken intents (B6) under the enemy roster.
		UVerticalBox* IntentBox = B.Make<UVerticalBox>(TEXT("IntentPanel"));
		IntentBox->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddRow(*IntentBox, Caps(B, nullptr, TEXT("NEXT KRAKEN TURN"), 14, Steel::RedText, 200), FMargin(0.f, 0.f, 0.f, 6.f), HAlign_Right);
		UVerticalBox* IntentRows = B.Make<UVerticalBox>(TEXT("IntentList"));
		IntentRows->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddRow(*IntentBox, IntentRows, FMargin(0.f), HAlign_Right);
		Place(*Root, IntentBox, FVector2D(1.f, 0.f), FVector2D(-18.f, 470.f));

		// B plate: the selected pirate on parchment, bottom left.
		UBorder* Plate = Framed(B, TEXT("SelectedPanel"), PaperBrush(), FMargin(32.f, 28.f, 34.f, 40.f));
		UHorizontalBox* PlateRow = B.Make<UHorizontalBox>();
		Plate->SetContent(PlateRow);
		UBorder* PortraitFrame = Framed(B, nullptr, RoundedBrush(Ink::PaperDark, 5.f, Ink::Dim, 3.f), FMargin(3.f));
		PortraitFrame->SetContent(Picture(B, TEXT("SelectedPortrait"), Icon(TEXT("T_ICON_Portrait_Female_01")), FVector2D(140.f, 150.f)));
		AddColumn(*PlateRow, PortraitFrame, FMargin(0.f, 0.f, 18.f, 0.f), false, VAlign_Top);
		UVerticalBox* PlateInfo = B.Make<UVerticalBox>();
		AddColumn(*PlateRow, Sized(B, PlateInfo, 240.f, 0.f), FMargin(0.f), false, VAlign_Top);
		AddRow(*PlateInfo, B.Text(TEXT("SelectedNameText"), TEXT("Sniper"), 32, Ink::Text));
		AddRow(*PlateInfo, B.Text(TEXT("SelectedInfoText"), TEXT("Crew"), 15, Ink::Dim, /*bBold*/ false), FMargin(0.f, -4.f, 0.f, 8.f));
		AddRow(*PlateInfo, InkBar(B, TEXT("SelectedHealthBar"), TEXT("SelectedHealthText"), 230.f, 18.f, 13), FMargin(0.f, 0.f, 0.f, 5.f));
		AddRow(*PlateInfo, InkBar(B, TEXT("SelectedManaBar"), TEXT("SelectedManaText"), 230.f, 14.f, 11), FMargin(0.f, 0.f, 0.f, 8.f));
		AddRow(*PlateInfo, B.Text(TEXT("SelectedBudgetText"), TEXT("Can move  ·  Can act"), 15, Ink::Text, /*bBold*/ false));
		Place(*Root, Plate, FVector2D(0.f, 1.f), FVector2D(14.f, -14.f));

		// B hand: the actions as cards along the bottom (fanned at runtime), the prompt strip above them.
		UVerticalBox* Bottom = B.Make<UVerticalBox>(TEXT("BottomPanel"));
		Bottom->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UBorder* PromptStrip = Framed(B, TEXT("PromptPanel"), PaperStripBrush(), FMargin(28.f, 10.f));
		UVerticalBox* PromptRows = B.Make<UVerticalBox>();
		PromptRows->SetVisibility(ESlateVisibility::HitTestInvisible);
		PromptStrip->SetContent(PromptRows);
		AddRow(*PromptRows, B.Text(TEXT("HintText"), TEXT("Click one of your pirates."), 18, Ink::Text, /*bBold*/ false), FMargin(0.f), HAlign_Center);
		UVerticalBox* Preview = B.Make<UVerticalBox>(TEXT("PreviewList"));
		Preview->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddRow(*PromptRows, Preview, FMargin(0.f, 2.f, 0.f, 0.f), HAlign_Center);
		AddRow(*PromptRows, B.Text(TEXT("MessageText"), TEXT("Not a valid target"), 18, Ink::Red), FMargin(0.f, 2.f, 0.f, 0.f), HAlign_Center);
		AddRow(*Bottom, PromptStrip, FMargin(0.f, 0.f, 0.f, 14.f), HAlign_Center);
		UHorizontalBox* Actions = B.Make<UHorizontalBox>(TEXT("ActionBar"));
		Actions->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		AddRow(*Bottom, Actions, FMargin(0.f), HAlign_Center);
		Place(*Root, Bottom, FVector2D(0.5f, 1.f), FVector2D(60.f, -8.f));

		// C End Turn, bottom right, with the non-modal "End turn?" prompt (C14) above it.
		UVerticalBox* EndTurnColumn = B.Make<UVerticalBox>();
		EndTurnColumn->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UBorder* Prompt = Framed(B, TEXT("EndTurnPrompt"), SteelPanelBrush(), FMargin(12.f, 4.f));
		Prompt->SetContent(Caps(B, nullptr, TEXT("END TURN?"), 15, Steel::SeaLight));
		Prompt->SetVisibility(ESlateVisibility::HitTestInvisible);
		AddRow(*EndTurnColumn, Prompt, FMargin(0.f, 0.f, 0.f, 8.f), HAlign_Right);
		UButton* EndTurn = B.Make<UButton>(TEXT("EndTurnButton"));
		EndTurn->SetStyle(ButtonStyle(TextureBrush(TEXT("HUD/T_HUD_FantasyWarrior_Bar_Diamond01"), Steel::Sea, 0.f, ESlateBrushDrawType::Box), 1.12f, 0.9f));
		{
			FButtonStyle Style = EndTurn->GetStyle();
			FSlateBrush Shape = Style.Normal;
			Shape.Margin = FMargin(0.16f, 0.f);
			Style.SetNormal(Shape);
			Shape.TintColor = FSlateColor(Steel::SeaLight);
			Style.SetHovered(Shape);
			Shape.TintColor = FSlateColor(Steel::Sea * 0.85f);
			Style.SetPressed(Shape);
			Shape.TintColor = FSlateColor(FLinearColor(0.3f, 0.36f, 0.4f));
			Style.SetDisabled(Shape);
			EndTurn->SetStyle(Style);
		}
		UHorizontalBox* EndTurnLabels = B.Make<UHorizontalBox>();
		EndTurnLabels->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UButtonSlot* Slot = Cast<UButtonSlot>(EndTurn->SetContent(EndTurnLabels)))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		AddColumn(*EndTurnLabels, Caps(B, TEXT("EndTurnText"), TEXT("END TURN"), 24, Steel::SeaInk, 140), FMargin(0.f, 0.f, 14.f, 0.f));
		AddColumn(*EndTurnLabels, KeyCap(B, TEXT("EndTurnKeyText"), TEXT("SPACE"), Steel::SeaInk));
		AddRow(*EndTurnColumn, Sized(B, EndTurn, 300.f, 62.f), FMargin(0.f), HAlign_Right);
		Place(*Root, EndTurnColumn, FVector2D(1.f, 1.f), FVector2D(-24.f, -28.f));

		// Banners in front of everything (never take clicks): C turn banner, B outcome page, Kraken phase in red.
		UBorder* Banner = Framed(B, TEXT("TurnBanner"), SteelPanelBrush(), FMargin(48.f, 14.f));
		Banner->SetVisibility(ESlateVisibility::HitTestInvisible);
		Banner->SetContent(Caps(B, TEXT("TurnBannerText"), TEXT("ROUND 1 - YOUR TURN"), 40, Steel::SeaLight, 100));
		Place(*Root, Banner, FVector2D(0.5f, 0.28f), FVector2D::ZeroVector, 10);

		UBorder* Outcome = Framed(B, TEXT("OutcomeBanner"), PaperBrush(), FMargin(70.f, 30.f));
		Outcome->SetVisibility(ESlateVisibility::HitTestInvisible);
		Outcome->SetContent(B.Text(TEXT("OutcomeText"), TEXT("Victory"), 56, Ink::Red));
		Place(*Root, Outcome, FVector2D(0.5f, 0.42f), FVector2D::ZeroVector, 11);

		UBorder* Phase = Framed(B, TEXT("PhaseBanner"), RoundedBrush(FLinearColor(0.32f, 0.03f, 0.02f, 0.92f), 4.f, Steel::Red, 2.f), FMargin(48.f, 14.f));
		Phase->SetVisibility(ESlateVisibility::HitTestInvisible);
		Phase->SetContent(Caps(B, TEXT("PhaseBannerText"), TEXT("THE KRAKEN IS ENRAGED!"), 42, Steel::RedText, 80));
		Place(*Root, Phase, FVector2D(0.5f, 0.36f), FVector2D::ZeroVector, 10);

		return B.Finish(Root);
	}

	/** F_DwHeading (Grenze SemiBold/Bold) and F_DwBody (Texturina SemiBold) from the Synty UI pack's font faces. */
	bool CreateUIFonts(TArray<FString>& Log)
	{
		auto Face = [&Log](const TCHAR* Path) -> UFontFace*
		{
			UFontFace* Found = LoadObject<UFontFace>(nullptr, *FString::Printf(TEXT("%s.%s"), Path, *FPackageName::GetShortName(Path)), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!Found)
			{
				Log.Add(FString::Printf(TEXT("ERROR font face %s is missing (Synty UI Fantasy Warrior HUD pack)"), Path));
			}
			return Found;
		};
		UFontFace* GrenzeSemi = Face(TEXT("/Game/Synty/UIFantasyWarriorHUD/Fonts/Grenze/Grenze-SemiBold"));
		UFontFace* GrenzeBold = Face(TEXT("/Game/Synty/UIFantasyWarriorHUD/Fonts/Grenze/Grenze-Bold"));
		UFontFace* Texturina = Face(TEXT("/Game/Synty/UIFantasyWarriorHUD/Fonts/Texturina/Texturina_18pt-SemiBold"));
		// Texturina Regular and Italic (SIL OFL 1.1, Omnibus-Type; Brandon approved 2026-11-20): the pack ships SemiBold only.
		// Sources and licence live in Tools/Content/Fonts/Texturina; imported once as font faces.
		auto ImportFace = [&Log, &Face](const TCHAR* File, const TCHAR* Name) -> UFontFace*
		{
			const FString Path = FontFolder / TEXT("Faces") / Name;
			if (!UEditorAssetLibrary::DoesAssetExist(Path))
			{
				// Built from the file bytes like UFontFileImportFactory does, minus its PostEditChange (needs Slate, absent headless).
				TArray<uint8> Bytes;
				const FString Source = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Tools/Content/Fonts/Texturina"), File));
				if (!FFileHelper::LoadFileToArray(Bytes, *Source))
				{
					Log.Add(FString::Printf(TEXT("ERROR cannot read %s"), *Source));
					return nullptr;
				}
				UFontFace* Created = FindOrCreate<UFontFace>(FontFolder / TEXT("Faces"), Name, Log);
				Created->InitializeFromBulkData(FPaths::GetCleanFilename(Source), EFontHinting::Default, Bytes.GetData(), Bytes.Num());
				Created->LoadingPolicy = EFontLoadingPolicy::Inline; // cooked with the asset, no loose file to ship
				Save(Created, Log);
			}
			return Face(*Path);
		};
		UFontFace* TexturinaRegular = ImportFace(TEXT("Texturina-Regular.ttf"), TEXT("Texturina-Regular"));
		UFontFace* TexturinaItalic = ImportFace(TEXT("Texturina-Italic.ttf"), TEXT("Texturina-Italic"));
		if (!GrenzeSemi || !GrenzeBold || !Texturina || !TexturinaRegular || !TexturinaItalic)
		{
			return false;
		}
		auto MakeFont = [&Log](const TCHAR* Name, UFontFace* Regular, UFontFace* Bold, UFontFace* Italic) -> UFont*
		{
			UFont* Asset = FindOrCreate<UFont>(FontFolder, Name, Log);
			Asset->FontCacheType = EFontCacheType::Runtime;
			FCompositeFont& Composite = Asset->GetMutableInternalCompositeFont();
			Composite = FCompositeFont();
			for (const TPair<const TCHAR*, UFontFace*>& Face : { TPair<const TCHAR*, UFontFace*>(TEXT("Regular"), Regular), TPair<const TCHAR*, UFontFace*>(TEXT("Bold"), Bold),
				TPair<const TCHAR*, UFontFace*>(TEXT("Italic"), Italic) })
			{
				FTypefaceEntry& Entry = Composite.DefaultTypeface.Fonts.AddDefaulted_GetRef();
				Entry.Name = Face.Key;
				Entry.Font = FFontData(Face.Value);
			}
			Asset->PostEditChange();
			return Save(Asset, Log) ? Asset : nullptr;
		};
		HeadingFont = MakeFont(TEXT("F_DwHeading"), GrenzeSemi, GrenzeBold, GrenzeSemi);
		BodyFont = MakeFont(TEXT("F_DwBody"), TexturinaRegular, Texturina, TexturinaItalic);
		return HeadingFont && BodyFont;
	}

	/** Sets a style default (often protected) on a widget class's defaults by name; false and a log line if it is not there. */
	template <typename T>
	bool SetStyle(UObject* Defaults, const TCHAR* Name, const T& Value, TArray<FString>& Log)
	{
		FProperty* Property = Defaults ? Defaults->GetClass()->FindPropertyByName(Name) : nullptr;
		if (!Property || Property->GetElementSize() != sizeof(T))
		{
			Log.Add(FString::Printf(TEXT("ERROR style property %s.%s is missing"), *GetNameSafe(Defaults ? Defaults->GetClass() : nullptr), Name));
			return false;
		}
		*Property->ContainerPtrToValuePtr<T>(Defaults) = Value;
		return true;
	}

	/** Combat HUD palette (hybrid B + C): pushed onto the WBP defaults the C++ widgets read at runtime. */
	bool ApplyCombatStyle(const TMap<FString, UWidgetBlueprint*>& Built, TArray<FString>& Log)
	{
		auto Defaults = [&Built](const TCHAR* Name) { return Built[Name]->GeneratedClass->GetDefaultObject(); };
		bool bOk = true;
		UObject* Card = Defaults(TEXT("WBP_UnitCard"));
		bOk &= SetStyle(Card, TEXT("AllyColor"), Steel::Text, Log);
		bOk &= SetStyle(Card, TEXT("EnemyColor"), Steel::RedText, Log);
		bOk &= SetStyle(Card, TEXT("AllyBarColor"), Steel::Red, Log);
		bOk &= SetStyle(Card, TEXT("EnemyBarColor"), Steel::Red, Log);
		bOk &= SetStyle(Card, TEXT("ManaColor"), Steel::Mp, Log);
		bOk &= SetStyle(Card, TEXT("FrameColor"), Steel::Line, Log);
		bOk &= SetStyle(Card, TEXT("SelectedFrameColor"), Steel::SeaLight, Log);
		bOk &= SetStyle(Card, TEXT("HoveredFrameColor"), Steel::Text, Log);
		bOk &= SetStyle(Card, TEXT("ActingFrameColor"), Steel::Red, Log);
		bOk &= SetStyle(Card, TEXT("FearOnColor"), Steel::Gold, Log);
		bOk &= SetStyle(Card, TEXT("FearOffColor"), FLinearColor(1.f, 1.f, 1.f, 0.15f), Log);
		bOk &= SetStyle(Card, TEXT("PipBrush"), TextureBrush(TEXT("HUD/T_HUD_FantasyWarrior_Mask_Diamond01"), FLinearColor::White, 0.f, ESlateBrushDrawType::Image), Log);
		bOk &= SetStyle(Card, TEXT("PipSize"), 11.f, Log);
		bOk &= SetStyle(Card, TEXT("StatusFont"), Font(false, 12), Log);
		bOk &= SetStyle(Card, TEXT("StatusChipColor"), FLinearColor(0.02f, 0.12f, 0.11f, 0.92f), Log);
		bOk &= SetStyle(Card, TEXT("StatusTextColor"), Steel::SeaLight, Log);

		UObject* Action = Defaults(TEXT("WBP_ActionButton"));
		bOk &= SetStyle(Action, TEXT("ReadyColor"), FLinearColor::White, Log);
		bOk &= SetStyle(Action, TEXT("ArmedColor"), FLinearColor(1.f, 0.94f, 0.86f), Log);
		bOk &= SetStyle(Action, TEXT("DisabledColor"), FLinearColor(0.72f, 0.69f, 0.64f), Log);
		bOk &= SetStyle(Action, TEXT("NameColor"), Ink::Text, Log);
		bOk &= SetStyle(Action, TEXT("DisabledNameColor"), Ink::Dim, Log);

		UObject* Hud = Defaults(TEXT("WBP_CombatHud"));
		bOk &= SetStyle(Hud, TEXT("AllyColor"), Steel::Sea, Log);
		bOk &= SetStyle(Hud, TEXT("EnemyColor"), Steel::Red, Log);
		bOk &= SetStyle(Hud, TEXT("PreviewColor"), Ink::Text, Log);
		bOk &= SetStyle(Hud, TEXT("LethalColor"), Ink::Red, Log);
		bOk &= SetStyle(Hud, TEXT("PreviewFont"), Font(false, 18), Log);
		bOk &= SetStyle(Hud, TEXT("EndTurnColor"), FLinearColor(0.85f, 0.85f, 0.85f), Log);
		bOk &= SetStyle(Hud, TEXT("EndTurnSuggestedColor"), FLinearColor::White, Log);
		bOk &= SetStyle(Hud, TEXT("SelectedHealthColor"), Ink::Hp, Log);
		bOk &= SetStyle(Hud, TEXT("SelectedManaColor"), Ink::Mp, Log);
		bOk &= SetStyle(Hud, TEXT("FanAnglePerCard"), 4.f, Log);
		bOk &= SetStyle(Hud, TEXT("FanDropPerCard"), 6.f, Log);

		UObject* Intent = Defaults(TEXT("WBP_Intent"));
		bOk &= SetStyle(Intent, TEXT("NormalColor"), Steel::Line, Log);
		bOk &= SetStyle(Intent, TEXT("LethalColor"), Steel::Red, Log);
		bOk &= SetStyle(Intent, TEXT("CaptainColor"), Steel::Gold, Log);
		bOk &= SetStyle(Intent, TEXT("MissColor"), FLinearColor(0.3f, 0.33f, 0.36f), Log);
		UObject* Marker = Defaults(TEXT("WBP_IntentMarker"));
		bOk &= SetStyle(Marker, TEXT("NormalColor"), Steel::Gold, Log);
		bOk &= SetStyle(Marker, TEXT("LethalColor"), Steel::RedText, Log);
		return bOk;
	}

	/** Run and shell screens (Ship's Log): ink buttons on parchment, inked chart stamps. */
	bool ApplyRunStyle(const TMap<FString, UWidgetBlueprint*>& Built, TArray<FString>& Log)
	{
		bool bOk = true;
		for (const TCHAR* Name : { TEXT("WBP_ChoiceButton"), TEXT("WBP_ChoiceCard"), TEXT("WBP_StepButton") })
		{
			UObject* Button = Built[Name]->GeneratedClass->GetDefaultObject();
			bOk &= SetStyle(Button, TEXT("ReadyColor"), FLinearColor::White, Log);
			bOk &= SetStyle(Button, TEXT("SelectedColor"), FLinearColor(1.f, 0.9f, 0.8f), Log);
			bOk &= SetStyle(Button, TEXT("DisabledColor"), FLinearColor(0.74f, 0.71f, 0.66f), Log);
			bOk &= SetStyle(Button, TEXT("GlyphColor"), Ink::Text, Log);
		}
		UObject* Node = Built[TEXT("WBP_MapNode")]->GeneratedClass->GetDefaultObject();
		bOk &= SetStyle(Node, TEXT("AheadColor"), Ink::Paper, Log);
		bOk &= SetStyle(Node, TEXT("ChoosableColor"), FLinearColor::FromSRGBColor(FColor(232, 160, 140)), Log);
		bOk &= SetStyle(Node, TEXT("CurrentColor"), FLinearColor::FromSRGBColor(FColor(222, 186, 110)), Log);
		bOk &= SetStyle(Node, TEXT("SailedColor"), FLinearColor::FromSRGBColor(FColor(196, 176, 140)), Log);
		bOk &= SetStyle(Node, TEXT("MissedColor"), FLinearColor(0.9f, 0.84f, 0.72f, 0.45f), Log);
		bOk &= SetStyle(Node, TEXT("BossColor"), FLinearColor::FromSRGBColor(FColor(200, 92, 74)), Log);
		bOk &= SetStyle(Node, TEXT("LabelColor"), Ink::Text, Log);
		bOk &= SetStyle(Node, TEXT("MissedLabelColor"), Ink::Dim, Log);
		bOk &= SetStyle(Node, TEXT("IconColor"), Ink::Text, Log);
		bOk &= SetStyle(Node, TEXT("FightIcon"), TObjectPtr<UTexture2D>(PackTexture(TEXT("Icons_Map/T_ICON_FantasyWarrior_Map_Skull01_Clean"))), Log);
		bOk &= SetStyle(Node, TEXT("ShopIcon"), TObjectPtr<UTexture2D>(PackTexture(TEXT("Icons_Map/T_ICON_FantasyWarrior_Map_ShopWeapons01_Clean"))), Log);
		bOk &= SetStyle(Node, TEXT("EventIcon"), TObjectPtr<UTexture2D>(PackTexture(TEXT("Icons_Map/T_ICON_FantasyWarrior_Map_Unknown01_Clean"))), Log);
		bOk &= SetStyle(Node, TEXT("BossIcon"), TObjectPtr<UTexture2D>(PackTexture(TEXT("Icons_Map/T_ICON_FantasyWarrior_Map_Monster01_Clean"))), Log);
		if (UDwMapScreen* Map = Built[TEXT("WBP_Map")]->GeneratedClass->GetDefaultObject<UDwMapScreen>())
		{
			Map->NodeSize = FVector2D(80.f, 80.f);
			Map->Spacing = FVector2D(150.f, 120.f);
			Map->RouteColor = FLinearColor(Ink::Dim.R, Ink::Dim.G, Ink::Dim.B, 0.6f);
			Map->OpenRouteColor = Ink::Red;
			Map->SailedRouteColor = Ink::Text;
		}
		return bOk;
	}

	// -----------------------------------------------------------------------------------------------------------------
	// UI art: icons and portraits (Tools/Content/ui_icon_map.json)

	/**
	 * Sets the soft texture property PropertyName ("Icon" or "Portrait") on every object the map names: a data asset, or
	 * a Blueprint's class defaults (the BP_Unit_* visuals). Only writes and saves what changed. Each owner keeps the
	 * field; this pass is the only writer, so content scripts that rebuild definitions never fight over it.
	 */
	bool AssignTextures(const TSharedPtr<FJsonObject>& Map, const FString& IconFolder, const FName PropertyName, TArray<FString>& Log)
	{
		bool bOk = true;
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : Map->Values)
		{
			const FString TextureName = Entry.Value->AsString();
			const FString TexturePath = TextureName.StartsWith(TEXT("/")) ? TextureName : IconFolder / TextureName;
			UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("%s.%s"), *TexturePath, *FPackageName::GetShortName(TexturePath)),
				nullptr, LOAD_NoWarn | LOAD_Quiet);
			UObject* Asset = LoadObject<UObject>(nullptr, *FString::Printf(TEXT("%s.%s"), *Entry.Key, *FPackageName::GetShortName(Entry.Key)),
				nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!Texture || !Asset)
			{
				Log.Add(FString::Printf(TEXT("ERROR ui_icon_map: %s -> %s (%s missing; run Tools/RenderIcons.ps1 -Import?)"), *Entry.Key, *TextureName,
					Texture ? *Entry.Key : *TexturePath));
				bOk = false;
				continue;
			}
			UBlueprint* Blueprint = Cast<UBlueprint>(Asset);
			UObject* Target = Blueprint && Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : Asset;
			FSoftObjectProperty* Property = FindFProperty<FSoftObjectProperty>(Target->GetClass(), PropertyName);
			if (!Property)
			{
				Log.Add(FString::Printf(TEXT("ERROR ui_icon_map: %s has no %s property"), *Entry.Key, *PropertyName.ToString()));
				bOk = false;
				continue;
			}
			FSoftObjectPtr* Value = Property->GetPropertyValuePtr_InContainer(Target);
			if (Value->ToSoftObjectPath() == FSoftObjectPath(Texture))
			{
				continue;
			}
			*Value = FSoftObjectPtr(Texture);
			Target->MarkPackageDirty();
			bOk &= Save(Asset, Log);
			Log.Add(FString::Printf(TEXT("Set %s.%s = %s"), *Entry.Key, *PropertyName.ToString(), *Texture->GetName()));
		}
		return bOk;
	}

	bool AssignUIArt(TArray<FString>& Log)
	{
		const FString File = FPaths::Combine(FPaths::ProjectDir(), TEXT("Tools/Content/ui_icon_map.json"));
		FString Json;
		TSharedPtr<FJsonObject> Root;
		if (!FFileHelper::LoadFileToString(Json, *File) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
		{
			Log.Add(FString::Printf(TEXT("ERROR cannot read %s"), *File));
			return false;
		}
		const FString IconFolder = Root->GetStringField(TEXT("iconFolder"));
		const TSharedPtr<FJsonObject>* Icons = nullptr;
		const TSharedPtr<FJsonObject>* Portraits = nullptr;
		if (!Root->TryGetObjectField(TEXT("icons"), Icons) || !Root->TryGetObjectField(TEXT("portraits"), Portraits))
		{
			Log.Add(TEXT("ERROR ui_icon_map.json needs \"icons\" and \"portraits\""));
			return false;
		}
		const bool bIcons = AssignTextures(*Icons, IconFolder, TEXT("Icon"), Log);
		const bool bPortraits = AssignTextures(*Portraits, IconFolder, TEXT("Portrait"), Log);
		return bIcons && bPortraits;
	}
}

TArray<FString> UDwUIContentLibrary::CreateUIContent()
{
	using namespace DwUIContent;
	TArray<FString> OutLog;
	bool bOk = true;
	auto Fail = [&OutLog]() { OutLog.Add(TEXT("CONTENT FAIL")); return OutLog; };

	if (!CreateUIFonts(OutLog))
	{
		return Fail();
	}
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
		{ TEXT("WBP_Intent"), UDwIntentWidget::StaticClass(), &BuildIntent },
		{ TEXT("WBP_IntentMarker"), UDwIntentMarkerWidget::StaticClass(), &BuildIntentMarker },
		{ TEXT("WBP_PrimaryLayout"), UDwPrimaryLayoutWidget::StaticClass(), &BuildPrimaryLayout },
		{ TEXT("WBP_CombatHud"), UDwCombatHudScreen::StaticClass(), &BuildCombatHud },
		{ TEXT("WBP_ChoiceButton"), UDwChoiceButtonWidget::StaticClass(), &BuildChoiceButton },
		{ TEXT("WBP_ChoiceCard"), UDwChoiceButtonWidget::StaticClass(), &BuildChoiceCard },
		{ TEXT("WBP_MapNode"), UDwMapNodeWidget::StaticClass(), &BuildMapNode },
		{ TEXT("WBP_Map"), UDwMapScreen::StaticClass(), &BuildMapScreen },
		{ TEXT("WBP_Rewards"), UDwRewardsScreen::StaticClass(), &BuildRewardsScreen },
		{ TEXT("WBP_LevelUp"), UDwLevelUpScreen::StaticClass(), &BuildLevelUpScreen },
		{ TEXT("WBP_Shop"), UDwShopScreen::StaticClass(), &BuildShopScreen },
		{ TEXT("WBP_NodeShell"), UDwNodeShellScreen::StaticClass(), &BuildNodeShellScreen },
		{ TEXT("WBP_StepButton"), UDwChoiceButtonWidget::StaticClass(), &BuildStepButton },
		{ TEXT("WBP_SettingRow"), UDwSettingRowWidget::StaticClass(), &BuildSettingRow }, // after WBP_StepButton
		{ TEXT("WBP_Menu"), UDwMenuScreen::StaticClass(), &BuildMenuScreen },
		{ TEXT("WBP_CaptainSelect"), UDwCaptainSelectScreen::StaticClass(), &BuildCaptainSelectScreen },
		{ TEXT("WBP_Settings"), UDwSettingsScreen::StaticClass(), &BuildSettingsScreen } };
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
		Hud->IntentClass = ClassOf(TEXT("WBP_Intent"));
		Hud->IntentMarkerClass = ClassOf(TEXT("WBP_IntentMarker"));
	}
	bOk &= ApplyCombatStyle(Built, OutLog);
	bOk &= ApplyRunStyle(Built, OutLog);
	if (UDwSettingsScreen* SettingsScreen = Built[TEXT("WBP_Settings")]->GeneratedClass->GetDefaultObject<UDwSettingsScreen>())
	{
		SettingsScreen->RowClass = ClassOf(TEXT("WBP_SettingRow"));
	}
	for (const TCHAR* Name : { TEXT("WBP_Map"), TEXT("WBP_Rewards"), TEXT("WBP_LevelUp"), TEXT("WBP_Shop"), TEXT("WBP_NodeShell"), TEXT("WBP_Menu"),
		TEXT("WBP_CaptainSelect"), TEXT("WBP_Settings") })
	{
		if (UDwRunScreen* Screen = Built[Name]->GeneratedClass->GetDefaultObject<UDwRunScreen>())
		{
			Screen->ChoiceButtonClass = ClassOf(TEXT("WBP_ChoiceButton"));
			Screen->CardButtonClass = ClassOf(TEXT("WBP_ChoiceCard"));
			Screen->LineFont = Font(false, 18);
			Screen->LineColor = Ink::Text;
		}
	}
	if (UDwMapScreen* Map = Built[TEXT("WBP_Map")]->GeneratedClass->GetDefaultObject<UDwMapScreen>())
	{
		Map->NodeWidgetClass = ClassOf(TEXT("WBP_MapNode"));
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
	Screens->Map = TSoftClassPtr<UDwActivatableScreen>(ClassOf(TEXT("WBP_Map")));
	Screens->Rewards = TSoftClassPtr<UDwActivatableScreen>(ClassOf(TEXT("WBP_Rewards")));
	Screens->LevelUp = TSoftClassPtr<UDwActivatableScreen>(ClassOf(TEXT("WBP_LevelUp")));
	Screens->Menu = TSoftClassPtr<UDwActivatableScreen>(ClassOf(TEXT("WBP_Menu")));
	Screens->CaptainSelect = TSoftClassPtr<UDwActivatableScreen>(ClassOf(TEXT("WBP_CaptainSelect")));
	Screens->Settings = TSoftClassPtr<UDwActivatableScreen>(ClassOf(TEXT("WBP_Settings")));
	Screens->Shop = TSoftClassPtr<UDwActivatableScreen>(ClassOf(TEXT("WBP_Shop")));
	Screens->Event = TSoftClassPtr<UDwActivatableScreen>(ClassOf(TEXT("WBP_NodeShell"))); // the event flow; a dedicated WBP_Event later
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

	// 5. Icons and portraits on definitions and unit visuals (Tools/Content/ui_icon_map.json).
	bOk &= AssignUIArt(OutLog);

	OutLog.Add(bOk ? TEXT("CONTENT PASS") : TEXT("CONTENT FAIL"));
	return OutLog;
}
