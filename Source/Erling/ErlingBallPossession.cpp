#include "ErlingBallPossession.h"
#include "ErlingTuning.h"
#include "Football.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

// Ball possession and dribbling, extracted from AFootballMode. The logic below is
// moved unchanged; only access to the mode-owned ball/shot state now goes through
// the owning AFootballMode (Mode->Ball, Mode->ShotInFlight, ...).

UErlingBallPossession::UErlingBallPossession()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UErlingBallPossession::ResetState()
{
	SprintDribbleFoot = NAME_None;
	SprintLeadFoot = NAME_None;
	CarryDribbleFoot = NAME_None;
	BallStopFoot = NAME_None;
	SprintContactUntil = 0;
	SprintReleaseUntil = 0;
	SprintNextTouchAt = 0;
	CarryFootLockUntil = 0;
	BallStopStarted = 0;
	SprintKickPending = false;
	SprintDribbleTouchCount = 0;
	CarryFootSwitchCount = 0;
	DribbleDirection = FVector::ZeroVector;
	SprintReleaseDirection = FVector::ZeroVector;
	LastPossessionDirection = FVector::ForwardVector;
	BallStopAnchor = FVector::ZeroVector;
	PossessionActive = false;
	HadControlInput = false;
	BallStopRequested = false;
	BallStopped = false;
	BallStopGesturePlayed = false;
	LastPossessionWasDigital = false;
	DribbleMinFootClearance = MAX_flt;
	SprintDribbleMinDistance = MAX_flt;
	SprintDribbleMaxDistance = 0;
	DribbleMinBodyAhead = MAX_flt;
	DribbleMinDirectionAhead = MAX_flt;
}

void UErlingBallPossession::ReleaseForShot()
{
	PossessionActive = false;
	BallStopRequested = false;
	BallStopped = false;
	BallStopGesturePlayed = false;
	BallStopFoot = NAME_None;
	BallStopAnchor = FVector::ZeroVector;
}

