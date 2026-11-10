// Copyright Brandon Hillig 2026

#include "UI/DwUIJuice.h"

#include "Animation/WidgetAnimation.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UI/DwUIManagerSubsystem.h"
#include "UI/DwUISoundMap.h"
#include "UObject/UnrealType.h"

namespace DwJuice
{
	const FName Intro(TEXT("Anim_Intro"));
	const FName Outro(TEXT("Anim_Outro"));
	const FName Hover(TEXT("Anim_Hover"));
	const FName Press(TEXT("Anim_Press"));
	const FName Disabled(TEXT("Anim_Disabled"));
	const FName Highlight(TEXT("Anim_Highlight"));

	UWidgetAnimation* FindAnimation(const UUserWidget& Widget, FName Name)
	{
		// Widget Blueprint animations are object properties of the generated class, named after the animation.
		const FObjectProperty* Property = FindFProperty<FObjectProperty>(Widget.GetClass(), Name);
		if (!Property || !Property->PropertyClass->IsChildOf(UWidgetAnimation::StaticClass()))
		{
			return nullptr;
		}
		return Cast<UWidgetAnimation>(Property->GetObjectPropertyValue_InContainer(&Widget));
	}

	bool Play(UUserWidget& Widget, FName Name)
	{
		UWidgetAnimation* Animation = FindAnimation(Widget, Name);
		if (!Animation)
		{
			return false;
		}
		Widget.PlayAnimation(Animation);
		return true;
	}

	void PlaySound(const UUserWidget& Widget, const FGameplayTag& Tag)
	{
		const UDwUIManagerSubsystem* Manager = UDwUIManagerSubsystem::Get(Widget.GetOwningLocalPlayer());
		const UDwUISoundMap* Map = Manager ? Manager->GetSoundMap() : nullptr;
		if (USoundBase* Sound = Map ? Map->FindSound(Tag) : nullptr)
		{
			UGameplayStatics::PlaySound2D(&Widget, Sound);
		}
	}
}
