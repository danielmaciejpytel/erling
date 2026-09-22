#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ErlingTuning.h"
#include "ErlingBallPossession.generated.h"

class AFootballPlayer;
class AFootballMode;

/**
 * Ball possession / dribbling state and logic. Owned by AFootballMode, which keeps
 * the physical ball and shot state; this component only decides how the player
 * carries, releases, recovers and traps it.
 */
UCLASS(ClassGroup = (Erling))
class ERLING_API UErlingBallPossession : public UActorComponent
{
	GENERATED_BODY()
public:
	UErlingBallPossession();

	void Dribble(AFootballPlayer* Player, float Dt);
	bool HasBall(const AFootballPlayer* Player) const;
	bool HasDribbleControl(const AFootballPlayer* Player) const;
	bool IsRecoverableSprintTouch(const AFootballPlayer* Player, float MaxGap = ErlingPossession::RecoveryAbandonDistance) const;
	void ClearSprintReleaseRecovery();
	/** Full reset used when the ball is placed back on the spot. */
	void ResetState();
	/** Clears possession and any pending trap when a shot leaves the foot. */
	void ReleaseForShot();

	// Sprint touch cycle: contact -> release -> chase.
	FName SprintDribbleFoot = NAME_None, SprintLeadFoot = NAME_None, CarryDribbleFoot = NAME_None;
	float SprintContactUntil = 0, SprintReleaseUntil = 0, SprintNextTouchAt = 0;
	float CarryFootLockUntil = 0;
	FVector DribbleDirection = FVector::ZeroVector, SprintReleaseDirection = FVector::ZeroVector;
	FVector LastPossessionDirection = FVector::ForwardVector;
	bool SprintKickPending = false, PossessionActive = false, HadControlInput = false, LastPossessionWasDigital = false;

	// Releasing movement while in possession traps the ball under one foot.
	float BallStopStarted = 0;
	FVector BallStopAnchor = FVector::ZeroVector;
	FName BallStopFoot = NAME_None;
	bool BallStopRequested = false, BallStopped = false, BallStopGesturePlayed = false;

	// Diagnostics read by the runtime checks.
	float DribbleMinFootClearance = MAX_flt, SprintDribbleMinDistance = MAX_flt, SprintDribbleMaxDistance = 0;
	float DribbleMinBodyAhead = MAX_flt, DribbleMinDirectionAhead = MAX_flt;
	int32 SprintDribbleTouchCount = 0, CarryFootSwitchCount = 0;
};
