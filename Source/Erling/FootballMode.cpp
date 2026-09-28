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
static bool HasStylizedGoalAtEnd(const UObject* WorldContext, int32 Sign)
{
	TArray<AActor*> Goals;
	UGameplayStatics::GetAllActorsOfClass(WorldContext, AErlingStylizedGoal::StaticClass(), Goals);
	for (const AActor* Goal : Goals)
		if (Goal && FMath::Abs(Goal->GetActorLocation().X - Sign * ErlingPitch::GoalLineX) < 100.f)
			return true;
	return false;
}

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
void AFootballMode::CreateField()
{
	TArray<AActor*> Existing;
	UGameplayStatics::GetAllActorsWithTag(this, TEXT("PitchBuilt"), Existing);
	if (!Existing.IsEmpty())
		return;
	Box(FVector(0, 0, -35), FVector(6800, 4800, 70), FLinearColor(.025f, .1f, .07f));
	for (int32 I = 0; I < 12; I++)
		Box(FVector(-2475 + I * 450, 0, -2), FVector(450, 3600, 5),
		    I % 2 ? FLinearColor(.055f, .25f, .125f) : FLinearColor(.04f, .21f, .10f), false);
	auto Line = [this](FVector P, FVector S)
	{
		Box(P, S, FLinearColor(.83f, .91f, .85f), false);
	};
	for (int S : {-1, 1})
	{
		Line(FVector(0, S * ErlingPitch::TouchlineY, 2), FVector(5400, 8, 3));
		Line(FVector(S * 2700, 0, 2), FVector(8, 3500, 3));
	}
	Line(FVector(0, 0, 2), FVector(8, 3500, 3));
	for (int I = 0; I < 64; I++)
	{
		float A = I * 2 * PI / 64;
		auto C = Box(FVector(500 * FMath::Cos(A), 500 * FMath::Sin(A), 2), FVector(50, 8, 3), FLinearColor(.83f, .91f, .85f), false);
		C->SetWorldRotation(FRotator(0, FMath::RadiansToDegrees(A) + 90, 0));
	}
	for (int S : {-1, 1})
	{
		const bool HasStylizedGoal = HasStylizedGoalAtEnd(this, S);
		for (int Y : {-1, 1})
		{
			Line(FVector(S * 2400, Y * 650, 2), FVector(600, 8, 3));
			if (!HasStylizedGoal)
				Box(FVector(S * ErlingPitch::GoalLineX, Y * ErlingPitch::GoalPostY, ErlingPitch::CrossbarZ * .5f),
				    FVector(18, 18, ErlingPitch::CrossbarZ), FLinearColor(.9f, .95f, 1));
		}
		Line(FVector(S * 2100, 0, 2), FVector(8, 1300, 3));
		if (!HasStylizedGoal)
		{
			Box(FVector(S * ErlingPitch::GoalLineX, 0, ErlingPitch::CrossbarZ), FVector(18, ErlingPitch::GoalPostY * 2 + 20, 18),
			    FLinearColor(.9f, .95f, 1));
			for (int Y = -350; Y <= 350; Y += 50)
				Box(FVector(S * ErlingPitch::NetBackX, Y, ErlingPitch::CrossbarZ * .5f), FVector(5, 3, ErlingPitch::CrossbarZ),
				    FLinearColor(.48f, .58f, .62f), false);
			for (int Z = 20; Z <= 260; Z += 40)
				Box(FVector(S * ErlingPitch::NetBackX, 0, Z), FVector(5, ErlingPitch::GoalPostY * 2, 3), FLinearColor(.48f, .58f, .62f), false);
		}
	}
	// Simple stands leave the gameplay area clear.
	for (int S : {-1, 1})
		for (int Row = 0; Row < 3; Row++)
			Box(FVector(0, S * (2050 + Row * 130), Row * 80 + 25), FVector(5400, 110, 50 + Row * 100), FLinearColor(.05f, .10f, .16f));
	auto Sun = GetWorld()->SpawnActor<ADirectionalLight>(FVector(0, 0, 1000), FRotator(-55, -30, 0));
	Sun->GetLightComponent()->SetIntensity(3.2f);
	Cast<UDirectionalLightComponent>(Sun->GetLightComponent())->bAtmosphereSunLight = true;
	auto Sky = GetWorld()->SpawnActor<ASkyLight>();
	Sky->GetLightComponent()->SetIntensity(.8f);
	Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
	Sky->GetLightComponent()->bRealTimeCapture = true;
	GetWorld()->SpawnActor<ASkyAtmosphere>();
}
void AFootballMode::BeginPlay()
{
	Super::BeginPlay();
	CreateField();
	CreateNetCollision();
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
	if (!GetWorld()->GetMapName().Contains(TEXT("Pitch_ArtDirection")))
	{
		auto Dome = GetWorld()->SpawnActor<AStaticMeshActor>();
		Dome->SetActorEnableCollision(false);
		auto DC = Dome->GetStaticMeshComponent();
		DC->SetMobility(EComponentMobility::Movable);
		DC->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
		DC->SetWorldScale3D(FVector(400));
		DC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		DC->SetCastShadow(false);
		DC->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Erling/Materials/M_Sky.M_Sky")));
	}
	if (auto M = LoadObject<UMaterialInterface>(nullptr, GetWorld()->GetMapName().Contains(TEXT("Pitch_ArtDirection"))
	                                                         ? TEXT("/Game/Erling/ArtDirection/M_Ball.M_Ball")
	                                                         : TEXT("/Game/Erling/Materials/M_Ball.M_Ball")))
		Ball->SetMaterial(0, M);
	if (GetWorld()->GetMapName().Contains(TEXT("Pitch_ArtDirection")))
	{
		Ball->SetRenderCustomDepth(true);
		Ball->SetCustomDepthStencilValue(1);
	}
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
void AFootballMode::CreateNetCollision()
{
	auto Net = [this](FVector P, FVector Size)
	{
		auto C = Box(P, Size, FLinearColor::White);
		C->SetVisibility(false);
		C->SetCastShadow(false);
		C->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		C->ComponentTags.Add(TEXT("GoalNet"));
		auto PM = NewObject<UPhysicalMaterial>(C);
		PM->Restitution = 0;
		PM->bOverrideRestitutionCombineMode = true;
		PM->RestitutionCombineMode = EFrictionCombineMode::Min;
		PM->Friction = .9f;
		C->SetPhysMaterialOverride(PM);
	};
	for (int S : {-1, 1})
	{
		if (HasStylizedGoalAtEnd(this, S))
			continue;
		Net(FVector(S * ErlingPitch::NetBackX, 0, ErlingPitch::CrossbarZ * .5f),
		    FVector(12, ErlingPitch::GoalPostY * 2, ErlingPitch::CrossbarZ));
		for (int Y : {-1, 1})
			Net(FVector(S * ErlingPitch::NetCenterX, Y * ErlingPitch::GoalPostY, ErlingPitch::CrossbarZ * .5f),
			    FVector(250, 12, ErlingPitch::CrossbarZ));
		Net(FVector(S * ErlingPitch::NetCenterX, 0, ErlingPitch::CrossbarZ), FVector(250, ErlingPitch::GoalPostY * 2, 12));
	}
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
