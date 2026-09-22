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
#include "Kismet/GameplayStatics.h"

void AFootballController::RunChecks()
{
    const double Now=GetWorld()->GetTimeSeconds();
    static bool JumpModelVisible=true,DemoTwoSteps=true,DemoContinuous=true,DemoVariedShotDistance=true,DemoShotRangeOk=true,DemoNoPostShotGlide=true;
    static float BallControlDribbleDistance=0.f,RunDribbleDistance=0.f,LastDemoShotAbsX=-1.f;
    static float DemoCarryMinGap=MAX_flt,DemoCarryMaxGap=0.f,DemoCarryMaxFootDistance=0.f,DemoCarryMaxRelativeSpeed=0.f;
    static int32 DemoCarrySamples=0;
    static FVector DemoWaitingBallAnchor=FVector::ZeroVector,ShotWindupStart=FVector::ZeroVector;
    static float DemoWaitingMaxDrift=0.f,DemoFirstReceiveGap=MAX_flt,DemoFirstReceiveFootDistance=MAX_flt,ShotWindupMaxDisplacement=0.f;
    static int32 DemoWaitingSamples=0,ShotWindupSamples=0;
    static bool DemoWaitingTracking=false;
    static int DemoShots=0;
    static bool DemoLeft=false,DemoRight=false,DemoWasPending=false;
    static FVector LastDemoBall=FVector::ZeroVector,SettingsPlayerBefore=FVector::ZeroVector,SettingsBallBefore=FVector::ZeroVector,DemoPostShotPosition=FVector::ZeroVector,HeldBallPosition=FVector::ZeroVector;
    static float VideoTurnMinDirectionAhead=MAX_flt,VideoTurnMinBodyAhead=MAX_flt,VideoTurnMaxGap=0.f;
    static int32 VideoTurnSamples=0;
    static float SprintStressMinBodyAhead=MAX_flt,SprintStressMaxGap=0.f;
    static int32 SprintStressSamples=0,SprintStressLostControlFrames=0;
    static float SprintChaseMaxYawStep=0.f,SprintChasePreviousYaw=0.f;
    static int32 SprintChaseYawSamples=0;
    static bool SprintChaseYawPrimed=false;
    static float NaturalBufferCommitAge=-1.f,NaturalBufferMinFoot=MAX_flt;
    static bool NaturalBufferWasActive=false;
    static FVector PivotExpectedLocal=FVector::ZeroVector;
    static float PivotMaxLocalDrift=0.f;
    static int32 PivotSamples=0;
    static bool DemoPostShotTracking=false;
    static FString SettingsAnimationBefore;
    static float SettingsAnimationTimeBefore=0.f;
    static int PreviousDemoPhase=0,DemoTurns=0;
    static bool DemoTurnsPromptly=true,DemoKeepsShotInFlight=true;
    static float CloseTurnMinFoot=MAX_flt;
    if(TestStage_Latest==90&&Avatar)
        if(auto* M=GetWorld()->GetAuthGameMode<AFootballMode>();M&&M->Ball)
            CloseTurnMinFoot=FMath::Min(CloseTurnMinFoot,FMath::Min(FVector::Dist2D(M->Ball->GetComponentLocation(),Avatar->GetMesh()->GetSocketLocation(TEXT("foot_l"))),FVector::Dist2D(M->Ball->GetComponentLocation(),Avatar->GetMesh()->GetSocketLocation(TEXT("foot_r")))));
    static FVector ContactTestAnchor=FVector::ZeroVector;
    static float ContactTestMaxFreeDrift=0.f,ContactTestMinForwardSpeed=MAX_flt,ContactTestMinFoot=MAX_flt,ContactTestNextFrame=0.f;
    static bool ContactTestAcquired=false;
    static int32 ContactTestSamples=0,ContactTestFrame=0;
    static FVector TurnStart;
    static float TurnBegin=0,TurnReverseTime=-1,TurnMaxSide=0,TurnFreeSide=0,TurnFreeMinX=MAX_flt;
    static bool TurnAcquired=false;
    static float TurnNextFrame=0;static int32 TurnFrame=0;
    if((TestStage_Latest==165||TestStage_Latest==167||TestStage_Latest==169)&&Avatar)
    {
        Forward(TestStage_Latest==165?0:-1);Right(TestStage_Latest==165?1:0);
        if(auto* M=GetWorld()->GetAuthGameMode<AFootballMode>();M&&M->Ball)
        {
            if(M->PossessionActive)TurnAcquired=true;
            if(!TurnAcquired){TurnFreeSide=FMath::Max(TurnFreeSide,FMath::Abs(M->Ball->GetPhysicsLinearVelocity().Y));TurnFreeMinX=FMath::Min(TurnFreeMinX,M->Ball->GetPhysicsLinearVelocity().X);}
            TurnMaxSide=FMath::Max(TurnMaxSide,FMath::Abs(Avatar->GetActorLocation().Y-TurnStart.Y));
            if(TurnReverseTime<0&&Avatar->GetVelocity().X< -150)TurnReverseTime=Now-TurnBegin;
            if(FParse::Param(FCommandLine::Get(),TEXT("TurnReview"))&&Now>=TurnNextFrame)
            {
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("TurnChecks")/FString::Printf(TEXT("turn_%d_%04d.png"),TestStage_Latest,TurnFrame++),true,false);
                TurnNextFrame=Now+.1f;
            }
        }
    }
    if((TestStage_Latest==157||TestStage_Latest==159)&&Avatar)
    {
        Forward(1);
        if(auto* M=GetWorld()->GetAuthGameMode<AFootballMode>();M&&M->Ball)
        {
            const FVector Position=M->Ball->GetComponentLocation();
            ContactTestMinFoot=FMath::Min(ContactTestMinFoot,FMath::Min(FVector::Dist2D(Position,Avatar->GetMesh()->GetSocketLocation(TEXT("foot_l"))),FVector::Dist2D(Position,Avatar->GetMesh()->GetSocketLocation(TEXT("foot_r")))));
            if(M->PossessionActive)ContactTestAcquired=true;
            if(!ContactTestAcquired)
            {
                ++ContactTestSamples;
                if(TestStage_Latest==157)ContactTestMaxFreeDrift=FMath::Max(ContactTestMaxFreeDrift,FVector::Dist2D(Position,ContactTestAnchor));
                else ContactTestMinForwardSpeed=FMath::Min(ContactTestMinForwardSpeed,M->Ball->GetPhysicsLinearVelocity().X);
            }
            if(FParse::Param(FCommandLine::Get(),TEXT("ContactOnly"))&&Now>=ContactTestNextFrame)
            {
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("ContactChecks")/FString::Printf(TEXT("frame_%04d.png"),ContactTestFrame++),true,false);
                ContactTestNextFrame=Now+.1f;
            }
        }
    }
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
            const float ShotAbsX=FMath::Abs(Avatar->GetActorLocation().X);DemoShotRangeOk&=ShotAbsX>=470.f&&ShotAbsX<=2130.f;
            if(LastDemoShotAbsX>=0)DemoVariedShotDistance&=FMath::Abs(ShotAbsX-LastDemoShotAbsX)>120.f;LastDemoShotAbsX=ShotAbsX;
            UE_LOG(LogTemp,Display,TEXT("DEMO_SHOT foot=%s x=%f receive_time=%f travel=%f"),*Avatar->CurrentAnimation,Avatar->GetActorLocation().X,Now-DemoReceiveAt,FVector::Dist2D(DemoReceivePosition,Avatar->GetActorLocation()));
        }
        if(DemoWasPending&&!PendingShot&&DemoPhase==1){DemoPostShotPosition=Avatar->GetActorLocation();DemoPostShotTracking=true;}
        if(DemoPostShotTracking&&DemoPhase==1&&!PendingShot)DemoNoPostShotGlide&=FVector::Dist2D(DemoPostShotPosition,Avatar->GetActorLocation())<8.f;
        if(DemoPhase!=1)DemoPostShotTracking=false;
        if(PendingShot&&DemoWasPending)DemoContinuous&=FVector::Dist(LastDemoBall,M->Ball->GetComponentLocation())<60;
        if(DemoWaitingTracking&&M->Ball)
        {
            if(DemoPhase==0)
            {
                DemoWaitingMaxDrift=FMath::Max(DemoWaitingMaxDrift,FVector::Dist2D(DemoWaitingBallAnchor,M->Ball->GetComponentLocation()));
                DemoWaitingSamples++;
            }
            else if(DemoPhase==2)
            {
                DemoFirstReceiveGap=FVector::Dist2D(M->Ball->GetComponentLocation(),Avatar->GetActorLocation());
                if(Avatar->GetMesh())
                    DemoFirstReceiveFootDistance=FMath::Min(FVector::Dist2D(M->Ball->GetComponentLocation(),Avatar->GetMesh()->GetSocketLocation(TEXT("foot_l"))),FVector::Dist2D(M->Ball->GetComponentLocation(),Avatar->GetMesh()->GetSocketLocation(TEXT("foot_r"))));
                DemoWaitingTracking=false;
            }
        }
        if(DemoPhase==2&&M->Ball&&Now-DemoReceiveAt>.35f)
        {
            const float Gap=FVector::Dist2D(M->Ball->GetComponentLocation(),Avatar->GetActorLocation());
            DemoCarryMinGap=FMath::Min(DemoCarryMinGap,Gap);DemoCarryMaxGap=FMath::Max(DemoCarryMaxGap,Gap);
            if(Avatar->GetMesh())
            {
                const float FootDistance=FMath::Min(FVector::Dist2D(M->Ball->GetComponentLocation(),Avatar->GetMesh()->GetSocketLocation(TEXT("foot_l"))),FVector::Dist2D(M->Ball->GetComponentLocation(),Avatar->GetMesh()->GetSocketLocation(TEXT("foot_r"))));
                DemoCarryMaxFootDistance=FMath::Max(DemoCarryMaxFootDistance,FootDistance);
            }
            DemoCarryMaxRelativeSpeed=FMath::Max(DemoCarryMaxRelativeSpeed,(M->Ball->GetPhysicsLinearVelocity()-Avatar->GetVelocity()).Size2D());
            DemoCarrySamples++;
        }
        DemoWasPending=PendingShot;LastDemoBall=M->Ball->GetComponentLocation();
    }
    if(TestStage_Latest>=77&&TestStage_Latest<=79&&Avatar)
    {
        if(auto* M=GetWorld()->GetAuthGameMode<AFootballMode>();M&&M->Ball&&M->HasDribbleControl(Avatar))
        {
            FVector Rel=M->Ball->GetComponentLocation()-Avatar->GetActorLocation();Rel.Z=0;
            const FVector DribbleForward=M->DribbleDirection.GetSafeNormal2D();
            const FVector BodyForward=Avatar->GetActorForwardVector().GetSafeNormal2D();
            if(!DribbleForward.IsNearlyZero())VideoTurnMinDirectionAhead=FMath::Min(VideoTurnMinDirectionAhead,FVector::DotProduct(Rel,DribbleForward));
            if(!BodyForward.IsNearlyZero())VideoTurnMinBodyAhead=FMath::Min(VideoTurnMinBodyAhead,FVector::DotProduct(Rel,BodyForward));
            VideoTurnMaxGap=FMath::Max(VideoTurnMaxGap,Rel.Size2D());VideoTurnSamples++;
        }
    }
    if(TestStage_Latest>=81&&TestStage_Latest<=88&&Avatar)
    {
        if(auto* M=GetWorld()->GetAuthGameMode<AFootballMode>();M&&M->Ball)
        {
            FVector Rel=M->Ball->GetComponentLocation()-Avatar->GetActorLocation();Rel.Z=0;
            const FVector BodyForward=Avatar->GetActorForwardVector().GetSafeNormal2D();
            SprintStressMaxGap=FMath::Max(SprintStressMaxGap,Rel.Size2D());
            if(!BodyForward.IsNearlyZero())SprintStressMinBodyAhead=FMath::Min(SprintStressMinBodyAhead,FVector::DotProduct(Rel,BodyForward));
            if(!M->HasDribbleControl(Avatar))SprintStressLostControlFrames++;
            SprintStressSamples++;
        }
    }
    if(TestStage_Latest==97&&Avatar)
    {
        if(auto* M=GetWorld()->GetAuthGameMode<AFootballMode>();M&&M->Ball&&PendingShot)
        {
            ShotWindupMaxDisplacement=FMath::Max(ShotWindupMaxDisplacement,FVector::Dist2D(ShotWindupStart,M->Ball->GetComponentLocation()));
            ShotWindupSamples++;
        }
    }
    if(TestStage_Latest==102&&Avatar)
    {
        const float YawNow=Avatar->GetActorRotation().Yaw;
        if(SprintChaseYawPrimed)
            SprintChaseMaxYawStep=FMath::Max(SprintChaseMaxYawStep,FMath::Abs(FMath::FindDeltaAngleDegrees(SprintChasePreviousYaw,YawNow)));
        SprintChasePreviousYaw=YawNow;SprintChaseYawPrimed=true;SprintChaseYawSamples++;
    }
    if(TestStage_Latest==106&&Avatar)
    {
        if(auto* M=GetWorld()->GetAuthGameMode<AFootballMode>();M&&M->Ball)
        {
            const FVector Local=M->Ball->GetComponentLocation();
            const FVector CurrentLocal=Avatar->GetActorTransform().InverseTransformPosition(Local);
            PivotMaxLocalDrift=FMath::Max(PivotMaxLocalDrift,FVector::Dist2D(CurrentLocal,PivotExpectedLocal));
            PivotSamples++;
        }
    }
    if(TestStage_Latest==152&&Avatar)
    {
        if(auto* M=GetWorld()->GetAuthGameMode<AFootballMode>();M&&M->Ball&&Avatar->GetMesh())
        {
            const FVector Ball=M->Ball->GetComponentLocation();
            const float FootDistance=FMath::Min(FVector::Dist2D(Ball,Avatar->GetMesh()->GetSocketLocation(TEXT("foot_l"))),FVector::Dist2D(Ball,Avatar->GetMesh()->GetSocketLocation(TEXT("foot_r"))));
            NaturalBufferMinFoot=FMath::Min(NaturalBufferMinFoot,FootDistance);
            if(NaturalBufferWasActive&&!ShotBufferActive&&(PendingShot||M->ShotInFlight)&&NaturalBufferCommitAge<0.f)
                NaturalBufferCommitAge=Now-ShotBufferStarted;
            NaturalBufferWasActive=ShotBufferActive;
        }
    }
    if(TestStage_Latest==19||TestStage_Latest==20||TestStage_Latest==50||TestStage_Latest==51||TestStage_Latest==52||TestStage_Latest==64||TestStage_Latest==66||TestStage_Latest==69||TestStage_Latest==72||TestStage_Latest==73)Forward(1);
    if(TestStage_Latest==65||TestStage_Latest==67||TestStage_Latest==90)Forward(-1);
    if(TestStage_Latest==68)Right(1);
    if(TestStage_Latest==81||TestStage_Latest==85)Forward(1);
    if(TestStage_Latest==83||TestStage_Latest==87||TestStage_Latest==90||TestStage_Latest==92)Forward(-1);
    if(TestStage_Latest==82||TestStage_Latest==86)Right(1);
    if(TestStage_Latest==84||TestStage_Latest==88)Right(-1);
    if(TestStage_Latest==94)Right(1);
    // Axis input is consumed each frame. These turn tests must actually hold the
    // requested direction, rather than rely on the old remote ball attraction.
    if(TestStage_Latest==57||TestStage_Latest==77)Forward(1);
    if(TestStage_Latest==58||TestStage_Latest==78)Right(1);
    if(TestStage_Latest==79)Forward(-1);
    if(TestStage_Latest==102)Right(-1);
    if(TestStage_Latest==124)Forward(-1);
    if(TestStage_Latest==128)Right(-1);
    if(TestStage_Latest==144)Forward(-1);
    if(TestStage_Latest==171)Forward(1);
    if(TestStage_Latest==172){Forward(0);Right(1);}
    if(TestStage_Latest==173){Forward(-1);Right(0);}
    if(TestAt==0){if(FParse::Param(FCommandLine::Get(),TEXT("ContactOnly")))TestStage_Latest=156;else if(FParse::Param(FCommandLine::Get(),TEXT("DemoOnly")))TestStage_Latest=46;else if(FParse::Param(FCommandLine::Get(),TEXT("TortureOnly")))TestStage_Latest=123;TestAt=Now+4;return;}
    if(Now<TestAt)return;
    auto* Mode=GetWorld()->GetAuthGameMode<AFootballMode>();
    if(!Avatar||!Mode||!Mode->Ball)return;
    float Wait=.8f;
    auto Check=[this](const TCHAR* Label,bool Pass){const FString Line=FString::Printf(TEXT("%s: %s\n"),Label,Pass?TEXT("PASS"):TEXT("FAIL"));Report+=Line;UE_LOG(LogTemp,Display,TEXT("CHECK %s"),*Line);};
    auto Screenshot=[](const TCHAR* Name){FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots_Latest")/(FString(Name)+TEXT(".png")),true,false);};
    auto PlaceBall=[&](){Mode->ResetBall();Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+Avatar->GetActorForwardVector()*125-FVector(0,0,74),false,nullptr,ETeleportType::TeleportPhysics);Mode->Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);};
    auto ResetPlayer=[&](){CancelPendingActions();Avatar->ClearAction();Avatar->PhysicalJump=false;Avatar->SetAnimation(TEXT("Idle_Breathe"));Avatar->AnimationBlend=1;Avatar->GetCharacterMovement()->StopMovementImmediately();Avatar->SetActorLocation(FVector(-350,0,100));Avatar->SetActorRotation(FRotator::ZeroRotator);KickCooldown=0;};
    auto PutBallAtNearestFoot=[&]()
    {
        const FVector Ball=Mode->Ball->GetComponentLocation();
        const FVector Left=Avatar->GetMesh()->GetSocketLocation(TEXT("foot_l"));
        const FVector RightFoot=Avatar->GetMesh()->GetSocketLocation(TEXT("foot_r"));
        const FVector Foot=FVector::Dist2D(Ball,Left)<=FVector::Dist2D(Ball,RightFoot)?Left:RightFoot;
        Mode->Ball->SetWorldLocation(FVector(Foot.X+38.f,Foot.Y,Mode->Ball->GetComponentLocation().Z),false,nullptr,ETeleportType::TeleportPhysics);
        Mode->Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);
    };
    auto BeginLooseBufferedShot=[&](float Power,const FVector& LoosePlanar)
    {
        ResetPlayer();Mode->ResetBall();SprintOn();MoveForward=0;MoveRight=0;Avatar->SetActorRotation(FRotator::ZeroRotator);
        const float BallZ=Mode->Ball->GetComponentLocation().Z;
        Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+FVector(125,0,BallZ-Avatar->GetActorLocation().Z),false,nullptr,ETeleportType::TeleportPhysics);
        Mode->Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);
        StartCharge();ChargeStarted=Now-Power;
        Mode->SprintReleaseDirection=FVector::ForwardVector;Mode->SprintReleaseUntil=Now+.2f;Mode->SprintKickPending=false;Mode->SprintContactUntil=0.f;Mode->SprintNextTouchAt=Now+2.f;
        Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+FVector(LoosePlanar.X,LoosePlanar.Y,BallZ-Avatar->GetActorLocation().Z),false,nullptr,ETeleportType::TeleportPhysics);
        Mode->Ball->SetPhysicsLinearVelocity(FVector(760,0,0));
        ReleaseCharge();
        SprintOff();
    };
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
        ChangeScreen(EScreen::Game);ResetPlayer();Mode->ResetBall();Mode->Ball->SetWorldLocation(FVector(1500,1000,22));BallControlOn();Check(TEXT("ball_control_ignored_without_ball"),!BallControlHeld&&FMath::IsNearlyEqual(Avatar->GetCharacterMovement()->MaxWalkSpeed,500.f));PlaceBall();BallControlOn();Wait=1.2f;break;
    case 19:
        Check(TEXT("ball_control_modifier_and_animation"),BallControlHeld&&FMath::Abs(Avatar->GetVelocity().Size2D()-190)<5&&Avatar->CurrentAnimation==TEXT("Walk"));BallControlOff();SprintOn();Wait=1.2f;break;
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
        ResetPlayer();ChangeScreen(EScreen::Main);DemoPhase=0;DemoDirection=1;Mode->ResetBall();Avatar->SetActorLocation(FVector(-350,0,98));
        DemoCarryMinGap=MAX_flt;DemoCarryMaxGap=0.f;DemoCarryMaxFootDistance=0.f;DemoCarryMaxRelativeSpeed=0.f;DemoCarrySamples=0;
        DemoWaitingBallAnchor=Mode->Ball->GetComponentLocation();DemoWaitingMaxDrift=0.f;DemoWaitingSamples=0;DemoFirstReceiveGap=MAX_flt;DemoFirstReceiveFootDistance=MAX_flt;DemoWaitingTracking=true;Wait=.1f;break;
    case 47:Wait=.1f;break;
    case 48:Wait=15.f;break;
    case 49:
        Check(TEXT("demo_turns_without_extra_run"),DemoTurns>=2&&DemoTurnsPromptly);
        Check(TEXT("demo_turn_preserves_ball_flight"),DemoTurns>=2&&DemoKeepsShotInFlight);
        Check(TEXT("demo_receives_and_runs_two_steps"),DemoShots>=2&&DemoTwoSteps);
        Check(TEXT("demo_uses_both_feet"),DemoLeft&&DemoRight);
        Check(TEXT("demo_ball_windup_continuous"),DemoContinuous);Screenshot(TEXT("14_Demo"));
        Check(TEXT("demo_randomizes_shot_distance"),DemoShots>=2&&DemoVariedShotDistance);
        Check(TEXT("demo_never_shoots_beyond_penalty_line"),DemoShots>=2&&DemoShotRangeOk);
        Check(TEXT("demo_stops_gliding_after_shot"),DemoShots>=2&&DemoNoPostShotGlide);
        UE_LOG(LogTemp,Display,TEXT("DEMO_CARRY samples=%d min_gap=%f max_gap=%f max_foot_distance=%f max_relative_speed=%f"),DemoCarrySamples,DemoCarryMinGap,DemoCarryMaxGap,DemoCarryMaxFootDistance,DemoCarryMaxRelativeSpeed);
        UE_LOG(LogTemp,Display,TEXT("DEMO_WAITING samples=%d max_drift=%f receive_gap=%f receive_foot=%f"),DemoWaitingSamples,DemoWaitingMaxDrift,DemoFirstReceiveGap,DemoFirstReceiveFootDistance);
        Check(TEXT("demo_waiting_ball_stays_fixed_until_runner_arrives"),DemoWaitingSamples>5&&DemoWaitingMaxDrift<2.f);
        Check(TEXT("demo_dribble_starts_only_at_real_foot_contact"),DemoFirstReceiveGap<118.f&&DemoFirstReceiveFootDistance<=66.f);
        Check(TEXT("demo_background_ball_rolls_instead_of_sticking_to_boot"),DemoCarrySamples>100&&DemoCarryMaxFootDistance>70.f&&DemoCarryMaxRelativeSpeed>70.f);
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
        Check(TEXT("run_dribble_close"),RunDribbleDistance<100.f&&RunDribbleDistance>BallControlDribbleDistance-10.f);Check(TEXT("run_dribble_tracks_foot"),FootDistance<80.f);}
        Mode->SprintDribbleTouchCount=0;Mode->DribbleMinFootClearance=MAX_flt;Mode->SprintDribbleMinDistance=MAX_flt;Mode->SprintDribbleMaxDistance=0;SprintOn();BallControlOn();Wait=2.4f;break;
    case 52:
        {const float SprintDistance=FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetActorLocation());const float FootDistance=FMath::Min(FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetMesh()->GetSocketLocation(TEXT("foot_l"))),FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetMesh()->GetSocketLocation(TEXT("foot_r"))));
        UE_LOG(LogTemp,Display,TEXT("DRIBBLE_SPRINT player=%f foot=%f min=%f max=%f touches=%d"),SprintDistance,FootDistance,Mode->SprintDribbleMinDistance,Mode->SprintDribbleMaxDistance,Mode->SprintDribbleTouchCount);
        Check(TEXT("sprint_dribble_releases_forward"),Mode->SprintDribbleMaxDistance>RunDribbleDistance+20.f&&Mode->SprintDribbleMaxDistance<210.f);
        Check(TEXT("sprint_dribble_returns_for_touch"),Mode->SprintDribbleMinDistance<110.f&&Mode->SprintDribbleTouchCount>0);
        Check(TEXT("dribble_never_penetrates_feet"),Mode->DribbleMinFootClearance>=37.5f);
        Check(TEXT("sprint_touch_reaches_foot_surface"),Mode->DribbleMinFootClearance<=41.5f);}
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
        Right(0);BallControlOff();ResetPlayer();ChangeScreen(EScreen::Main);DemoPhase=0;DemoDirection=1;DemoShotTargetX=500.f;DemoShots=0;DemoWasPending=false;PreviousDemoPhase=0;Mode->ResetBall();Avatar->SetActorLocation(FVector(-350,0,98));Wait=.5f;break;
    case 59:
        SettingsPlayerBefore=Avatar->GetActorLocation();SettingsBallBefore=Mode->Ball->GetComponentLocation();Saved->PauseInSettings=false;SettingsReturn=EScreen::Main;ChangeScreen(EScreen::Settings);Check(TEXT("pause_in_settings_off_is_live"),!IsPaused());Wait=3.f;break;
    case 60:
        Check(TEXT("settings_live_demo_player_moves"),FVector::Dist2D(SettingsPlayerBefore,Avatar->GetActorLocation())>100.f);Check(TEXT("settings_live_demo_still_kicks"),DemoShots>0);
        Saved->PauseInSettings=true;SetPause(true);Check(TEXT("pause_in_settings_on_pauses_world"),IsPaused());Wait=0.f;break;
    case 61:
        SettingsPlayerBefore=Avatar->GetActorLocation();SettingsBallBefore=Mode->Ball->GetComponentLocation();SettingsAnimationBefore=Avatar->CurrentAnimation;SettingsAnimationTimeBefore=Avatar->AnimationTime;Wait=0.f;break;
    case 62:
        Check(TEXT("paused_settings_preserves_player"),FVector::Dist(SettingsPlayerBefore,Avatar->GetActorLocation())<.1f);Check(TEXT("paused_settings_preserves_ball"),FVector::Dist(SettingsBallBefore,Mode->Ball->GetComponentLocation())<.1f);Check(TEXT("paused_settings_preserves_animation"),Avatar->CurrentAnimation==SettingsAnimationBefore&&FMath::Abs(Avatar->AnimationTime-SettingsAnimationTimeBefore)<.001f);
        Saved->PauseInSettings=false;SetPause(false);ChangeScreen(EScreen::Main);Check(TEXT("pause_in_settings_off_resumes_world"),!IsPaused());
        Saved->Language=0;Check(TEXT("language_english_text"),Localize(TEXT("Settings"),TEXT("Ustawienia"))==TEXT("Settings"));
        Saved->Language=1;Check(TEXT("language_polish_text"),Localize(TEXT("Settings"),TEXT("Ustawienia"))==TEXT("Ustawienia"));UGameplayStatics::SaveGameToSlot(Saved,TEXT("ErlingProfile_Test"),0);
        {auto Loaded=Cast<UFootballSave>(UGameplayStatics::LoadGameFromSlot(TEXT("ErlingProfile_Test"),0));Check(TEXT("language_persistence"),Loaded&&Loaded->Language==1);}
        Saved->Language=0;UGameplayStatics::SaveGameToSlot(Saved,TEXT("ErlingProfile_Test"),0);ResetPlayer();Wait=.1f;break;
    case 63:
        ChangeScreen(EScreen::Game);ResetPlayer();Saved->CameraMode=0;Yaw=0;MoveForward=0;MoveRight=0;PlaceBall();
        {auto* Move=Cast<UErlingMovement>(Avatar->GetCharacterMovement());Move->SetMovementMode(MOVE_Walking);Move->Velocity=FVector(510,0,0);Move->LastMoveInputDirection=FVector(1,0,0);Move->TurnSkidRemaining=0;}
        Forward(1);Wait=.35f;break;
    case 64:
        Forward(-1);Wait=.45f;break;
    case 65:
        {auto* Move=Cast<UErlingMovement>(Avatar->GetCharacterMovement());const float Distance=FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetActorLocation());
        UE_LOG(LogTemp,Display,TEXT("POSSESSION_REVERSE_RUN distance=%f yaw=%f skid=%f"),Distance,Avatar->GetActorRotation().Yaw,Move->TurnSkidRemaining);
        Check(TEXT("run_reverse_keeps_dribble_control"),Distance<205.f&&Mode->HasDribbleControl(Avatar));Check(TEXT("run_reverse_suppresses_off_ball_skid"),Move->TurnSkidRemaining<=KINDA_SMALL_NUMBER);Check(TEXT("run_reverse_turns_player_with_ball"),Avatar->GetActorForwardVector().X<-.45f);}
        Forward(0);ResetPlayer();PlaceBall();SprintOn();{auto* Move=Cast<UErlingMovement>(Avatar->GetCharacterMovement());Move->SetMovementMode(MOVE_Walking);Move->Velocity=FVector(765,0,0);Move->LastMoveInputDirection=FVector(1,0,0);Move->TurnSkidRemaining=0;}Forward(1);Wait=.45f;break;
    case 66:
        Forward(-1);Wait=.5f;break;
    case 67:
        {auto* Move=Cast<UErlingMovement>(Avatar->GetCharacterMovement());const float Distance=FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetActorLocation());
        UE_LOG(LogTemp,Display,TEXT("POSSESSION_REVERSE_SPRINT distance=%f yaw=%f skid=%f"),Distance,Avatar->GetActorRotation().Yaw,Move->TurnSkidRemaining);
        Check(TEXT("sprint_reverse_keeps_ball_recoverable"),Distance<230.f&&Mode->HasDribbleControl(Avatar));Check(TEXT("sprint_reverse_uses_controlled_cut"),Move->TurnSkidRemaining<=KINDA_SMALL_NUMBER);}
        Forward(0);SprintOff();ResetPlayer();PlaceBall();SprintOn();Avatar->SetActorRotation(FRotator::ZeroRotator);
        {auto* Move=Cast<UErlingMovement>(Avatar->GetCharacterMovement());Move->SetMovementMode(MOVE_Walking);Move->Velocity=FVector(765,0,0);Move->LastMoveInputDirection=FVector(1,0,0);Move->TurnSkidRemaining=0;}
        Mode->DribbleDirection=FVector(1,0,0);Mode->SprintReleaseDirection=FVector(1,0,0);Mode->SprintReleaseUntil=Now+.3f;Mode->Ball->SetPhysicsLinearVelocity(FVector(1000,0,0));Right(1);Wait=.12f;break;
    case 68:
        {const FVector V=Mode->Ball->GetPhysicsLinearVelocity();const FVector Heading=V.GetSafeNormal2D();const float YawAbs=FMath::Abs(FMath::FindDeltaAngleDegrees(0.f,Avatar->GetActorRotation().Yaw));
        UE_LOG(LogTemp,Display,TEXT("SPRINT_RELEASE_TURN ball_velocity=%s actor_yaw=%f dribble_yaw=%f"),*V.ToString(),Avatar->GetActorRotation().Yaw,Mode->DribbleDirection.Rotation().Yaw);
        Check(TEXT("sprint_release_heading_ignores_new_input"),FVector::DotProduct(Heading,Mode->SprintReleaseDirection.GetSafeNormal2D())>.995f&&FMath::Abs(V.Y)<35.f);
        Check(TEXT("sprint_release_does_not_turn_runner_before_contact"),YawAbs<5.f);}
        Right(0);SprintOff();Mode->SprintReleaseUntil=0;ResetPlayer();MoveForward=1;MoveRight=1;TestDigitalMoveIntent=true;Avatar->SetActorRotation(FRotator::ZeroRotator);PlaceBall();FireShot(.5f);
        Check(TEXT("digital_diagonal_shot_is_assisted_on_target"),PendingShot&&PendingHasAim&&FMath::Abs(PendingAimTarget.Y)<319.f);
        Check(TEXT("digital_diagonal_shot_can_select_goal_corner"),FMath::Abs(PendingAimTarget.Y)>220.f);
        TestDigitalMoveIntent=false;ResetPlayer();Avatar->SetActorRotation(FRotator(0,20,0));PlaceBall();MoveForward=1;MoveRight=0;FireShot(.5f);
        Check(TEXT("full_stick_analog_shot_remains_unassisted"),PendingShot&&PendingHasAim&&FMath::Abs(PendingAimTarget.Y)>319.f);
        MoveForward=0;MoveRight=0;ResetPlayer();PlaceBall();Avatar->BallTrapCount=0;Mode->LastPossessionDirection=FVector(1,0,0);
        {auto* Move=Cast<UErlingMovement>(Avatar->GetCharacterMovement());Move->SetMovementMode(MOVE_Walking);Move->Velocity=FVector(510,0,0);Move->LastMoveInputDirection=FVector(1,0,0);Move->TurnSkidRemaining=0;}Forward(1);Wait=.35f;break;
    case 69:
        Forward(0);Wait=.72f;break;
    case 70:
        {const FVector V=Mode->Ball->GetPhysicsLinearVelocity();const float D=FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetActorLocation());const FVector Rel=Mode->Ball->GetComponentLocation()-Avatar->GetActorLocation();const float Ahead=FVector::DotProduct(Rel.GetSafeNormal2D(),Mode->LastPossessionDirection.GetSafeNormal2D())*Rel.Size2D();
        UE_LOG(LogTemp,Display,TEXT("BALL_TRAP stopped=%d velocity=%s distance=%f ahead=%f traps=%d"),Mode->BallStopped?1:0,*V.ToString(),D,Ahead,Avatar->BallTrapCount);
        Check(TEXT("releasing_move_traps_ball"),Mode->BallStopped&&Avatar->BallTrapCount>0);
        Check(TEXT("trapped_ball_is_stationary"),V.Size2D()<3.f);
        Check(TEXT("trapped_ball_stays_in_front_of_body"),Ahead>34.f&&D<105.f);}
        HeldBallPosition=Mode->Ball->GetComponentLocation();Wait=.4f;break;
    case 71:
        Check(TEXT("trapped_ball_does_not_roll_away"),FVector::Dist2D(HeldBallPosition,Mode->Ball->GetComponentLocation())<2.f&&Mode->Ball->GetPhysicsLinearVelocity().Size2D()<3.f);
        Avatar->SetActorRotation(FRotator(0,90,0));FireShot(.5f);
        {const FVector Remembered=Mode->LastPossessionDirection.GetSafeNormal2D();Check(TEXT("stopped_shot_uses_last_possession_direction"),PendingShot&&FVector::DotProduct(PendingVelocity.GetSafeNormal2D(),Remembered)>.94f);}
        ResetPlayer();PlaceBall();Mode->DribbleDirection=FVector(1,0,0);Mode->LastPossessionDirection=FVector(1,0,0);Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+FVector(8,0,-76),false,nullptr,ETeleportType::TeleportPhysics);Mode->Ball->SetPhysicsLinearVelocity(FVector(160,0,0));Forward(1);Wait=.12f;break;
    case 72:
        {const FVector Rel=Mode->Ball->GetComponentLocation()-Avatar->GetActorLocation();const float Ahead=FVector::DotProduct(Rel,Mode->DribbleDirection.GetSafeNormal2D());
        UE_LOG(LogTemp,Display,TEXT("BODY_CORRIDOR ball_rel=%s ahead=%f"),*Rel.ToString(),Ahead);Check(TEXT("controlled_ball_cannot_sit_under_player"),Ahead>=34.f);}
        Forward(0);ResetPlayer();PlaceBall();SprintOn();{auto* Move=Cast<UErlingMovement>(Avatar->GetCharacterMovement());Move->SetMovementMode(MOVE_Walking);Move->Velocity=FVector(765,0,0);Move->LastMoveInputDirection=FVector(1,0,0);Move->TurnSkidRemaining=0;}Forward(1);Wait=.45f;break;
    case 73:
        Forward(0);Wait=1.3f;break;
    case 74:
        {const float D=FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetActorLocation());UE_LOG(LogTemp,Display,TEXT("SPRINT_STOP_FINAL distance=%f ball_v=%s player_v=%s pending=%d stopped=%d release_dir=%s age=%f"),D,*Mode->Ball->GetPhysicsLinearVelocity().ToString(),*Avatar->GetVelocity().ToString(),Mode->BallStopRequested?1:0,Mode->BallStopped?1:0,*Mode->SprintReleaseDirection.ToString(),Now-Mode->BallStopStarted);Check(TEXT("sprint_release_stop_recovers_ball"),Mode->BallStopped&&D<120.f);Check(TEXT("sprint_stop_ball_is_stationary"),Mode->Ball->GetPhysicsLinearVelocity().Size2D()<3.f);}
        SprintOff();ResetPlayer();PlaceBall();Mode->HadControlInput=true;Mode->DribbleDirection=FVector(1,0,0);Mode->LastPossessionDirection=FVector(1,0,0);Mode->SprintReleaseDirection=FVector(1,0,0);Mode->SprintReleaseUntil=Now+.18f;
        {auto* Move=Cast<UErlingMovement>(Avatar->GetCharacterMovement());Move->SetMovementMode(MOVE_Walking);Move->Velocity=FVector(765,0,0);Move->LastMoveInputDirection=FVector(1,0,0);}
        Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+FVector(130,0,-76),false,nullptr,ETeleportType::TeleportPhysics);Mode->Ball->SetPhysicsLinearVelocity(FVector(900,0,0));MoveForward=0;MoveRight=0;Wait=.07f;break;
    case 75:
        {const FVector V=Mode->Ball->GetPhysicsLinearVelocity();const float D=FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetActorLocation());
        UE_LOG(LogTemp,Display,TEXT("PENDING_STOP_RELEASE distance=%f velocity=%s pending=%d stopped=%d"),D,*V.ToString(),Mode->BallStopRequested?1:0,Mode->BallStopped?1:0);
        Check(TEXT("stop_request_does_not_recall_active_sprint_release"),Mode->BallStopRequested&&!Mode->BallStopped&&Mode->SprintReleaseUntil>Now&&FVector::DotProduct(V.GetSafeNormal2D(),Mode->SprintReleaseDirection.GetSafeNormal2D())>.995f&&FMath::Abs(V.Y)<25.f);}
        Wait=1.05f;break;
    case 76:
        {const float D=FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetActorLocation());UE_LOG(LogTemp,Display,TEXT("PENDING_STOP_FINAL distance=%f ball_v=%s player_v=%s pending=%d stopped=%d release_until_delta=%f release_dir=%s age=%f"),D,*Mode->Ball->GetPhysicsLinearVelocity().ToString(),*Avatar->GetVelocity().ToString(),Mode->BallStopRequested?1:0,Mode->BallStopped?1:0,Mode->SprintReleaseUntil-Now,*Mode->SprintReleaseDirection.ToString(),Now-Mode->BallStopStarted);Check(TEXT("pending_stop_traps_after_release_reacquire"),Mode->BallStopped&&D<120.f&&Mode->Ball->GetPhysicsLinearVelocity().Size2D()<3.f);}
        SprintOff();Forward(0);Right(0);ResetPlayer();PlaceBall();Mode->DribbleMinDirectionAhead=MAX_flt;Mode->DribbleMinBodyAhead=MAX_flt;Mode->CarryFootSwitchCount=0;
        VideoTurnMinDirectionAhead=MAX_flt;VideoTurnMinBodyAhead=MAX_flt;VideoTurnMaxGap=0.f;VideoTurnSamples=0;Forward(1);Wait=.4f;break;
    case 77:
        Forward(0);Right(1);Wait=.45f;break;
    case 78:
        Right(0);Forward(-1);Wait=.45f;break;
    case 79:
        {const float FinalGap=FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetActorLocation());
        UE_LOG(LogTemp,Display,TEXT("VIDEO_TURN_REGRESSION samples=%d min_dribble_ahead=%f min_body_ahead=%f max_gap=%f final_gap=%f foot_switches=%d"),VideoTurnSamples,VideoTurnMinDirectionAhead,VideoTurnMinBodyAhead,VideoTurnMaxGap,FinalGap,Mode->CarryFootSwitchCount);
        Check(TEXT("run_turn_ball_stays_ahead_of_dribble_heading"),VideoTurnSamples>20&&VideoTurnMinDirectionAhead>=8.f);
        Check(TEXT("run_turn_ball_never_lags_behind_body"),VideoTurnSamples>20&&VideoTurnMinBodyAhead>=0.f);
        Check(TEXT("run_turn_ball_never_needs_large_catch_up"),VideoTurnSamples>20&&VideoTurnMaxGap<125.f&&FinalGap<105.f);
        Check(TEXT("carry_foot_does_not_ping_pong_during_turn"),Mode->CarryFootSwitchCount<=4);}
        Forward(0);Right(0);Wait=.1f;break;
    case 80:
        ResetPlayer();PlaceBall();SprintOn();SprintStressMinBodyAhead=MAX_flt;SprintStressMaxGap=0.f;SprintStressSamples=0;SprintStressLostControlFrames=0;
        Forward(1);Wait=.45f;break;
    case 81:Forward(0);Right(1);Wait=.16f;break;
    case 82:Right(0);Forward(-1);Wait=.16f;break;
    case 83:Forward(0);Right(-1);Wait=.16f;break;
    case 84:Right(0);Forward(1);Wait=.16f;break;
    case 85:Forward(0);Right(1);Wait=.16f;break;
    case 86:Right(0);Forward(-1);Wait=.16f;break;
    case 87:Forward(0);Right(-1);Wait=.16f;break;
    case 88:
        {const float FinalGap=FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetActorLocation());
        UE_LOG(LogTemp,Display,TEXT("SPRINT_STRESS samples=%d lost_frames=%d min_body_ahead=%f max_gap=%f final_gap=%f"),SprintStressSamples,SprintStressLostControlFrames,SprintStressMinBodyAhead,SprintStressMaxGap,FinalGap);
        Check(TEXT("sprint_rapid_turns_keep_ball_recoverable"),SprintStressSamples>50&&SprintStressLostControlFrames==0&&SprintStressMaxGap<250.f);
        Check(TEXT("sprint_rapid_turns_do_not_hide_ball_far_behind_body"),SprintStressSamples>50&&SprintStressMinBodyAhead>-70.f);
        Check(TEXT("sprint_rapid_turns_finish_with_ball_nearby"),FinalGap<180.f);}
        Forward(0);Right(0);SprintOff();Wait=.1f;break;
    case 89:
        ResetPlayer();PlaceBall();SprintOn();Mode->SprintDribbleTouchCount=0;Mode->DribbleDirection=FVector(1,0,0);Mode->SprintReleaseDirection=FVector(1,0,0);Mode->SprintReleaseUntil=Now+.3f;Mode->SprintNextTouchAt=0.f;
        {auto* Move=Cast<UErlingMovement>(Avatar->GetCharacterMovement());Move->SetMovementMode(MOVE_Walking);Move->Velocity=FVector(765,0,0);Move->LastMoveInputDirection=FVector(1,0,0);Move->TurnSkidRemaining=0;
        const FVector Foot=Avatar->GetMesh()->GetSocketLocation(TEXT("foot_l"));const float BallZ=Mode->Ball->GetComponentLocation().Z;
        Mode->Ball->SetWorldLocation(FVector(Foot.X+45.f,Foot.Y,BallZ),false,nullptr,ETeleportType::TeleportPhysics);Mode->Ball->SetPhysicsLinearVelocity(FVector(900,0,0));}
        // Dispatch the touching pose now: the next physics/animation step can move
        // both feet and ball apart before the test ever presents this contact.
        Forward(-1);Mode->Dribble(Avatar,1.f/60.f);Wait=.14f;break;
    case 90:
        UE_LOG(LogTemp,Display,TEXT("CLOSE_TURN_CONTACT min_foot=%f"),CloseTurnMinFoot);
        {const FVector V=Mode->Ball->GetPhysicsLinearVelocity();const FVector Released=Mode->SprintReleaseDirection.GetSafeNormal2D();
        UE_LOG(LogTemp,Display,TEXT("SPRINT_CLOSE_TURN touch_count=%d release_dir=%s ball_v=%s release_delta=%f"),Mode->SprintDribbleTouchCount,*Released.ToString(),*V.ToString(),Mode->SprintReleaseUntil-Now);
        Check(TEXT("close_sprint_turn_reacquires_with_real_foot_contact"),Mode->SprintDribbleTouchCount>0&&FVector::DotProduct(Released,FVector::ForwardVector)<.995f);
        Check(TEXT("close_sprint_turn_releases_along_new_heading"),!Released.IsNearlyZero()&&FVector::DotProduct(V.GetSafeNormal2D(),Released)>.9f);}
        Forward(0);SprintOff();Wait=.1f;break;
    case 91:
        ResetPlayer();PlaceBall();SprintOn();Mode->SprintDribbleTouchCount=0;Mode->DribbleDirection=FVector(1,0,0);Mode->SprintReleaseDirection=FVector(1,0,0);Mode->SprintReleaseUntil=Now+.3f;Mode->SprintNextTouchAt=0.f;
        {auto* Move=Cast<UErlingMovement>(Avatar->GetCharacterMovement());Move->SetMovementMode(MOVE_Walking);Move->Velocity=FVector(765,0,0);Move->LastMoveInputDirection=FVector(1,0,0);Move->TurnSkidRemaining=0;
        const FVector Foot=Avatar->GetMesh()->GetSocketLocation(TEXT("foot_l"));const float BallZ=Mode->Ball->GetComponentLocation().Z;
        Mode->Ball->SetWorldLocation(FVector(Foot.X+68.f,Foot.Y,BallZ),false,nullptr,ETeleportType::TeleportPhysics);Mode->Ball->SetPhysicsLinearVelocity(FVector(900,0,0));}
        Forward(-1);Wait=.08f;break;
    case 92:
        {const FVector V=Mode->Ball->GetPhysicsLinearVelocity();const FVector Released=Mode->SprintReleaseDirection.GetSafeNormal2D();
        UE_LOG(LogTemp,Display,TEXT("SPRINT_FALSE_RECONTACT touch_count=%d release_dir=%s ball_v=%s release_delta=%f"),Mode->SprintDribbleTouchCount,*Released.ToString(),*V.ToString(),Mode->SprintReleaseUntil-Now);
        Check(TEXT("sprint_release_does_not_recontact_from_remote_foot_distance"),Mode->SprintDribbleTouchCount==0&&Mode->SprintReleaseUntil>Now&&FVector::DotProduct(Released,FVector::ForwardVector)>.995f&&FVector::DotProduct(V.GetSafeNormal2D(),Released)>.995f);}
        Forward(0);SprintOff();Wait=.1f;break;
    case 93:
        ResetPlayer();PlaceBall();SprintOn();Mode->SprintDribbleTouchCount=0;Mode->DribbleDirection=FVector(1,0,0);Mode->SprintReleaseDirection=FVector(1,0,0);Mode->SprintReleaseUntil=Now-.01f;Mode->SprintNextTouchAt=Now+1.f;
        {auto* Move=Cast<UErlingMovement>(Avatar->GetCharacterMovement());Move->SetMovementMode(MOVE_Walking);Move->Velocity=FVector(765,0,0);Move->LastMoveInputDirection=FVector(1,0,0);Move->TurnSkidRemaining=0;
        const float BallZ=Mode->Ball->GetComponentLocation().Z;Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+FVector(160,0,BallZ-Avatar->GetActorLocation().Z),false,nullptr,ETeleportType::TeleportPhysics);Mode->Ball->SetPhysicsLinearVelocity(FVector(850,0,0));}
        Right(1);Wait=.08f;break;
    case 94:
        {const FVector V=Mode->Ball->GetPhysicsLinearVelocity();const FVector Released=Mode->SprintReleaseDirection.GetSafeNormal2D();
        UE_LOG(LogTemp,Display,TEXT("SPRINT_FAR_POST_RELEASE touch_count=%d release_dir=%s ball_v=%s gap=%f"),Mode->SprintDribbleTouchCount,*Released.ToString(),*V.ToString(),FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetActorLocation()));
        Check(TEXT("far_sprint_ball_stays_free_until_real_reacquire"),Mode->SprintDribbleTouchCount==0&&FVector::DotProduct(V.GetSafeNormal2D(),Released)>.995f&&FMath::Abs(V.Y)<25.f);}
        Right(0);SprintOff();Wait=.1f;break;
    case 95:
        ResetPlayer();PlaceBall();SprintOn();Mode->SprintDribbleFoot=TEXT("foot_l");Mode->SprintLeadFoot=TEXT("foot_l");Mode->SprintContactUntil=Now+.2f;Mode->SprintReleaseUntil=Now+.3f;Mode->SprintNextTouchAt=Now+.2f;Mode->SprintKickPending=true;Mode->SprintReleaseDirection=FVector(1,0,0);
        {const bool Launched=Mode->LaunchShot(Avatar,FVector(1700,300,200));Check(TEXT("shot_launches_from_sprint_state"),Launched);
        Check(TEXT("shot_clears_stale_sprint_recovery_state"),Launched&&Mode->SprintReleaseDirection.IsNearlyZero()&&Mode->SprintReleaseUntil==0.f&&Mode->SprintContactUntil==0.f&&!Mode->SprintKickPending&&Mode->SprintDribbleFoot==NAME_None&&Mode->SprintLeadFoot==NAME_None);}
        SprintOff();Wait=.1f;break;
    case 96:
        ResetPlayer();Mode->ResetBall();Avatar->SetActorLocation(FVector(-350,0,100));Avatar->SetActorRotation(FRotator::ZeroRotator);MoveForward=0;MoveRight=0;
        {const FVector Start=Avatar->GetActorLocation()+Avatar->GetActorForwardVector()*132.f+Avatar->GetActorRightVector()*48.f-FVector(0,0,76.f);
        Mode->Ball->SetWorldLocation(Start,false,nullptr,ETeleportType::TeleportPhysics);Mode->Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);ShotWindupStart=Start;ShotWindupMaxDisplacement=0.f;ShotWindupSamples=0;FireShot(.5f);
        Check(TEXT("shot_windup_started_from_offset_live_ball"),PendingShot);}
        Wait=FMath::Clamp(ShotContactTime*.45f,.02f,.06f);break;
    case 97:
        UE_LOG(LogTemp,Display,TEXT("SHOT_WINDUP_CONTINUITY samples=%d displacement=%f pending=%d contact_time=%f"),ShotWindupSamples,ShotWindupMaxDisplacement,PendingShot?1:0,ShotContactTime);
        Check(TEXT("shot_windup_does_not_teleport_ball_to_contact_marker"),ShotWindupSamples>0&&ShotWindupMaxDisplacement<24.f);
        Wait=.5f;break;
    case 98:
        Check(TEXT("physical_windup_still_launches_shot"),Mode->ShotInFlight&&!PendingShot&&Mode->Ball->GetPhysicsLinearVelocity().Size()>500.f);
        Wait=.1f;break;
    case 99:
        ResetPlayer();Mode->ResetBall();Avatar->SetActorLocation(FVector(-350,0,100));Avatar->SetActorRotation(FRotator::ZeroRotator);MoveForward=0;MoveRight=0;
        {auto* Move=Cast<UErlingMovement>(Avatar->GetCharacterMovement());Move->SetMovementMode(MOVE_Walking);Move->Velocity=FVector(765,0,0);Move->LastMoveInputDirection=FVector(1,0,0);Move->TurnSkidRemaining=0;
        const float BallZ=Mode->Ball->GetComponentLocation().Z;Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+FVector(195,0,BallZ-Avatar->GetActorLocation().Z),false,nullptr,ETeleportType::TeleportPhysics);Mode->Ball->SetPhysicsLinearVelocity(FVector(1050,0,0));
        FireShot(.5f);Check(TEXT("moving_outer_possession_shot_commits"),PendingShot);}
        Wait=.68f;break;
    case 100:
        {const float Gap=FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetActorLocation());UE_LOG(LogTemp,Display,TEXT("COMMITTED_MOVING_SHOT gap=%f shot_in_flight=%d pending=%d ball_v=%s"),Gap,Mode->ShotInFlight?1:0,PendingShot?1:0,*Mode->Ball->GetPhysicsLinearVelocity().ToString());
        Check(TEXT("committed_moving_shot_launches_without_reacquire_teleport"),Mode->ShotInFlight&&!PendingShot&&Mode->Ball->GetPhysicsLinearVelocity().Size()>500.f);}
        Wait=.1f;break;
    case 101:
        ResetPlayer();PlaceBall();SprintOn();Mode->SprintDribbleTouchCount=0;Mode->DribbleDirection=FVector(1,0,0);Mode->SprintReleaseDirection=FVector(1,0,0);Mode->SprintReleaseUntil=Now+.35f;Mode->SprintNextTouchAt=Now+1.f;Mode->SprintKickPending=false;Mode->SprintContactUntil=0.f;
        SprintChaseMaxYawStep=0.f;SprintChasePreviousYaw=Avatar->GetActorRotation().Yaw;SprintChaseYawSamples=0;SprintChaseYawPrimed=false;
        {auto* Move=Cast<UErlingMovement>(Avatar->GetCharacterMovement());Move->SetMovementMode(MOVE_Walking);Move->Velocity=FVector(765,0,0);Move->LastMoveInputDirection=FVector(1,0,0);Move->TurnSkidRemaining=0;
        const float BallZ=Mode->Ball->GetComponentLocation().Z;Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+FVector(175,85,BallZ-Avatar->GetActorLocation().Z),false,nullptr,ETeleportType::TeleportPhysics);Mode->Ball->SetPhysicsLinearVelocity(FVector(860,0,0));}
        Right(-1);Wait=.12f;break;
    case 102:
        {FVector Gap=Mode->Ball->GetComponentLocation()-Avatar->GetActorLocation();Gap.Z=0;const FVector Chase=Gap.GetSafeNormal2D();const FVector Vel=Avatar->GetVelocity().GetSafeNormal2D();const FVector Facing=Avatar->GetActorForwardVector().GetSafeNormal2D();
        UE_LOG(LogTemp,Display,TEXT("SPRINT_CHASE_LOCK gap=%f chase=%s velocity=%s facing=%s input=%s pending=%d"),Gap.Size2D(),*Chase.ToString(),*Vel.ToString(),*Facing.ToString(),*GetMoveIntentWorld().ToString(),Mode->SprintKickPending?1:0);
        Check(TEXT("sprint_release_chase_ignores_steering_until_contact"),!Mode->SprintKickPending&&FVector::DotProduct(Vel,Chase)>.985f);
        Check(TEXT("sprint_release_chase_faces_ball"),FVector::DotProduct(Facing,Chase)>.96f);
        Check(TEXT("sprint_release_chase_rotation_is_smooth"),SprintChaseYawSamples>=4&&SprintChaseMaxYawStep<7.f);
        UE_LOG(LogTemp,Display,TEXT("SPRINT_CHASE_SMOOTH samples=%d max_yaw_step=%f"),SprintChaseYawSamples,SprintChaseMaxYawStep);}
        Right(0);SprintOff();Wait=.1f;break;
    case 103:
        ResetPlayer();PlaceBall();MoveForward=0;MoveRight=0;FireShot(.5f);
        Check(TEXT("missed_contact_shot_commits_before_ball_is_taken"),PendingShot);
        if(PendingShot)
        {
            const float BallZ=Mode->Ball->GetComponentLocation().Z;
            Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+FVector(900,500,BallZ-Avatar->GetActorLocation().Z),false,nullptr,ETeleportType::TeleportPhysics);
            Mode->Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);
        }
        Wait=.72f;break;
    case 104:
        UE_LOG(LogTemp,Display,TEXT("SHOT_CANCEL_RECOVERY pending=%d in_flight=%d damping=%f gap=%f"),PendingShot?1:0,Mode->ShotInFlight?1:0,Mode->Ball->GetLinearDamping(),FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetActorLocation()));
        Check(TEXT("missed_physical_shot_cancels_without_remote_launch"),!PendingShot&&!Mode->ShotInFlight);
        Check(TEXT("missed_physical_shot_restores_free_ball_damping"),FMath::IsNearlyEqual(Mode->Ball->GetLinearDamping(),.3f,.02f));
        Wait=.1f;break;
    case 105:
        ResetPlayer();Mode->ResetBall();MoveForward=0;MoveRight=0;Avatar->SetActorLocation(FVector(-350,0,100));Avatar->SetActorRotation(FRotator::ZeroRotator);Yaw=90.f;
        {auto* Move=Cast<UErlingMovement>(Avatar->GetCharacterMovement());Move->SetMovementMode(MOVE_Walking);Move->Velocity=FVector::ZeroVector;Move->LastMoveInputDirection=FVector::ZeroVector;Move->TurnSkidRemaining=0;
        const FVector HoldWorld=Avatar->GetActorLocation()+FVector(56.f,14.f,-78.f);
        Mode->Ball->SetWorldLocation(HoldWorld,false,nullptr,ETeleportType::TeleportPhysics);Mode->Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);Mode->Ball->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
        Mode->BallStopped=true;Mode->BallStopRequested=false;Mode->BallStopGesturePlayed=true;Mode->BallStopFoot=TEXT("foot_r");Mode->PossessionActive=true;Mode->HadControlInput=false;Mode->DribbleDirection=FVector::ForwardVector;Mode->LastPossessionDirection=FVector::ForwardVector;
        Mode->BallStopAnchor=Avatar->GetActorTransform().InverseTransformPosition(HoldWorld);PivotExpectedLocal=Mode->BallStopAnchor;PivotMaxLocalDrift=0.f;PivotSamples=0;
        Avatar->BeginTurn(90.f);}
        Wait=.65f;break;
    case 106:
        {FVector Rel=Mode->Ball->GetComponentLocation()-Avatar->GetActorLocation();Rel.Z=0.f;const float Ahead=FVector::DotProduct(Rel,Avatar->GetActorForwardVector().GetSafeNormal2D());
        UE_LOG(LogTemp,Display,TEXT("PIVOT_BALL_HOLD samples=%d local_drift=%f ahead=%f gap=%f yaw=%f"),PivotSamples,PivotMaxLocalDrift,Ahead,Rel.Size2D(),Avatar->GetActorRotation().Yaw);
        Check(TEXT("stopped_ball_rotates_with_player_in_place"),PivotSamples>20&&PivotMaxLocalDrift<3.f&&Ahead>35.f);
        Check(TEXT("pivot_preserves_stationary_shot_direction_memory"),FVector::DotProduct(Mode->LastPossessionDirection.GetSafeNormal2D(),FVector::ForwardVector)>.98f);
        Check(TEXT("pivot_syncs_dribble_heading_to_body"),FVector::DotProduct(Mode->DribbleDirection.GetSafeNormal2D(),Avatar->GetActorForwardVector().GetSafeNormal2D())>.98f);}
        Forward(1);Wait=.12f;break;
    case 107:
        {FVector Rel=Mode->Ball->GetComponentLocation()-Avatar->GetActorLocation();Rel.Z=0.f;const float Ahead=FVector::DotProduct(Rel,Avatar->GetActorForwardVector().GetSafeNormal2D());
        UE_LOG(LogTemp,Display,TEXT("PIVOT_MOVE_RESUME ahead=%f gap=%f ball_v=%s"),Ahead,Rel.Size2D(),*Mode->Ball->GetPhysicsLinearVelocity().ToString());
        Check(TEXT("moving_after_pivot_does_not_snap_ball_from_behind"),Ahead>8.f&&Rel.Size2D()<125.f);}
        Forward(0);Yaw=0.f;Wait=.1f;break;
    case 108:
        ResetPlayer();Mode->ResetBall();MoveForward=0;MoveRight=0;Avatar->SetActorLocation(FVector(-350,0,100));Avatar->SetActorRotation(FRotator::ZeroRotator);
        {const float BallZ=Mode->Ball->GetComponentLocation().Z;Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+FVector(125,0,BallZ-Avatar->GetActorLocation().Z),false,nullptr,ETeleportType::TeleportPhysics);Mode->Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);
        StartCharge();Check(TEXT("shot_charge_arms_while_in_possession"),Charging&&ShotChargeArmed);
        ChargeStarted=Now-.72f;Mode->SprintReleaseDirection=FVector::ForwardVector;Mode->SprintReleaseUntil=Now+.25f;Mode->SprintKickPending=false;Mode->SprintContactUntil=0.f;
        Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+FVector(260,35,BallZ-Avatar->GetActorLocation().Z),false,nullptr,ETeleportType::TeleportPhysics);Mode->Ball->SetPhysicsLinearVelocity(FVector(820,0,0));ReleaseCharge();}
        Check(TEXT("shot_release_buffers_when_sprint_touch_is_temporarily_loose"),!Charging&&ShotBufferActive&&!PendingShot&&ShotBufferSeconds>.65f&&ShotBufferSeconds<.8f);
        Wait=.2f;break;
    case 109:
        Check(TEXT("buffered_shot_survives_without_immediate_recontact"),ShotBufferActive&&!PendingShot&&!Mode->ShotInFlight);
        SprintOff();Avatar->GetCharacterMovement()->StopMovementImmediately();
        {const FVector Foot=Avatar->GetMesh()->GetSocketLocation(TEXT("foot_r"));const float BallZ=Mode->Ball->GetComponentLocation().Z;
        Mode->Ball->SetWorldLocation(FVector(Foot.X+42.f,Foot.Y,BallZ),false,nullptr,ETeleportType::TeleportPhysics);Mode->Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);}
        Wait=.08f;break;
    case 110:
        Check(TEXT("buffered_shot_commits_at_next_real_foot_contact"),!ShotBufferActive&&PendingShot&&Avatar->Action==AFootballPlayer::EAction::Kick);
        Wait=.7f;break;
    case 111:
        Check(TEXT("buffered_shot_eventually_launches"),Mode->ShotInFlight&&!PendingShot&&Mode->Ball->GetPhysicsLinearVelocity().Size()>500.f);
        ResetPlayer();Mode->ResetBall();SprintOn();MoveForward=0;MoveRight=0;
        {const float BallZ=Mode->Ball->GetComponentLocation().Z;Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+FVector(125,0,BallZ-Avatar->GetActorLocation().Z),false,nullptr,ETeleportType::TeleportPhysics);StartCharge();ChargeStarted=Now-.5f;
        Mode->SprintReleaseDirection=FVector::ForwardVector;Mode->SprintReleaseUntil=Now+.2f;Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+FVector(310,120,BallZ-Avatar->GetActorLocation().Z),false,nullptr,ETeleportType::TeleportPhysics);ReleaseCharge();SprintOff();}
        Check(TEXT("shot_timeout_scenario_starts_buffered"),ShotBufferActive);Wait=ShotBufferWindow+.12f;break;
    case 112:
        UE_LOG(LogTemp,Display,TEXT("SHOT_BUFFER_TIMEOUT active=%d pending=%d in_flight=%d age=%f window=%f"),ShotBufferActive?1:0,PendingShot?1:0,Mode->ShotInFlight?1:0,Now-ShotBufferStarted,ShotBufferWindow);
        Check(TEXT("buffered_shot_expires_without_contact"),!ShotBufferActive&&!PendingShot&&!Mode->ShotInFlight);
        Wait=.1f;break;
    case 113:
        ResetPlayer();Mode->ResetBall();SprintOn();MoveForward=0;MoveRight=0;
        {const float BallZ=Mode->Ball->GetComponentLocation().Z;Mode->SprintReleaseDirection=FVector::ForwardVector;Mode->SprintReleaseUntil=Now+.12f;
        Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+FVector(300,25,BallZ-Avatar->GetActorLocation().Z),false,nullptr,ETeleportType::TeleportPhysics);Mode->Ball->SetPhysicsLinearVelocity(FVector(760,0,0));StartCharge();}
        Check(TEXT("shot_charge_can_arm_during_recent_recoverable_sprint_touch"),Charging&&ShotChargeArmed);Charging=false;ShotChargeArmed=false;Wait=.1f;break;
    case 114:
        Mode->SprintReleaseDirection=FVector::ForwardVector;Mode->SprintReleaseUntil=Now-2.f;
        {const float BallZ=Mode->Ball->GetComponentLocation().Z;Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+FVector(300,25,BallZ-Avatar->GetActorLocation().Z),false,nullptr,ETeleportType::TeleportPhysics);StartCharge();}
        Check(TEXT("shot_charge_does_not_arm_from_stale_sprint_release_state"),!Charging&&!ShotChargeArmed);SprintOff();Wait=.1f;break;
    case 115:
        ResetPlayer();Mode->ResetBall();SprintOn();MoveForward=0;MoveRight=0;Avatar->SetActorRotation(FRotator::ZeroRotator);
        {auto* Move=Cast<UErlingMovement>(Avatar->GetCharacterMovement());Move->SetMovementMode(MOVE_Walking);Move->Velocity=FVector(765,0,0);Move->LastMoveInputDirection=FVector(1,0,0);Move->TurnSkidRemaining=0;
        const float BallZ=Mode->Ball->GetComponentLocation().Z;Mode->SprintReleaseDirection=FVector::ForwardVector;Mode->SprintReleaseUntil=Now+.22f;Mode->SprintKickPending=false;Mode->SprintContactUntil=0.f;
        Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+FVector(285,95,BallZ-Avatar->GetActorLocation().Z),false,nullptr,ETeleportType::TeleportPhysics);Mode->Ball->SetPhysicsLinearVelocity(FVector(820,0,0));StartCharge();}
        Check(TEXT("charging_arms_on_recoverable_sprint_touch"),Charging&&ShotChargeArmed&&Mode->IsRecoverableSprintTouch(Avatar));
        Right(-1);Wait=.12f;break;
    case 116:
        {FVector Gap=Mode->Ball->GetComponentLocation()-Avatar->GetActorLocation();Gap.Z=0.f;const FVector Chase=Gap.GetSafeNormal2D();const FVector Velocity=Avatar->GetVelocity().GetSafeNormal2D();const FVector Facing=Avatar->GetActorForwardVector().GetSafeNormal2D();
        UE_LOG(LogTemp,Display,TEXT("CHARGING_SPRINT_CHASE gap=%f chase=%s velocity=%s facing=%s charging=%d"),Gap.Size2D(),*Chase.ToString(),*Velocity.ToString(),*Facing.ToString(),Charging?1:0);
        Check(TEXT("charging_preserves_sprint_release_chase"),Charging&&FVector::DotProduct(Velocity,Chase)>.985f);
        Check(TEXT("charging_preserves_sprint_release_facing"),Charging&&FVector::DotProduct(Facing,Chase)>.95f);}
        Right(0);Charging=false;ShotChargeArmed=false;SprintOff();Wait=.1f;break;
    case 117:
        ResetPlayer();Mode->ResetBall();SprintOn();MoveForward=0;MoveRight=0;
        {const float BallZ=Mode->Ball->GetComponentLocation().Z;Mode->SprintReleaseDirection=FVector::ForwardVector;Mode->SprintReleaseUntil=Now-1.5f;Mode->SprintKickPending=false;Mode->SprintContactUntil=0.f;Mode->SprintNextTouchAt=Now+1.f;
        Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+FVector(300,0,BallZ-Avatar->GetActorLocation().Z),false,nullptr,ETeleportType::TeleportPhysics);Mode->Dribble(Avatar,.016f);}
        Check(TEXT("expired_sprint_release_state_is_cleared"),Mode->SprintReleaseDirection.IsNearlyZero()&&Mode->SprintReleaseUntil==0.f&&Mode->SprintContactUntil==0.f&&!Mode->SprintKickPending);
        Wait=.1f;break;
    case 118:
        Mode->ResetBall();Mode->SprintReleaseDirection=FVector::ForwardVector;Mode->SprintReleaseUntil=Now+.2f;Mode->SprintKickPending=false;Mode->SprintContactUntil=0.f;Mode->SprintNextTouchAt=Now+1.f;
        {const float BallZ=Mode->Ball->GetComponentLocation().Z;Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+FVector(460,0,BallZ-Avatar->GetActorLocation().Z),false,nullptr,ETeleportType::TeleportPhysics);Mode->Dribble(Avatar,.016f);}
        Check(TEXT("hard_lost_sprint_release_state_is_cleared"),Mode->SprintReleaseDirection.IsNearlyZero()&&Mode->SprintReleaseUntil==0.f&&Mode->SprintContactUntil==0.f&&!Mode->SprintKickPending);
        SprintOff();Wait=.1f;break;
    case 119:
        ResetPlayer();Mode->ResetBall();SprintOn();MoveForward=0;MoveRight=0;TestDigitalMoveIntent=false;Avatar->SetActorRotation(FRotator::ZeroRotator);
        {auto* Move=Cast<UErlingMovement>(Avatar->GetCharacterMovement());Move->SetMovementMode(MOVE_Walking);Move->Velocity=FVector(765,0,0);Move->LastMoveInputDirection=FVector::ForwardVector;Move->TurnSkidRemaining=0;
        const float BallZ=Mode->Ball->GetComponentLocation().Z;Mode->SprintReleaseDirection=FVector::ForwardVector;Mode->SprintReleaseUntil=Now+.22f;Mode->SprintKickPending=false;Mode->SprintContactUntil=0.f;
        Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+FVector(285,95,BallZ-Avatar->GetActorLocation().Z),false,nullptr,ETeleportType::TeleportPhysics);Mode->Ball->SetPhysicsLinearVelocity(FVector(820,0,0));StartCharge();}
        Check(TEXT("analog_charge_caches_pre_chase_shot_aim"),Charging&&FVector::DotProduct(ShotChargeAimDirection,FVector::ForwardVector)>.995f);
        Right(-1);Wait=.12f;break;
    case 120:
        {const float FacingYaw=Avatar->GetActorRotation().Yaw;ReleaseCharge();
        Check(TEXT("analog_charge_can_face_ball_without_overwriting_aim"),ShotBufferActive&&FMath::Abs(FacingYaw)>2.f&&FVector::DotProduct(ShotBufferAimDirection,FVector::ForwardVector)>.995f);
        Right(0);SprintOff();Avatar->GetCharacterMovement()->StopMovementImmediately();
        const FVector Foot=Avatar->GetMesh()->GetSocketLocation(TEXT("foot_r"));const float BallZ=Mode->Ball->GetComponentLocation().Z;
        Mode->Ball->SetWorldLocation(FVector(Foot.X+42.f,Foot.Y,BallZ),false,nullptr,ETeleportType::TeleportPhysics);Mode->Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);}
        Wait=.08f;break;
    case 121:
        {const FVector ShotHeading=PendingVelocity.GetSafeNormal2D();
        UE_LOG(LogTemp,Display,TEXT("BUFFERED_ANALOG_AIM actor_yaw=%f cached=%s shot=%s"),Avatar->GetActorRotation().Yaw,*ShotBufferAimDirection.ToString(),*ShotHeading.ToString());
        Check(TEXT("buffered_analog_shot_preserves_pre_chase_aim"),PendingShot&&FVector::DotProduct(ShotHeading,FVector::ForwardVector)>.98f);}
        Wait=.7f;break;
    case 122:
        Check(TEXT("buffered_analog_shot_launches_after_preserved_aim"),Mode->ShotInFlight&&!PendingShot);
        TestDigitalMoveIntent=false;Wait=.1f;break;
    case 123:
        ResetPlayer();Mode->ResetBall();Mode->LastShot=-10.f;SprintOn();MoveForward=0;MoveRight=0;Avatar->SetActorRotation(FRotator::ZeroRotator);
        {auto* Move=Cast<UErlingMovement>(Avatar->GetCharacterMovement());Move->SetMovementMode(MOVE_Walking);Move->Velocity=FVector(765,0,0);Move->LastMoveInputDirection=FVector::ForwardVector;Move->TurnSkidRemaining=0;
        const float BallZ=Mode->Ball->GetComponentLocation().Z;Mode->SprintReleaseDirection=FVector::ForwardVector;Mode->SprintReleaseUntil=Now+.28f;Mode->SprintKickPending=false;Mode->SprintContactUntil=0.f;Mode->SprintNextTouchAt=Now+1.f;
        Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+FVector(190,20,BallZ-Avatar->GetActorLocation().Z),false,nullptr,ETeleportType::TeleportPhysics);Mode->Ball->SetPhysicsLinearVelocity(FVector(830,0,0));StartCharge();}
        Check(TEXT("torture_180_charge_arms_during_release"),Charging&&ShotChargeArmed);Forward(-1);Wait=.22f;break;
    case 124:
        {FVector Gap=Mode->Ball->GetComponentLocation()-Avatar->GetActorLocation();Gap.Z=0.f;const FVector Chase=Gap.GetSafeNormal2D();const FVector Vel=Avatar->GetVelocity().GetSafeNormal2D();const FVector Facing=Avatar->GetActorForwardVector().GetSafeNormal2D();const FVector BallHeading=Mode->Ball->GetPhysicsLinearVelocity().GetSafeNormal2D();
        UE_LOG(LogTemp,Display,TEXT("TORTURE_180_CHARGE gap=%f chase_dot=%f facing_dot=%f ball=%s cached=%s"),Gap.Size2D(),FVector::DotProduct(Vel,Chase),FVector::DotProduct(Facing,Chase),*BallHeading.ToString(),*ShotChargeAimDirection.ToString());
        Check(TEXT("torture_180_charge_keeps_runner_on_ball"),Charging&&Gap.Size2D()<420.f&&FVector::DotProduct(Vel,Chase)>.98f&&FVector::DotProduct(Facing,Chase)>.93f);
        Check(TEXT("torture_180_charge_does_not_steer_free_ball"),FVector::DotProduct(BallHeading,FVector::ForwardVector)>.995f&&FMath::Abs(Mode->Ball->GetPhysicsLinearVelocity().Y)<25.f);
        Check(TEXT("torture_180_charge_preserves_pre_turn_aim"),FVector::DotProduct(ShotChargeAimDirection,FVector::ForwardVector)>.995f);
        ReleaseCharge();Forward(0);
        Check(TEXT("torture_180_release_keeps_shot_command"),PendingShot||ShotBufferActive);
        if(ShotBufferActive)PutBallAtNearestFoot();}
        Wait=.08f;break;
    case 125:
        Check(TEXT("torture_180_shot_commits_on_recontact"),PendingShot||Mode->ShotInFlight);
        Wait=.75f;break;
    case 126:
        Check(TEXT("torture_180_shot_launches_once_and_clears_recovery"),Mode->ShotInFlight&&!PendingShot&&Mode->SprintReleaseDirection.IsNearlyZero()&&!Mode->SprintKickPending&&Mode->SprintContactUntil==0.f);
        SprintOff();Forward(0);Wait=.1f;break;
    case 127:
        ResetPlayer();Mode->ResetBall();Mode->LastShot=-10.f;SprintOn();MoveForward=0;MoveRight=0;TestDigitalMoveIntent=false;Avatar->SetActorRotation(FRotator(0,30,0));
        {auto* Move=Cast<UErlingMovement>(Avatar->GetCharacterMovement());Move->SetMovementMode(MOVE_Walking);Move->Velocity=FVector(765,0,0);Move->LastMoveInputDirection=FVector::ForwardVector;Move->TurnSkidRemaining=0;
        const float BallZ=Mode->Ball->GetComponentLocation().Z;Mode->SprintReleaseDirection=FVector::ForwardVector;Mode->SprintReleaseUntil=Now+.3f;Mode->SprintKickPending=false;Mode->SprintContactUntil=0.f;Mode->SprintNextTouchAt=Now+1.f;
        Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+FVector(280,-90,BallZ-Avatar->GetActorLocation().Z),false,nullptr,ETeleportType::TeleportPhysics);Mode->Ball->SetPhysicsLinearVelocity(FVector(820,0,0));StartCharge();}
        Check(TEXT("angled_release_shot_caches_analog_aim"),Charging&&FMath::Abs(ShotChargeAimDirection.Rotation().Yaw-30.f)<1.f);Right(-1);Wait=.18f;break;
    case 128:
        {const FVector BallHeading=Mode->Ball->GetPhysicsLinearVelocity().GetSafeNormal2D();const float FacingYaw=Avatar->GetActorRotation().Yaw;const FVector CachedAim=ShotChargeAimDirection;
        UE_LOG(LogTemp,Display,TEXT("TORTURE_ANGLED_RELEASE facing_yaw=%f cached_yaw=%f ball=%s"),FacingYaw,CachedAim.Rotation().Yaw,*BallHeading.ToString());
        Check(TEXT("angled_release_charge_keeps_ball_ballistic"),FVector::DotProduct(BallHeading,FVector::ForwardVector)>.995f&&FMath::Abs(Mode->Ball->GetPhysicsLinearVelocity().Y)<25.f);
        Check(TEXT("angled_release_chase_can_rotate_body_without_aim_drift"),FMath::Abs(FMath::FindDeltaAngleDegrees(30.f,FacingYaw))>4.f&&FMath::Abs(FMath::FindDeltaAngleDegrees(30.f,CachedAim.Rotation().Yaw))<1.f);
        ReleaseCharge();Right(0);Check(TEXT("angled_release_shot_buffers_until_contact"),ShotBufferActive&&!PendingShot);
        PutBallAtNearestFoot();}
        Wait=.08f;break;
    case 129:
        {const float Error=FMath::Abs(FMath::FindDeltaAngleDegrees(30.f,PendingVelocity.GetSafeNormal2D().Rotation().Yaw));
        UE_LOG(LogTemp,Display,TEXT("TORTURE_ANGLED_SHOT pending=%d yaw=%f error=%f"),PendingShot?1:0,PendingVelocity.GetSafeNormal2D().Rotation().Yaw,Error);
        Check(TEXT("angled_sprint_release_shot_preserves_cached_heading"),PendingShot&&Error<2.5f);}
        Wait=.75f;break;
    case 130:
        Check(TEXT("angled_sprint_release_shot_launches"),Mode->ShotInFlight&&!PendingShot);
        SprintOff();TestDigitalMoveIntent=false;Wait=.1f;break;
    case 131:
        BeginLooseBufferedShot(.55f,FVector(320,35,0));Check(TEXT("buffer_050_starts"),ShotBufferActive);Wait=.5f;break;
    case 132:
        {const float Age=Now-ShotBufferStarted;UE_LOG(LogTemp,Display,TEXT("BUFFER_REACQUIRE_050 age=%f active=%d"),Age,ShotBufferActive?1:0);Check(TEXT("buffer_survives_050_before_contact"),ShotBufferActive&&!PendingShot&&Age>.45f&&Age<.7f);PutBallAtNearestFoot();}Wait=.08f;break;
    case 133:
        Check(TEXT("buffer_commits_after_050_recontact"),!ShotBufferActive&&PendingShot);Wait=.75f;break;
    case 134:
        Check(TEXT("buffer_050_launches"),Mode->ShotInFlight&&!PendingShot);Wait=.1f;break;
    case 135:
        BeginLooseBufferedShot(.7f,FVector(330,-40,0));Check(TEXT("buffer_100_starts"),ShotBufferActive);Wait=1.f;break;
    case 136:
        {const float Age=Now-ShotBufferStarted;UE_LOG(LogTemp,Display,TEXT("BUFFER_REACQUIRE_100 age=%f active=%d"),Age,ShotBufferActive?1:0);Check(TEXT("buffer_survives_100_before_contact"),ShotBufferActive&&!PendingShot&&Age>.9f&&Age<1.15f);PutBallAtNearestFoot();}Wait=.08f;break;
    case 137:
        Check(TEXT("buffer_commits_after_100_recontact"),!ShotBufferActive&&PendingShot);Wait=.75f;break;
    case 138:
        Check(TEXT("buffer_100_launches"),Mode->ShotInFlight&&!PendingShot);Wait=.1f;break;
    case 139:
        BeginLooseBufferedShot(.85f,FVector(340,45,0));Check(TEXT("buffer_130_starts"),ShotBufferActive);Wait=1.3f;break;
    case 140:
        {const float Age=Now-ShotBufferStarted;UE_LOG(LogTemp,Display,TEXT("BUFFER_REACQUIRE_130 age=%f active=%d"),Age,ShotBufferActive?1:0);Check(TEXT("buffer_survives_130_before_contact"),ShotBufferActive&&!PendingShot&&Age>1.2f&&Age<1.39f);PutBallAtNearestFoot();}Wait=.08f;break;
    case 141:
        Check(TEXT("buffer_commits_after_130_recontact"),!ShotBufferActive&&PendingShot);Wait=.75f;break;
    case 142:
        Check(TEXT("buffer_130_launches"),Mode->ShotInFlight&&!PendingShot);Wait=.1f;break;
    case 143:
        ResetPlayer();Mode->ResetBall();Mode->LastShot=-10.f;SprintOn();MoveForward=0;MoveRight=0;Mode->SprintDribbleTouchCount=0;Mode->DribbleDirection=FVector::ForwardVector;Mode->SprintReleaseDirection=FVector::ForwardVector;Mode->SprintReleaseUntil=Now+.3f;Mode->SprintNextTouchAt=0.f;
        {auto* Move=Cast<UErlingMovement>(Avatar->GetCharacterMovement());Move->SetMovementMode(MOVE_Walking);Move->Velocity=FVector(765,0,0);Move->LastMoveInputDirection=FVector::ForwardVector;Move->TurnSkidRemaining=0;
        const FVector Foot=Avatar->GetMesh()->GetSocketLocation(TEXT("foot_l"));const float BallZ=Mode->Ball->GetComponentLocation().Z;Mode->Ball->SetWorldLocation(FVector(Foot.X+45.f,Foot.Y,BallZ),false,nullptr,ETeleportType::TeleportPhysics);Mode->Ball->SetPhysicsLinearVelocity(FVector(900,0,0));StartCharge();}
        Check(TEXT("rapid_touch_shot_charge_starts_before_recontact"),Charging);Forward(-1);Wait=.14f;break;
    case 144:
        {const int32 Touches=Mode->SprintDribbleTouchCount;UE_LOG(LogTemp,Display,TEXT("RAPID_TOUCH_SHOT touches=%d release=%s pending_touch=%d charging=%d"),Touches,*Mode->SprintReleaseDirection.ToString(),Mode->SprintKickPending?1:0,Charging?1:0);
        Check(TEXT("rapid_touch_occurs_while_shot_is_charging"),Charging&&Touches>=1&&!Mode->SprintReleaseDirection.IsNearlyZero());
        ReleaseCharge();Forward(0);Check(TEXT("rapid_touch_shot_command_survives_release"),PendingShot||ShotBufferActive);if(ShotBufferActive)PutBallAtNearestFoot();}
        Wait=.08f;break;
    case 145:
        Check(TEXT("rapid_touch_buffer_commits_on_next_contact"),PendingShot||Mode->ShotInFlight);Wait=.75f;break;
    case 146:
        Check(TEXT("rapid_sprint_touch_to_shot_launches_cleanly"),Mode->ShotInFlight&&!PendingShot&&Mode->SprintReleaseDirection.IsNearlyZero()&&!Mode->SprintKickPending);
        SprintOff();Forward(0);Wait=.1f;break;
    case 147:
        ResetPlayer();Mode->ResetBall();Mode->LastShot=-10.f;SprintOn();MoveForward=0;MoveRight=0;Mode->SprintDribbleTouchCount=0;Avatar->SetActorRotation(FRotator::ZeroRotator);
        {auto* Move=Cast<UErlingMovement>(Avatar->GetCharacterMovement());Move->SetMovementMode(MOVE_Walking);Move->Velocity=FVector(765,0,0);Move->LastMoveInputDirection=FVector::ForwardVector;Move->TurnSkidRemaining=0;
        const float BallZ=Mode->Ball->GetComponentLocation().Z;Mode->SprintReleaseDirection=FVector::ForwardVector;Mode->SprintReleaseUntil=Now+.25f;Mode->SprintKickPending=true;Mode->SprintContactUntil=Now+.09f;Mode->SprintNextTouchAt=Now+1.f;
        Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+FVector(460,0,BallZ-Avatar->GetActorLocation().Z),false,nullptr,ETeleportType::TeleportPhysics);Mode->Dribble(Avatar,.016f);}
        Check(TEXT("hard_loss_during_pending_contact_clears_all_recovery"),Mode->SprintReleaseDirection.IsNearlyZero()&&!Mode->SprintKickPending&&Mode->SprintContactUntil==0.f);
        {const float BallZ=Mode->Ball->GetComponentLocation().Z;Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+FVector(0,300,BallZ-Avatar->GetActorLocation().Z),false,nullptr,ETeleportType::TeleportPhysics);Mode->Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);}
        Wait=.18f;break;
    case 148:
        {const FVector Gap=(Mode->Ball->GetComponentLocation()-Avatar->GetActorLocation()).GetSafeNormal2D();const FVector Facing=Avatar->GetActorForwardVector().GetSafeNormal2D();
        UE_LOG(LogTemp,Display,TEXT("HARD_LOSS_REENTRY release=%s pending=%d yaw=%f facing_ball_dot=%f touches=%d"),*Mode->SprintReleaseDirection.ToString(),Mode->SprintKickPending?1:0,Avatar->GetActorRotation().Yaw,FVector::DotProduct(Facing,Gap),Mode->SprintDribbleTouchCount);
        Check(TEXT("hard_loss_reentry_does_not_reactivate_old_recovery"),Mode->SprintReleaseDirection.IsNearlyZero()&&!Mode->SprintKickPending&&Mode->SprintDribbleTouchCount==0&&FMath::Abs(Avatar->GetActorRotation().Yaw)<5.f&&FVector::DotProduct(Facing,Gap)<.5f);}
        SprintOff();Wait=.1f;break;
    case 149:
        ResetPlayer();Mode->ResetBall();Mode->LastShot=-10.f;SprintOn();MoveForward=0;MoveRight=0;
        {const float BallZ=Mode->Ball->GetComponentLocation().Z;Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+FVector(125,0,BallZ-Avatar->GetActorLocation().Z),false,nullptr,ETeleportType::TeleportPhysics);Mode->Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);
        Mode->SprintReleaseDirection=FVector::ForwardVector;Mode->SprintReleaseUntil=Now+.12f;StartCharge();
        Check(TEXT("hard_loss_during_charge_starts_from_valid_possession"),Charging&&ShotChargeArmed);
        Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+FVector(470,0,BallZ-Avatar->GetActorLocation().Z),false,nullptr,ETeleportType::TeleportPhysics);Mode->Ball->SetPhysicsLinearVelocity(FVector(900,0,0));Mode->Dribble(Avatar,.016f);}
        Wait=1.05f;break;
    case 150:
        Check(TEXT("hard_loss_during_charge_invalidates_recovery_context"),!Mode->IsRecoverableSprintTouch(Avatar)&&Mode->SprintReleaseDirection.IsNearlyZero());
        ReleaseCharge();
        UE_LOG(LogTemp,Display,TEXT("HARD_LOSS_CHARGE_RELEASE buffer=%d pending=%d charging=%d gap=%f"),ShotBufferActive?1:0,PendingShot?1:0,Charging?1:0,FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetActorLocation()));
        Check(TEXT("hard_loss_during_charge_does_not_create_fresh_buffer"),!ShotBufferActive&&!PendingShot&&!Charging);
        SprintOff();Wait=.1f;break;
    case 151:
        ResetPlayer();Mode->ResetBall();Mode->LastShot=-10.f;SprintOn();MoveForward=0;MoveRight=0;Avatar->SetActorRotation(FRotator::ZeroRotator);
        NaturalBufferCommitAge=-1.f;NaturalBufferMinFoot=MAX_flt;NaturalBufferWasActive=false;
        {auto* Move=Cast<UErlingMovement>(Avatar->GetCharacterMovement());Move->SetMovementMode(MOVE_Walking);Move->Velocity=FVector(765,0,0);Move->LastMoveInputDirection=FVector::ForwardVector;Move->TurnSkidRemaining=0;
        const float BallZ=Mode->Ball->GetComponentLocation().Z;Mode->SprintReleaseDirection=FVector::ForwardVector;Mode->SprintReleaseUntil=Now+.18f;Mode->SprintKickPending=false;Mode->SprintContactUntil=0.f;Mode->SprintNextTouchAt=Now+1.f;
        Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+FVector(260,45,BallZ-Avatar->GetActorLocation().Z),false,nullptr,ETeleportType::TeleportPhysics);Mode->Ball->SetPhysicsLinearVelocity(FVector(780,0,0));StartCharge();ChargeStarted=Now-.65f;ReleaseCharge();NaturalBufferWasActive=ShotBufferActive;}
        Check(TEXT("natural_chase_buffer_starts_without_forced_contact"),ShotBufferActive&&!PendingShot);Wait=1.35f;break;
    case 152:
        UE_LOG(LogTemp,Display,TEXT("NATURAL_BUFFER_RECONTACT commit_age=%f min_foot=%f active=%d pending=%d in_flight=%d gap=%f"),NaturalBufferCommitAge,NaturalBufferMinFoot,ShotBufferActive?1:0,PendingShot?1:0,Mode->ShotInFlight?1:0,FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetActorLocation()));
        Check(TEXT("natural_chase_recontacts_before_buffer_timeout"),NaturalBufferCommitAge>0.f&&NaturalBufferCommitAge<ShotBufferWindow&&NaturalBufferMinFoot<=72.f&&!ShotBufferActive);
        Check(TEXT("natural_chase_buffer_commits_or_launches_shot"),PendingShot||Mode->ShotInFlight);
        Wait=.7f;break;
    case 153:
        Check(TEXT("natural_chase_buffered_shot_launches_cleanly"),Mode->ShotInFlight&&!PendingShot&&Mode->SprintReleaseDirection.IsNearlyZero()&&!Mode->SprintKickPending);
        SprintOff();Wait=.1f;break;
    case 154:
        ResetPlayer();Mode->ResetBall();Mode->LastShot=-10.f;SprintOn();MoveForward=0;MoveRight=0;Mode->SprintDribbleTouchCount=0;
        {auto* Move=Cast<UErlingMovement>(Avatar->GetCharacterMovement());Move->SetMovementMode(MOVE_Walking);Move->Velocity=FVector(590,0,0);Move->LastMoveInputDirection=FVector::ForwardVector;Move->TurnSkidRemaining=0;
        const float BallZ=Mode->Ball->GetComponentLocation().Z;Mode->BallStopRequested=true;Mode->BallStopStarted=Now-.1f;Mode->HadControlInput=false;
        Mode->SprintReleaseDirection=FVector::ForwardVector;Mode->SprintReleaseUntil=Now+.2f;Mode->SprintKickPending=true;Mode->SprintContactUntil=Now-.01f;Mode->SprintNextTouchAt=Now+1.f;
        Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+FVector(300,0,BallZ-Avatar->GetActorLocation().Z),false,nullptr,ETeleportType::TeleportPhysics);Mode->Ball->SetPhysicsLinearVelocity(FVector(780,0,0));Mode->Dribble(Avatar,.016f);}
        UE_LOG(LogTemp,Display,TEXT("STOP_STALE_PENDING_CLEAN release=%s pending=%d stop=%d gap=%f touches=%d"),*Mode->SprintReleaseDirection.ToString(),Mode->SprintKickPending?1:0,Mode->BallStopRequested?1:0,FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetActorLocation()),Mode->SprintDribbleTouchCount);
        Check(TEXT("sprint_stop_remote_pending_contact_is_cancelled"),Mode->SprintReleaseDirection.IsNearlyZero()&&!Mode->SprintKickPending&&Mode->SprintContactUntil==0.f&&Mode->SprintDribbleTouchCount==0);
        Forward(1);Wait=.12f;break;
    case 155:
        UE_LOG(LogTemp,Display,TEXT("STOP_STALE_PENDING_RESUME release=%s pending=%d touches=%d ball_v=%s"),*Mode->SprintReleaseDirection.ToString(),Mode->SprintKickPending?1:0,Mode->SprintDribbleTouchCount,*Mode->Ball->GetPhysicsLinearVelocity().ToString());
        Check(TEXT("sprint_stop_resume_does_not_resurrect_remote_touch"),Mode->SprintReleaseDirection.IsNearlyZero()&&!Mode->SprintKickPending&&Mode->SprintDribbleTouchCount==0);
        Forward(0);SprintOff();Wait=.1f;break;
    case 156:
        ChangeScreen(EScreen::Game);Saved->CameraMode=0;Yaw=0;Pitch=-30;ResetPlayer();Forward(1);
        for(float Step:{1.f/30.f,1.f/60.f,1.f/120.f})
        {
            Mode->ResetBall();Mode->LastShot=-10.f;Avatar->GetCharacterMovement()->Velocity=FVector(510,0,0);
            const FVector Far=Avatar->GetActorLocation()+FVector(180,0,-74);
            Mode->Ball->SetWorldLocation(Far,false,nullptr,ETeleportType::TeleportPhysics);
            Mode->Dribble(Avatar,Step);
            Check(*FString::Printf(TEXT("loose_ball_no_magnet_%dfps"),FMath::RoundToInt(1.f/Step)),Mode->Ball->GetPhysicsLinearVelocity().Size2D()<.01f&&FVector::Dist2D(Far,Mode->Ball->GetComponentLocation())<.01f&&!Mode->PossessionActive);
            Mode->SprintReleaseDirection=FVector::ForwardVector;Mode->SprintReleaseUntil=Now-.2f;Mode->PossessionActive=true;
            const FVector Roll(610,80,0);Mode->Ball->SetPhysicsLinearVelocity(Roll);SprintOff();Mode->Dribble(Avatar,Step);
            Check(*FString::Printf(TEXT("sprint_to_run_no_recall_%dfps"),FMath::RoundToInt(1.f/Step)),Mode->Ball->GetPhysicsLinearVelocity().Equals(Roll,.01f)&&FVector::Dist2D(Far,Mode->Ball->GetComponentLocation())<.01f&&!Mode->PossessionActive);
        }
        ResetPlayer();Mode->ResetBall();Mode->LastShot=-10.f;SprintOff();
        ContactTestAnchor=Avatar->GetActorLocation()+FVector(350,0,-74);
        Mode->Ball->SetWorldLocation(ContactTestAnchor,false,nullptr,ETeleportType::TeleportPhysics);
        ContactTestAcquired=false;ContactTestSamples=0;ContactTestMaxFreeDrift=0;ContactTestMinFoot=MAX_flt;
        Forward(1);Wait=1.6f;break;
    case 157:
        UE_LOG(LogTemp,Display,TEXT("LOOSE_RECEIVE free_drift=%f samples=%d min_foot=%f acquired=%d"),ContactTestMaxFreeDrift,ContactTestSamples,ContactTestMinFoot,ContactTestAcquired);
        Check(TEXT("approach_stationary_ball_stays_put_before_contact"),ContactTestSamples>5&&ContactTestMaxFreeDrift<2.f);
        Check(TEXT("approach_acquires_at_foot_and_runs_normally"),ContactTestAcquired&&ContactTestMinFoot<=55.f&&Mode->PossessionActive&&Avatar->GetVelocity().Size2D()>400.f);
        Forward(0);Wait=.1f;break;
    case 158:
        ResetPlayer();Mode->ResetBall();Mode->LastShot=-10.f;SprintOn();Forward(1);
        Avatar->GetCharacterMovement()->Velocity=FVector(765,0,0);
        Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+FVector(210,0,-74),false,nullptr,ETeleportType::TeleportPhysics);
        Mode->Ball->SetPhysicsLinearVelocity(FVector(850,0,0));Mode->Ball->SetLinearDamping(.08f);
        Mode->SprintReleaseDirection=FVector::ForwardVector;Mode->SprintReleaseUntil=Now+.12f;Mode->PossessionActive=false;
        SprintOff();ContactTestAcquired=false;ContactTestSamples=0;ContactTestMinForwardSpeed=MAX_flt;ContactTestMinFoot=MAX_flt;
        Wait=3.0f;break;
    case 159:
        UE_LOG(LogTemp,Display,TEXT("SPRINT_RUN_RECONTACT min_vx=%f samples=%d min_foot=%f acquired=%d gap=%f"),ContactTestMinForwardSpeed,ContactTestSamples,ContactTestMinFoot,ContactTestAcquired,FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetActorLocation()));
        Check(TEXT("sprint_to_run_ball_never_reverses_before_contact"),ContactTestSamples>5&&ContactTestMinForwardSpeed>=-.5f);
        Check(TEXT("sprint_to_run_player_catches_then_carries"),ContactTestAcquired&&ContactTestMinFoot<=55.f&&Mode->PossessionActive&&!Sprint&&Avatar->GetVelocity().Size2D()>400.f);
        Forward(0);Wait=.1f;break;
    case 160:
        ResetPlayer();Avatar->SetActorLocation(FVector(2300,500,100));Yaw=0;Pitch=-40;CameraPivotInitialized=false;Wait=.6f;break;
    case 161:
        Screenshot(TEXT("Goal_Roof_Positive"));Wait=.2f;break;
    case 162:
        ResetPlayer();Avatar->SetActorLocation(FVector(-2300,500,100));Yaw=180;Pitch=-40;CameraPivotInitialized=false;Wait=.6f;break;
    case 163:
        Screenshot(TEXT("Goal_Roof_Negative"));Wait=.2f;break;
    case 164:
    case 166:
    case 168:
        ChangeScreen(EScreen::Game);Yaw=0;ResetPlayer();Mode->ResetBall();Mode->LastShot=-10.f;
        SprintOn();Avatar->GetCharacterMovement()->Velocity=FVector(765,0,0);
        TurnStart=Avatar->GetActorLocation();TurnBegin=Now;TurnAcquired=false;TurnMaxSide=0;TurnFreeSide=0;TurnFreeMinX=MAX_flt;TurnReverseTime=-1;
        Mode->Ball->SetWorldLocation(TurnStart+FVector(210,0,-74),false,nullptr,ETeleportType::TeleportPhysics);
        Mode->Ball->SetPhysicsLinearVelocity(FVector(850,0,0));Mode->Ball->SetLinearDamping(.08f);
        Mode->SprintReleaseDirection=FVector::ForwardVector;Mode->SprintReleaseUntil=Now+.12f;Mode->PossessionActive=false;
        if(TestStage_Latest!=168)SprintOff();
        Forward(TestStage_Latest==164?0:-1);Right(TestStage_Latest==164?1:0);Wait=2.f;break;
    case 165:
    case 167:
    case 169:
        UE_LOG(LogTemp,Display,TEXT("RELEASE_TURN stage=%d contact=%d free_side=%f free_min_x=%f reverse_time=%f max_side=%f gap=%f"),TestStage_Latest,TurnAcquired,TurnFreeSide,TurnFreeMinX,TurnReverseTime,TurnMaxSide,FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetActorLocation()));
        Check(*FString::Printf(TEXT("release_turn_%d_physically_recovers"),TestStage_Latest),TurnAcquired&&FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetActorLocation())<240.f);
        Check(*FString::Printf(TEXT("release_turn_%d_free_ball_keeps_momentum"),TestStage_Latest),TurnFreeSide<25.f&&TurnFreeMinX>=-.5f);
        // Allow natural foot-phase variation at 30/60/120 Hz, while requiring
        // a substantial reduction from the reproduced 465 cm pre-fix arc.
        if(TestStage_Latest==169)Check(TEXT("sprint_reverse_is_compact_after_recontact"),TurnReverseTime>0&&TurnReverseTime<1.45f&&TurnMaxSide<280.f);
        Forward(0);Right(0);SprintOff();Wait=.1f;break;
    case 170:
        ChangeScreen(EScreen::Game);Yaw=0;ResetPlayer();Mode->ResetBall();Mode->LastShot=-10.f;
        MoveForward=0;MoveRight=0;SprintOn();PutBallAtNearestFoot();Forward(1);Wait=1.f;break;
    case 171:
        Forward(0);Right(1);Wait=1.f;break;
    case 172:
        UE_LOG(LogTemp,Display,TEXT("FACING right yaw=%f dribble=%f speed=%f possession=%d stopped=%d stop=%d rootyaw=%f"),Avatar->GetActorRotation().Yaw,Mode->DribbleDirection.Rotation().Yaw,Avatar->GetVelocity().Size2D(),Mode->PossessionActive,Mode->BallStopped,Mode->BallStopRequested,Avatar->GetMesh()->GetSocketRotation(TEXT("root")).Yaw);
        Check(TEXT("sprint_right_turn_faces_travel"),FVector::DotProduct(Avatar->GetActorForwardVector(),Avatar->GetVelocity().GetSafeNormal2D())>.85f);
        Forward(-1);Right(0);Wait=1.f;break;
    case 173:
        UE_LOG(LogTemp,Display,TEXT("FACING reverse yaw=%f dribble=%f speed=%f possession=%d stopped=%d stop=%d rootyaw=%f"),Avatar->GetActorRotation().Yaw,Mode->DribbleDirection.Rotation().Yaw,Avatar->GetVelocity().Size2D(),Mode->PossessionActive,Mode->BallStopped,Mode->BallStopRequested,Avatar->GetMesh()->GetSocketRotation(TEXT("root")).Yaw);
        Check(TEXT("sprint_reverse_turn_faces_travel"),FVector::DotProduct(Avatar->GetActorForwardVector(),Avatar->GetVelocity().GetSafeNormal2D())>.85f);
        Forward(0);Right(0);Wait=.1f;break;
    case 174:
        ResetPlayer();Mode->ResetBall();Mode->LastShot=-10.f;MoveForward=0;MoveRight=0;SprintOn();
        Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+FVector(-55,45,-74),false,nullptr,ETeleportType::TeleportPhysics);
        Mode->SprintReleaseDirection=FVector::ForwardVector;Mode->SprintReleaseUntil=Now+.5f;Mode->PossessionActive=false;
        Wait=.15f;break;
    case 175:
        Check(TEXT("close_sprint_recovery_does_not_freeze_body_yaw"),FMath::Abs(Avatar->GetActorRotation().Yaw)>20.f);
        SprintOff();Wait=.1f;break;
    case 176:
    {
        Mode->ResetBall();Mode->ShotInFlight=true;
        for(float Sign:{-1.f,1.f})
        {
            PitchShotSeen=-100.f;PitchShotHoldRemaining=0.f;PitchShotOffset=FVector::ZeroVector;Mode->LastShot=Now;Mode->Scored=false;Mode->BallHidden=false;
            Mode->Ball->SetWorldLocation(FVector(0,0,100),false,nullptr,ETeleportType::TeleportPhysics);
            Mode->Ball->SetPhysicsLinearVelocity(FVector(Sign*1600,0,200));
            const FVector Cam(0,1500,1150),Normal(0,-280,170);
            const FVector Start=UpdatePitchShotTarget(1.f/60,Cam,Normal,68);
            Check(Sign>0?TEXT("pitch_camera_tracks_offscreen_right_goal"):TEXT("pitch_camera_tracks_offscreen_left_goal"),PitchShotTracking&&Sign*(Start.X-Normal.X)>0.f&&(Start-Normal).Size()<100.f);
            for(int32 I=0;I<60;I++)UpdatePitchShotTarget(1.f/60,Cam,Normal,68);
            const float BeforeReturn=PitchShotOffset.Size();
            if(Sign>0)Mode->Scored=true;else Mode->BallHidden=true;
            UpdatePitchShotTarget(1.f/60,Cam,Normal,68);
            Check(Sign>0?TEXT("pitch_camera_goal_holds_before_return"):TEXT("pitch_camera_miss_holds_before_return"),!PitchShotTracking&&PitchShotHoldRemaining>.5f&&PitchShotOffset.Size()>BeforeReturn*.95f);
            for(int32 I=0;I<300;I++)UpdatePitchShotTarget(1.f/60,Cam,Normal,68);
            Check(TEXT("pitch_camera_return_settles"),PitchShotOffset.Size()<1.f);
        }
        Mode->Scored=false;Mode->BallHidden=false;PitchShotSeen=-100.f;PitchShotHoldRemaining=0.f;PitchShotOffset=FVector::ZeroVector;
        Mode->Ball->SetWorldLocation(FVector(2400,0,100),false,nullptr,ETeleportType::TeleportPhysics);
        Mode->Ball->SetPhysicsLinearVelocity(FVector(1600,0,100));
        const FVector Normal(2400,-280,170);
        Check(TEXT("pitch_camera_visible_goal_keeps_normal_view"),UpdatePitchShotTarget(1.f/60,FVector(2400,1500,1150),Normal,68).Equals(Normal)&&!PitchShotTracking);
        Mode->ResetBall();Wait=.1f;break;
    }
    case 177:
    {
        Mode->ResetBall();auto* GoalBall=Mode->Ball;
        GoalBall->SetWorldLocation(FVector(2880,0,24),false,nullptr,ETeleportType::TeleportPhysics);
        Mode->Scored=true;Mode->ResetBall();GoalBall=Mode->FinishedBalls.Last();
        Check(TEXT("scored_ball_remains_visible_in_goal_after_reset"),Mode->Ball!=GoalBall&&GoalBall->IsVisible()&&GoalBall->IsSimulatingPhysics()&&GoalBall->GetComponentLocation().Equals(FVector(2880,0,24),.1f));
        auto* MissBall=Mode->Ball;MissBall->SetWorldLocation(FVector(2850,900,24),false,nullptr,ETeleportType::TeleportPhysics);
        Mode->HideMiss();
        Check(TEXT("missed_ball_stays_visible_and_physical"),MissBall->IsVisible()&&MissBall->IsSimulatingPhysics());
        Mode->ResetBall();MissBall=Mode->FinishedBalls.Last();
        Check(TEXT("missed_ball_retained_with_fresh_playable_ball"),Mode->Ball!=MissBall&&MissBall->IsVisible()&&MissBall->GetComponentLocation().Equals(FVector(2850,900,24),.1f)&&!Mode->BallHidden&&!Mode->Scored&&Mode->Ball->IsSimulatingPhysics());
        const FVector ActiveVelocity(100,200,0);Mode->Ball->SetPhysicsLinearVelocity(ActiveVelocity);
        MissBall->SetPhysicsLinearVelocity(FVector(500,0,0));
        auto* Net=Mode->Box(FVector(10000,0,0),FVector(1),FLinearColor::White,false);Net->ComponentTags.Add(TEXT("GoalNet"));
        Mode->NetHit(MissBall,nullptr,Net,FVector::ZeroVector,FHitResult());
        Check(TEXT("old_ball_net_contact_does_not_stop_active_ball"),Mode->Ball->GetPhysicsLinearVelocity().Equals(ActiveVelocity,.1f)&&MissBall->GetPhysicsLinearVelocity().Size2D()<.1f);
        Net->GetOwner()->Destroy();Mode->ResetBall();Wait=.1f;break;
    }
    case 178:
    {
        Mode->ResetBall();PitchViewInitialized=false;PitchShotTracking=false;PitchShotHoldRemaining=0.f;
        const FVector Start(100,200,100),Moved(1100,-600,100);
        UpdatePitchViewCenter(1.f/60,Start);Mode->ShotInFlight=true;Mode->LastShot=Now;
        for(int32 I=0;I<60;I++)UpdatePitchViewCenter(1.f/60,Moved);
        Check(TEXT("pitch_shot_anchor_ignores_player_movement"),PitchViewCenter.Equals(Start,.01f));
        Mode->Scored=true;PitchShotHoldRemaining=1.f;
        Check(TEXT("pitch_goal_hold_anchor_ignores_player_movement"),UpdatePitchViewCenter(1.f/60,Moved).Equals(Start,.01f));
        PitchShotHoldRemaining=0.f;const FVector First=UpdatePitchViewCenter(1.f/60,Moved);
        Check(TEXT("pitch_anchor_returns_without_snap"),!First.Equals(Start)&&(First-Start).Size()<100.f&&(First-Moved).Size()>100.f);
        for(int32 I=0;I<240;I++)UpdatePitchViewCenter(1.f/60,Moved);
        Check(TEXT("pitch_anchor_return_reaches_current_player"),PitchViewCenter.Equals(Moved,.1f));
        Mode->ResetBall();PitchViewInitialized=false;Wait=.1f;break;
    }
    default:
        FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("runtime_checks_Latest.txt")));
        FPlatformMisc::RequestExit(false);return;
    }
    TestStage_Latest++;TestAt=Now+Wait;
}
