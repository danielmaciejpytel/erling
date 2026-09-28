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
AFootballMode::AFootballMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	DefaultPawnClass = AFootballPlayer::StaticClass();
	PlayerControllerClass = AFootballController::StaticClass();
	Possession = CreateDefaultSubobject<UErlingBallPossession>(TEXT("Possession"));
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
	auto A = GetWorld()->SpawnActor<AStaticMeshActor>(FVector(0, 0, 40), FRotator::ZeroRotator);
	Ball = A->GetStaticMeshComponent();
	Ball->SetMobility(EComponentMobility::Movable);
	Ball->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	Ball->SetWorldScale3D(FVector(ErlingBall::MeshScale));
	Ball->SetCollisionProfileName(TEXT("PhysicsActor"));
	Ball->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Ball->SetSimulatePhysics(true);
	Ball->SetMassOverrideInKg(NAME_None, ErlingBall::MassKg);
	Ball->SetLinearDamping(ErlingBall::LinearDamping);
	Ball->SetAngularDamping(ErlingBall::AngularDamping);
	Ball->BodyInstance.bUseCCD = true;
	Ball->SetNotifyRigidBodyCollision(true);
	Ball->OnComponentHit.AddDynamic(this, &AFootballMode::NetHit);
	if (auto M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Erling/Materials/M_Ball.M_Ball")))
		Ball->SetMaterial(0, M);
	Ball->SetRenderCustomDepth(true);
	Ball->SetCustomDepthStencilValue(1);
	auto PM = NewObject<UPhysicalMaterial>(this);
	PM->Restitution = ErlingBall::Restitution;
	PM->Friction = ErlingBall::Friction;
	Ball->SetPhysMaterialOverride(PM);
	ResetBall();
}
void AFootballMode::ResetBall()
{
	if (!Ball)
		return;
	if (Scored || BallHidden || ShotInFlight)
		PreserveFinishedBall();
	LastShooter.Reset();
	BallHidden = false;
	ShotInFlight = false;
	Possession->ResetState();
	if (auto PC = Cast<AFootballController>(GetWorld()->GetFirstPlayerController()); PC && PC->Avatar)
		PC->Avatar->CancelBallTrap();
	Ball->SetVisibility(true);
	Ball->SetSimulatePhysics(true);
	Ball->SetLinearDamping(ErlingBall::LinearDamping);
	Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Ball->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	Ball->SetWorldLocation(FVector(0, 0, ErlingBall::RestHeight), false, nullptr, ETeleportType::TeleportPhysics);
	Scored = false;
	ResetAt = 0;
	PreviousBall = Ball->GetComponentLocation();
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
	if (!Ball || !P || BallHidden || Scored || FVector::Dist2D(Ball->GetComponentLocation(), P->GetActorLocation()) > 240)
		return;
	LastShooter = P;
	LastShot = GetWorld()->GetTimeSeconds();
	ShotInFlight = true;
	Ball->SetLinearDamping(.015f);
	Ball->SetPhysicsLinearVelocity(ShotVelocity(Ball->GetComponentLocation(), P->GetActorForwardVector(), Seconds));
	if (auto PC = Cast<AFootballController>(GetWorld()->GetFirstPlayerController()))
		PC->PlayEffect(TEXT("kick"));
}
void AFootballMode::HideMiss()
{
	if (BallHidden)
		return;
	if (auto PC = Cast<AFootballController>(GetWorld()->GetFirstPlayerController()))
		PC->PlayEffect(TEXT("fail"));
	BallHidden = true;
	Ball->SetLinearDamping(1.2f);
	ResetAt = GetWorld()->GetTimeSeconds() + 1.2f;
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
	auto P = Ball->GetComponentLocation();
	const FVector OldBall = PreviousBall;
	PreviousBall = P;
	float T = GetWorld()->GetTimeSeconds();
	if (ResetAt > 0 && T >= ResetAt)
	{
		ResetBall();
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
		ResetBall();
}
void AFootballMode::NetHit(
    UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (HitComponent && OtherComp && OtherComp->ComponentHasTag(TEXT("GoalNet")))
	{
		auto V = HitComponent->GetPhysicsLinearVelocity();
		if (auto* Goal = Cast<AErlingStylizedGoal>(OtherComp->GetOwner()))
			Goal->ReactToBall(OtherComp, HitComponent, Hit.ImpactPoint, V.Size());
		HitComponent->SetPhysicsLinearVelocity(FVector(0, 0, FMath::Min(V.Z, 0.f)));
		HitComponent->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
	}
}
void AFootballMode::PreserveFinishedBall()
{
	// Preserve the completed shot as an independent physical ball. Keep the live
	// component stable so input/contact code never observes a newly initialized body.
	auto* Old = Ball;
	auto* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(Old->GetComponentLocation(), Old->GetComponentRotation());
	if (!Actor)
		return;
	auto* Fresh = Actor->GetStaticMeshComponent();
	Fresh->SetMobility(EComponentMobility::Movable);
	Fresh->SetStaticMesh(Old->GetStaticMesh());
	Fresh->SetWorldScale3D(Old->GetComponentScale());
	for (int32 I = 0; I < Old->GetNumMaterials(); ++I)
		Fresh->SetMaterial(I, Old->GetMaterial(I));
	Fresh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Fresh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Fresh->SetSimulatePhysics(true);
	Fresh->SetMassOverrideInKg(NAME_None, ErlingBall::MassKg);
	Fresh->SetAngularDamping(ErlingBall::AngularDamping);
	Fresh->BodyInstance.bUseCCD = true;
	Fresh->SetPhysMaterialOverride(Old->BodyInstance.GetSimplePhysicalMaterial());
	Fresh->SetRenderCustomDepth(Old->bRenderCustomDepth);
	Fresh->SetCustomDepthStencilValue(Old->CustomDepthStencilValue);
	Fresh->SetNotifyRigidBodyCollision(true);
	Fresh->OnComponentHit.AddDynamic(this, &AFootballMode::NetHit);
	Fresh->SetVisibility(true);
	Fresh->SetLinearDamping(1.2f);
	Fresh->SetPhysicsLinearVelocity(Old->GetPhysicsLinearVelocity());
	Fresh->SetPhysicsAngularVelocityInRadians(Old->GetPhysicsAngularVelocityInRadians());
	// Finished attempts keep ground/net physics without deflecting the live ball.
	Fresh->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
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
