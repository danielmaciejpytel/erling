#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ErlingReferee.generated.h"

class AFootballController;
class AFootballMode;
class AFootballPlayer;

/**
 * Match rules for the live ball: shot state, goal and miss detection, the score
 * and the delayed reset after a finished attempt. Owned by AFootballMode, which
 * keeps the ball and possession; another game mode can reuse or replace it.
 */
UCLASS(ClassGroup = (Erling))
class ERLING_API UErlingReferee : public UActorComponent
{
	GENERATED_BODY()
public:
	UErlingReferee();

	/** Goal / miss / out-of-world checks for one gameplay frame. May reset the ball. */
	void Update(AFootballController* PC);
	/** A shot left the player's foot. */
	void StartShot(AFootballPlayer* Shooter);
	/** The attempt missed: keep the ball visible and schedule a reset. */
	void HideMiss();

	int32 Goals = 0;
	float ResetAt = 0;
	bool Scored = false;
	bool ShotInFlight = false, BallHidden = false;
	float LastShot = -10;
	FVector PreviousBall = FVector::ZeroVector;
	TWeakObjectPtr<AFootballPlayer> LastShooter;

private:
	AFootballMode* Mode() const;
};
