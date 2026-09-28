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
namespace
{
// Order matches the FaceExpression index used by M_Face.
struct FFaceMotion { float Duration, First, Interval; bool bSigned; };
struct FFaceStyle { const TCHAR* Name; FFaceMotion Blink, Gaze, Mouth, Accent; };
constexpr FFaceMotion Off{0.f, 0.f, 0.f, false};
const FFaceStyle FaceStyles[] = {
	// Scowl: short blinks, a sideways glare, the tongue drops a little, the brows pinch down.
	{TEXT("angry"),   {.30f, 2.2f, 4.4f, false}, {.70f, 3.4f, 6.6f, true}, {.50f, 5.6f, 7.9f, false}, {.55f, 1.2f, 5.3f, false}},
	// Closed smiling eyes: cheeks lift them; the tongue bobs with the laugh.
	{TEXT("happy"),   Off, Off, {.55f, 2.4f, 5.2f, false}, {.70f, 1.1f, 4.1f, false}},
	// Star eyes: a small shrink pulse and a twinkle that turns each way in turn.
	{TEXT("joy"),     {.34f, 1.6f, 3.7f, false}, Off, {.50f, 3.0f, 5.6f, false}, {.80f, .9f, 3.3f, true}},
	// Approved shocked timings; the mouth stays still.
	{TEXT("shocked"), {.36f, 2.7f, 3.8f, false}, {.64f, 4.2f, 8.0f, true}, Off, {.60f, 8.5f, 8.7f, false}},
	// Asleep: slow breathing of the closed eyes and a lazily swinging drop of drool.
	{TEXT("sleepy"),  Off, Off, {1.8f, 1.5f, 4.6f, true}, {2.0f, 3.2f, 6.3f, false}},
	// Heavy, slow blinks, a drifting look and a hanging tongue that sways.
	{TEXT("tired"),   {.95f, 2.0f, 5.2f, false}, {1.3f, 4.0f, 8.4f, true}, {1.6f, 1.2f, 5.9f, true}, Off},
};
constexpr float FaceBlendTime = .2f;
int32 FaceIndex(const FString& TexturePath)
{
	FString Name = FPaths::GetBaseFilename(TexturePath);
	Name.RemoveFromStart(TEXT("T_"));
	for (int32 I = 0; I < UE_ARRAY_COUNT(FaceStyles); ++I)
		if (Name.Equals(FaceStyles[I].Name, ESearchCase::IgnoreCase))
			return I;
	return INDEX_NONE;
}
}
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
		if (auto M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Erling/Materials/M_Face.M_Face")))
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
	UpdateFaceMaterial(Dt);
	UpdateAction(Dt);
}
void AFootballPlayer::SetFaceExpression(const FString& TexturePath)
{
	const int32 Expression = FaceIndex(TexturePath);
	if (Expression == INDEX_NONE)
	{
		UE_LOG(LogTemp, Error, TEXT("Unknown face expression %s"), *TexturePath);
		return;
	}
	if (!FaceMaterial || Expression == NextFace)
	{
		QueuedFace = INDEX_NONE;
		return;
	}
	if (CurrentFace == INDEX_NONE)
	{
		CurrentFace = NextFace = Expression;
		FaceMaterial->SetScalarParameterValue(TEXT("FaceExpression"), Expression);
		FaceMaterial->SetScalarParameterValue(TEXT("NextFaceExpression"), Expression);
		FaceMaterial->SetScalarParameterValue(TEXT("FaceBlend"), 0.f);
		ScheduleFaceMotion(Expression);
		return;
	}
	if (CurrentFace != NextFace)
	{
		QueuedFace = Expression;
		return;
	}
	NextFace = Expression;
	FaceBlendElapsed = 0.f;
	FaceMaterial->SetScalarParameterValue(TEXT("NextFaceExpression"), Expression);
	FaceMaterial->SetScalarParameterValue(TEXT("FaceBlend"), 0.f);
	ScheduleFaceMotion(Expression);
}
void AFootballPlayer::ScheduleFaceMotion(int32 Expression)
{
	const FFaceStyle& Style = FaceStyles[Expression];
	NextBlinkAt = FaceMotionElapsed + Style.Blink.First;
	NextGazeAt = FaceMotionElapsed + Style.Gaze.First;
	NextMouthAt = FaceMotionElapsed + Style.Mouth.First;
	NextAccentAt = FaceMotionElapsed + Style.Accent.First;
}
void AFootballPlayer::UpdateFaceMaterial(float Dt)
{
	if (!FaceMaterial || NextFace == INDEX_NONE)
		return;
	if (CurrentFace != NextFace)
	{
		FaceBlendElapsed = FMath::Min(FaceBlendElapsed + Dt, FaceBlendTime);
		FaceMaterial->SetScalarParameterValue(TEXT("FaceBlend"), FaceBlendElapsed / FaceBlendTime);
		if (FaceBlendElapsed >= FaceBlendTime)
		{
			CurrentFace = NextFace;
			FaceMaterial->SetScalarParameterValue(TEXT("FaceExpression"), CurrentFace);
			FaceMaterial->SetScalarParameterValue(TEXT("FaceBlend"), 0.f);
			FaceBlendElapsed = 0.f;
			const int32 Queued = QueuedFace;
			QueuedFace = INDEX_NONE;
			if (Queued != INDEX_NONE && Queued != CurrentFace)
				SetFaceExpression(FaceStyles[Queued].Name);
		}
	}
	// One clip plays at a time; a clip a face does not use is never started.
	const FFaceStyle& Style = FaceStyles[NextFace];
	FaceMotionElapsed += Dt;
	if (FaceClip == EFaceClip::Idle)
	{
		if (Style.Blink.Duration > 0.f && FaceMotionElapsed >= NextBlinkAt)
		{
			FaceClip = EFaceClip::Blink;
			NextBlinkAt = FaceMotionElapsed + Style.Blink.Interval;
		}
		else if (Style.Gaze.Duration > 0.f && FaceMotionElapsed >= NextGazeAt)
		{
			FaceClip = EFaceClip::Gaze;
			NextGazeAt = FaceMotionElapsed + Style.Gaze.Interval;
			GazeDirection = -GazeDirection;
		}
		else if (Style.Mouth.Duration > 0.f && FaceMotionElapsed >= NextMouthAt)
		{
			FaceClip = EFaceClip::Mouth;
			NextMouthAt = FaceMotionElapsed + Style.Mouth.Interval;
			MouthDirection = Style.Mouth.bSigned ? -MouthDirection : 1;
		}
		else if (Style.Accent.Duration > 0.f && FaceMotionElapsed >= NextAccentAt)
		{
			FaceClip = EFaceClip::Accent;
			NextAccentAt = FaceMotionElapsed + Style.Accent.Interval;
			AccentDirection = Style.Accent.bSigned ? -AccentDirection : 1;
		}
		FaceClipElapsed = 0.f;
	}
	float Blink = 0.f, Gaze = 0.f, Mouth = 0.f, Accent = 0.f;
	if (FaceClip != EFaceClip::Idle)
	{
		const FFaceMotion& Motion = FaceClip == EFaceClip::Blink ? Style.Blink :
			FaceClip == EFaceClip::Gaze ? Style.Gaze :
			FaceClip == EFaceClip::Mouth ? Style.Mouth : Style.Accent;
		// A clip the new face does not use still finishes over the default length.
		const float Duration = Motion.Duration > 0.f ? Motion.Duration : .5f;
		FaceClipElapsed += Dt;
		const float Phase = FMath::Clamp(FaceClipElapsed / Duration, 0.f, 1.f);
		const float Envelope = FMath::Square(FMath::Sin(PI * Phase));
		switch (FaceClip)
		{
		case EFaceClip::Blink: Blink = Envelope; break;
		case EFaceClip::Gaze: Gaze = GazeDirection * Envelope; break;
		case EFaceClip::Mouth: Mouth = MouthDirection * Envelope; break;
		case EFaceClip::Accent: Accent = AccentDirection * Envelope; break;
		default: break;
		}
		if (Phase >= 1.f)
		{
			FaceClip = EFaceClip::Idle;
			FaceClipElapsed = 0.f;
		}
	}
	FaceMaterial->SetVectorParameterValue(TEXT("FaceMorph"), FLinearColor(Blink, Gaze, Mouth, Accent));
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
				SetFaceExpression(O.Texture);
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
