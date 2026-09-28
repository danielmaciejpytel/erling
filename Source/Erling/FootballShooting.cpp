#include "Football.h"
#include "ErlingAnimation.h"
#include "ErlingInterface.h"
#include "ErlingTuning.h"
#include "ErlingStylizedGoal.h"
#include "HAL/PlatformProcess.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "HAL/IConsoleManager.h"
#include "SkeletalRenderPublic.h"
#include "TimerManager.h"
#include "AudioDevice.h"
#include "UnrealClient.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#endif
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
void AFootballController::StartCharge()
{
	if (Screen != EScreen::Game || !Avatar || !Avatar->CanAct() || GetWorld()->GetTimeSeconds() < KickCooldown)
		return;
	auto* M = GetWorld()->GetAuthGameMode<AFootballMode>();
	if (!M || !M->Ball || M->Referee->BallHidden || M->Referee->Scored || M->Referee->ShotInFlight)
		return;
	const float Now = GetWorld()->GetTimeSeconds();
	const bool RecoverableSprintTouch = Sprint && M->IsRecoverableSprintTouch(Avatar);
	if (!M->HasBall(Avatar) && !M->HasDribbleControl(Avatar) && !RecoverableSprintTouch)
		return;
	ShotBufferActive = false;
	ShotChargeArmed = true;
	Charging = true;
	ChargeStarted = Now;
	ShotChargeAimDirection = Avatar->GetActorForwardVector().GetSafeNormal2D();
}
float AFootballController::GetShotChargePower() const
{
	return Charging ? ErlingShot::PowerFromHoldSeconds(GetWorld()->GetTimeSeconds() - ChargeStarted) : ShotBufferSeconds;
}
void AFootballController::ReleaseCharge()
{
	if (!Charging)
		return;
	const float Now = GetWorld()->GetTimeSeconds();
	const float Seconds = GetShotChargePower();
	const FVector ChargedAim = ShotChargeAimDirection;
	Charging = false;
	if (Screen != EScreen::Game || !ShotChargeArmed)
	{
		ShotChargeArmed = false;
		return;
	}
	ShotChargeArmed = false;
	auto* M = GetWorld()->GetAuthGameMode<AFootballMode>();
	if (M && M->HasBall(Avatar) && Avatar->CanAct() && Now >= KickCooldown)
	{
		FireShot(Seconds, ChargedAim);
		if (PendingShot)
			return;
	}
	const bool RecoverableAtRelease =
	    M && Avatar->CanAct() && (M->HasDribbleControl(Avatar) || (Sprint && M->IsRecoverableSprintTouch(Avatar)));
	if (!RecoverableAtRelease)
	{
		ShotBufferActive = false;
		ShotBufferAimDirection = FVector::ZeroVector;
		return;
	}
	// The user already committed the shot input. A temporary sprint touch / loose-ball
	// phase must not eat that command. Keep the selected power and execute at the next
	// genuine ball contact, but only for a short football-action window.
	ShotBufferActive = true;
	ShotBufferStarted = Now;
	ShotBufferSeconds = Seconds;
	ShotBufferAimDirection = ChargedAim;
}
void AFootballController::Kick()
{
	FireShot(.5f);
}
FErlingShotEvaluation AFootballController::EvaluateShot(float PowerSeconds, FVector AimOverride) const
{
	FErlingShotEvaluation Shot;
	auto* M = GetWorld()->GetAuthGameMode<AFootballMode>();
	if (!Avatar || !M || !M->Ball)
		return Shot;
	const EScreen ActiveScreen = Screen == EScreen::Settings ? SettingsReturn : Screen;
	const bool Demo = ActiveScreen == EScreen::Main || ActiveScreen == EScreen::Credits;
	const FVector BallPosition = M->Ball->GetComponentLocation();
	FVector ShotDirection = !AimOverride.IsNearlyZero() ? AimOverride.GetSafeNormal2D() : Avatar->GetActorForwardVector();
	const FVector CurrentIntent = !Demo ? GetMoveIntentWorld() : FVector::ZeroVector;
	const bool CurrentDigital = !Demo && HasDigitalMoveIntent();
	const bool UseRememberedDirection = !Demo && M->Possession->BallStopped && CurrentIntent.IsNearlyZero() &&
	                                    M->HasDribbleControl(Avatar) && !M->Possession->LastPossessionDirection.IsNearlyZero();
	const bool DigitalShotIntent = CurrentDigital || (UseRememberedDirection && M->Possession->LastPossessionWasDigital);
	if (CurrentDigital)
	{
		if (!CurrentIntent.IsNearlyZero())
			ShotDirection = CurrentIntent;
	}
	else if (UseRememberedDirection)
		ShotDirection = M->Possession->LastPossessionDirection.GetSafeNormal2D();
	Shot.Velocity = AFootballMode::ShotVelocity(BallPosition, ShotDirection, PowerSeconds);
	// Digital WASD has only eight directions. Treat those directions as shot intent,
	// not literal 45-degree ballistics: preserve the selected side but compress it
	// into the mouth of the goal, similar to an assisted football-game shot model.
	const bool GamepadShotIntent = !Demo && bUsingGamepadInput && CurrentDigital;
	const bool AssistedShotIntent = DigitalShotIntent || GamepadShotIntent;
	const float AimAssistStrength = DigitalShotIntent ? 1.f : .35f;
	if (!Demo && AssistedShotIntent && FMath::Abs(Shot.Velocity.X) > 1.f)
	{
		const float GoalX = ErlingPitch::GoalLineFor(Shot.Velocity.X);
		const FVector GoalDirection = (FVector(GoalX, 0, BallPosition.Z) - BallPosition).GetSafeNormal2D();
		const FVector RawDirection = Shot.Velocity.GetSafeNormal2D();
		if (FVector::DotProduct(RawDirection, GoalDirection) > .25f)
		{
			const float RawFlight = (GoalX - BallPosition.X) / Shot.Velocity.X;
			if (RawFlight > 0.f)
			{
				const float RawSide = BallPosition.Y + Shot.Velocity.Y * RawFlight;
				constexpr float AimMargin = 292.f;
				const float FullAssistedSide = RawSide * AimMargin / FMath::Sqrt(RawSide * RawSide + AimMargin * AimMargin);
				const float AssistedSide = FMath::Lerp(RawSide, FullAssistedSide, AimAssistStrength);
				const FVector AssistedDirection = (FVector(GoalX, AssistedSide, BallPosition.Z) - BallPosition).GetSafeNormal2D();
				Shot.Velocity = AFootballMode::ShotVelocity(BallPosition, AssistedDirection, PowerSeconds);
			}
		}
	}
	float CrossingHeight = -1, CrossingSide = MAX_flt;
	bool HasCrossing = false;
	if (FMath::Abs(Shot.Velocity.X) > 1)
	{
		const float GoalX = ErlingPitch::GoalLineFor(Shot.Velocity.X);
		const float Flight = (GoalX - BallPosition.X) / Shot.Velocity.X;
		if (Flight > 0)
		{
			CrossingHeight = BallPosition.Z + Shot.Velocity.Z * Flight + .5f * GetWorld()->GetGravityZ() * Flight * Flight;
			CrossingSide = BallPosition.Y + Shot.Velocity.Y * Flight;
			HasCrossing = true;
		}
	}
	Shot.bHasAim = CrossingHeight >= 0 && FMath::Abs(CrossingSide) < 5000;
	Shot.AimTarget = FVector(ErlingPitch::GoalLineFor(Shot.Velocity.X), CrossingSide, CrossingHeight);
	Shot.Kind = ErlingShot::Classify(PowerSeconds, Shot.Velocity.Size2D(), HasCrossing, CrossingHeight);
	return Shot;
}
void AFootballController::FireShot(float Seconds, FVector AimOverride)
{
	auto* M = GetWorld()->GetAuthGameMode<AFootballMode>();
	const EScreen ActiveScreen = Screen == EScreen::Settings ? SettingsReturn : Screen;
	const bool Demo = ActiveScreen == EScreen::Main || ActiveScreen == EScreen::Credits;
	if ((ActiveScreen != EScreen::Game && !Demo) || !Avatar || !M || !Avatar->CanAct() || !M->HasBall(Avatar))
		return;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < KickCooldown)
		return;
	const FVector BallPosition = M->Ball->GetComponentLocation();
	const FErlingShotEvaluation Shot = EvaluateShot(Seconds, AimOverride);
	PendingVelocity = Shot.Velocity;
	PendingHasAim = Shot.bHasAim;
	PendingAimTarget = Shot.AimTarget;
	const bool Aimed = FMath::Abs(PendingAimTarget.Y) < ErlingPitch::AimedHalfWidth;
	const bool AboveBar = Aimed && Seconds >= 1.35f && PendingAimTarget.Z - 22 > 269;
	// Perfect under-bar timing immediately selects the flip, regardless of whether the shot eventually scores.
	const bool Flip = !Demo && Shot.Kind == EErlingShotKind::UnderBar;
	PendingTrip = !Demo && AboveBar && FMath::FRand() < TripChance;
	const FVector ShotPlanarDirection = PendingVelocity.GetSafeNormal2D();
	const FVector ShotRight = ShotPlanarDirection.IsNearlyZero() ? Avatar->GetActorRightVector()
	                                                             : FRotationMatrix(ShotPlanarDirection.Rotation()).GetUnitAxis(EAxis::Y);
	const bool Left = FVector::DotProduct(BallPosition - Avatar->GetActorLocation(), ShotRight) < 0;
	const FString Clip = Flip ? TEXT("Salto_Shoot") : Left ? TEXT("Kick_Left") : TEXT("Kick_Right");
	ShotEntryVelocity = Avatar->GetVelocity();
	ShotEntryVelocity.Z = 0;
	const float Rate = Flip ? 2.6f : 2.2f;
	const float FlipContactFraction = .36f * .43f;
	if (!Avatar->StartAction(Flip ? AFootballPlayer::EAction::Flip : AFootballPlayer::EAction::Kick, Clip, Rate,
	                         Flip ? FlipContactFraction : 0.f))
		return;
	if (Flip)
	{
		const float Speed = FMath::Clamp(ShotEntryVelocity.Size2D(), 320.f, 765.f);
		Avatar->EntryVelocity =
		    (ShotEntryVelocity.IsNearlyZero() ? Avatar->GetActorForwardVector() : ShotEntryVelocity.GetSafeNormal()) * Speed;
		Avatar->ActionVelocity = Avatar->EntryVelocity * 1.15f;
	}
	ShotBallStart = BallPosition;
	ShotActorStart = Avatar->GetActorLocation();
	const float Length = Avatar->Animations.FindRef(Clip)->GetPlayLength();
	ShotContactTime = Flip ? 0.f : Length * .43f / Rate;
	ShotRecoverTime = ShotContactTime + .1f;
	PendingShot = true;
	KickCooldown = Now + .3f;
	GoalSpacePending = false;
	SpaceHeld = false;
}
void AFootballController::Reset()
{
	if (Screen == EScreen::Game && Avatar && Avatar->IsMovementLocked())
		return;
	Charging = false;
	ShotChargeArmed = false;
	ShotBufferActive = false;
	ShotChargeAimDirection = FVector::ZeroVector;
	ShotBufferAimDirection = FVector::ZeroVector;
	if (auto M = GetWorld()->GetAuthGameMode<AFootballMode>())
		M->ResetBall();
}
