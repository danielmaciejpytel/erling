#include "Football.h"
#include "ErlingBall.h"
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
AFootballMode::AFootballMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	DefaultPawnClass = AFootballPlayer::StaticClass();
	PlayerControllerClass = AFootballController::StaticClass();
	Possession = CreateDefaultSubobject<UErlingBallPossession>(TEXT("Possession"));
	Referee = CreateDefaultSubobject<UErlingReferee>(TEXT("Referee"));
}
UStaticMeshComponent* AFootballMode::Box(const FVector& P, const FVector& Size, const FLinearColor& Color, bool Collision)
{
	auto A = GetWorld()->SpawnActor<AStaticMeshActor>(P, FRotator::ZeroRotator);
	auto C = A->GetStaticMeshComponent();
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	C->SetWorldScale3D(Size / 100);
	C->SetCollisionEnabled(Collision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	if (auto M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Erling/Materials/M_World.M_World")))
	{
		auto D = UMaterialInstanceDynamic::Create(M, this);
		D->SetVectorParameterValue(TEXT("Tint"), Color);
		C->SetMaterial(0, D);
	}
	return C;
}
void AFootballMode::BeginPlay()
{
	Super::BeginPlay();
	Box(FVector(0, 0, -55), FVector(20000, 20000, 100), FLinearColor(.04f, .18f, .08f));
	GoalSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Erling/Audio/goal.goal"));
	BallActor = GetWorld()->SpawnActor<AErlingBall>(FVector(0, 0, 40), FRotator::ZeroRotator);
	BallActor->ConfigureLiveBall();
	Ball = BallActor->GetStaticMeshComponent();
	ResetBall();
}
void AFootballMode::ResetBall()
{
	if (!Ball)
		return;
	if (Referee->Scored || Referee->BallHidden || Referee->ShotInFlight)
		PreserveFinishedBall();
	Referee->LastShooter.Reset();
	Referee->BallHidden = false;
	Referee->ShotInFlight = false;
	Possession->ResetState();
	if (auto PC = Cast<AFootballController>(GetWorld()->GetFirstPlayerController()); PC && PC->Avatar)
		PC->Avatar->CancelBallTrap();
	Ball->SetVisibility(true);
	Ball->SetSimulatePhysics(true);
	Ball->SetLinearDamping(ErlingBall::LinearDamping);
	Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Ball->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	Ball->SetWorldLocation(FVector(0, 0, ErlingBall::RestHeight), false, nullptr, ETeleportType::TeleportPhysics);
	Referee->Scored = false;
	Referee->ResetAt = 0;
	Referee->PreviousBall = Ball->GetComponentLocation();
}
FVector AFootballMode::ShotVelocity(const FVector& Position, const FVector& Direction, float Seconds)
{
	FVector F = Direction.GetSafeNormal2D();
	float C = FMath::Clamp(Seconds * 2.f, 0.f, 3.f);
	float Speed = C <= 1 ? FMath::Lerp(850.f, 2200.f, C) : C <= 2 ? FMath::Lerp(2200.f, 2800.f, C - 1) : FMath::Lerp(2800.f, 3200.f, C - 2);
	if (C < .15f)
		return F * Speed;
	float GoalHeight = C <= 1 ? FMath::Lerp(22.f, 130.f, C) : C <= 2 ? FMath::Lerp(130.f, 220.f, C - 1) : FMath::Lerp(220.f, 420.f, C - 2);
	float Distance = FMath::Abs(F.X) > .15f ? FMath::Abs((ErlingPitch::GoalLineFor(F.X) - Position.X) / F.X) : 1800.f;
	Distance = FMath::Clamp(Distance, 200.f, 5000.f);
	float T = Distance / Speed;
	float Z = FMath::Clamp((GoalHeight - Position.Z + .5f * 980 * T * T) / T, 0.f, 1300.f);
	return F * Speed + FVector(0, 0, Z);
}
void AFootballMode::Kick(AFootballPlayer* P, float Seconds)
{
	if (!Ball || !P || Referee->BallHidden || Referee->Scored || FVector::Dist2D(Ball->GetComponentLocation(), P->GetActorLocation()) > 240)
		return;
	Referee->StartShot(P);
	Ball->SetLinearDamping(.015f);
	Ball->SetPhysicsLinearVelocity(ShotVelocity(Ball->GetComponentLocation(), P->GetActorForwardVector(), Seconds));
	if (auto PC = Cast<AFootballController>(GetWorld()->GetFirstPlayerController()))
		PC->PlayEffect(TEXT("kick"));
}
void AFootballMode::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!Ball)
		return;
	auto PC = Cast<AFootballController>(GetWorld()->GetFirstPlayerController());
	if (!PC)
		return;
	const bool GameplayActive = PC->Screen == AFootballController::EScreen::Game ||
	                            (PC->Screen == AFootballController::EScreen::Settings &&
	                                PC->SettingsReturn == AFootballController::EScreen::Game && !PC->IsPaused());
	if (!GameplayActive)
		return;
	Dribble(PC->Avatar, Dt);
	Referee->Update(PC);
}
void AFootballMode::NetHit(
    UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	BallActor->NetHit(HitComponent, OtherActor, OtherComp, NormalImpulse, Hit);
}
void AFootballMode::PreserveFinishedBall()
{
	auto* Fresh = BallActor->SpawnFinishedCopy();
	if (!Fresh)
		return;
	FinishedBalls.Add(Fresh);
	// Keep only the most recent attempts. Without a cap every reset after a shot
	// would leave another simulated actor on the pitch for the rest of the session.
	while (FinishedBalls.Num() > ErlingBall::MaxFinishedBalls)
	{
		UStaticMeshComponent* Oldest = FinishedBalls[0];
		FinishedBalls.RemoveAt(0);
		if (IsValid(Oldest) && Oldest->GetOwner())
			Oldest->GetOwner()->Destroy();
	}
}
