#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ErlingCameraRig.generated.h"

class AFootballController;
class AFootballMode;

/**
 * Gameplay, menu and wardrobe camera. Owned by AFootballController, which keeps
 * the view camera actor, look input (Yaw/Pitch) and saved camera mode; this
 * component only decides where that camera sits and looks each frame.
 */
UCLASS(ClassGroup = (Erling))
class ERLING_API UErlingCameraRig : public UActorComponent
{
	GENERATED_BODY()
public:
	UErlingCameraRig();

	void Update(float Dt, bool bGameplayView, bool bEditorView, AFootballMode* GameplayMode, const FVector& P);
	float UpdatePitchCameraLead(float Dt);
	FVector UpdatePitchViewCenter(float Dt, const FVector& Desired);
	FVector UpdatePitchShotTarget(float Dt, const FVector& Cam, const FVector& NormalTarget, float Fov);

	FVector CameraPivot = FVector::ZeroVector;
	bool CameraPivotInitialized = false;
	float PitchCameraLeadX = 0;
	FVector PitchShotOffset = FVector::ZeroVector;
	FVector PitchViewCenter = FVector::ZeroVector;
	bool PitchViewInitialized = false;
	bool PitchViewReturning = false;
	float PitchShotSeen = -100.f, PitchShotStarted = 0.f, PitchShotGoalX = 0.f;
	bool PitchShotTracking = false;
	float PitchShotHoldRemaining = 0.f;
	FVector PitchShotHoldTarget = FVector::ZeroVector;

private:
	AFootballController* Controller() const;
};
