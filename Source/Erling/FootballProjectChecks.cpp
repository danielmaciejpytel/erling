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
#if !UE_BUILD_SHIPPING
void AFootballController::RunProjectChecks()
{
	static bool DemoReturned = false, DemoShotFacedGoal = false;
	if (TestStage == 27 || TestStage == 28)
	{
		DemoReturned |= DemoDirection == -1;
		if (PendingShot)
			DemoShotFacedGoal |= Avatar->GetActorForwardVector().X * DemoDirection > .9f;
	}
#if WITH_EDITOR
	if (FAssetCompilingManager::Get().GetNumRemainingAssets() > 0)
		return;
#endif
	if (TestStarted == 0)
		TestStarted = FPlatformTime::Seconds() + 3;
	if (TestStage == 12 || TestStage == 13 || TestStage == 16 || TestStage == 17)
		Forward(1);
	if (TestStage == 17 && Avatar->GetCharacterMovement()->IsFalling())
		TestJumpObserved = true;
	if (FPlatformTime::Seconds() < TestStarted)
		return;
	auto Check = [this](const FString& Name, bool Ok)
	{
		FString Line = FString::Printf(TEXT("%s: %s\n"), *Name, Ok ? TEXT("PASS") : TEXT("FAIL"));
		TestReport += Line;
		UE_LOG(LogTemp, Display, TEXT("ERLING_CHECK %s"), *Line);
	};
	auto Shot = [](const FString& Name)
	{
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots") / (Name + TEXT(".png")), true, false);
	};
	auto Mode = GetWorld()->GetAuthGameMode<AFootballMode>();
	UE_LOG(LogTemp, Display, TEXT("CHECK_POSITION stage=%d avatar=%s ball=%s"), TestStage, *Avatar->GetActorLocation().ToString(),
	    Mode && Mode->Ball ? *Mode->Ball->GetComponentLocation().ToString() : TEXT("none"));
	switch (TestStage)
	{
		case 0:
			Selection = {1, 0, 1, 1, 1};
			Avatar->ApplyKit(Selection, Catalog);
			Check(TEXT("catalog_5_categories"), Catalog.Num() == 5);
			Check(TEXT("body_loaded"), Avatar->GetMesh()->GetSkeletalMeshAsset() != nullptr);
			Check(TEXT("animations_loaded"), Avatar->Animations.Num() == 25);
			Shot(TEXT("01_Main"));
			break;
		case 1:
			ChangeScreen(EScreen::Editor);
			Cycle(0, -1);
			Cycle(1, 1);
			Check(TEXT("live_bald"), Avatar->Pieces.Num() == 3);
			Check(TEXT("face_material"), Avatar->FaceMaterial != nullptr);
			break;
		case 2:
		{
			TArray<FFinalSkinVertex> Vertices;
			Avatar->GetMesh()->GetCPUSkinnedVertices(Vertices, 0);
			float MinZ = MAX_flt;
			for (const auto& V : Vertices)
				MinZ = FMath::Min(
				    MinZ, static_cast<float>(Avatar->GetMesh()->GetComponentTransform().TransformPosition(FVector(V.Position)).Z));
			UE_LOG(LogTemp, Display, TEXT("FOOT_MIN_Z %f"), MinZ);
		}
			Shot(TEXT("02_Editor"));
			SaveAppearance();
			{
				auto Loaded = Cast<UFootballSave>(UGameplayStatics::LoadGameFromSlot(ProfileSlot(), 0));
				Check(TEXT("appearance_persistence"), Loaded && Loaded->Appearance == Selection);
			}
			break;
		case 3:
			Cycle(2, -1);
			BackFromEditor();
			Check(TEXT("cancel_draft"), Selection == Saved->Appearance);
			ChangeScreen(EScreen::Game);
			Check(TEXT("game_mode"), Screen == EScreen::Game && !IsPaused());
			break;
		case 4:
			Shot(TEXT("03_Play"));
			// Kick in this stage. The screenshot stalls the frame longer than the
			// wall-clock delay to stage 5, so a game-time timer would not fire first.
			if (Mode && Mode->Ball)
			{
				Mode->Ball->SetWorldLocation(Avatar->GetActorLocation() + FVector(140, 0, -65), false, nullptr, ETeleportType::TeleportPhysics);
				Mode->Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);
				Mode->Kick(Avatar);
			}
			break;
		case 5:
			if (Mode && Mode->Ball)
			{
				Check(TEXT("ball_kick_impulse"), Mode->Ball->GetPhysicsLinearVelocity().Size() > 100);
				Mode->ResetBall();
				Mode->PreviousBall = FVector(2600, 0, 90);
				Mode->Ball->SetWorldLocation(FVector(2800, 0, 90), false, nullptr, ETeleportType::TeleportPhysics);
				int32 Before = Mode->Goals;
				Mode->Tick(.01f);
				Check(TEXT("goal_count"), Mode->Goals == Before + 1);
				Mode->Tick(.01f);
				Check(TEXT("goal_debounce"), Mode->Goals == Before + 1);
			}
			PauseToggle();
			Check(TEXT("pause"), IsPaused());
			break;
		case 6:
			Shot(TEXT("04_Pause"));
			break;
		case 7:
			PauseToggle();
			Check(TEXT("resume"), !IsPaused());
			ChangeScreen(EScreen::Editor);
			Cycle(0, 1);
			Cycle(1, 1);
			break;
		case 8:
			Shot(TEXT("05_Editor_Hair"));
			break;
		case 9:
			for (int C = 2; C < 5; C++)
				Cycle(C, -1);
			Check(TEXT("remove_all_clothes"), Avatar->Pieces.Num() == 4);
			break;
		case 10:
			Shot(TEXT("06_Body"));
			break;
		case 11:
			for (int C = 2; C < 5; C++)
				Cycle(C, 1);
			Check(TEXT("restore_all_clothes"), Avatar->Pieces.Num() == 7);
			ChangeScreen(EScreen::Game);
			TestPosition = Avatar->GetActorLocation();
			break;
		case 12:
			Check(TEXT("walking_input"),
			    FVector::Dist2D(TestPosition, Avatar->GetActorLocation()) > 200 && Avatar->CurrentAnimation == TEXT("Run"));
			TestPosition = Avatar->GetActorLocation();
			SprintOn();
			break;
		case 13:
			Check(TEXT("sprint_input"),
			    FVector::Dist2D(TestPosition, Avatar->GetActorLocation()) > 600 && Avatar->CurrentAnimation == TEXT("Sprint"));
			SprintOff();
			Avatar->GetCharacterMovement()->StopMovementImmediately();
			if (Mode && Mode->Ball)
			{
				Mode->ResetBall();
				Mode->Ball->SetWorldLocation(Avatar->GetActorLocation() + Avatar->GetActorForwardVector() * 140 - FVector(0, 0, 65), false,
				    nullptr, ETeleportType::TeleportPhysics);
				Mode->Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);
			}
			Kick();
			break;
		case 14:
			Check(TEXT("kick_action"), Mode && Mode->Ball && Mode->Ball->GetPhysicsLinearVelocity().Size() > 100);
			Shot(TEXT("07_Action"));
			break;
		case 15:
			Mode->ResetBall();
			Mode->LastShot = -10;
			Avatar->SetActorLocation(FVector(-350, 0, 100));
			Avatar->GetCharacterMovement()->StopMovementImmediately();
			Mode->Ball->SetWorldLocation(FVector(-240, 0, 22));
			SprintOn();
			break;
		case 16:
		{
			const float BallSpeed = Mode->Ball->GetPhysicsLinearVelocity().Size(), PlayerSpeed = Avatar->GetVelocity().Size2D(),
			            BallZ = Mode->Ball->GetComponentLocation().Z,
			            Gap = FVector::Dist2D(Mode->Ball->GetComponentLocation(), Avatar->GetActorLocation());
			UE_LOG(LogTemp, Display, TEXT("BASELINE_SPRINT_DRIBBLE ball_speed=%f player_speed=%f ball_z=%f gap=%f"), BallSpeed, PlayerSpeed,
			    BallZ, Gap);
			Check(TEXT("sprint_dribble_controlled"), BallSpeed < 1000 && PlayerSpeed > 700 && BallZ < 60 && Gap < 175);
		}
			GoalCelebrationUsed = true;
			JumpPressed();
			break;
		case 17:
			Check(TEXT("jump_and_land"), TestJumpObserved && !Avatar->GetCharacterMovement()->IsFalling());
			JumpReleased();
			SprintOff();
			ChangeScreen(EScreen::Editor);
			Selection[1] = 3;
			Avatar->ApplyKit(Selection, Catalog);
			ZoomEditor(20);
			Check(TEXT("zoom_limit"), EditorZoom == 1);
			{
				float Before = Avatar->GetActorRotation().Yaw;
				TurnEditor(1);
				Check(TEXT("editor_rotation"), !FMath::IsNearlyEqual(Before, Avatar->GetActorRotation().Yaw));
			}
			break;
		case 18:
			Shot(TEXT("08_Editor_Zoom"));
			Check(TEXT("music_loaded_playing"), Music && Music->Sound && Music->IsPlaying());
			UpdateMusicVolume(0);
			Check(TEXT("music_mute"), Music && Music->VolumeMultiplier == 0);
			UpdateMusicVolume(.5f);
			Saved->Quality = 0;
			ApplyQuality();
			break;
		case 19:
			Shot(TEXT("09_Low"));
			break;
		case 20:
			Saved->Quality = 3;
			ApplyQuality();
			break;
		case 21:
			Shot(TEXT("10_Ultra"));
			Check(TEXT("graphics_high"), UGameUserSettings::GetGameUserSettings()->GetShadowQuality() == 3);
			Check(TEXT("music_continuity"), Music && Music->IsPlaying());
			break;
		case 22:
			ChangeScreen(EScreen::Game);
			PauseToggle();
			Check(TEXT("music_pause_continuity"), Music && Music->IsPlaying() && Music->bIsUISound);
			break;
		case 23:
		{
			ChangeScreen(EScreen::Game);
			Mode->ResetBall();
			Mode->Ball->SetWorldLocation(FVector(2800, 1000, 22));
			Mode->PreviousBall = FVector(2650, 1000, 22);
			Mode->ShotInFlight = true;
			Mode->Tick(.01f);
			Check(TEXT("miss_disappears"), Mode->BallHidden);
			Mode->ResetBall();
			Mode->Ball->SetWorldLocation(FVector(2800, 1000, 22));
			Mode->Tick(.01f);
			Check(TEXT("carried_ball_outside_stays"), !Mode->BallHidden);
			Check(TEXT("goal_sound_loaded"), Mode->GoalSound != nullptr);
			FVector Pos(0, 0, 22), Dir(1, 0, 0);
			auto Pass = AFootballMode::ShotVelocity(Pos, Dir, 0);
			Check(TEXT("tap_is_ground_pass"), Pass.Z == 0);
			for (int C = 1; C <= 3; C++)
			{
				auto V = AFootballMode::ShotVelocity(Pos, Dir, C * .5f);
				float T = ErlingPitch::GoalLineX / V.X;
				float Height = 22 + V.Z * T - 490 * T * T;
				Check(FString::Printf(TEXT("charge_%d_height"), C), FMath::Abs(Height - (C == 1 ? 130 : C == 2 ? 220 : 420)) < 2);
			}
			Mode->ResetBall();
			Mode->Ball->SetWorldLocation(Avatar->GetActorLocation() + Avatar->GetActorForwardVector() * 125 - FVector(0, 0, 74), false,
			    nullptr, ETeleportType::TeleportPhysics);
			StartCharge();
			ChargeStarted = GetWorld()->GetTimeSeconds() - 2;
			ReleaseCharge();
			Check(TEXT("release_fires_charge"), !Charging && Avatar->Action == AFootballPlayer::EAction::Kick);
			break;
		}
		case 24:
			ChangeScreen(EScreen::Credits);
			break;
		case 25:
			Shot(TEXT("11_Credits"));
			break;
		case 26:
			ChangeScreen(EScreen::Main);
			DemoDirection = 1;
			DemoPhase = 0;
			DemoShotTargetX = 500.f;
			Mode->ResetBall();
			Avatar->GetCharacterMovement()->StopMovementImmediately();
			Avatar->SetActorRotation(FRotator::ZeroRotator);
			Avatar->SetActorLocation(FVector(-350, 0, 98));
			break;
		case 27:
			Shot(TEXT("12_Demo_Right"));
			break;
		case 28:
			Check(TEXT("demo_alternates"), DemoReturned);
			Check(TEXT("demo_faces_shot"), DemoShotFacedGoal);
			Shot(TEXT("13_Demo_Left"));
			break;
		case 29:
			ChangeScreen(EScreen::Game);
			Mode->ResetBall();
			Mode->Scored = true;
			// This tick still runs the demo update, which zeroes the ball's velocity.
			// Launch it on the next tick, once the game screen is active.
			GetWorldTimerManager().SetTimerForNextTick(
			    [Mode]()
			    {
				    Mode->Ball->SetWorldLocation(FVector(2800, 0, 160), false, nullptr, ETeleportType::TeleportPhysics);
				    Mode->Ball->SetPhysicsLinearVelocity(FVector(3200, 0, 0));
			    });
			break;
		case 30:
			Check(TEXT("net_stops_ball"), Mode->Ball->GetComponentLocation().X < 2980 && Mode->Ball->GetComponentLocation().X > 2800 &&
			                                  Mode->Ball->GetPhysicsLinearVelocity().Size2D() < 100);
			Check(TEXT("net_ball_lands"), Mode->Ball->GetComponentLocation().Z < 40);
			Check(TEXT("all_effects_loaded"), Effects.Num() == 5 && Effects.FindRef(TEXT("jump")) && Effects.FindRef(TEXT("kick")) &&
			                                      Effects.FindRef(TEXT("fail")) && Effects.FindRef(TEXT("click")));
			UpdateEffectsVolume(.5f);
			PlayEffect(TEXT("goal"), 1.5f);
			Check(TEXT("goal_gain_150_percent"),
			    !ActiveEffects.IsEmpty() && FMath::IsNearlyEqual(ActiveEffects.Last()->VolumeMultiplier, .75f));
			UpdateEffectsVolume(0);
			Check(TEXT("effects_mute"), !ActiveEffects.IsEmpty() && ActiveEffects.Last()->VolumeMultiplier == 0);
			Check(TEXT("effects_do_not_mute_music"), Music && Music->VolumeMultiplier > 0 && Music->IsPlaying());
			UpdateEffectsVolume(.8f);
			SaveSettings();
			{
				auto Loaded = Cast<UFootballSave>(UGameplayStatics::LoadGameFromSlot(ProfileSlot(), 0));
				Check(TEXT("effects_volume_saved"), Loaded && FMath::IsNearlyEqual(Loaded->EffectsVolume, .8f));
			}
			SettingsReturn = EScreen::Game;
			ChangeScreen(EScreen::Settings);
			break;
		case 31:
			Shot(TEXT("14_Settings_Effects"));
			break;
		default:
			FFileHelper::SaveStringToFile(TestReport, *(FPaths::ProjectSavedDir() / TEXT("runtime_checks.txt")));
			FPlatformMisc::RequestExit(false);
			break;
	}
	TestStage++;
	TestStarted = FPlatformTime::Seconds() + (TestStage == 5 ? .35 : TestStage == 14 ? .5 : 2.0);
}
#endif // !UE_BUILD_SHIPPING
