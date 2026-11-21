// Copyright Brandon Hillig 2026

#include "UI/DwShellScreens.h"

#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Core/DwGameplayTags.h"

namespace DwShellScreens
{
	const FLinearColor Light(0.95f, 0.92f, 0.85f);
}

// ---------------------------------------------------------------------------------------------------------------------
// Menu

UDwMenuScreen::UDwMenuScreen()
{
	bIsBackHandler = true;
}

void UDwMenuScreen::NativeOnViewModelChanged()
{
	const UDwMenuViewModel* Menu = GetViewModel<UDwMenuViewModel>();
	if (!Menu)
	{
		return;
	}
	TitleText->SetText(Menu->GetTitle());
	FSlateFontInfo TitleFont = TitleText->GetFont();
	TitleSize = TitleSize > 0 ? TitleSize : TitleFont.Size;
	const FString Title = Menu->GetTitle().ToString();
	const bool bLong = LongTitleSize > 0 && !Title.Contains(TEXT("\n")) && Title.Len() > LongTitleChars;
	TitleFont.Size = bLong ? LongTitleSize : TitleSize;
	TitleText->SetFont(TitleFont);
	FillLines(LineList, Menu->GetLines(), 20, DwShellScreens::Light);
	LineList->SetVisibility(Menu->GetLines().IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	FillChoices(ButtonList, Menu->GetButtons());
}

bool UDwMenuScreen::NativeOnHandleBackAction()
{
	UDwMenuViewModel* Menu = GetViewModel<UDwMenuViewModel>();
	if (Menu && Menu->HasBack())
	{
		PlayUISound(DwTags::UI_Sound_Cancel);
		Menu->HandleBack();
	}
	return true; // a menu always eats Esc: it never falls through to the game underneath
}

// ---------------------------------------------------------------------------------------------------------------------
// Captain select

UDwCaptainSelectScreen::UDwCaptainSelectScreen()
{
	bIsBackHandler = true;
}

void UDwCaptainSelectScreen::NativeOnViewModelChanged()
{
	const UDwCaptainSelectViewModel* Select = GetViewModel<UDwCaptainSelectViewModel>();
	if (!Select)
	{
		return;
	}
	FillChoices(CaptainList, Select->GetCaptains(), EDwChoiceStyle::Poster);
	// Wanted posters pinned up slightly askew.
	for (int32 Index = 0; Index < CaptainList->GetChildrenCount(); ++Index)
	{
		if (UWidget* Poster = CaptainList->GetChildAt(Index); Poster && PosterTilts.Num() > 0)
		{
			Poster->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			Poster->SetRenderTransformAngle(PosterTilts[Index % PosterTilts.Num()]);
		}
	}
	DetailTitleText->SetText(Select->GetDetailTitle());
	FillLines(DetailList, Select->GetDetailLines(), 18, DwShellScreens::Light);
	for (UWidget* Child : DetailList->GetAllChildren())
	{
		if (UTextBlock* Line = Cast<UTextBlock>(Child))
		{
			Line->SetAutoWrapText(true);
		}
	}
	const TArray<TObjectPtr<UDwChoiceViewModel>> Buttons = { Select->GetBack(), Select->GetSetSail() }; // Back, then the primary on the right
	FillChoices(ButtonList, Buttons);
}

bool UDwCaptainSelectScreen::NativeOnHandleBackAction()
{
	if (UDwCaptainSelectViewModel* Select = GetViewModel<UDwCaptainSelectViewModel>())
	{
		PlayUISound(DwTags::UI_Sound_Cancel);
		Select->HandleBack();
	}
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Settings

void UDwSettingRowWidget::NativeOnViewModelChanged()
{
	const UDwSettingRowViewModel* Row = GetViewModel<UDwSettingRowViewModel>();
	if (!Row)
	{
		return;
	}
	LabelText->SetText(Row->GetLabel());
	ValueText->SetText(Row->GetValue());
	NoteText->SetText(Row->GetNote());
	NoteText->SetVisibility(Row->GetNote().IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	DecreaseButton->SetViewModel(Row->GetDecrease());
	IncreaseButton->SetViewModel(Row->GetIncrease());
}

UDwSettingsScreen::UDwSettingsScreen()
{
	bIsBackHandler = true;
}

void UDwSettingsScreen::NativeOnViewModelChanged()
{
	const UDwSettingsViewModel* Settings = GetViewModel<UDwSettingsViewModel>();
	if (!Settings || !RowClass)
	{
		return;
	}
	const TArray<TObjectPtr<UDwSettingRowViewModel>>& Rows = Settings->GetRows();
	if (RowList->GetChildrenCount() != Rows.Num())
	{
		RowList->ClearChildren();
		for (int32 Index = 0; Index < Rows.Num(); ++Index)
		{
			RowList->AddChild(CreateWidget<UDwSettingRowWidget>(this, RowClass));
		}
	}
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		if (UDwSettingRowWidget* Row = Cast<UDwSettingRowWidget>(RowList->GetChildAt(Index)))
		{
			Row->SetViewModel(Rows[Index]);
		}
	}
	const TArray<TObjectPtr<UDwChoiceViewModel>> Buttons = { Settings->GetBack() };
	FillChoices(ButtonList, Buttons);
}

bool UDwSettingsScreen::NativeOnHandleBackAction()
{
	if (UDwSettingsViewModel* Settings = GetViewModel<UDwSettingsViewModel>())
	{
		PlayUISound(DwTags::UI_Sound_Cancel);
		Settings->HandleBack();
	}
	return true;
}
