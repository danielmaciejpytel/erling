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
AFootballPlayer::AFootballPlayer(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UErlingMovement>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	GetCapsuleComponent()->InitCapsuleSize(55, 96);
	GetMesh()->SetRelativeLocation(FVector(0, 0, -96));
	GetMesh()->SetRelativeRotation(FRotator(0, -90, 0));
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetForcedLOD(1);
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Face = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Face"));
	Face->SetupAttachment(GetMesh());
	Face->SetRelativeLocation(FVector(0, .065871f, 0));
	Face->SetCastShadow(false);
	Face->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->MaxWalkSpeed = 500;
	GetCharacterMovement()->bEnablePhysicsInteraction = false;
	GetCharacterMovement()->JumpZVelocity = 420;
	GetCharacterMovement()->GravityScale = 1.4f;
	GetCharacterMovement()->MaxAcceleration = 1400;
	GetCharacterMovement()->BrakingDecelerationWalking = 1800;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0, 520, 0);
	bUseControllerRotationYaw = false;
}
void AFootballPlayer::BeginPlay()
{
	Super::BeginPlay();
	InitializeAssets();
}
void AFootballPlayer::InitializeAssets()
{
	if (GetMesh()->GetSkeletalMeshAsset() && Face->GetSkeletalMeshAsset() && FaceMaterial && Animations.Num() == 25 &&
	    Animations.Contains(TEXT("Idle_Relaxed")))
		return;
	if (!GetMesh()->GetSkeletalMeshAsset())
		GetMesh()->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Erling/Meshes/SK_body.SK_body")));
	ConfigurePiece(GetMesh());
	if (!Face->GetSkeletalMeshAsset())
		Face->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Erling/Meshes/SK_face.SK_face")));
	Face->SetRelativeLocation(FVector::ZeroVector);
	ConfigurePiece(Face);
	if (!FaceMaterial)
	{
		if (auto M = LoadObject<UMaterialInterface>(nullptr, GetWorld()->GetMapName().Contains(TEXT("Pitch_ArtDirection"))
		                                                         ? TEXT("/Game/Erling/ArtDirection/M_Face.M_Face")
		                                                         : TEXT("/Game/Erling/Materials/M_Face.M_Face")))
		{
			FaceMaterial = UMaterialInstanceDynamic::Create(M, this);
			Face->SetMaterial(0, FaceMaterial);
		}
	}
	Animations.Remove(TEXT("Idle_LookAround"));
	for (auto N : {TEXT("Idle_Breathe"), TEXT("Idle_Relaxed"), TEXT("Idle_WeightShift"), TEXT("Walk"), TEXT("Run"), TEXT("Sprint"),
	         TEXT("Kick_Right"), TEXT("Kick_Left"), TEXT("Jump"), TEXT("Jump_Run"), TEXT("JoyJump_Run"), TEXT("JoyJump_Standing"),
	         TEXT("Salto"), TEXT("Salto_Shoot"), TEXT("Slide_From_Run"), TEXT("Trip_Roll_From_Run"), TEXT("Celebrate_Victory"), TEXT("Dance_Disco"),
	         TEXT("Dance_Robot"), TEXT("Fall_Backward"), TEXT("GetUp_Back"), TEXT("GetUp_Front"), TEXT("Meme_RageStomp"),
	         TEXT("Meme_Shrug"), TEXT("Turn_Step")})
	{
		const FString AssetName = FString(TEXT("AN_")) + N;
		FString Path = FString::Printf(TEXT("/Game/Erling/Animations/%s.%s"), *AssetName, *AssetName);
		if (auto A = LoadObject<UAnimSequence>(nullptr, *Path))
			Animations.Add(N, A);
		else
			UE_LOG(LogTemp, Error, TEXT("Missing animation %s"), N);
	}
	SetAnimation(TEXT("Idle_Breathe"));
}
void AFootballPlayer::SetAnimation(const FString& Name, bool Loop)
{
	if (CurrentAnimation == Name)
		return;
	if (auto A = Animations.FindRef(Name))
	{
		const bool Gait = (Name == TEXT("Walk") || Name == TEXT("Run") || Name == TEXT("Sprint"));
		const bool WasGait = (CurrentAnimation == TEXT("Walk") || CurrentAnimation == TEXT("Run") || CurrentAnimation == TEXT("Sprint"));
		float Phase = 0;
		if (auto Old = Animations.FindRef(CurrentAnimation))
			Phase = AnimationTime / FMath::Max(.01f, Old->GetPlayLength());
		PreviousAnimation = CurrentAnimation;
		PreviousAnimationTime = AnimationTime;
		PreviousLoops = AnimationLoops;
		PreviousRate = PlaybackRate;
		CurrentAnimation = Name;
		AnimationTime = Gait && WasGait ? Phase * A->GetPlayLength() : 0;
		AnimationLoops = Loop;
		AnimationBlend = PreviousAnimation.IsEmpty() ? 1 : 0;
	}
}
void AFootballPlayer::Tick(float Dt)
{
	Super::Tick(Dt);
	UpdateAction(Dt);
}
void AFootballPlayer::ApplyKit(const TArray<int32>& Values, const TArray<FKitCategory>& Cats)
{
	for (auto P : Pieces)
		if (P)
			P->DestroyComponent();
	Pieces.Empty();
	for (int32 C = 0; C < Cats.Num(); C++)
	{
		if (!Values.IsValidIndex(C) || !Cats[C].Options.IsValidIndex(Values[C]))
			continue;
		const auto& O = Cats[C].Options[Values[C]];
		if (C == 1)
		{
			if (FaceMaterial && !O.Texture.IsEmpty())
				FaceMaterial->SetTextureParameterValue(TEXT("FaceTexture"), LoadObject<UTexture2D>(nullptr, *O.Texture));
			continue;
		}
		for (const auto& Path : O.Meshes)
			if (auto M = LoadObject<USkeletalMesh>(nullptr, *Path))
			{
				auto P = NewObject<USkeletalMeshComponent>(this);
				P->SetupAttachment(GetMesh());
				P->SetSkeletalMesh(M);
				P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				P->RegisterComponent();
				ConfigurePiece(P);
				Pieces.Add(P);
			}
	}
}
void AFootballPlayer::Landed(const FHitResult& Hit)
{
	const bool Audible = GetVelocity().Z < -150;
	Super::Landed(Hit);
	if (PhysicalJump)
		if (auto Sequence = Animations.FindRef(CurrentAnimation))
		{
			AnimationTime = Sequence->GetPlayLength() * .76f;
			PlaybackRate = CurrentAnimation == TEXT("Jump_Run") ? 3.f * JumpAnimationScale : 2.6f;
		}
	if (Audible)
		if (auto PC = Cast<AFootballController>(GetController()))
			if (PC->Screen == AFootballController::EScreen::Game)
				PC->PlayEffect(TEXT("jump"));
}
