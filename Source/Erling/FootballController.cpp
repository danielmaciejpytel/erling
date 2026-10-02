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
#include "ErlingProfile.h"
AFootballController::AFootballController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	bAutoManageActiveCameraTarget = false;
	CameraRig = CreateDefaultSubobject<UErlingCameraRig>(TEXT("CameraRig"));
	AudioVideo = CreateDefaultSubobject<UErlingAudioVideo>(TEXT("AudioVideo"));
}
void AFootballController::BeginPlay()
{
	Super::BeginPlay();
#if !UE_BUILD_SHIPPING
	// Parse the opt-in test switches once instead of on every Tick.
	bRunLatestChecks = FParse::Param(FCommandLine::Get(), TEXT("ErlingTest_Latest"));
	bRunProjectChecks = FParse::Param(FCommandLine::Get(), TEXT("ErlingTest"));
	// Test runs start from a default UFootballSave: drop the disposable test slot once per process.
	// Guarded by name so the player's ErlingProfile slot can never be deleted here.
	static bool bTestSlotReset = false;
	if (!bTestSlotReset && FCString::Strcmp(ProfileSlot(), TEXT("ErlingProfile_Test")) == 0)
	{
		bTestSlotReset = true;
		if (UGameplayStatics::DoesSaveGameExist(ProfileSlot(), 0))
			UGameplayStatics::DeleteGameInSlot(ProfileSlot(), 0);
	}
#endif
	Avatar = Cast<AFootballPlayer>(GetPawn());
	if (!Avatar)
	{
		Avatar = GetWorld()->SpawnActor<AFootballPlayer>(FVector(-350, 0, 100), FRotator::ZeroRotator);
		Possess(Avatar);
	}
	AudioVideo->LoadEffects();
	Avatar->InitializeAssets();
	Avatar->SetActorLocation(FVector(-350, 0, 100));
	LoadCatalog();
	Saved = Cast<UFootballSave>(UGameplayStatics::LoadGameFromSlot(ProfileSlot(), 0));
	if (!Saved)
		Saved = Cast<UFootballSave>(UGameplayStatics::CreateSaveGameObject(UFootballSave::StaticClass()));
	Saved->Language = FMath::Clamp(Saved->Language, 0, 1);
	if (Saved->CameraMode < 0 || Saved->CameraMode > 2)
		Saved->CameraMode = Saved->ReducedMotion ? 1 : 0;
	Saved->ReducedMotion = Saved->CameraMode == 1;
	Selection = Saved->Appearance;
	if (!Catalog.IsEmpty())
	{
		Selection.SetNum(Catalog.Num());
		for (int I = 0; I < Catalog.Num(); I++)
			Selection[I] = FMath::Clamp(Selection[I], 0, FMath::Max(0, Catalog[I].Options.Num() - 1));
	}
	else
		UE_LOG(LogTemp, Error, TEXT("Wardrobe catalog is empty; keeping the saved appearance unchanged"));
	Avatar->ApplyKit(Selection, Catalog);
	ViewCamera = GetWorld()->SpawnActor<ACameraActor>();
	ViewCamera->GetCameraComponent()->FieldOfView = 48;
	SetViewTarget(ViewCamera);
	ChangeScreen(EScreen::Main);
	AudioVideo->StartMusic();
	ApplyQuality();
}
void AFootballController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindAction(TEXT("BallControl"), IE_Pressed, this, &AFootballController::BallControlOn);
	InputComponent->BindAction(TEXT("BallControl"), IE_Released, this, &AFootballController::BallControlOff);
	InputComponent->BindAction(TEXT("Slide"), IE_Pressed, this, &AFootballController::SlidePressed);
	InputComponent->BindAction(TEXT("Jump"), IE_Pressed, this, &AFootballController::JumpPressed);
	InputComponent->BindAction(TEXT("Jump"), IE_Released, this, &AFootballController::JumpReleased);
	InputComponent->BindAxis(TEXT("EditorTurn"), this, &AFootballController::TurnEditor);
	InputComponent->BindAxis(TEXT("EditorZoom"), this, &AFootballController::ZoomEditor);
	InputComponent->BindAxis(TEXT("EditorGamepadTurn"), this, &AFootballController::EditorGamepadTurn);
	InputComponent->BindAxis(TEXT("EditorGamepadZoom"), this, &AFootballController::EditorGamepadZoom);
	InputComponent->BindAxis(TEXT("Forward"), this, &AFootballController::Forward);
	InputComponent->BindAxis(TEXT("Right"), this, &AFootballController::Right);
	InputComponent->BindAxis(TEXT("LookX"), this, &AFootballController::LookX);
	InputComponent->BindAxis(TEXT("LookY"), this, &AFootballController::LookY);
	InputComponent->BindAxis(TEXT("GamepadLookX"), this, &AFootballController::GamepadLookX);
	InputComponent->BindAxis(TEXT("GamepadLookY"), this, &AFootballController::GamepadLookY);
	InputComponent->BindAction(TEXT("Sprint"), IE_Pressed, this, &AFootballController::SprintOn);
	InputComponent->BindAction(TEXT("Sprint"), IE_Released, this, &AFootballController::SprintOff);
	InputComponent->BindAction(TEXT("Kick"), IE_Pressed, this, &AFootballController::StartCharge);
	InputComponent->BindAction(TEXT("Kick"), IE_Released, this, &AFootballController::ReleaseCharge);
	InputComponent->BindAction(TEXT("ResetBall"), IE_Pressed, this, &AFootballController::Reset);
	auto& B = InputComponent->BindAction(TEXT("Pause"), IE_Pressed, this, &AFootballController::PauseToggle);
	B.bExecuteWhenPaused = true;
}
bool AFootballController::InputKey(const FInputKeyEventArgs& Params)
{
	if (Params.Event==IE_Pressed || Params.Event==IE_Repeat || !FMath::IsNearlyZero(Params.AmountDepressed))
		bUsingGamepadInput=Params.IsGamepad();
	return Super::InputKey(Params);
}
void AFootballController::Forward(float V)
{
	const bool NewPress = !FMath::IsNearlyZero(V) && !FMath::IsNearlyEqual(V, MoveForward);
	MoveForward = V;
	if (Screen == EScreen::Game && Avatar)
	{
		if (NewPress)
		{
			Avatar->CancelBallTrap();
			if (Avatar->ActionInterruptible)
				Avatar->ClearAction();
		}
		// A gamepad's smoothed world intent is applied once, in Tick.
		if (Avatar->IsMovementLocked() || bUsingGamepadInput)
			return;
		const float MoveYaw = Saved && Saved->CameraMode == 2 ? -90.f : Yaw;
		Avatar->AddMovementInput(FRotator(0, MoveYaw, 0).Vector(), V);
	}
}
void AFootballController::Right(float V)
{
	const bool NewPress = !FMath::IsNearlyZero(V) && !FMath::IsNearlyEqual(V, MoveRight);
	MoveRight = V;
	if (Screen == EScreen::Game && Avatar)
	{
		if (NewPress)
		{
			Avatar->CancelBallTrap();
			if (Avatar->ActionInterruptible)
				Avatar->ClearAction();
		}
		if (Avatar->IsMovementLocked() || bUsingGamepadInput)
			return;
		const float MoveYaw = Saved && Saved->CameraMode == 2 ? -90.f : Yaw;
		Avatar->AddMovementInput(FRotationMatrix(FRotator(0, MoveYaw, 0)).GetUnitAxis(EAxis::Y), V);
	}
}
FVector AFootballController::GetMoveIntentWorld() const
{
	const float MoveYaw = Saved && Saved->CameraMode == 2 ? -90.f : Yaw;
	const FVector F = FRotator(0, MoveYaw, 0).Vector(), R = FRotationMatrix(FRotator(0, MoveYaw, 0)).GetUnitAxis(EAxis::Y);
	FVector D = F * MoveForward + R * MoveRight;
	D.Z = 0;
	return D.GetClampedToMaxSize(1.f);
}
bool AFootballController::HasDigitalMoveIntent() const
{
	return TestDigitalMoveIntent || IsInputKeyDown(EKeys::W) || IsInputKeyDown(EKeys::A) || IsInputKeyDown(EKeys::S) ||
	       IsInputKeyDown(EKeys::D);
}
void AFootballController::LookX(float V)
{
	if (Screen == EScreen::Game && Saved && Saved->CameraMode != 2)
	{
		Yaw += V * Saved->Sensitivity * 1.7f;
		if (Avatar && !FMath::IsNearlyZero(V) && FMath::Abs(FMath::FindDeltaAngleDegrees(Avatar->GetActorRotation().Yaw, Yaw)) > 65)
			Avatar->BeginTurn(Yaw);
	}
}
void AFootballController::LookY(float V)
{
	if (Screen == EScreen::Game && Saved && Saved->CameraMode != 2)
		Pitch = FMath::Clamp(Pitch + V * Saved->Sensitivity, -40.f, 5.f);
}
void AFootballController::GamepadLookX(float V)
{
	if (Screen == EScreen::Game && Saved && Saved->CameraMode != 2 && !FMath::IsNearlyZero(V))
	{
		Yaw += V * Saved->Sensitivity * 120.f * GetWorld()->GetDeltaSeconds();
		if (Avatar && FMath::Abs(FMath::FindDeltaAngleDegrees(Avatar->GetActorRotation().Yaw, Yaw)) > 65)
			Avatar->BeginTurn(Yaw);
	}
}
void AFootballController::GamepadLookY(float V)
{
	if (Screen == EScreen::Game && Saved && Saved->CameraMode != 2 && !FMath::IsNearlyZero(V))
		Pitch = FMath::Clamp(Pitch + V * Saved->Sensitivity * 90.f * GetWorld()->GetDeltaSeconds(), -40.f, 5.f);
}
void AFootballController::SprintOn()
{
	if (Screen != EScreen::Game)
		return;
	Sprint = true;
	if (Avatar)
		Avatar->GetCharacterMovement()->MaxWalkSpeed = 765;
}
void AFootballController::SprintOff()
{
	Sprint = false;
	if (!Avatar)
		return;
	auto* Mode = GetWorld()->GetAuthGameMode<AFootballMode>();
	if (BallControlHeld && (!Mode || !Mode->HasDribbleControl(Avatar)))
		BallControlHeld = false;
	Avatar->GetCharacterMovement()->MaxWalkSpeed = BallControlHeld ? 190 : 500;
}
void AFootballController::Tick(float Dt)
{
	Super::Tick(Dt);
#if !UE_BUILD_SHIPPING
	if (UI)
		UI->RunUIChecks();
#endif
	UpdateActions(Dt);
	auto* GameplayMode = GetWorld()->GetAuthGameMode<AFootballMode>();
	if (BallControlHeld)
	{
		if (!GameplayMode || !GameplayMode->HasDribbleControl(Avatar))
			BallControlOff();
	}
#if !UE_BUILD_SHIPPING
	if (bRunLatestChecks)
		RunChecks();
#endif
	if (!Avatar || !ViewCamera)
		return;
	if (Screen == EScreen::Settings && IsPaused())
		return;
	const EScreen ActiveScreen = Screen == EScreen::Settings ? SettingsReturn : Screen;
	if (!bUsingGamepadInput || ActiveScreen != EScreen::Game || Avatar->IsMovementLocked())
		SmoothedGamepadMoveInput = FVector::ZeroVector;
	else
	{
		FVector TargetMoveInput = GetMoveIntentWorld().GetClampedToMaxSize(1.f);
		const float SmoothingSpeed = TargetMoveInput.IsNearlyZero() ? 24.f : 18.f;
		SmoothedGamepadMoveInput = FMath::VInterpTo(SmoothedGamepadMoveInput, TargetMoveInput, Dt, SmoothingSpeed).GetClampedToMaxSize(1.f);
		// GetMoveIntentWorld() is already world-space and Forward/Right add nothing for the pad.
		Avatar->AddMovementInput(SmoothedGamepadMoveInput.GetSafeNormal(), SmoothedGamepadMoveInput.Size());
	}
	UpdateEditorInput(Dt);
	if (Avatar->GetActorLocation().Z < -250)
	{
		Avatar->GetCharacterMovement()->StopMovementImmediately();
		Avatar->SetActorLocation(FVector(-350, 0, 100));
		Reset();
	}
#if !UE_BUILD_SHIPPING
	if (bRunProjectChecks)
		RunProjectChecks();
#endif
	UpdateAvatarFacing(Dt, ActiveScreen, GameplayMode);
	auto P = Avatar->GetActorLocation();
	if (ActiveScreen == EScreen::Main || ActiveScreen == EScreen::Credits)
		UpdateDemo(Dt);
	UpdateCamera(Dt, ActiveScreen, GameplayMode, P);
}
void AFootballController::UpdateAvatarFacing(float Dt, EScreen ActiveScreen, AFootballMode* GameplayMode)
{
	if (auto* Movement = Avatar->GetCharacterMovement())
	{
		const bool OnBall =
		    ActiveScreen == EScreen::Game && GameplayMode && GameplayMode->HasDribbleControl(Avatar) && Avatar->CanAct() && !Charging;
		const float Now = GetWorld()->GetTimeSeconds();
		bool SprintReleaseLocked = false, SprintReleaseChase = false;
		FVector SprintChaseFacing = FVector::ZeroVector;
		if (ActiveScreen == EScreen::Game && GameplayMode && GameplayMode->Ball &&
		    (Sprint || (!GameplayMode->Possession->PossessionActive && !GetMoveIntentWorld().IsNearlyZero())) && Avatar->CanAct() &&
		    !GameplayMode->Possession->BallStopRequested && !GameplayMode->Possession->BallStopped && !GameplayMode->Referee->ShotInFlight &&
		    !GameplayMode->Possession->SprintKickPending && Now >= GameplayMode->Possession->SprintContactUntil &&
		    GameplayMode->IsRecoverableSprintTouch(Avatar))
		{
			FVector Gap = GameplayMode->Ball->GetComponentLocation() - Avatar->GetActorLocation();
			const float GapDistance = Gap.Size2D();
			SprintReleaseLocked = Gap.Z <= -35.f && GapDistance > 48.f && GapDistance < 420.f;
			// Preserve an aligned foot approach, but never lock a sideways/backward body.
			const bool NeedsFacingCorrection = FVector::DotProduct(Avatar->GetActorForwardVector(), Gap.GetSafeNormal2D()) < .707107f;
			SprintReleaseChase = SprintReleaseLocked && (GapDistance > 80.f || NeedsFacingCorrection);
			if (SprintReleaseChase)
				SprintChaseFacing = Gap.GetSafeNormal2D();
		}
		const float Speed = Avatar->GetVelocity().Size2D();
		const float SpeedAlpha = FMath::Clamp((Speed - 180.f) / (765.f - 180.f), 0.f, 1.f);
		const bool SprintCutFacing =
		    Sprint && OnBall && FVector::DotProduct(Avatar->GetActorForwardVector(), GetMoveIntentWorld().GetSafeNormal2D()) < .3f;
		const float DesiredYawRate =
		    SprintReleaseChase ? 360.f
		    : OnBall ? (BallControlHeld ? 660.f : FMath::Max(FMath::Lerp(480.f, 200.f, SpeedAlpha), SprintCutFacing ? 420.f : 0.f))
		             : 520.f;
		Movement->RotationRate.Yaw = FMath::FInterpTo(Movement->RotationRate.Yaw, DesiredYawRate, Dt, 10.f);
		if (SprintReleaseChase)
		{
			Movement->bOrientRotationToMovement = false;
			if (!SprintChaseFacing.IsNearlyZero())
			{
				const float CurrentYaw = Avatar->GetActorRotation().Yaw;
				const float TargetYaw = SprintChaseFacing.Rotation().Yaw;
				const float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentYaw, TargetYaw);
				const float EaseAlpha = 1.f - FMath::Exp(-8.f * Dt);
				const float MaxStep = DesiredYawRate * Dt;
				const float Step = FMath::Clamp(DeltaYaw * EaseAlpha, -MaxStep, MaxStep);
				const float NewYaw = FMath::UnwindDegrees(CurrentYaw + Step);
				Avatar->SetActorRotation(FRotator(0, NewYaw, 0));
			}
		}
		else if (SprintReleaseLocked)
		{
			Movement->bOrientRotationToMovement = false;
		}
		else if (OnBall)
		{
			Movement->bOrientRotationToMovement = false;
			// A trapped ball is already secure. Do not rotate the player back toward the
			// previous dribble heading after a deliberate standing Turn_Step.
			if (!GameplayMode->Possession->BallStopRequested && !GameplayMode->Possession->BallStopped && !Avatar->TurningInPlace)
			{
				FVector Facing = GameplayMode->Possession->DribbleDirection.GetSafeNormal2D();
				if (Facing.IsNearlyZero())
					Facing = Avatar->GetVelocity().GetSafeNormal2D();
				if (!Facing.IsNearlyZero())
				{
					const float CurrentYaw = Avatar->GetActorRotation().Yaw;
					const float NewYaw = FMath::FixedTurn(CurrentYaw, Facing.Rotation().Yaw, DesiredYawRate * Dt);
					Avatar->SetActorRotation(FRotator(0, NewYaw, 0));
				}
			}
		}
		else if (Avatar->CanAct() && !Avatar->TurningInPlace && !Avatar->PreviewAnimation)
			Movement->bOrientRotationToMovement = true;
	}
}
void AFootballController::JumpPressed()
{
	if (Screen != EScreen::Game || !Avatar || SpaceHeld)
		return;
	SpaceHeld = true;
	SpaceStarted = GetWorld()->GetTimeSeconds();
	GoalHoldRequested = false;
	if (Avatar->CanAct() && WasInputKeyJustPressed(EKeys::RightMouseButton))
	{
		SlidePressed();
		if (!Avatar->CanAct())
			return;
	}
	if (!GoalCelebrationUsed && SpaceStarted < GoalUntil)
	{
		GoalSpacePending = true;
		return;
	}
	if (!Avatar->CanAct())
		return;
	Avatar->StartPhysicalJump();
}
void AFootballController::JumpReleased()
{
	if (GoalSpacePending && SpaceHeld && Screen == EScreen::Game)
	{
		GoalHoldRequested |= GetWorld()->GetTimeSeconds() - SpaceStarted >= 1.f;
		BeginCelebration(GoalHoldRequested);
	}
	SpaceHeld = false;
	if (Avatar)
		Avatar->StopJumping();
}
void AFootballController::TurnEditor(float V)
{
	if (Screen == EScreen::Editor && Avatar && !FMath::IsNearlyZero(V))
		Avatar->AddActorWorldRotation(FRotator(0, -V * 90 * GetWorld()->GetDeltaSeconds(), 0));
}
void AFootballController::ZoomEditor(float V)
{
	if (Screen == EScreen::Editor)
		EditorZoom = FMath::Clamp(EditorZoom + V * .15f, 0.f, 1.f);
}
void AFootballController::EditorGamepadTurn(float V)
{
	if (Screen == EScreen::Editor && Avatar && !FMath::IsNearlyZero(V))
		Avatar->AddActorWorldRotation(FRotator(0, -V * 120.f * GetWorld()->GetDeltaSeconds(), 0));
}
void AFootballController::EditorGamepadZoom(float V)
{
	if (Screen == EScreen::Editor && !FMath::IsNearlyZero(V))
		EditorZoom = FMath::Clamp(EditorZoom + V * .9f * GetWorld()->GetDeltaSeconds(), 0.f, 1.f);
}
void AFootballController::UpdateEditorInput(float Dt)
{
	if (Screen != EScreen::Editor)
	{
		Dragging = false;
		MouseWasDown = false;
		return;
	}
	float X, Y;
	if (!GetMousePosition(X, Y))
		return;
	bool Down = IsInputKeyDown(EKeys::LeftMouseButton);
	if (Down && !MouseWasDown && !(UI && UI->IsPointerOverPanel()))
	{
		FVector Center = Avatar->GetActorLocation() + FVector(0, 0, 2);
		FVector2D Lo(1e9, 1e9), Hi(-1e9, -1e9);
		for (int I = 0; I < 8; I++)
		{
			FVector2D Pixel;
			ProjectWorldLocationToScreen(Center + FVector(I & 1 ? 95 : -95, I & 2 ? 65 : -65, I & 4 ? 100 : -96), Pixel);
			Lo.X = FMath::Min(Lo.X, Pixel.X);
			Lo.Y = FMath::Min(Lo.Y, Pixel.Y);
			Hi.X = FMath::Max(Hi.X, Pixel.X);
			Hi.Y = FMath::Max(Hi.Y, Pixel.Y);
		}
		int W, H;
		GetViewportSize(W, H);
		Dragging = X > W * .40f && Y < H * .86f && X >= Lo.X && X <= Hi.X && Y >= Lo.Y && Y <= Hi.Y;
	}
	if (Down && Dragging)
		Avatar->AddActorWorldRotation(FRotator(0, -(X - LastMouseX) * .45f, 0));
	if (!Down)
		Dragging = false;
	LastMouseX = X;
	MouseWasDown = Down;
}
