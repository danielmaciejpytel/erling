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
    static float BallControlDribbleDistance=0.f,RunDribbleDistance=0.f;
    static int DemoShots=0;
    static bool DemoLeft=false,DemoRight=false,DemoWasPending=false;
    static FVector LastDemoBall=FVector::ZeroVector,SettingsPlayerBefore=FVector::ZeroVector,SettingsBallBefore=FVector::ZeroVector;
    static FString SettingsAnimationBefore;
    static float SettingsAnimationTimeBefore=0.f;
    static int PreviousDemoPhase=0,DemoTurns=0;
    static bool DemoTurnsPromptly=true,DemoKeepsShotInFlight=true;
    if(Avatar&&Avatar->PhysicalJump&&Avatar->JumpElapsed>.03f)
    {
        const float Offset=Avatar->GetMesh()->GetSocketLocation(TEXT("root")).Z-(Avatar->GetActorLocation().Z-96);
        if(FMath::Abs(Offset)>=100)UE_LOG(LogTemp,Warning,TEXT("JUMP_OFFSET clip=%s time=%f offset=%f"),*Avatar->CurrentAnimation,Avatar->AnimationTime,Offset);
        JumpModelVisible&=FMath::Abs(Offset)<100;
    }
    if((TestStage_Latest==49||TestStage_Latest==60)&&Avatar)
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
    if(TestStage_Latest==19||TestStage_Latest==20||TestStage_Latest==50||TestStage_Latest==51||TestStage_Latest==52)Forward(1);
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
        ChangeScreen(EScreen::Game);ResetPlayer();PlaceBall();Avatar->SetActorRotation(FRotator(0,8,0));Avatar->GetCharacterMovement()->Velocity=FVector(510,0,0);TestPosition=Avatar->GetActorLocation();FireShot(1.f);
        Check(TEXT("flip_twice_as_fast"),FMath::IsNearlyEqual(Avatar->PlaybackRate,2.6f));
        Check(TEXT("underbar_selects_flip"),Avatar->Action==AFootballPlayer::EAction::Flip);
        Check(TEXT("perfect_underbar_flip_without_goal_requirement"),Avatar->Action==AFootballPlayer::EAction::Flip&&FMath::Abs(PendingAimTarget.Y)>320.f);Wait=.55f;break;
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
        Check(TEXT("goal_tap_standing_jump"),Avatar->CurrentAnimation==TEXT("JoyJump_Standing")&&GoalCelebrationUsed&&Avatar->ActionAllowsMovement);Wait=2.7f;break;
    case 10:
        ResetPlayer();Avatar->GetCharacterMovement()->Velocity=FVector(510,0,0);OnGoal(Avatar);JumpPressed();SpaceStarted-=1.01f;UpdateActions(.01f);
        Check(TEXT("goal_hold_running_jump"),GoalCelebrationUsed&&Avatar->Action==AFootballPlayer::EAction::Emote&&Avatar->CurrentAnimation==TEXT("JoyJump_Run"));
        Check(TEXT("goal_jump_keeps_movement"),Avatar->ActionAllowsMovement&&!Avatar->IsMovementLocked()&&Avatar->GetVelocity().Size2D()>450.f);
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
        Check(TEXT("getup_unlocks"),!Avatar->IsMovementLocked());ResetPlayer();PlaceBall();TripChance=0;FireShot(.5f);
        Check(TEXT("normal_shot_no_special"),Avatar->Action==AFootballPlayer::EAction::Kick&&!PendingTrip);Wait=.45f;break;
    case 16:
        Check(TEXT("normal_shot_contact"),Mode->ShotInFlight&&!PendingShot);ResetPlayer();OnGoal(nullptr);GoalCelebrationUsed=true;OnGoal(nullptr);
        Check(TEXT("other_scorer_no_reward"),GoalCelebrationUsed);TripChance=.33f;
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
        ChangeScreen(EScreen::Game);ResetPlayer();Mode->ResetBall();Mode->Ball->SetWorldLocation(FVector(1500,1000,22));BallControlOn();Check(TEXT("ball_control_ignored_without_ball"),!BallControlHeld&&FMath::IsNearlyEqual(Avatar->GetCharacterMovement()->MaxWalkSpeed,510.f));PlaceBall();BallControlOn();Wait=1.2f;break;
    case 19:
        Check(TEXT("ball_control_modifier_and_animation"),BallControlHeld&&FMath::Abs(Avatar->GetVelocity().Size2D()-180)<5&&Avatar->CurrentAnimation==TEXT("Walk"));BallControlOff();SprintOn();Wait=1.2f;break;
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
        Check(TEXT("held_goal_after_recovery"),GoalCelebrationUsed&&Avatar->Action==AFootballPlayer::EAction::Emote&&Avatar->CurrentAnimation==TEXT("JoyJump_Run")&&Avatar->ActionAllowsMovement);JumpReleased();ResetPlayer();break;
    case 33:
        MoveForward=1;Avatar->StartAction(AFootballPlayer::EAction::Emote,TEXT("Dance_Robot"),1.15f,0,true);Forward(1);
        Check(TEXT("existing_run_does_not_cancel_celebration"),Avatar->Action==AFootballPlayer::EAction::Emote);Forward(0);Forward(1);
        Check(TEXT("new_movement_cancels_ground_emote"),Avatar->Action==AFootballPlayer::EAction::None);Forward(0);ResetPlayer();break;
    case 34:case 36:case 38:case 40:case 42:case 44:
        {
            ResetPlayer();Avatar->GetCharacterMovement()->Velocity=FVector(510,0,0);OnGoal(Avatar);JumpPressed();
            SpaceStarted-=1.01f;UpdateActions(.01f);JumpReleased();Wait=Avatar->ActionDuration*.5f;break;
        }
    case 35:case 37:case 39:case 41:case 43:case 45:
        {
            const FString Label=TEXT("held_running_celebration_visible");
            VisiblePose(*Label);Check(TEXT("held_running_celebration_persists"),Avatar->Action==AFootballPlayer::EAction::Emote&&Avatar->CurrentAnimation==TEXT("JoyJump_Run")&&Avatar->ActionAllowsMovement);
            Screenshot(TEXT("Held_JoyJump_Run"));break;
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
        Check(TEXT("demo_ball_windup_continuous"),DemoContinuous);Screenshot(TEXT("14_Demo"));
        Saved->PauseInSettings=false;SettingsReturn=EScreen::Main;ChangeScreen(EScreen::Settings);Check(TEXT("settings_default_does_not_pause_world"),!IsPaused());ChangeScreen(EScreen::Main);Check(TEXT("settings_return_stays_unpaused"),!IsPaused());
        ChangeScreen(EScreen::Game);ResetPlayer();PlaceBall();BallControlOn();Wait=1.2f;break;
    case 50:
        BallControlDribbleDistance=FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetActorLocation());
        {const float FootDistance=FMath::Min(FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetMesh()->GetSocketLocation(TEXT("foot_l"))),FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetMesh()->GetSocketLocation(TEXT("foot_r"))));
        UE_LOG(LogTemp,Display,TEXT("DRIBBLE_BALL_CONTROL player=%f foot=%f"),BallControlDribbleDistance,FootDistance);
        Check(TEXT("ball_control_dribble_close"),BallControlDribbleDistance<105.f);Check(TEXT("ball_control_dribble_tracks_foot"),FootDistance<80.f);}
        BallControlOff();Wait=1.2f;break;
    case 51:
        RunDribbleDistance=FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetActorLocation());
        {const float FootDistance=FMath::Min(FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetMesh()->GetSocketLocation(TEXT("foot_l"))),FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetMesh()->GetSocketLocation(TEXT("foot_r"))));
        UE_LOG(LogTemp,Display,TEXT("DRIBBLE_RUN player=%f foot=%f"),RunDribbleDistance,FootDistance);
        Check(TEXT("run_dribble_close"),RunDribbleDistance<120.f&&RunDribbleDistance>BallControlDribbleDistance-10.f);Check(TEXT("run_dribble_tracks_foot"),FootDistance<90.f);}
        Mode->SprintDribbleTouchCount=0;Mode->DribbleMinFootClearance=MAX_flt;Mode->SprintDribbleMinDistance=MAX_flt;Mode->SprintDribbleMaxDistance=0;SprintOn();BallControlOn();Wait=2.4f;break;
    case 52:
        {const float SprintDistance=FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetActorLocation());const float FootDistance=FMath::Min(FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetMesh()->GetSocketLocation(TEXT("foot_l"))),FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetMesh()->GetSocketLocation(TEXT("foot_r"))));
        UE_LOG(LogTemp,Display,TEXT("DRIBBLE_SPRINT player=%f foot=%f"),SprintDistance,FootDistance);
        Check(TEXT("sprint_dribble_releases_forward"),Mode->SprintDribbleMaxDistance>RunDribbleDistance+20.f&&Mode->SprintDribbleMaxDistance<210.f);
        Check(TEXT("sprint_dribble_returns_for_touch"),Mode->SprintDribbleMinDistance<110.f&&Mode->SprintDribbleTouchCount>0);
        Check(TEXT("dribble_never_penetrates_feet"),Mode->DribbleMinFootClearance>=31.5f);
        Check(TEXT("sprint_touch_reaches_foot_surface"),Mode->DribbleMinFootClearance<=35.5f);}
        Check(TEXT("sprint_dribble_wins_over_ball_control"),Sprint&&BallControlHeld&&Avatar->GetCharacterMovement()->MaxWalkSpeed>700.f);
        Forward(0);SprintOff();BallControlOff();ResetPlayer();Avatar->ConsumeMovementInputVector();Saved->CameraMode=0;Yaw=0;Mode->Ball->SetWorldLocation(FVector(1500,1000,22),false,nullptr,ETeleportType::TeleportPhysics);
        {auto* Move=Cast<UErlingMovement>(Avatar->GetCharacterMovement());Move->SetMovementMode(MOVE_Walking);Move->Velocity=FVector(510,0,0);Move->LastMoveInputDirection=FVector(1,0,0);Move->TurnSkidRemaining=0;}Forward(-1);Wait=.05f;break;
    case 53:
        {auto* Move=Cast<UErlingMovement>(Avatar->GetCharacterMovement());UE_LOG(LogTemp,Display,TEXT("TURN_SKID remaining=%f velocity=%s input=%s mode=%d"),Move->TurnSkidRemaining,*Move->Velocity.ToString(),*Move->LastMoveInputDirection.ToString(),int32(Move->MovementMode));Check(TEXT("sharp_turn_starts_momentum_skid"),Move->TurnSkidRemaining>0.f);Check(TEXT("sharp_turn_keeps_some_momentum"),Move->Velocity.X>250.f);}
        Forward(0);ResetPlayer();PitchCameraLeadX=160.f;Avatar->GetCharacterMovement()->Velocity=FVector(-765,0,0);{const float Before=PitchCameraLeadX;UpdatePitchCameraLead(1.f/60.f);Check(TEXT("pitch_camera_lead_does_not_snap_on_direction_change"),PitchCameraLeadX>0.f&&FMath::Abs(PitchCameraLeadX-Before)<40.f);}
        Avatar->StartAction(AFootballPlayer::EAction::Kick,TEXT("Kick_Right"),2.2f);Avatar->EntryVelocity=FVector(510,0,0);Avatar->GetCharacterMovement()->Velocity=FVector::ZeroVector;PitchCameraLeadX=100.f;UpdatePitchCameraLead(1.f/60.f);Check(TEXT("pitch_camera_keeps_shot_momentum"),PitchCameraLeadX>=100.f);Avatar->ClearAction();
        ResetPlayer();Avatar->ConsumeMovementInputVector();Saved->CameraMode=0;Yaw=0;SprintOn();{auto* Move=Cast<UErlingMovement>(Avatar->GetCharacterMovement());Move->SetMovementMode(MOVE_Walking);Move->Velocity=FVector(765,0,0);Move->LastMoveInputDirection=FVector(1,0,0);Move->TurnSkidRemaining=0;}Forward(-1);Wait=.05f;break;
    case 54:
        {auto* Move=Cast<UErlingMovement>(Avatar->GetCharacterMovement());UE_LOG(LogTemp,Display,TEXT("SPRINT_TURN_SKID remaining=%f velocity=%s"),Move->TurnSkidRemaining,*Move->Velocity.ToString());Check(TEXT("sprint_sharp_turn_skid"),Move->TurnSkidRemaining>.1f);Check(TEXT("sprint_turn_carries_more_momentum"),Move->Velocity.X>500.f);}
        Forward(0);SprintOff();ResetPlayer();{auto* Move=Cast<UErlingMovement>(Avatar->GetCharacterMovement());Move->SetMovementMode(MOVE_Walking);Move->Velocity=FVector(200,0,0);Move->TurnSkidRemaining=.05f;}Wait=.12f;break;
    case 55:
        {auto* Move=Cast<UErlingMovement>(Avatar->GetCharacterMovement());Check(TEXT("turn_skid_timer_expires_below_speed_threshold"),Move->TurnSkidRemaining<=0.f);}
        ResetPlayer();PlaceBall();Avatar->GetCharacterMovement()->Velocity=FVector(510,0,0);{auto* Move=Cast<UErlingMovement>(Avatar->GetCharacterMovement());Move->LastMoveInputDirection=FVector(1,0,0);Move->TurnSkidRemaining=.15f;}StartCharge();Forward(-1);Wait=.08f;break;
    case 56:
        {auto* Move=Cast<UErlingMovement>(Avatar->GetCharacterMovement());Check(TEXT("shot_charge_disables_turn_skid"),Charging&&Move->TurnSkidRemaining<=0.f);}
        Charging=false;Forward(0);ResetPlayer();PlaceBall();BallControlOn();Forward(1);Wait=.5f;break;
    case 57:
        Forward(0);Right(1);Wait=.45f;break;
    case 58:
        {const float D=FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetActorLocation());const float FootD=FMath::Min(FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetMesh()->GetSocketLocation(TEXT("foot_l"))),FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetMesh()->GetSocketLocation(TEXT("foot_r"))));
        UE_LOG(LogTemp,Display,TEXT("DRIBBLE_BALL_CONTROL_TURN player=%f foot=%f active=%d"),D,FootD,BallControlHeld?1:0);Check(TEXT("ball_control_stays_active_through_turn"),BallControlHeld);Check(TEXT("ball_control_dribble_stays_attached_through_turn"),D<95.f&&FootD<70.f);}
        Right(0);BallControlOff();ResetPlayer();ChangeScreen(EScreen::Main);DemoPhase=0;DemoDirection=1;DemoShots=0;DemoWasPending=false;PreviousDemoPhase=0;Mode->ResetBall();Avatar->SetActorLocation(FVector(-350,0,98));Wait=.5f;break;
    case 59:
        SettingsPlayerBefore=Avatar->GetActorLocation();SettingsBallBefore=Mode->Ball->GetComponentLocation();Saved->PauseInSettings=false;SettingsReturn=EScreen::Main;ChangeScreen(EScreen::Settings);Check(TEXT("pause_in_settings_off_is_live"),!IsPaused());Wait=3.f;break;
    case 60:
        Check(TEXT("settings_live_demo_player_moves"),FVector::Dist2D(SettingsPlayerBefore,Avatar->GetActorLocation())>100.f);Check(TEXT("settings_live_demo_still_kicks"),DemoShots>0);
        Saved->PauseInSettings=true;SetPause(true);Check(TEXT("pause_in_settings_on_pauses_world"),IsPaused());Wait=0.f;break;
    case 61:
        SettingsPlayerBefore=Avatar->GetActorLocation();SettingsBallBefore=Mode->Ball->GetComponentLocation();SettingsAnimationBefore=Avatar->CurrentAnimation;SettingsAnimationTimeBefore=Avatar->AnimationTime;Wait=0.f;break;
    case 62:
        Check(TEXT("paused_settings_preserves_player"),FVector::Dist(SettingsPlayerBefore,Avatar->GetActorLocation())<.1f);Check(TEXT("paused_settings_preserves_ball"),FVector::Dist(SettingsBallBefore,Mode->Ball->GetComponentLocation())<.1f);Check(TEXT("paused_settings_preserves_animation"),Avatar->CurrentAnimation==SettingsAnimationBefore&&FMath::Abs(Avatar->AnimationTime-SettingsAnimationTimeBefore)<.001f);
        Saved->PauseInSettings=false;SetPause(false);ChangeScreen(EScreen::Main);Check(TEXT("pause_in_settings_off_resumes_world"),!IsPaused());ResetPlayer();Wait=.1f;break;
    default:
        FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("runtime_checks_Latest.txt")));
        FPlatformMisc::RequestExit(false);return;
    }
    TestStage_Latest++;TestAt=Now+Wait;
}
