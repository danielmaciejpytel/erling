#include "Football.h"
#include "ErlingAnimation.h"
#include "Animation/AnimSequence.h"
#include "Animation/MorphTarget.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "UnrealClient.h"
#include "SkeletalRenderPublic.h"

void AFootballController::RunChecks()
{
    const double Now=GetWorld()->GetTimeSeconds();
    static bool JumpModelVisible=true,DemoTwoSteps=true,DemoContinuous=true;
    static int DemoShots=0;
    static bool DemoLeft=false,DemoRight=false,DemoWasPending=false;
    static FVector LastDemoBall=FVector::ZeroVector;
    static int PreviousDemoPhase=0,DemoTurns=0;
    static bool DemoTurnsPromptly=true,DemoKeepsShotInFlight=true;
    if(Avatar&&Avatar->PhysicalJump&&Avatar->JumpElapsed>.03f)
    {
        const float Offset=Avatar->GetMesh()->GetSocketLocation(TEXT("root")).Z-(Avatar->GetActorLocation().Z-96);
        if(FMath::Abs(Offset)>=100)UE_LOG(LogTemp,Warning,TEXT("JUMP_OFFSET clip=%s time=%f offset=%f"),*Avatar->CurrentAnimation,Avatar->AnimationTime,Offset);
        JumpModelVisible&=FMath::Abs(Offset)<100;
    }
    if(TestStage_Latest==49&&Avatar)
    {
        auto* M=GetWorld()->GetAuthGameMode<AFootballMode>();
        if(PreviousDemoPhase==1&&DemoPhase==3)
        {
            DemoTurns++;DemoTurnsPromptly&=Now-DemoShotAt<.85f;
            DemoKeepsShotInFlight&=M->ShotInFlight&&!PendingShot;
            UE_LOG(LogTemp,Display,TEXT("DEMO_TURN delay_from_shot_start=%f x=%f"),Now-DemoShotAt,Avatar->GetActorLocation().X);
        }
        PreviousDemoPhase=DemoPhase;
        if(PendingShot&&!DemoWasPending)
        {
            DemoShots++;DemoLeft|=Avatar->CurrentAnimation==TEXT("Kick_Left");DemoRight|=Avatar->CurrentAnimation==TEXT("Kick_Right");
            DemoTwoSteps&=Now-DemoReceiveAt>=.6f&&FVector::Dist2D(DemoReceivePosition,Avatar->GetActorLocation())>=250;
            UE_LOG(LogTemp,Display,TEXT("DEMO_SHOT foot=%s x=%f receive_time=%f travel=%f"),*Avatar->CurrentAnimation,Avatar->GetActorLocation().X,Now-DemoReceiveAt,FVector::Dist2D(DemoReceivePosition,Avatar->GetActorLocation()));
        }
        if(PendingShot&&DemoWasPending)DemoContinuous&=FVector::Dist(LastDemoBall,M->Ball->GetComponentLocation())<60;
        DemoWasPending=PendingShot;LastDemoBall=M->Ball->GetComponentLocation();
    }
    if(TestStage_Latest==19||TestStage_Latest==20)Forward(1);
    if(TestAt==0){if(FParse::Param(FCommandLine::Get(),TEXT("DemoOnly")))TestStage_Latest=46;TestAt=Now+4;return;}
    if(Now<TestAt)return;
    auto* Mode=GetWorld()->GetAuthGameMode<AFootballMode>();
    if(!Avatar||!Mode||!Mode->Ball)return;
    float Wait=.8f;
    auto Check=[this](const TCHAR* Label,bool Pass){const FString Line=FString::Printf(TEXT("%s: %s\n"),Label,Pass?TEXT("PASS"):TEXT("FAIL"));Report+=Line;UE_LOG(LogTemp,Display,TEXT("CHECK %s"),*Line);};
    auto Screenshot=[](const TCHAR* Name){FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots_Latest")/(FString(Name)+TEXT(".png")),true,false);};
    auto PlaceBall=[&](){Mode->ResetBall();Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+Avatar->GetActorForwardVector()*125-FVector(0,0,74),false,nullptr,ETeleportType::TeleportPhysics);Mode->Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);};
    auto ResetPlayer=[&](){CancelPendingActions();Avatar->ClearAction();Avatar->PhysicalJump=false;Avatar->SetAnimation(TEXT("Idle_Breathe"));Avatar->AnimationBlend=1;Avatar->GetCharacterMovement()->StopMovementImmediately();Avatar->SetActorLocation(FVector(-350,0,100));Avatar->SetActorRotation(FRotator::ZeroRotator);KickCooldown=0;};
    auto VisiblePose=[&](const TCHAR* Label)
    {
        TArray<USkeletalMeshComponent*> Parts=Avatar->Pieces;Parts.Add(Avatar->GetMesh());Parts.Add(Avatar->Face);
        float Min=MAX_flt,Max=-MAX_flt;
        for(auto* Part:Parts){TArray<FFinalSkinVertex> Vertices;Part->GetCPUSkinnedVertices(Vertices,0);for(const auto& V:Vertices){const float Z=Part->GetComponentTransform().TransformPosition(FVector(V.Position)).Z;Min=FMath::Min(Min,Z);Max=FMath::Max(Max,Z);}}
        UE_LOG(LogTemp,Display,TEXT("VISIBLE_POSE %s z=%f..%f actor=%f"),Label,Min,Max,Avatar->GetActorLocation().Z);
        Check(Label,FMath::IsFinite(Min)&&Min>-5&&Max<400&&Max-Min>80);
    };
    switch(TestStage_Latest)
    {
    case 0:
        Check(TEXT("all_24_animations"),Avatar->Animations.Num()==24);
        Check(TEXT("current_body"),Avatar->GetMesh()->GetSkeletalMeshAsset()->GetPathName().EndsWith(TEXT("SK_body.SK_body")));
        Check(TEXT("native_blending"),Cast<UErlingAnimInstance>(Avatar->GetMesh()->GetAnimInstance())!=nullptr);
        ChangeScreen(EScreen::Editor);Selection={1,0,1,1,1};Avatar->ApplyKit(Selection,Catalog);break;
    case 1:
        Screenshot(TEXT("01_Creator"));Check(TEXT("wardrobe_complete"),Avatar->Pieces.Num()==7&&Catalog.Num()==5);
        for(auto* P:Avatar->Pieces)if(P->GetSkeletalMeshAsset()->GetName()==TEXT("SK_shirt"))Check(TEXT("shirt_correctives_imported"),P->GetSkeletalMeshAsset()->GetMorphTargets().Num()>3800);
        ChangeScreen(EScreen::Game);ResetPlayer();PlaceBall();FlipChance=1;Avatar->GetCharacterMovement()->Velocity=FVector(510,0,0);TestPosition=Avatar->GetActorLocation();FireShot(1.f);
        Check(TEXT("flip_twice_as_fast"),FMath::IsNearlyEqual(Avatar->PlaybackRate,2.6f));
        Check(TEXT("underbar_selects_flip"),Avatar->Action==AFootballPlayer::EAction::Flip);Wait=.55f;break;
    case 2:
        Check(TEXT("flip_releases_ball_at_contact"),Mode->ShotInFlight&&!PendingShot);
        Check(TEXT("flip_locks_movement"),Avatar->IsMovementLocked());Screenshot(TEXT("02_FlipStart"));Wait=.9f;break;
    case 3:Screenshot(TEXT("03_FlipAir"));Wait=1.6f;break;
    case 4:
        Check(TEXT("flip_unlocks"),!Avatar->IsMovementLocked());Check(TEXT("flip_travels_forward"),Avatar->GetActorLocation().X-TestPosition.X>500);ResetPlayer();PlaceBall();
        TripChance=1;Avatar->GetCharacterMovement()->Velocity=FVector(510,0,0);FireShot(1.5f);
        Check(TEXT("overbar_schedules_trip"),PendingTrip);Wait=.65f;break;
    case 5:
        Check(TEXT("trip_started"),Avatar->Action==AFootballPlayer::EAction::Trip);
        Check(TEXT("trip_keeps_entry_velocity"),FMath::IsNearlyEqual(Avatar->ActionVelocity.Size2D(),510.f,8.f));
        Forward(-1);Check(TEXT("trip_blocks_input"),Avatar->GetPendingMovementInputVector().IsNearlyZero());
        Screenshot(TEXT("04_Trip"));Wait=2.5f;break;
    case 6:
        Check(TEXT("trip_completes_and_unlocks"),Avatar->Action==AFootballPlayer::EAction::None);
        ResetPlayer();Mode->ResetBall();Mode->Ball->SetWorldLocation(FVector(1500,1000,22));
        Avatar->GetCharacterMovement()->Velocity=FVector(510,0,0);Sprint=false;TestPosition=Avatar->GetActorLocation();SlidePressed();
        Check(TEXT("run_slide_started"),Avatar->Action==AFootballPlayer::EAction::Slide&&Avatar->SlideDistance==520);
        Check(TEXT("slide_twice_as_fast"),Avatar->ActionDuration<.76f);Wait=.6f;break;
    case 7:Screenshot(TEXT("05_Slide"));Wait=1.2f;break;
    case 8:
        {const float Travel=FVector::Dist2D(TestPosition,Avatar->GetActorLocation());UE_LOG(LogTemp,Display,TEXT("RUN_SLIDE_TRAVEL distance=%f expected=520"),Travel);Check(TEXT("run_slide_distance"),FMath::Abs(Travel-520)<25);}
        ResetPlayer();Avatar->GetCharacterMovement()->Velocity=FVector(765,0,0);Sprint=true;TestPosition=Avatar->GetActorLocation();SlidePressed();
        Check(TEXT("sprint_slide_distance_selected"),Avatar->SlideDistance==650);Wait=1.8f;break;
    case 9:
        Check(TEXT("sprint_slide_longer"),FMath::Abs(FVector::Dist2D(TestPosition,Avatar->GetActorLocation())-650)<25);
        ResetPlayer();PlaceBall();Avatar->GetCharacterMovement()->Velocity=FVector(510,0,0);SlidePressed();
        Check(TEXT("ball_blocks_slide"),Avatar->Action==AFootballPlayer::EAction::None);
        Avatar->GetCharacterMovement()->StopMovementImmediately();OnGoal(Avatar);JumpPressed();JumpReleased();
        Check(TEXT("goal_tap_joy_jump"),Avatar->CurrentAnimation==TEXT("JoyJump_Run")&&GoalCelebrationUsed);Wait=2.7f;break;
    case 10:
        OnGoal(Avatar);JumpPressed();SpaceStarted-=1.01f;UpdateActions(.01f);
        Check(TEXT("goal_hold_random_emote"),GoalCelebrationUsed&&Avatar->Action==AFootballPlayer::EAction::Emote&&Avatar->CurrentAnimation!=TEXT("JoyJump_Run"));
        Screenshot(TEXT("06_Celebration"));Wait=3.2f;break;
    case 11:
        ResetPlayer();GoalCelebrationUsed=false;GoalUntil=GetWorld()->GetTimeSeconds()-.01f;JumpPressed();
        Check(TEXT("expired_goal_normal_jump"),Avatar->PhysicalJump&&!GoalSpacePending);Wait=.22f;break;
    case 12:
        Check(TEXT("physical_jump_airborne"),Avatar->GetCharacterMovement()->IsFalling());VisiblePose(TEXT("standing_jump_mesh_visible"));Screenshot(TEXT("07_Jump"));Wait=1.f;break;
    case 13:
        Check(TEXT("jump_lands"),!Avatar->GetCharacterMovement()->IsFalling());ResetPlayer();
        Avatar->StartAction(AFootballPlayer::EAction::Fall,TEXT("Fall_Backward"),1.4f);Wait=1.5f;break;
    case 14:
        Check(TEXT("back_fall_chains_getup"),Avatar->Action==AFootballPlayer::EAction::GetUp&&Avatar->CurrentAnimation==TEXT("GetUp_Back"));Wait=2.f;break;
    case 15:
        Check(TEXT("getup_unlocks"),!Avatar->IsMovementLocked());ResetPlayer();PlaceBall();TripChance=0;FlipChance=0;FireShot(1.f);
        Check(TEXT("normal_shot_no_special"),Avatar->Action==AFootballPlayer::EAction::Kick&&!PendingTrip);Wait=.45f;break;
    case 16:
        Check(TEXT("normal_shot_contact"),Mode->ShotInFlight&&!PendingShot);ResetPlayer();OnGoal(nullptr);GoalCelebrationUsed=true;OnGoal(nullptr);
        Check(TEXT("other_scorer_no_reward"),GoalCelebrationUsed);TripChance=.33f;FlipChance=.5f;
        ChangeScreen(EScreen::Editor);Selection={1,3,1,1,1};Avatar->ApplyKit(Selection,Catalog);Wait=1.f;break;
    case 17:
        Screenshot(TEXT("08_CreatorFinal"));
        Check(TEXT("creator_after_gameplay"),Avatar->Pieces.Num()==7&&Avatar->FaceMaterial!=nullptr&&!Avatar->IsMovementLocked());
        {TArray<USkeletalMeshComponent*> Components=Avatar->Pieces;Components.Add(Avatar->GetMesh());Components.Add(Avatar->Face);
         float Min=MAX_flt,Max=-MAX_flt;bool Materials=true,Curves=false;
         for(auto* Piece:Components){Materials&=Piece->GetMaterial(0)!=nullptr;TArray<FFinalSkinVertex> V;Piece->GetCPUSkinnedVertices(V,0);for(const auto& P:V){const float Z=Piece->GetComponentTransform().TransformPosition(FVector(P.Position)).Z;Min=FMath::Min(Min,Z);Max=FMath::Max(Max,Z);}if(Piece->GetSkeletalMeshAsset()->GetName()==TEXT("SK_shirt"))for(const auto& Morph:Piece->GetSkeletalMeshAsset()->GetMorphTargets())if(Piece->GetAnimInstance()->GetCurveValue(Morph->GetFName())>.001f){Curves=true;break;}}
         UE_LOG(LogTemp,Display,TEXT("VISUAL_BOUNDS z=%f..%f actor=%s root=%s"),Min,Max,*Avatar->GetActorLocation().ToString(),*Avatar->GetMesh()->GetSocketLocation(TEXT("root")).ToString());
         Check(TEXT("materials_assigned"),Materials);Check(TEXT("live_corrective_curves"),Curves);Check(TEXT("feet_on_pitch"),Min>=-1&&Min<6);}
        break;
    case 18:
        ChangeScreen(EScreen::Game);ResetPlayer();Mode->ResetBall();Mode->Ball->SetWorldLocation(FVector(1500,1000,22));WalkOn();Wait=1.2f;break;
    case 19:
        Check(TEXT("walk_modifier_and_animation"),FMath::Abs(Avatar->GetVelocity().Size2D()-180)<5&&Avatar->CurrentAnimation==TEXT("Walk"));WalkOff();SprintOn();Wait=1.2f;break;
    case 20:
        Check(TEXT("sprint_animation_and_speed"),Avatar->GetVelocity().Size2D()>750&&Avatar->CurrentAnimation==TEXT("Sprint"));SprintOff();Avatar->GetCharacterMovement()->StopMovementImmediately();Avatar->ConsumeMovementInputVector();Avatar->BeginTurn(90);Wait=.7f;break;
    case 21:
        Check(TEXT("turn_step_finishes"),!Avatar->TurningInPlace&&FMath::Abs(FMath::FindDeltaAngleDegrees(Avatar->GetActorRotation().Yaw,90))<2);
        ChangeScreen(EScreen::Editor);CycleAnimationPreview(1);Wait=.4f;break;
    case 22:
        Check(TEXT("creator_animation_preview"),Avatar->PreviewAnimation&&Avatar->AnimationTime>.1f);Cycle(0,-1);
        Check(TEXT("preview_preserves_wardrobe_switch"),Avatar->PreviewAnimation&&Avatar->Pieces.Num()==3);Cycle(0,1);
        Screenshot(TEXT("09_Preview"));break;
    case 23:
        Avatar->PreviewAnimation=true;Avatar->SetAnimation(TEXT("Shot_Flip_Land"),false);Avatar->AnimationTime=Avatar->Animations.FindRef(TEXT("Shot_Flip_Land"))->GetPlayLength()*.55f;Avatar->PlaybackRate=0;EditorZoom=.3f;Wait=.5f;break;
    case 24:Screenshot(TEXT("10_FlipClose"));break;
    case 25:
        Avatar->SetAnimation(TEXT("Trip_Roll_From_Run"),false);Avatar->AnimationTime=Avatar->Animations.FindRef(TEXT("Trip_Roll_From_Run"))->GetPlayLength()*.5f;Avatar->PlaybackRate=0;Wait=.5f;break;
    case 26:Screenshot(TEXT("11_RollClose"));break;
    case 27:
        ChangeScreen(EScreen::Game);ResetPlayer();GoalCelebrationUsed=true;Mode->Ball->SetWorldLocation(FVector(1500,1000,22));
        Avatar->GetCharacterMovement()->Velocity=FVector(510,0,0);JumpPressed();JumpReleased();Wait=.22f;break;
    case 28:
        Check(TEXT("running_jump_airborne"),Avatar->GetCharacterMovement()->IsFalling());VisiblePose(TEXT("running_jump_mesh_visible"));Screenshot(TEXT("12_RunJump"));Wait=1.3f;break;
    case 29:
        ResetPlayer();GoalCelebrationUsed=true;Avatar->GetCharacterMovement()->Velocity=FVector(765,0,0);JumpPressed();JumpReleased();Wait=.22f;break;
    case 30:
        Check(TEXT("sprint_jump_airborne"),Avatar->GetCharacterMovement()->IsFalling());VisiblePose(TEXT("sprint_jump_mesh_visible"));Screenshot(TEXT("13_SprintJump"));Wait=1.3f;break;
    case 31:
        Check(TEXT("all_jump_frames_near_capsule"),JumpModelVisible);ResetPlayer();
        Avatar->StartAction(AFootballPlayer::EAction::Kick,TEXT("Kick_Right"),2.2f);OnGoal(Avatar);JumpPressed();
        Check(TEXT("goal_press_accepted_during_recovery"),GoalSpacePending);Wait=1.1f;break;
    case 32:
        Check(TEXT("held_goal_after_recovery"),GoalCelebrationUsed&&Avatar->Action==AFootballPlayer::EAction::Emote&&Avatar->CurrentAnimation!=TEXT("JoyJump_Run"));JumpReleased();ResetPlayer();break;
    case 33:
        MoveForward=1;Avatar->StartAction(AFootballPlayer::EAction::Emote,TEXT("Dance_Robot"),1.15f,0,true);Forward(1);
        Check(TEXT("existing_run_does_not_cancel_celebration"),Avatar->Action==AFootballPlayer::EAction::Emote);Forward(0);Forward(1);
        Check(TEXT("new_movement_cancels_ground_emote"),Avatar->Action==AFootballPlayer::EAction::None);Forward(0);ResetPlayer();break;
    case 34:case 36:case 38:case 40:case 42:case 44:
        {
            ResetPlayer();OnGoal(Avatar);JumpPressed();const int Choice=(TestStage_Latest-34)/2;
            for(int Seed=0;Seed<1000;Seed++){FMath::RandInit(Seed*104729);if(FMath::RandHelper(6)==Choice){FMath::RandInit(Seed*104729);break;}}
            SpaceStarted-=1.01f;UpdateActions(.01f);JumpReleased();Wait=Avatar->ActionDuration*.5f;break;
        }
    case 35:case 37:case 39:case 41:case 43:case 45:
        {
            const FString Label=TEXT("held_celebration_visible_")+Avatar->CurrentAnimation;
            VisiblePose(*Label);Check(TEXT("hold_not_replaced_by_short_jump"),Avatar->Action==AFootballPlayer::EAction::Emote&&Avatar->CurrentAnimation!=TEXT("JoyJump_Run"));
            Screenshot(*FString::Printf(TEXT("Held_%s"),*Avatar->CurrentAnimation));break;
        }
    case 46:
        ResetPlayer();ChangeScreen(EScreen::Main);DemoPhase=0;DemoDirection=1;Mode->ResetBall();Avatar->SetActorLocation(FVector(-350,0,98));Wait=.1f;break;
    case 47:Wait=.1f;break;
    case 48:Wait=13.f;break;
    case 49:
        Check(TEXT("demo_turns_without_extra_run"),DemoTurns>=2&&DemoTurnsPromptly);
        Check(TEXT("demo_turn_preserves_ball_flight"),DemoTurns>=2&&DemoKeepsShotInFlight);
        Check(TEXT("demo_receives_and_runs_two_steps"),DemoShots>=2&&DemoTwoSteps);
        Check(TEXT("demo_uses_both_feet"),DemoLeft&&DemoRight);
        Check(TEXT("demo_ball_windup_continuous"),DemoContinuous);Screenshot(TEXT("14_Demo"));break;
    default:
        FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("runtime_checks_Latest.txt")));
        FPlatformMisc::RequestExit(false);return;
    }
    TestStage_Latest++;TestAt=Now+Wait;
}
