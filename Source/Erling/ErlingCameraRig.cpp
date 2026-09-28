#include "ErlingCameraRig.h"
#include "Football.h"
#include "ErlingTuning.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

UErlingCameraRig::UErlingCameraRig()
{
	PrimaryComponentTick.bCanEverTick = false;
}
AFootballController* UErlingCameraRig::Controller() const
{
	return CastChecked<AFootballController>(GetOwner());
}
void UErlingCameraRig::Update(float Dt, bool bGameplayView, bool bEditorView, AFootballMode* GameplayMode, const FVector& P)
{
	AFootballController* C = Controller();
	ACameraActor* ViewCamera = C->ViewCamera;
	FVector Cam, Target;
	float Fov = 48;
	const bool GameplayView = bGameplayView;
	auto& Look = ViewCamera->GetCameraComponent()->PostProcessSettings;
	Look.bOverride_DepthOfFieldFocalDistance = true;
	Look.DepthOfFieldFocalDistance = (!GameplayView && !bEditorView) ? 780.f : 0.f;
	Look.bOverride_DepthOfFieldFstop = true;
	Look.DepthOfFieldFstop = 4.f;

	if (GameplayView)
	{
		const int32 Mode = C->Saved ? FMath::Clamp(C->Saved->CameraMode, 0, 2) : 0;
		if (!CameraPivotInitialized)
		{
			CameraPivot = P;
			CameraPivotInitialized = true;
		}
		if (Mode == 0)
			CameraPivot = FMath::VInterpTo(CameraPivot, P, Dt, 8.f);
		else if (Mode == 1)
			CameraPivot = P;
		else
			CameraPivot = FMath::VInterpTo(CameraPivot, P, Dt, 5.5f);
		if (Mode == 2)
		{
			UpdatePitchCameraLead(Dt);
			const FVector Center = UpdatePitchViewCenter(Dt, CameraPivot + FVector(PitchCameraLeadX, 0, 0));
			Cam = Center + FVector(0, 1500, 1050);
			Target = Center + FVector(0, -280, 70);
			Fov = 68;
			Target = UpdatePitchShotTarget(Dt, Cam, Target, Fov);
		}
		else
		{
			PitchViewInitialized = false;
			PitchShotTracking = false;
			PitchShotHoldRemaining = 0.f;
			PitchShotOffset = FVector::ZeroVector;
			if (GameplayMode)
				PitchShotSeen = GameplayMode->LastShot;
			const float TopDown = FMath::Clamp((-C->Pitch - 15.f) / 25.f, 0.f, 1.f);
			const float LookAhead = FMath::Lerp(350.f, 180.f, TopDown);
			const FVector ForwardDir = FRotator(0, C->Yaw, 0).Vector();
			Target = CameraPivot + ForwardDir * LookAhead + FVector(0, 0, 40);
			Cam = CameraPivot - FRotator(C->Pitch, C->Yaw, 0).Vector() * 750 + FVector(0, 0, 300);
			Fov = 65;
		}
		ViewCamera->SetActorLocationAndRotation(Cam, (Target - Cam).Rotation());
		ViewCamera->GetCameraComponent()->SetFieldOfView(FMath::FInterpTo(ViewCamera->GetCameraComponent()->FieldOfView, Fov, Dt, 8.f));
	}
	else if (!bEditorView)
	{
		// Low presentation camera keeps the existing football demo and saved wardrobe.
		const FVector Side(.31f, .95f, 0);
		Target = P + Side * 145.f + FVector(0, 0, -10);
		Cam = P + FVector(740, -245, 45);
		Fov = 42;
		const bool Snap = ViewCamera->GetActorLocation().IsNearlyZero();
		ViewCamera->SetActorLocationAndRotation(
		    Cam, Snap ? (Target - Cam).Rotation() : FMath::RInterpTo(ViewCamera->GetActorRotation(), (Target - Cam).Rotation(), Dt, 5.f));
		ViewCamera->GetCameraComponent()->SetFieldOfView(FMath::FInterpTo(ViewCamera->GetCameraComponent()->FieldOfView, Fov, Dt, 5.f));
	}
	else
	{
		bool Close = bEditorView;
		Target = P + FVector(0, 0, 0);
		if (Close)
			Fov = FMath::Lerp(48.f, 34.f, C->EditorZoom);
		FVector Side = FVector(.65f, .76f, 0);
		Target += Side * (Close ? 170 * Fov / 48 : 220);
		Cam = P + FVector(Close ? 470 : 760, Close ? -620 : -1000, Close ? 190 : 310);
		float Speed = 4;
		auto New = FMath::VInterpTo(ViewCamera->GetActorLocation(), Cam, Dt, Speed);
		auto R = FMath::RInterpTo(ViewCamera->GetActorRotation(), (Target - Cam).Rotation(), Dt, Speed);
		ViewCamera->SetActorLocationAndRotation(New, R);
		ViewCamera->GetCameraComponent()->SetFieldOfView(FMath::FInterpTo(ViewCamera->GetCameraComponent()->FieldOfView, Fov, Dt, Speed));
	}
}
float UErlingCameraRig::UpdatePitchCameraLead(float Dt)
{
	const AFootballPlayer* Avatar = Controller()->Avatar;
	if (!Avatar)
		return PitchCameraLeadX;
	const bool ShotAction = Avatar->Action == AFootballPlayer::EAction::Kick || Avatar->Action == AFootballPlayer::EAction::Flip;
	const float LeadVelocityX = ShotAction ? Avatar->EntryVelocity.X : Avatar->GetVelocity().X;
	const float DesiredLeadX = FMath::Clamp(LeadVelocityX * .22f, -260.f, 260.f);
	PitchCameraLeadX = FMath::FInterpTo(PitchCameraLeadX, DesiredLeadX, Dt, 4.5f);
	return PitchCameraLeadX;
}
FVector UErlingCameraRig::UpdatePitchShotTarget(float Dt, const FVector& Cam, const FVector& NormalTarget, float Fov)
{
	auto* M = GetWorld()->GetAuthGameMode<AFootballMode>();
	if (!M || !M->Ball)
		return NormalTarget;
	const float Now = GetWorld()->GetTimeSeconds();
	const FVector BallPosition = M->Ball->GetComponentLocation(), Velocity = M->Ball->GetPhysicsLinearVelocity();
	if (M->ShotInFlight && M->LastShot != PitchShotSeen)
	{
		PitchShotSeen = M->LastShot;
		PitchShotTracking = false;
		PitchShotHoldRemaining = 0.f;
		PitchShotGoalX = ErlingPitch::GoalPlaneFor(Velocity.X);
		// Evaluate the goal mouth in the normal player-anchored view at shot release.
		const FRotationMatrix Basis((NormalTarget - Cam).Rotation());
		int32 Width = 0, Height = 0;
		Controller()->GetViewportSize(Width, Height);
		const float Aspect = Width > 0 && Height > 0 ? float(Width) / Height : 16.f / 9.f;
		const float TanHorizontal = FMath::Tan(FMath::DegreesToRadians(Fov * .5f));
		bool GoalVisible = true;
		for (float Side : {-ErlingPitch::GoalPostY, ErlingPitch::GoalPostY})
			for (float Z : {0.f, ErlingPitch::CrossbarZ})
			{
				const FVector Delta = FVector(PitchShotGoalX, Side, Z) - Cam;
				const float Depth = FVector::DotProduct(Delta, Basis.GetUnitAxis(EAxis::X));
				GoalVisible &= Depth > 0.f &&
				               FMath::Abs(FVector::DotProduct(Delta, Basis.GetUnitAxis(EAxis::Y))) <= Depth * TanHorizontal * .95f &&
				               FMath::Abs(FVector::DotProduct(Delta, Basis.GetUnitAxis(EAxis::Z))) <= Depth * TanHorizontal / Aspect * .95f;
			}
		PitchShotTracking = !GoalVisible && !M->Scored && !M->BallHidden && FMath::Abs(Velocity.X) > 150.f;
		PitchShotStarted = Now;
	}
	if (PitchShotTracking && (M->Scored || M->BallHidden || !M->ShotInFlight || Now - PitchShotStarted > 8.f ||
	                             (Now - PitchShotStarted > .4f && (Velocity.Size2D() < 120.f || Velocity.X * PitchShotGoalX < 0.f))))
	{
		PitchShotTracking = false;
		PitchShotHoldRemaining = M->Scored ? 1.4f : .65f;
		PitchShotHoldTarget = FMath::Lerp(BallPosition, FVector(PitchShotGoalX, 0, 130), .12f);
	}
	FVector DesiredOffset = FVector::ZeroVector;
	if (PitchShotTracking)
	{
		const FVector Goal(PitchShotGoalX, 0, 130);
		const FVector Aim = FMath::Lerp(BallPosition + Velocity * .12f, Goal, .12f);
		DesiredOffset = Aim - NormalTarget;
	}
	else if (PitchShotHoldRemaining > 0.f)
	{
		PitchShotHoldRemaining = FMath::Max(0.f, PitchShotHoldRemaining - Dt);
		DesiredOffset = PitchShotHoldTarget - NormalTarget;
	}
	// Only the viewing direction changes; camera translation remains player driven.
	const float Alpha = 1.f - FMath::Exp(-(PitchShotTracking ? 3.5f : 2.8f) * Dt);
	PitchShotOffset = FMath::Lerp(PitchShotOffset, DesiredOffset, Alpha);
	if (!PitchShotTracking && PitchShotOffset.SizeSquared() < 1.f)
		PitchShotOffset = FVector::ZeroVector;
	return NormalTarget + PitchShotOffset;
}
FVector UErlingCameraRig::UpdatePitchViewCenter(float Dt, const FVector& Desired)
{
	auto* M = GetWorld()->GetAuthGameMode<AFootballMode>();
	if (!PitchViewInitialized)
	{
		PitchViewCenter = Desired;
		PitchViewInitialized = true;
		PitchViewReturning = false;
	}
	const float ShotAge = M ? GetWorld()->GetTimeSeconds() - M->LastShot : 0.f;
	const bool LiveShot = M && M->Ball && M->ShotInFlight && !M->Scored && !M->BallHidden && ShotAge < 8.f &&
	                      (ShotAge <= .4f || M->Ball->GetPhysicsLinearVelocity().Size2D() >= 120.f);
	// Freeze the complete camera anchor, including movement lead. The live player
	// pivot may keep updating, but cannot move or tilt the shot's camera frame.
	if (LiveShot || PitchShotTracking || PitchShotHoldRemaining > 0.f)
	{
		PitchViewReturning = true;
		return PitchViewCenter;
	}
	if (PitchViewReturning)
	{
		PitchViewCenter = FMath::Lerp(PitchViewCenter, Desired, 1.f - FMath::Exp(-2.8f * Dt));
		if (PitchViewCenter.Equals(Desired, .1f))
			PitchViewReturning = false;
	}
	else
		PitchViewCenter = Desired;
	return PitchViewCenter;
}
