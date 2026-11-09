// Copyright Brandon Hillig 2026

#include "Abilities/Tasks/DwAT_MoveAlongPath.h"

#include "GameFramework/Actor.h"
#include "Presentation/DwPresentationSubsystem.h"

UDwAT_MoveAlongPath::UDwAT_MoveAlongPath()
{
	bTickingTask = true;
}

UDwAT_MoveAlongPath* UDwAT_MoveAlongPath::MoveAlongPath(UGameplayAbility* OwningAbility, const TArray<FVector>& InPoints, float InSpeed)
{
	UDwAT_MoveAlongPath* Task = NewAbilityTask<UDwAT_MoveAlongPath>(OwningAbility);
	Task->Points = InPoints;
	Task->Speed = FMath::Max(InSpeed, 1.f);
	return Task;
}

void UDwAT_MoveAlongPath::Activate()
{
	Super::Activate();
	AActor* Avatar = GetAvatarActor();
	if (!Avatar || Points.IsEmpty() || UDwPresentationSubsystem::IsInstant(this))
	{
		Finish();
	}
}

void UDwAT_MoveAlongPath::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);
	AActor* Avatar = GetAvatarActor();
	if (!Avatar || IsFinished())
	{
		return;
	}
	float Budget = Speed * UDwPresentationSubsystem::GetPlaybackRate(this) * DeltaTime;
	FVector Location = Avatar->GetActorLocation();
	while (Budget > 0.f && Points.IsValidIndex(NextPoint))
	{
		const FVector ToPoint = Points[NextPoint] - Location;
		const float Distance = ToPoint.Size();
		if (Distance <= Budget)
		{
			Location = Points[NextPoint++];
			Budget -= Distance;
		}
		else
		{
			Location += ToPoint / Distance * Budget;
			Budget = 0.f;
		}
		const FVector Flat(ToPoint.X, ToPoint.Y, 0.f);
		if (!Flat.IsNearlyZero())
		{
			Avatar->SetActorRotation(Flat.Rotation());
		}
	}
	Avatar->SetActorLocation(Location);
	if (!Points.IsValidIndex(NextPoint))
	{
		Finish();
	}
}

void UDwAT_MoveAlongPath::Finish()
{
	if (AActor* Avatar = GetAvatarActor(); Avatar && !Points.IsEmpty())
	{
		Avatar->SetActorLocation(Points.Last());
	}
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnCompleted.Broadcast();
	}
	EndTask();
}
