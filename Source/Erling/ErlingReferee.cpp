#include "ErlingReferee.h"
#include "Football.h"
#include "ErlingTuning.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

UErlingReferee::UErlingReferee()
{
	PrimaryComponentTick.bCanEverTick = false;
}
AFootballMode* UErlingReferee::Mode() const
{
	return CastChecked<AFootballMode>(GetOwner());
}
void UErlingReferee::StartShot(AFootballPlayer* Shooter)
{
	LastShooter = Shooter;
	LastShot = GetWorld()->GetTimeSeconds();
	ShotInFlight = true;
}
void UErlingReferee::HideMiss()
{
	if (BallHidden)
		return;
	if (auto PC = Cast<AFootballController>(GetWorld()->GetFirstPlayerController()))
		PC->PlayEffect(TEXT("fail"));
	BallHidden = true;
	Mode()->Ball->SetLinearDamping(1.2f);
	ResetAt = GetWorld()->GetTimeSeconds() + 1.2f;
}
void UErlingReferee::Update(AFootballController* PC)
{
	AFootballMode* M = Mode();
	auto P = M->Ball->GetComponentLocation();
	const FVector OldBall = PreviousBall;
	PreviousBall = P;
	float T = GetWorld()->GetTimeSeconds();
	if (ResetAt > 0 && T >= ResetAt)
	{
		M->ResetBall();
		return;
	}
	float CrossingY = P.Y, CrossingZ = P.Z;
	const float Plane = ErlingPitch::GoalPlaneFor(P.X);
	const bool Crossed = FMath::Abs(P.X) > ErlingPitch::GoalPlaneX && FMath::Abs(OldBall.X) <= ErlingPitch::GoalPlaneX;
	if (Crossed && !FMath::IsNearlyEqual(P.X, OldBall.X))
	{
		const FVector Crossing = FMath::Lerp(OldBall, P, (Plane - OldBall.X) / (P.X - OldBall.X));
		CrossingY = Crossing.Y;
		CrossingZ = Crossing.Z;
	}
	if (!Scored && Crossed && FMath::Abs(CrossingY) < ErlingPitch::ScoringHalfWidth && CrossingZ < ErlingPitch::ScoringMaxZ &&
	    CrossingZ > 0)
	{
		Goals++;
		PC->OnGoal(LastShooter.Get());
		PC->PlayEffect(TEXT("goal"), 1.5f);
		Scored = true;
		ResetAt = T + 1.5f;
		PC->Toast = PC->Localize(TEXT("GOAL!  +1"), TEXT("GOL!  +1"));
		PC->ToastUntil = T + 2;
	}
	if (!Scored && ShotInFlight && !BallHidden &&
	    ((FMath::Abs(P.X) > ErlingPitch::GoalPlaneX) ||
	        (FMath::Abs(P.X) > ErlingPitch::MissCheckX &&
	            (FMath::Abs(P.Y) > ErlingPitch::ScoringHalfWidth || P.Z > ErlingPitch::ScoringMaxZ)) ||
	        FMath::Abs(P.Y) > ErlingPitch::TouchlineY))
		HideMiss();
	if (P.Z < -200)
		M->ResetBall();
}