bool UErlingBallPossession::HasBall(const AFootballPlayer* P) const
{
	const AFootballMode* Mode = GetOwner<AFootballMode>();
	if (!Mode)
		return false;
	if (!P || !Mode->Ball || Mode->Scored || Mode->BallHidden || Mode->ShotInFlight)
		return false;
	const FVector Delta = Mode->Ball->GetComponentLocation() - P->GetActorLocation();
	return Delta.Size2D() < ErlingPossession::HasBallRadius && Delta.Z < -25 && Delta.Z > -125;
}
bool UErlingBallPossession::HasDribbleControl(const AFootballPlayer* P) const
{
	const AFootballMode* Mode = GetOwner<AFootballMode>();
	if (!Mode)
		return false;
	if (!P || !Mode->Ball || Mode->Scored || Mode->BallHidden || Mode->ShotInFlight || P->GetCharacterMovement()->IsFalling())
		return false;
	const FVector Delta = Mode->Ball->GetComponentLocation() - P->GetActorLocation();
	float ControlRadius = ErlingPossession::ControlRadius;
	if (GetWorld()->GetTimeSeconds() < SprintReleaseUntil)
		ControlRadius = ErlingPossession::ReleaseControlRadius;
	else if (BallStopRequested && !SprintReleaseDirection.IsNearlyZero())
		ControlRadius = ErlingPossession::ReleaseControlRadius;
	else if (const auto* PC = Cast<AFootballController>(P->GetController()); PC && PC->Sprint)
		ControlRadius = ErlingPossession::SprintControlRadius;
	return Delta.Size2D() <= ControlRadius && Delta.Z <= -35.f;
}
bool UErlingBallPossession::IsRecoverableSprintTouch(const AFootballPlayer* P, float MaxGap) const
{
	const AFootballMode* Mode = GetOwner<AFootballMode>();
	if (!Mode)
		return false;
	if (!P || !Mode->Ball || Mode->BallHidden || Mode->Scored || Mode->ShotInFlight || SprintReleaseDirection.IsNearlyZero())
		return false;
	const FVector Gap = Mode->Ball->GetComponentLocation() - P->GetActorLocation();
	const float Now = GetWorld()->GetTimeSeconds();
	return Gap.Size2D() < MaxGap && Gap.Z <= -35.f && Now <= SprintReleaseUntil + ErlingPossession::RecoveryWindow;
}
void UErlingBallPossession::ClearSprintReleaseRecovery()
{
	SprintDribbleFoot = NAME_None;
	SprintLeadFoot = NAME_None;
	SprintContactUntil = 0.f;
	SprintReleaseUntil = 0.f;
	SprintNextTouchAt = 0.f;
	SprintKickPending = false;
	SprintReleaseDirection = FVector::ZeroVector;
}
void UErlingBallPossession::Dribble(AFootballPlayer* Player, float Dt)
{
	AFootballMode* Mode = GetOwner<AFootballMode>();
	if (!Mode || !Mode->Ball)
		return;
	if (!Player || Player->IsMovementLocked() || Mode->Scored || Mode->BallHidden ||
	    GetWorld()->GetTimeSeconds() - Mode->LastShot < ErlingPossession::PostShotLockout || Player->GetCharacterMovement()->IsFalling())
		return;
	const float Now = GetWorld()->GetTimeSeconds();
	FVector Pos = Mode->Ball->GetComponentLocation();
	const FVector Base = Player->GetActorLocation();
	FVector To = Pos - Base;
	To.Z = 0;
	if (SprintKickPending)
	{
		// SprintKickPending represents a physical foot contact that is still completing.
		// It must not inherit the wider 420 cm sprint-stop recovery envelope. If the ball
		// has already left any plausible foot-contact radius, that contact no longer exists.
		bool PendingFootContact = To.Size2D() <= 150.f;
		if (Player->GetMesh() && Player->GetMesh()->DoesSocketExist(TEXT("foot_l")) && Player->GetMesh()->DoesSocketExist(TEXT("foot_r")))
		{
			const FVector LeftFoot = Player->GetMesh()->GetSocketLocation(TEXT("foot_l"));
			const FVector RightFoot = Player->GetMesh()->GetSocketLocation(TEXT("foot_r"));
			PendingFootContact = FMath::Min(FVector::Dist2D(Pos, LeftFoot), FVector::Dist2D(Pos, RightFoot)) <= 104.f;
		}
		if (!PendingFootContact)
		{
			PossessionActive = false;
			ClearSprintReleaseRecovery();
		}
	}
	if (!SprintReleaseDirection.IsNearlyZero() && !SprintKickPending && Now >= SprintContactUntil && !IsRecoverableSprintTouch(Player))
	{
		// Sprint-release recovery is a short-lived physical context, not permanent
		// possession metadata. Once its age, distance or playable-height envelope expires,
		// clear it so an old touch cannot resurrect chase/facing later.
		ClearSprintReleaseRecovery();
	}
	// A pending sprint-stop is deliberately allowed to live outside the normal dribble
	// radius. The ball is still free there; keeping the state alive only lets the runner
	// finish physically catching the touch instead of silently cancelling the requested
	// trap as soon as the gap crosses 270 cm.
	const bool PendingSprintStopRecovery = BallStopRequested && !SprintReleaseDirection.IsNearlyZero();
	const float RecoveryAbandonDistance = ErlingPossession::RecoveryAbandonDistance;
	const float Speed = Player->GetVelocity().Size2D();
	if (To.Size() > (PendingSprintStopRecovery ? RecoveryAbandonDistance : ErlingPossession::ReleaseControlRadius) || Pos.Z > Base.Z - 35)
	{
		if (SprintKickPending || !IsRecoverableSprintTouch(Player))
			ClearSprintReleaseRecovery();
		CarryDribbleFoot = NAME_None;
		BallStopFoot = NAME_None;
		DribbleDirection = FVector::ZeroVector;
		PossessionActive = false;
		HadControlInput = false;
		BallStopRequested = false;
		BallStopped = false;
		BallStopGesturePlayed = false;
		Player->CancelBallTrap();
		return;
	}
	const bool SprintReleaseActive = Now < SprintReleaseUntil;
	const auto* PC = Cast<AFootballController>(Player->GetController());
	const bool SprintTouchActive = Now < SprintContactUntil || SprintKickPending;
	const bool Sprinting = SprintReleaseActive || SprintTouchActive || (PC && PC->Sprint && Speed > 560.f);
	const bool BallControl = PC && PC->BallControlHeld && !Sprinting;
	const FVector VelocityDirection = Player->GetVelocity().GetSafeNormal2D();
	FVector DesiredDirection = VelocityDirection;
	bool HasControlInput = false;
	float TurnAmount = 0.f;
	const bool PlayerControlled = PC && (PC->Screen == AFootballController::EScreen::Game ||
	                                        (PC->Screen == AFootballController::EScreen::Settings &&
	                                            PC->SettingsReturn == AFootballController::EScreen::Game && !PC->IsPaused()));
	if (PlayerControlled)
	{
		FVector Input = PC->GetMoveIntentWorld();
		Input.Z = 0;
		if (!Input.IsNearlyZero())
		{
			HasControlInput = true;
			Input.Normalize();
			const float InputDotVelocity = VelocityDirection.IsNearlyZero() ? 1.f : FVector::DotProduct(Input, VelocityDirection);
			TurnAmount = FMath::Clamp((1.f - InputDotVelocity) * .5f, 0.f, 1.f);
			DesiredDirection = Input;
		}
	}
	const bool ControlledEnvelope = HasDribbleControl(Player);
	if (HasControlInput)
	{
		BallStopRequested = false;
		BallStopped = false;
		BallStopGesturePlayed = false;
		BallStopFoot = NAME_None;
		BallStopAnchor = FVector::ZeroVector;
		Player->CancelBallTrap();
		LastPossessionWasDigital = PC && PC->HasDigitalMoveIntent();
	}
	else if (PlayerControlled && HadControlInput && ControlledEnvelope)
	{
		BallStopRequested = true;
		BallStopStarted = Now;
		BallStopped = false;
		BallStopGesturePlayed = false;
	}
	HadControlInput = HasControlInput;
	const float SpeedAlpha = FMath::Clamp((Speed - 180.f) / (765.f - 180.f), 0.f, 1.f);
	const bool SprintReversal = PC && PC->Sprint && PossessionActive && TurnAmount > .35f;
	const float PossessionTurnRate = BallControl ? 660.f : FMath::Max(FMath::Lerp(480.f, 200.f, SpeedAlpha), SprintReversal ? 420.f : 0.f);
	if (DribbleDirection.IsNearlyZero())
		DribbleDirection = !VelocityDirection.IsNearlyZero()         ? VelocityDirection
		                   : !LastPossessionDirection.IsNearlyZero() ? LastPossessionDirection
		                                                             : Player->GetActorForwardVector();
	if (HasControlInput && !DesiredDirection.IsNearlyZero())
	{
		const float NewYaw = FMath::FixedTurn(DribbleDirection.Rotation().Yaw, DesiredDirection.Rotation().Yaw, PossessionTurnRate * Dt);
		DribbleDirection = FRotator(0, NewYaw, 0).Vector();
	}
	else if (!PlayerControlled && !DesiredDirection.IsNearlyZero())
		DribbleDirection = DesiredDirection;
	FVector Direction = DribbleDirection.GetSafeNormal2D();
	if (!BallStopRequested && !BallStopped && !Direction.IsNearlyZero() && ControlledEnvelope)
		LastPossessionDirection = Direction;
	if (Direction.IsNearlyZero())
		Direction = LastPossessionDirection.GetSafeNormal2D();
	if (Direction.IsNearlyZero() || (!BallControl && !ControlledEnvelope && FVector::DotProduct(To.GetSafeNormal(), Direction) < -.25f))
		return;
	// The broad control envelope is useful for movement/recovery, but is not a
	// physical touch. A loose ball, including an expired sprint release, cannot be
	// steered toward a carry anchor until the runner actually reaches it.
	const bool ContactFeet =
	    Player->GetMesh() && Player->GetMesh()->DoesSocketExist(TEXT("foot_l")) && Player->GetMesh()->DoesSocketExist(TEXT("foot_r"));
	const float ContactDistance = ContactFeet ? FMath::Min(FVector::Dist2D(Pos, Player->GetMesh()->GetSocketLocation(TEXT("foot_l"))),
	                                                FVector::Dist2D(Pos, Player->GetMesh()->GetSocketLocation(TEXT("foot_r"))))
	                                          : To.Size2D();
	const bool ReleasedTouch = !SprintReleaseDirection.IsNearlyZero() && !SprintKickPending && Now >= SprintContactUntil &&
	                           !(PossessionActive && BallStopFoot != NAME_None);
	const bool PhysicalContact = ContactDistance <= (ContactFeet ? (ReleasedTouch ? 52.f : 48.f) : 70.f);
	if (Sprinting && ContactFeet)
	{
		SprintDribbleMinDistance = FMath::Min(SprintDribbleMinDistance, To.Size2D());
		SprintDribbleMaxDistance = FMath::Max(SprintDribbleMaxDistance, To.Size2D());
	}
	if ((!PossessionActive || ReleasedTouch || To.Size2D() > 150.f) && !PhysicalContact)
	{
		PossessionActive = false;
		CarryDribbleFoot = NAME_None;
		CarryFootLockUntil = 0.f;
		// Rolling resistance slows a released ball; it never recalls or redirects it.
		if (ReleasedTouch)
			Mode->Ball->SetLinearDamping(Now < SprintReleaseUntil ? .08f : BallStopRequested ? 1.65f : .62f);
		return;
	}
	PossessionActive = true;
	const FVector BodyForward = Player->GetActorForwardVector().GetSafeNormal2D();
	const FVector BodyRight = Player->GetActorRightVector().GetSafeNormal2D();

	const bool HasFeet =
	    Player->GetMesh() && Player->GetMesh()->DoesSocketExist(TEXT("foot_l")) && Player->GetMesh()->DoesSocketExist(TEXT("foot_r"));
	FVector Left = Base, Right = Base;
	constexpr float FootClearance = ErlingPossession::FootClearance;
	if (HasFeet)
	{
		Left = Player->GetMesh()->GetSocketLocation(TEXT("foot_l"));
		Right = Player->GetMesh()->GetSocketLocation(TEXT("foot_r"));
		if (!SprintReleaseActive)
		{
			bool Corrected = false;
			for (const FVector Foot : {Left, Right})
			{
				FVector D = Pos - Foot;
				D.Z = 0;
				const float L = D.Size();
				if (L < FootClearance)
				{
					Pos += (L > 1.f ? D / L : Direction) * (FootClearance - L);
					Corrected = true;
				}
			}
			// The two foot circles alone leave a hole between the legs. Keep a controlled
			// ball out of that central corridor as well, otherwise a tight turn can visually
			// put the sphere underneath the pelvis while technically clearing both sockets.
			FVector Relative = Pos - Base;
			Relative.Z = 0;
			const FVector Side = FVector::CrossProduct(FVector::UpVector, Direction).GetSafeNormal();
			const float Forward = FVector::DotProduct(Relative, Direction), Lateral = FVector::DotProduct(Relative, Side);
			if (ControlledEnvelope && Forward > -18.f && Forward < 38.f && FMath::Abs(Lateral) < 44.f)
			{
				Pos += Direction * (38.f - Forward);
				Corrected = true;
			}
			if (ControlledEnvelope && HasControlInput && Relative.Size2D() < 100.f)
			{
				const float DirectionAhead = FVector::DotProduct(Pos - Base, Direction);
				if (DirectionAhead < 18.f)
				{
					Pos += Direction * (18.f - DirectionAhead);
					Corrected = true;
				}
				if (!BodyForward.IsNearlyZero())
				{
					const float BodyAhead = FVector::DotProduct(Pos - Base, BodyForward);
					if (BodyAhead < 8.f)
					{
						Pos += BodyForward * (8.f - BodyAhead);
						Corrected = true;
					}
				}
			}
			if (Corrected)
				Mode->Ball->SetWorldLocation(
				    FVector(Pos.X, Pos.Y, Mode->Ball->GetComponentLocation().Z), false, nullptr, ETeleportType::TeleportPhysics);
		}
		DribbleMinFootClearance = FMath::Min(DribbleMinFootClearance, FMath::Min(FVector::Dist2D(Pos, Left), FVector::Dist2D(Pos, Right)));
	}

	Mode->ShotInFlight = false;
	auto KeepTargetOutsideFeet = [&](FVector Target, float Radius)
	{
		if (!HasFeet)
			return Target;
		for (const FVector Foot : {Left, Right})
		{
			FVector D = Target - Foot;
			D.Z = 0;
			const float L = D.Size();
			if (L < Radius)
				Target += (L > 1.f ? D / L : Direction) * (Radius - L);
		}
		return Target;
	};
	auto KeepTargetOutsideBody = [&](FVector Target)
	{
		FVector Relative = Target - Base;
		Relative.Z = 0;
		const float Forward = FVector::DotProduct(Relative, Direction);
		if (Forward < 52.f)
			Target += Direction * (52.f - Forward);
		if (!BodyForward.IsNearlyZero())
		{
			Relative = Target - Base;
			Relative.Z = 0;
			const float BodyAhead = FVector::DotProduct(Relative, BodyForward);
			if (BodyAhead < 26.f)
				Target += BodyForward * (26.f - BodyAhead);
		}
		return Target;
	};
	auto AvoidFeet = [&](FVector V, float MaxPlanarSpeed)
	{
		if (!HasFeet || Dt <= SMALL_NUMBER)
			return V;
		FVector Next = Pos + V * Dt;
		for (const FVector Foot : {Left, Right})
		{
			FVector D = Next - Foot;
			D.Z = 0;
			const float L = D.Size();
			if (L < FootClearance + 1.f)
				Next += (L > 1.f ? D / L : Direction) * (FootClearance + 1.f - L);
		}
		FVector Relative = Next - Base;
		Relative.Z = 0;
		if (ControlledEnvelope && !SprintReleaseActive && Relative.Size2D() < 150.f)
		{
			const float Forward = FVector::DotProduct(Relative, Direction);
			if (Forward < 30.f)
				Next += Direction * (30.f - Forward);
			if (!BodyForward.IsNearlyZero())
			{
				Relative = Next - Base;
				Relative.Z = 0;
				const float BodyAhead = FVector::DotProduct(Relative, BodyForward);
				if (BodyAhead < 16.f)
					Next += BodyForward * (16.f - BodyAhead);
			}
		}
		FVector Planar = (Next - Pos) / Dt;
		Planar.Z = 0;
		Planar = Planar.GetClampedToMaxSize(MaxPlanarSpeed);
		V.X = Planar.X;
		V.Y = Planar.Y;
		return V;
	};
	const float PlayerGap = FVector::Dist2D(Pos, Base);

	// Releasing the stick/key may request a stop while the ball is still in a genuine
	// sprint release. Remember that request, but do not recall or curve the free ball.
	// Trapping may begin only after the release expires or the player has really caught
	// back up into close-touch range.
	const bool SprintStopRecovery = PlayerControlled && !HasControlInput && BallStopRequested && !SprintReleaseDirection.IsNearlyZero();
	if (SprintStopRecovery && PlayerGap > 125.f)
	{
		const FVector V = Mode->Ball->GetPhysicsLinearVelocity();
		// While the touch is genuinely released keep its original low damping and heading.
		// Once that release window ends, natural rolling resistance can slow the free ball
		// enough for the still-coasting player to catch it. No XY steering is introduced.
		Mode->Ball->SetLinearDamping(SprintReleaseActive ? .08f : 1.65f);
		Mode->Ball->SetPhysicsAngularVelocityInRadians(FVector(-V.Y / 22, V.X / 22, 0));
		return;
	}

	// Releasing movement while still in possession is an explicit trap, not a point
	// where physics is abandoned. The player catches the rolling ball with one foot,
	// kills its momentum, then holds it stationary until movement or a shot resumes.
	if (PlayerControlled && !HasControlInput && ControlledEnvelope && (BallStopRequested || BallStopped))
	{
		SprintReleaseUntil = 0;
		SprintContactUntil = 0;
		SprintKickPending = false;
		SprintDribbleFoot = NAME_None;
		SprintLeadFoot = NAME_None;
		if (BallStopFoot == NAME_None && HasFeet)
			BallStopFoot = FVector::Dist2D(Pos, Left) <= FVector::Dist2D(Pos, Right) ? TEXT("foot_l") : TEXT("foot_r");
		const FVector Foot = BallStopFoot == TEXT("foot_l") ? Left : Right;
		float StopSide = 0.f;
		if (HasFeet && !BodyRight.IsNearlyZero())
			StopSide = FMath::Clamp(FVector::DotProduct(Foot - Base, BodyRight), -22.f, 22.f);
		FVector Target = Base + Direction * 56.f + BodyRight * StopSide * .72f;
		Target.Z = Pos.Z;
		Target = KeepTargetOutsideFeet(KeepTargetOutsideBody(Target), FootClearance + 2.f);
		if (BallStopped)
		{
			// Once trapped, BallStopAnchor is stored in player-local space. This makes the
			// held ball orbit with a Turn_Step instead of remaining nailed to its old world
			// position and ending up behind the player after an in-place camera turn.
			if (BallStopAnchor.IsNearlyZero())
				BallStopAnchor = Player->GetActorTransform().InverseTransformPosition(Target);
			const FVector HoldWorld = Player->GetActorTransform().TransformPosition(BallStopAnchor);
			if (!BodyForward.IsNearlyZero())
				DribbleDirection = BodyForward;
			Mode->Ball->SetWorldLocation(HoldWorld, false, nullptr, ETeleportType::TeleportPhysics);
			Mode->Ball->SetLinearDamping(2.f);
			Mode->Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);
			Mode->Ball->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
			return;
		}
		FVector C = Target - Pos;
		C.Z = 0;
		const float TargetDistance = C.Size();
		if (!BallStopGesturePlayed && HasFeet && TargetDistance < 92.f && Speed < 220.f)
		{
			Player->StartBallTrap(BallStopFoot);
			BallStopGesturePlayed = Player->BallTrapActive;
		}
		const bool SprintRecoveryTrap = !SprintReleaseDirection.IsNearlyZero();
		const float CatchGain = TargetDistance > 90.f ? (SprintRecoveryTrap ? 7.f : 10.f) : (SprintRecoveryTrap ? 12.f : 24.f);
		const float PlayerCarry = SprintRecoveryTrap ? .88f : .28f;
		// A sprint trap starts while the runner still has meaningful forward speed. Match
		// most of that speed first, then bleed the remaining target error away. Pulling only
		// toward the anchor here can reverse the ball through the player's legs and create
		// the exact snap the trap is supposed to remove.
		FVector Desired = (Player->GetVelocity() * PlayerCarry + C * CatchGain).GetClampedToMaxSize(FMath::Max(180.f, Speed + 120.f));
		const float CatchInterp = TargetDistance > 90.f ? (SprintRecoveryTrap ? 14.f : 18.f) : (SprintRecoveryTrap ? 18.f : 32.f);
		FVector V = FMath::VInterpTo(Mode->Ball->GetPhysicsLinearVelocity(), Desired, Dt, CatchInterp);
		V.Z = FMath::Clamp(V.Z, -120.f, 20.f);
		V = AvoidFeet(V, FMath::Max(180.f, Speed + 120.f));
		Mode->Ball->SetLinearDamping(.9f);
		Mode->Ball->SetPhysicsLinearVelocity(V);
		Mode->Ball->SetPhysicsAngularVelocityInRadians(FVector(-V.Y / 22, V.X / 22, 0));
		const bool GestureContact = !HasFeet || (BallStopGesturePlayed && (!Player->BallTrapActive || Player->BallTrapElapsed > .07f));
		if (TargetDistance < 10.f && Speed < 38.f && V.Size2D() < 125.f && GestureContact)
		{
			BallStopped = true;
			BallStopRequested = false;
			BallStopAnchor = Player->GetActorTransform().InverseTransformPosition(Target);
			Mode->Ball->SetWorldLocation(Target, false, nullptr, ETeleportType::TeleportPhysics);
			Mode->Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);
			Mode->Ball->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
			Mode->Ball->SetLinearDamping(2.f);
		}
		return;
	}
	if (Speed < 25.f && !(BallControl && HasControlInput))
		return;
	if (Sprinting && HasFeet)
	{
		CarryDribbleFoot = NAME_None;
		CarryFootLockUntil = 0;
		const float LeftFootDistance = FVector::Dist2D(Pos, Left), RightFootDistance = FVector::Dist2D(Pos, Right);
		const FName NearestFoot = LeftFootDistance <= RightFootDistance ? TEXT("foot_l") : TEXT("foot_r");
		const float NearestFootDistance = FMath::Min(LeftFootDistance, RightFootDistance);
		const bool StrongTurn = HasControlInput && ControlledEnvelope && TurnAmount > .5f && PlayerGap < 120.f;
		const float SprintFootContactRadius = FootClearance + 14.f;
		const bool ReleaseFootRecontact = SprintReleaseActive && HasControlInput && ControlledEnvelope && PlayerGap < 118.f &&
		                                  NearestFootDistance < SprintFootContactRadius;
		// A released sprint touch stays genuinely free unless the runner physically catches
		// it again. On a hard cut, a ball that has returned to a real foot-contact envelope
		// can be planted immediately instead of being allowed to pass behind the silhouette
		// until an arbitrary release timer expires.
		const bool PhysicalTurnContact =
		    (StrongTurn && NearestFootDistance < SprintFootContactRadius || ReleaseFootRecontact) && Now >= SprintNextTouchAt;
		if (PhysicalTurnContact && Now < SprintReleaseUntil)
		{
			SprintReleaseUntil = 0.f;
			SprintDribbleFoot = NearestFoot;
			SprintLeadFoot = NearestFoot;
			SprintContactUntil = Now + .095f;
			SprintKickPending = true;
			SprintNextTouchAt = Now + .16f;
		}
		if (Now < SprintReleaseUntil)
		{
			// Once the ball has been pushed ahead it is physically free. Player input may
			// curve the runner, but must never bend the already-released ball in mid-roll.
			FVector V = Mode->Ball->GetPhysicsLinearVelocity();
			Mode->Ball->SetLinearDamping(.08f);
			Mode->Ball->SetPhysicsAngularVelocityInRadians(FVector(-V.Y / 22, V.X / 22, 0));
			return;
		}
		if (PhysicalTurnContact && !SprintKickPending && Now >= SprintContactUntil)
		{
			SprintDribbleFoot = NearestFoot;
			SprintLeadFoot = NearestFoot;
			SprintContactUntil = Now + .095f;
			SprintKickPending = true;
			SprintNextTouchAt = Now + .16f;
		}
		const FName Lead =
		    FVector::DotProduct(Left - Base, Direction) >= FVector::DotProduct(Right - Base, Direction) ? TEXT("foot_l") : TEXT("foot_r");
		const FVector LeadFoot = Lead == TEXT("foot_l") ? Left : Right;
		const bool LeadChanged = Lead != SprintLeadFoot;
		SprintLeadFoot = Lead;
		const float TouchInterval = FMath::Lerp(.24f, .16f, TurnAmount), ReleaseDuration = FMath::Lerp(.25f, .17f, TurnAmount),
		            ReleasePush = FMath::Lerp(225.f, 180.f, TurnAmount);
		if (Now >= SprintNextTouchAt && Now >= SprintReleaseUntil && LeadChanged && FVector::Dist2D(Pos, LeadFoot) < 92.f)
		{
			SprintDribbleFoot = Lead;
			SprintContactUntil = Now + .085f;
			SprintKickPending = true;
			SprintNextTouchAt = Now + TouchInterval;
		}
		if (Now < SprintContactUntil && SprintDribbleFoot != NAME_None)
		{
			const FVector Foot = SprintDribbleFoot == TEXT("foot_l") ? Left : Right;
			FVector Target = KeepTargetOutsideFeet(KeepTargetOutsideBody(Foot + Direction * FootClearance), FootClearance + 2.f);
			Target.Z = Pos.Z;
			FVector C = Target - Pos;
			C.Z = 0;
			FVector Desired = (Player->GetVelocity() + C * 18.f).GetClampedToMaxSize(Speed + 180.f);
			FVector V = FMath::VInterpTo(Mode->Ball->GetPhysicsLinearVelocity(), Desired, Dt, 30.f);
			V.Z = FMath::Clamp(Mode->Ball->GetPhysicsLinearVelocity().Z, -180.f, 30.f);
			V = AvoidFeet(V, Speed + 180.f);
			Mode->Ball->SetLinearDamping(.35f);
			Mode->Ball->SetPhysicsLinearVelocity(V);
			Mode->Ball->SetPhysicsAngularVelocityInRadians(FVector(-V.Y / 22, V.X / 22, 0));
			return;
		}
		if (SprintKickPending && Now >= SprintContactUntil)
		{
			SprintKickPending = false;
			PossessionActive = false;
			SprintReleaseUntil = Now + ReleaseDuration;
			SprintDribbleTouchCount++;
			SprintReleaseDirection = Direction;
			const float Along = FMath::Max(0.f, FVector::DotProduct(Player->GetVelocity(), SprintReleaseDirection));
			FVector V = SprintReleaseDirection * (Along + ReleasePush);
			V.Z = 0;
			Mode->Ball->SetLinearDamping(.08f);
			Mode->Ball->SetPhysicsLinearVelocity(V);
			Mode->Ball->SetPhysicsAngularVelocityInRadians(FVector(-V.Y / 22, V.X / 22, 0));
			return;
		}
		const bool ReleasedBallStillFar = !SprintReleaseDirection.IsNearlyZero() && !SprintKickPending && PlayerGap > 125.f;
		if (ReleasedBallStillFar)
		{
			// Expiring the release timer is not a physical touch. If the ball is still well
			// ahead of the runner, keep it rolling on its own trajectory until the player
			// actually catches up. Steering it toward Base+Direction here was the hidden
			// "magnet" that could make a ball disappear behind the player and then snap back
			// into a plausible dribble position during rapid sprint turns.
			const FVector V = Mode->Ball->GetPhysicsLinearVelocity();
			Mode->Ball->SetLinearDamping(.62f);
			Mode->Ball->SetPhysicsAngularVelocityInRadians(FVector(-V.Y / 22, V.X / 22, 0));
			return;
		}
		const FVector Target = KeepTargetOutsideFeet(KeepTargetOutsideBody(Base + Direction * 72.f), FootClearance + 2.f);
		FVector C = Target - Pos;
		C.Z = 0;
		FVector Desired = (Direction * Speed + C * 6.5f).GetClampedToMaxSize(Speed + 120.f);
		FVector V = FMath::VInterpTo(Mode->Ball->GetPhysicsLinearVelocity(), Desired, Dt, 12.f);
		V.Z = FMath::Clamp(Mode->Ball->GetPhysicsLinearVelocity().Z, -180.f, 30.f);
		V = AvoidFeet(V, Speed + 120.f);
		Mode->Ball->SetLinearDamping(.22f);
		Mode->Ball->SetPhysicsLinearVelocity(V);
		Mode->Ball->SetPhysicsAngularVelocityInRadians(FVector(-V.Y / 22, V.X / 22, 0));
		return;
	}

	SprintDribbleFoot = NAME_None;
	SprintLeadFoot = NAME_None;
	SprintContactUntil = 0;
	SprintReleaseUntil = 0;
	SprintReleaseDirection = FVector::ZeroVector;
	SprintKickPending = false;
	// The menu background is intentionally a softer, more cinematic carry. Gameplay
	// tuning became much tighter to solve under-body/behind-player defects, but applying
	// that same high-gain controller to the autonomous demo makes the ball look glued to
	// the runner's boots. Keep the old rolling run feel here while retaining predictive
	// foot avoidance.
	if (!PlayerControlled)
	{
		CarryDribbleFoot = NAME_None;
		CarryFootLockUntil = 0;
		FVector Target = Base + Direction * 90.f;
		if (HasFeet)
		{
			const FVector Foot = FVector::Dist2D(Pos, Left) <= FVector::Dist2D(Pos, Right) ? Left : Right;
			Target = FMath::Lerp(Target, Foot + Direction * FootClearance, .55f);
			Target = KeepTargetOutsideFeet(KeepTargetOutsideBody(Target), FootClearance + 1.f);
		}
		FVector C = Target - Pos;
		C.Z = 0;
		FVector Desired = (Player->GetVelocity() + C * 11.5f).GetClampedToMaxSize(Speed + 190.f);
		FVector V = FMath::VInterpTo(Mode->Ball->GetPhysicsLinearVelocity(), Desired, Dt, 20.f);
		V.Z = FMath::Clamp(Mode->Ball->GetPhysicsLinearVelocity().Z, -180.f, 30.f);
		V = AvoidFeet(V, Speed + 190.f);
		Mode->Ball->SetLinearDamping(.3f);
		Mode->Ball->SetPhysicsLinearVelocity(V);
		Mode->Ball->SetPhysicsAngularVelocityInRadians(FVector(-V.Y / 22, V.X / 22, 0));
		return;
	}
	const float PlayerDistance = BallControl ? 50.f : 72.f;
	FVector Target = Base + Direction * PlayerDistance;
	if (HasFeet)
	{
		const float BallSide = BodyRight.IsNearlyZero() ? 0.f : FVector::DotProduct(Pos - Base, BodyRight);
		FName Preferred = CarryDribbleFoot;
		if (FMath::Abs(BallSide) > 10.f)
		{
			const float LeftSide = FVector::DotProduct(Left - Base, BodyRight), RightSide = FVector::DotProduct(Right - Base, BodyRight);
			Preferred = FMath::Abs(BallSide - LeftSide) <= FMath::Abs(BallSide - RightSide) ? TEXT("foot_l") : TEXT("foot_r");
		}
		if (CarryDribbleFoot == NAME_None)
		{
			CarryDribbleFoot = FVector::Dist2D(Pos, Left) <= FVector::Dist2D(Pos, Right) ? TEXT("foot_l") : TEXT("foot_r");
			CarryFootLockUntil = Now + .24f;
		}
		if (Now >= CarryFootLockUntil && Preferred != CarryDribbleFoot)
		{
			const FVector Current = CarryDribbleFoot == TEXT("foot_l") ? Left : Right, Other = Preferred == TEXT("foot_l") ? Left : Right;
			const float Ahead = FVector::DotProduct(Pos - Base, Direction), OtherDistance = FVector::Dist2D(Pos, Other),
			            CurrentDistance = FVector::Dist2D(Pos, Current);
			if (Ahead > 24.f && OtherDistance < 72.f && OtherDistance + 12.f < CurrentDistance)
			{
				CarryDribbleFoot = Preferred;
				CarryFootLockUntil = Now + (BallControl ? .22f : .30f);
				CarryFootSwitchCount++;
			}
		}
		const FVector Foot = CarryDribbleFoot == TEXT("foot_l") ? Left : Right;
		const float FootSide = BodyRight.IsNearlyZero() ? 0.f : FMath::Clamp(FVector::DotProduct(Foot - Base, BodyRight), -26.f, 26.f);
		const FVector StableTarget = Base + Direction * PlayerDistance + BodyRight * FootSide * .62f;
		const FVector FootTarget = Foot + Direction * (FootClearance + 4.f);
		Target = FMath::Lerp(StableTarget, FootTarget, BallControl ? .48f : .30f);
		Target = KeepTargetOutsideFeet(KeepTargetOutsideBody(Target), FootClearance + 2.f);
	}
	FVector C = Target - Pos;
	C.Z = 0;
	const FVector CarryVelocity =
	    BallControl ? Direction * FMath::Max(150.f, Speed) : FMath::Lerp(Player->GetVelocity(), Direction * Speed, .45f);
	const float DirectionAhead = FVector::DotProduct(Pos - Base, Direction);
	const float BehindAlpha = FMath::Clamp((34.f - DirectionAhead) / 70.f, 0.f, 1.f);
	const float CorrectionGain = (BallControl ? 30.f : 18.f) + TurnAmount * 8.f + BehindAlpha * 12.f;
	const float ExtraSpeed = (BallControl ? 240.f : 220.f) + BehindAlpha * 100.f;
	const float FollowSpeed = (BallControl ? 42.f : 31.f) + TurnAmount * 7.f + BehindAlpha * 9.f;
	FVector Desired = (CarryVelocity + C * CorrectionGain).GetClampedToMaxSize(Speed + ExtraSpeed);
	FVector V = FMath::VInterpTo(Mode->Ball->GetPhysicsLinearVelocity(), Desired, Dt, FollowSpeed);
	V.Z = FMath::Clamp(Mode->Ball->GetPhysicsLinearVelocity().Z, -180.f, 30.f);
	V = AvoidFeet(V, Speed + ExtraSpeed);
	if (ControlledEnvelope && !SprintReleaseActive)
	{
		const FVector Relative = Pos - Base;
		DribbleMinDirectionAhead = FMath::Min(DribbleMinDirectionAhead, FVector::DotProduct(Relative, Direction));
		if (!BodyForward.IsNearlyZero())
			DribbleMinBodyAhead = FMath::Min(DribbleMinBodyAhead, FVector::DotProduct(Relative, BodyForward));
	}
	Mode->Ball->SetLinearDamping(BallControl ? .42f : .3f);
	Mode->Ball->SetPhysicsLinearVelocity(V);
	Mode->Ball->SetPhysicsAngularVelocityInRadians(FVector(-V.Y / 22, V.X / 22, 0));
}
