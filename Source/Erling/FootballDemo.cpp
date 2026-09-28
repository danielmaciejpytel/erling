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
void AFootballController::UpdateDemo(float Dt)
{
	auto M = GetWorld()->GetAuthGameMode<AFootballMode>();
	if (!M || !M->Ball)
		return;
	const float T = GetWorld()->GetTimeSeconds();
	const float X = Avatar->GetActorLocation().X;
	// Turn as soon as the kick finishes; let the released ball complete its flight.
	if (DemoPhase == 1 && !PendingShot && Avatar->CanAct())
	{
		DemoDirection = -DemoDirection;
		DemoPhase = 3;
	}
	if (DemoPhase == 3 && T - DemoShotAt >= 1.25f)
	{
		DemoPhase = 0;
		M->ResetBall();
	}
	// Entering the menu near midfield needs a short approach, not a completed-shot state.
	if (DemoPhase == 4 && X * DemoDirection >= 300.f && Avatar->CanAct())
	{
		DemoDirection = -DemoDirection;
		DemoPhase = 0;
		M->ResetBall();
	}
	Avatar->GetCharacterMovement()->MaxWalkSpeed = 500;
	FVector Direction(DemoDirection, 0, 0);
	Direction.Y = FMath::Clamp(-Avatar->GetActorLocation().Y * .01f, -.3f, .3f);
	if (!Avatar->IsMovementLocked())
		Avatar->AddMovementInput(Direction.GetSafeNormal(), 1);
	FVector To = M->Ball->GetComponentLocation() - Avatar->GetActorLocation();
	To.Z = 0;
	if (DemoPhase == 0)
	{
		// The midfield ball is a waiting ball. It must not anticipate the runner or slide
		// toward a future receive point. The first movement comes from a real foot contact.
		M->Ball->SetLinearDamping(2.f);
		M->Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);
		M->Ball->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
	}
	const bool HasDemoFeet =
	    Avatar->GetMesh() && Avatar->GetMesh()->DoesSocketExist(TEXT("foot_l")) && Avatar->GetMesh()->DoesSocketExist(TEXT("foot_r"));
	float DemoFootDistance = MAX_flt;
	if (HasDemoFeet)
		DemoFootDistance =
		    FMath::Min(FVector::Dist2D(M->Ball->GetComponentLocation(), Avatar->GetMesh()->GetSocketLocation(TEXT("foot_l"))),
		        FVector::Dist2D(M->Ball->GetComponentLocation(), Avatar->GetMesh()->GetSocketLocation(TEXT("foot_r"))));
	const float DemoPlayerGap = To.Size2D();
	const bool RealDemoReceive =
	    DemoPlayerGap < 118.f && ((HasDemoFeet && DemoFootDistance <= 62.f) || (!HasDemoFeet && DemoPlayerGap < 88.f));
	if (Avatar->CanAct() && DemoPhase == 0 && RealDemoReceive && To.X * DemoDirection > 18 && FMath::Abs(To.Y) < 82 &&
	    Avatar->GetActorForwardVector().X * DemoDirection > .9f)
	{
		DemoPhase = 2;
		DemoReceiveAt = T;
		DemoReceivePosition = Avatar->GetActorLocation();
		const float Side = FVector::DotProduct(To, Avatar->GetActorRightVector());
		DemoFootSide = FMath::Abs(Side) > 8 ? FMath::Sign(Side) * 18.f : DemoDirection * 18.f;
	}
	if (DemoPhase == 2 && Avatar->CanAct())
	{
		M->Dribble(Avatar, Dt);
		// Ease the received ball toward the available foot without teleporting it.
		const FVector SideTarget = Avatar->GetActorLocation() + Avatar->GetActorRightVector() * DemoFootSide;
		const float SideError = FVector::DotProduct(SideTarget - M->Ball->GetComponentLocation(), Avatar->GetActorRightVector());
		const FVector BallVelocity = M->Ball->GetPhysicsLinearVelocity();
		const FVector Right = Avatar->GetActorRightVector();
		M->Ball->SetPhysicsLinearVelocity(BallVelocity + Right * (SideError * 5.f - FVector::DotProduct(BallVelocity, Right) * .72f));
		if (DemoShotTargetX == 0.f)
		{
			static const float Distances[] = {500.f, 800.f, 1100.f, 1400.f, 1700.f, 2100.f};
			int32 Choice = FMath::RandRange(0, UE_ARRAY_COUNT(Distances) - 1);
			if (Choice == DemoShotDistanceIndex)
				Choice = (Choice + FMath::RandRange(1, UE_ARRAY_COUNT(Distances) - 1)) % UE_ARRAY_COUNT(Distances);
			DemoShotDistanceIndex = Choice;
			DemoShotTargetX = Distances[Choice] * DemoDirection;
		}
		const bool ReachedShotPoint =
		    DemoDirection > 0 ? Avatar->GetActorLocation().X >= DemoShotTargetX : Avatar->GetActorLocation().X <= DemoShotTargetX;
		if (T - DemoReceiveAt >= .35f && ReachedShotPoint && M->HasBall(Avatar))
		{
			FireShot(1.f);
			if (PendingShot)
			{
				DemoPhase = 1;
				DemoShotAt = T;
				DemoShotTargetX = 0.f;
			}
		}
	}
}
