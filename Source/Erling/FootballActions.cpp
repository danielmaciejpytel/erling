#include "Football.h"
#include "ErlingAnimation.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

void AFootballPlayer::ConfigurePiece(USkeletalMeshComponent* Piece)
{
    Piece->SetLeaderPoseComponent(nullptr);
    Piece->SetForcedLOD(1);
    Piece->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Piece->SetAnimationMode(EAnimationMode::AnimationBlueprint);
    Piece->SetAnimInstanceClass(UErlingAnimInstance::StaticClass());
    Piece->AddTickPrerequisiteActor(this);
    Piece->bEnableUpdateRateOptimizations=false;
    // Flip/root motion can move the stylized modular pieces far outside their rest bounds.
    // A generous dynamic bound prevents one-frame component culling during the airborne flip.
    Piece->SetBoundsScale(4.f);
    // The comparison map owns its materials; the original Pitch keeps its look.
    if(GetWorld()->GetMapName().Contains(TEXT("Pitch_ArtDirection")))
    {
        Piece->SetRenderCustomDepth(true);
        Piece->SetCustomDepthStencilValue(1);
        for(int32 Index=0;Index<Piece->GetNumMaterials();++Index)
        {
            if(auto* Original=Piece->GetMaterial(Index))
            {
                const FString Path=FString::Printf(TEXT("/Game/Erling/ArtDirection/%s.%s"),*Original->GetName(),*Original->GetName());
                if(auto* Styled=LoadObject<UMaterialInterface>(nullptr,*Path))Piece->SetMaterial(Index,Styled);
            }
        }
    }
}
bool AFootballPlayer::CanAct()const
{
    return Action==EAction::None&&!GetCharacterMovement()->IsFalling()&&!PhysicalJump;
}
bool AFootballPlayer::StartAction(EAction Kind,const FString& Clip,float Rate,float StartFraction,bool Interruptible)
{
    UAnimSequence* Sequence=Animations.FindRef(Clip);
    if(!Sequence)return false;
    CancelBallTrap();
    TurningInPlace=false;
    EntryVelocity=GetVelocity();EntryVelocity.Z=0;ActionStartLocation=GetActorLocation();
    SetAnimation(Clip,false);PlaybackRate=Rate;AnimationTime=Sequence->GetPlayLength()*StartFraction;
    ActionStartTime=AnimationTime;ActionDuration=(Sequence->GetPlayLength()-AnimationTime)/Rate;
    ActionElapsed=0;Action=Kind;ActionInterruptible=Interruptible;ActionAllowsMovement=false;PhysicalJump=false;
    ActionVelocity=FVector::ZeroVector;GetCharacterMovement()->bOrientRotationToMovement=false;
    ConsumeMovementInputVector();StopJumping();
    return true;
}
void AFootballPlayer::ClearAction()
{
    const bool WasSlide=Action==EAction::Slide;
    TurningInPlace=false;
    Action=EAction::None;ActionElapsed=0;ActionVelocity=FVector::ZeroVector;ActionInterruptible=false;ActionAllowsMovement=false;
    if(WasSlide){auto& Velocity=GetCharacterMovement()->Velocity;Velocity.X=0;Velocity.Y=0;}
    GetCharacterMovement()->bOrientRotationToMovement=true;KickUntil=0;PlaybackRate=1;
}
void AFootballPlayer::StartBallTrap(FName Foot)
{
    if(Action!=EAction::None||PhysicalJump||TurningInPlace||GetCharacterMovement()->IsFalling())return;
    const FString Clip=Foot==TEXT("foot_l")?TEXT("Kick_Left"):TEXT("Kick_Right");
    auto* Sequence=Animations.FindRef(Clip);if(!Sequence)return;
    BallTrapActive=true;BallTrapElapsed=0.f;BallTrapFoot=Foot;BallTrapCount++;
    TurningInPlace=false;SetAnimation(Clip,false);PlaybackRate=1.f;
    // Only use the preparation/contact part of the kick clip. It reads as a short
    // sole/inside-foot trapping gesture without turning the stop into a shot.
    AnimationTime=Sequence->GetPlayLength()*.12f;
    GetCharacterMovement()->bOrientRotationToMovement=false;
}
void AFootballPlayer::CancelBallTrap()
{
    if(!BallTrapActive)return;
    BallTrapActive=false;BallTrapElapsed=0.f;BallTrapFoot=NAME_None;PlaybackRate=1.f;
    if(Action==EAction::None&&!TurningInPlace)GetCharacterMovement()->bOrientRotationToMovement=true;
}
void AFootballPlayer::UpdateAction(float Dt)
{
    auto Advance=[this,Dt](const FString& Name,float& Time,float Rate,bool Loop)
    {if(auto Sequence=Animations.FindRef(Name)){const float Length=Sequence->GetPlayLength();Time+=Dt*Rate;Time=Loop?FMath::Fmod(Time,FMath::Max(.01f,Length)):FMath::Min(Time,Length);}};
    Advance(CurrentAnimation,AnimationTime,PlaybackRate,AnimationLoops);
    Advance(PreviousAnimation,PreviousAnimationTime,PreviousRate,PreviousLoops);
    AnimationBlend=FMath::Min(1.f,AnimationBlend+Dt/FMath::Max(.01f,BlendSeconds));
    if(PreviewAnimation)return;
    if(Action!=EAction::None)
    {
        ActionElapsed+=Dt;
        if(Action==EAction::Slide)
        {
            // Midpoint integration keeps the travel consistent at different frame rates.
            const float U=FMath::Clamp((ActionElapsed-.5f*Dt)/ActionDuration,0.f,1.f);
            ActionVelocity=EntryVelocity.GetSafeNormal()*SlideDistance*2.f/ActionDuration*(1.f-U);
        }
        else if(Action==EAction::Flip)
            ActionVelocity=EntryVelocity*FMath::Lerp(1.15f,.75f,FMath::SmoothStep(.65f,1.f,ActionElapsed/ActionDuration));
        else if(Action==EAction::Trip)
            ActionVelocity=EntryVelocity*(1.f-FMath::SmoothStep(.5f,.8f,ActionElapsed));
        else if(Action==EAction::Kick)
            ActionVelocity=EntryVelocity*FMath::Lerp(1.f,.45f,FMath::SmoothStep(0.f,.3f,ActionElapsed));
        else ActionVelocity=FVector::ZeroVector;
        if(ActionElapsed+KINDA_SMALL_NUMBER>=ActionDuration)
        {
            if(Action==EAction::Fall){StartAction(EAction::GetUp,TEXT("GetUp_Back"),1.25f);return;}
            ClearAction();
        }
        else return;
    }
    const float Speed=GetVelocity().Size2D();
    if(TurningInPlace)
    {
        if(Speed>15||GetCharacterMovement()->IsFalling()){TurningInPlace=false;GetCharacterMovement()->bOrientRotationToMovement=true;}
        else
        {
            TurnElapsed+=Dt;
            const float Alpha=FMath::SmoothStep(0.f,1.f,FMath::Clamp(TurnElapsed/.6f,0.f,1.f));
            SetActorRotation(FRotator(0,TurnStartYaw+FMath::FindDeltaAngleDegrees(TurnStartYaw,TurnTargetYaw)*Alpha,0));
            if(Alpha<1)return;
            TurningInPlace=false;GetCharacterMovement()->bOrientRotationToMovement=true;
        }
    }
    if(GetCharacterMovement()->IsFalling())
    {
        if(!PhysicalJump){PhysicalJump=true;JumpElapsed=0;JumpAnimationScale=1.f;SetAnimation(Speed>80?TEXT("Jump_Run"):TEXT("Jump"),false);}
        JumpElapsed+=Dt;
        // Hold the flight pose; landing advances the recovery rather than replaying a second takeoff.
        if(auto Sequence=Animations.FindRef(CurrentAnimation))
        {
            const float Phase=GetVelocity().Z>40?.44f:GetVelocity().Z< -40?.65f:.55f;
            const float Desired=Sequence->GetPlayLength()*Phase;
            AnimationTime=FMath::FInterpTo(AnimationTime,Desired,Dt,CurrentAnimation==TEXT("Jump_Run")?14.f*JumpAnimationScale:12.f);
        }
        return;
    }
    if(PhysicalJump)
    {
        if(auto Sequence=Animations.FindRef(CurrentAnimation);Sequence&&AnimationTime<Sequence->GetPlayLength()-.01f)return;
        PhysicalJump=false;
    }
    if(BallTrapActive)
    {
        BallTrapElapsed+=Dt;
        const FString Clip=BallTrapFoot==TEXT("foot_l")?TEXT("Kick_Left"):TEXT("Kick_Right");
        if(CurrentAnimation!=Clip)SetAnimation(Clip,false);
        if(auto Sequence=Animations.FindRef(Clip))
        {
            const float Alpha=FMath::SmoothStep(0.f,1.f,FMath::Clamp(BallTrapElapsed/.24f,0.f,1.f));
            AnimationTime=Sequence->GetPlayLength()*FMath::Lerp(.12f,.31f,Alpha);
        }
        if(BallTrapElapsed<.24f)return;
        BallTrapActive=false;BallTrapElapsed=0.f;BallTrapFoot=NAME_None;PlaybackRate=1.f;
        SetAnimation(TEXT("Idle_Breathe"));
        GetCharacterMovement()->bOrientRotationToMovement=true;
    }
    if(Speed>15)
    {
        IdleElapsed=0;
        // Hysteresis and shared normalized phase avoid chatter and swapped supporting feet.
        const float RunThreshold=CurrentAnimation==TEXT("Walk")?215:180;
        const float SprintThreshold=CurrentAnimation==TEXT("Sprint")?595:635;
        const auto* PC=Cast<AFootballController>(GetController());
        const bool ControlledWalk=PC&&PC->BallControlHeld&&!PC->Sprint;
        const FString Clip=ControlledWalk?TEXT("Walk"):Speed>SprintThreshold?TEXT("Sprint"):Speed>RunThreshold?TEXT("Run"):TEXT("Walk");
        SetAnimation(Clip);
        const float Reference=Clip==TEXT("Sprint")?765:Clip==TEXT("Run")?510:180;
        PlaybackRate=FMath::FInterpTo(PlaybackRate,FMath::Clamp(Speed/Reference,.65f,1.3f),Dt,10.f);
    }
    else
    {
        PlaybackRate=1;IdleElapsed+=Dt;
        const int32 Phase=int32(IdleElapsed/6.f)%4;
        SetAnimation(Phase==1?TEXT("Idle_LookAround"):Phase==3?TEXT("Idle_WeightShift"):TEXT("Idle_Breathe"));
    }
}
void AFootballPlayer::StartPhysicalJump()
{
    if(!CanAct())return;
    TurningInPlace=false;GetCharacterMovement()->bOrientRotationToMovement=true;
    const auto* PC=Cast<AFootballController>(GetController());
    // Latch at takeoff so releasing sprint in mid-air does not change animation tempo.
    JumpAnimationScale=PC&&PC->Sprint&&GetVelocity().Size2D()>560.f?1.05f:1.f;
    PhysicalJump=true;JumpElapsed=0;PlaybackRate=GetVelocity().Size2D()>80?2.3f*JumpAnimationScale:2.f;
    SetAnimation(GetVelocity().Size2D()>80?TEXT("Jump_Run"):TEXT("Jump"),false);
    if(auto Sequence=Animations.FindRef(CurrentAnimation))AnimationTime=Sequence->GetPlayLength()*.28f;
    Jump();
    // Consume the edge now: a press and release within one frame must still jump.
    CheckJumpInput(0.f);
}
void AFootballPlayer::BeginTurn(float TargetYaw)
{
    if(!CanAct()||TurningInPlace||GetVelocity().Size2D()>15)return;
    TurningInPlace=true;TurnElapsed=0;TurnStartYaw=GetActorRotation().Yaw;TurnTargetYaw=TargetYaw;
    SetAnimation(TEXT("Turn_Step"),false);PlaybackRate=3.f;GetCharacterMovement()->bOrientRotationToMovement=false;
}

bool AFootballMode::HasBall(const AFootballPlayer* P)const
{
    if(!P||!Ball||Scored||BallHidden||ShotInFlight)return false;
    const FVector Delta=Ball->GetComponentLocation()-P->GetActorLocation();
    return Delta.Size2D()<205&&Delta.Z< -25&&Delta.Z> -125;
}
bool AFootballMode::HasDribbleControl(const AFootballPlayer* P)const
{
    if(!P||!Ball||Scored||BallHidden||ShotInFlight||P->GetCharacterMovement()->IsFalling())return false;
    const FVector Delta=Ball->GetComponentLocation()-P->GetActorLocation();
    float ControlRadius=230.f;
    if(GetWorld()->GetTimeSeconds()<SprintReleaseUntil)ControlRadius=270.f;
    else if(BallStopRequested&&!SprintReleaseDirection.IsNearlyZero())ControlRadius=270.f;
    else if(const auto* PC=Cast<AFootballController>(P->GetController());PC&&PC->Sprint)ControlRadius=245.f;
    return Delta.Size2D()<=ControlRadius&&Delta.Z<=-35.f;
}
bool AFootballMode::IsRecoverableSprintTouch(const AFootballPlayer* P,float MaxGap)const
{
    if(!P||!Ball||BallHidden||Scored||ShotInFlight||SprintReleaseDirection.IsNearlyZero())return false;
    const FVector Gap=Ball->GetComponentLocation()-P->GetActorLocation();
    const float Now=GetWorld()->GetTimeSeconds();
    return Gap.Size2D()<MaxGap&&Gap.Z<=-35.f&&Now<=SprintReleaseUntil+1.35f;
}
void AFootballMode::ClearSprintReleaseRecovery()
{
    SprintDribbleFoot=NAME_None;SprintLeadFoot=NAME_None;SprintContactUntil=0.f;
    SprintReleaseUntil=0.f;SprintNextTouchAt=0.f;SprintKickPending=false;
    SprintReleaseDirection=FVector::ZeroVector;
}
bool AFootballMode::LaunchShot(AFootballPlayer* P,const FVector& Velocity,bool CommittedShot)
{
    if(!P||!Ball||BallHidden||Scored||(!CommittedShot&&FVector::Dist2D(Ball->GetComponentLocation(),P->GetActorLocation())>240))return false;
    LastShot=GetWorld()->GetTimeSeconds();LastShooter=P;ShotInFlight=true;PossessionActive=false;BallStopRequested=false;BallStopped=false;BallStopGesturePlayed=false;BallStopFoot=NAME_None;BallStopAnchor=FVector::ZeroVector;P->CancelBallTrap();
    ClearSprintReleaseRecovery();
    Ball->SetLinearDamping(0.f);Ball->SetPhysicsLinearVelocity(Velocity);
    if(auto PC=Cast<AFootballController>(P->GetController()))PC->PlayEffect(TEXT("kick"));
    return true;
}
void AFootballController::SlidePressed()
{
    auto* Mode=GetWorld()->GetAuthGameMode<AFootballMode>();
    if(Screen!=EScreen::Game||!Avatar||!Mode||!Avatar->CanAct()||Mode->HasBall(Avatar)||Avatar->GetVelocity().Size2D()<200)return;
    CancelPendingActions();
    const bool WasSprinting=Sprint&&Avatar->GetVelocity().Size2D()>600;
    if(Avatar->StartAction(AFootballPlayer::EAction::Slide,TEXT("Slide_From_Run"),3.2f,.25f))
        Avatar->SlideDistance=SlideRunDistance*(WasSprinting?SlideSprintMultiplier:1.f);
}
void AFootballController::OnGoal(AFootballPlayer* Scorer)
{
    if(Scorer!=Avatar)return;
    GoalUntil=GetWorld()->GetTimeSeconds()+5.f;GoalCelebrationUsed=false;
}
void AFootballController::BeginCelebration(bool Held)
{
    if(GoalCelebrationUsed||!Avatar){GoalSpacePending=false;return;}
    if(!Avatar->CanAct())return;
    GoalSpacePending=false;
    const FString Clip=Held?TEXT("JoyJump_Run"):TEXT("JoyJump_Standing");
    const float StartFraction=Held?.25f:0.f;
    if(Avatar->StartAction(AFootballPlayer::EAction::Emote,Clip,1.32f,StartFraction,false))
    {
        // Goal jumps are celebrations, not roots that pin the player to the grass.
        // Keep normal CharacterMovement active so an already-running player carries on.
        Avatar->ActionAllowsMovement=true;
        Avatar->GetCharacterMovement()->bOrientRotationToMovement=true;
        GoalCelebrationUsed=true;
    }
}
void AFootballController::CancelPendingActions()
{
    Charging=false;ShotChargeArmed=false;ShotBufferActive=false;ShotChargeAimDirection=FVector::ZeroVector;ShotBufferAimDirection=FVector::ZeroVector;PendingShot=false;PendingTrip=false;GoalSpacePending=false;SpaceHeld=false;GoalHoldRequested=false;
}
void AFootballController::UpdateActions(float Dt)
{
    const EScreen ActiveScreen=Screen==EScreen::Settings?SettingsReturn:Screen;
    if((ActiveScreen!=EScreen::Game&&ActiveScreen!=EScreen::Main&&ActiveScreen!=EScreen::Credits)||!Avatar)return;
    const float Now=GetWorld()->GetTimeSeconds();
    bool PhysicalShotContact=false;
    if(ShotBufferActive)
    {
        auto* Mode=GetWorld()->GetAuthGameMode<AFootballMode>();
        const bool Invalid=ActiveScreen!=EScreen::Game||!Mode||!Mode->Ball||Mode->BallHidden||Mode->Scored||Mode->ShotInFlight;
        if(Invalid||Now-ShotBufferStarted>ShotBufferWindow)
        {
            ShotBufferActive=false;
        }
        else if(!PendingShot&&Avatar->CanAct()&&Now>=KickCooldown)
        {
            const FVector BallPosition=Mode->Ball->GetComponentLocation();
            const FVector Gap=BallPosition-Avatar->GetActorLocation();
            bool RealContact=Mode->BallStopped;
            if(Avatar->GetMesh()&&Avatar->GetMesh()->DoesSocketExist(TEXT("foot_l"))&&Avatar->GetMesh()->DoesSocketExist(TEXT("foot_r")))
            {
                const FVector Left=Avatar->GetMesh()->GetSocketLocation(TEXT("foot_l"));
                const FVector Right=Avatar->GetMesh()->GetSocketLocation(TEXT("foot_r"));
                const FVector Foot=FVector::Dist2D(BallPosition,Left)<=FVector::Dist2D(BallPosition,Right)?Left:Right;
                const FVector FootGap=BallPosition-Foot;
                RealContact=FVector::Dist2D(BallPosition,Foot)<=72.f&&FMath::Abs(FootGap.Z)<=75.f;
            }
            else RealContact=Gap.Size2D()<=95.f&&Gap.Z<=-25.f;
            if(RealContact)
            {
                const float BufferedPower=ShotBufferSeconds;
                FireShot(BufferedPower,ShotBufferAimDirection);
                if(PendingShot)ShotBufferActive=false;
            }
        }
    }
    if(GoalSpacePending)
    {
        GoalHoldRequested|=SpaceHeld&&Now-SpaceStarted>=1.f;
        if(GoalHoldRequested||!SpaceHeld)BeginCelebration(GoalHoldRequested);
    }
    if(PendingShot)
    {
        if(auto* Mode=GetWorld()->GetAuthGameMode<AFootballMode>();Mode&&Mode->Ball&&!Mode->BallHidden)
        {
            const float Side=Avatar->CurrentAnimation==TEXT("Kick_Left")?-18.f:18.f;
            const FVector Contact=Avatar->GetActorLocation()+Avatar->GetActorForwardVector()*82+Avatar->GetActorRightVector()*Side-FVector(0,0,74);
            const bool Demo=ActiveScreen==EScreen::Main||ActiveScreen==EScreen::Credits;
            if(Demo)
            {
                // The scripted menu shot may still stage the ball during its cinematic
                // wind-up. Gameplay must never use this path: a live possessed ball
                // keeps its real world position until the striking foot reaches it.
                const float ContactBlend=FMath::SmoothStep(0.f,1.f,FMath::Clamp(Avatar->ActionElapsed/FMath::Max(.01f,ShotContactTime),0.f,1.f));
                const FVector Position=FMath::Lerp(ShotBallStart+Avatar->GetActorLocation()-ShotActorStart,Contact,ContactBlend);
                Mode->Ball->SetWorldLocation(Position,false,nullptr,ETeleportType::TeleportPhysics);
                Mode->Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);
            }
            else
            {
                // EA-FC-style ownership: the animation comes to the ball, not the ball
                // to an authored contact marker. Preserve continuous physics during the
                // very short plant/wind-up so there is no visible pre-shot teleport.
                Mode->Ball->SetLinearDamping(.72f);
                const bool NormalKick=Avatar->Action==AFootballPlayer::EAction::Kick;
                if(NormalKick)
                {
                    FVector StrikeDirection=PendingVelocity.GetSafeNormal2D();
                    if(StrikeDirection.IsNearlyZero())StrikeDirection=Avatar->GetActorForwardVector().GetSafeNormal2D();
                    const FVector StrikeRight=FRotationMatrix(StrikeDirection.Rotation()).GetUnitAxis(EAxis::Y);
                    const FVector BallPosition=Mode->Ball->GetComponentLocation();

                    // Move/rotate the player into a contextual strike position around the
                    // ball. The ball itself is never translated. This is the missing half
                    // of removing the old teleport: a committed player now adjusts the
                    // run-up so the authored kick can physically meet the live ball.
                    const float CurrentYaw=Avatar->GetActorRotation().Yaw;
                    const float TargetYaw=StrikeDirection.Rotation().Yaw;
                    const float NewYaw=FMath::FixedTurn(CurrentYaw,TargetYaw,900.f*Dt);
                    Avatar->SetActorRotation(FRotator(0.f,NewYaw,0.f));

                    const FVector Facing=Avatar->GetActorForwardVector().GetSafeNormal2D();
                    const FVector FacingRight=Avatar->GetActorRightVector().GetSafeNormal2D();
                    FVector DesiredActor=BallPosition-Facing*82.f-FacingRight*Side;
                    DesiredActor.Z=Avatar->GetActorLocation().Z;
                    FVector Approach=DesiredActor-Avatar->GetActorLocation();Approach.Z=0.f;
                    // Aim to arrive at the physical strike position only slightly after
                    // the authored contact frame. The longer +.34 s window below is a
                    // safety deadline, not the desired feel; normal shots should still
                    // meet the ball promptly instead of feeling queued/sluggish.
                    const float ApproachDeadline=ShotContactTime+.08f;
                    const float Remaining=FMath::Max(.06f,ApproachDeadline-Avatar->ActionElapsed);
                    FVector DesiredApproach=FVector::ZeroVector;
                    const FVector BallPlanarVelocity=Mode->Ball->GetPhysicsLinearVelocity().GetSafeNormal2D()*Mode->Ball->GetPhysicsLinearVelocity().Size2D();
                    if(Approach.Size2D()>4.f||BallPlanarVelocity.Size2D()>20.f)
                    {
                        const FVector CorrectionVelocity=Approach/Remaining;
                        DesiredApproach=(BallPlanarVelocity+CorrectionVelocity).GetClampedToMaxSize(1050.f);
                    }
                    if(auto* Movement=Avatar->GetCharacterMovement())
                    {
                        // UpdateAction rebuilds ActionVelocity from EntryVelocity each
                        // frame. Blend from the actual persisted movement velocity instead,
                        // then feed the result back into both paths.
                        FVector CurrentPlanar=Movement->Velocity;CurrentPlanar.Z=0.f;
                        Avatar->ActionVelocity=FMath::VInterpTo(CurrentPlanar,DesiredApproach,Dt,24.f);
                        Movement->Velocity.X=Avatar->ActionVelocity.X;
                        Movement->Velocity.Y=Avatar->ActionVelocity.Y;
                    }

                    if(Avatar->GetMesh())
                    {
                        const FName FootName=Avatar->CurrentAnimation==TEXT("Kick_Left")?TEXT("foot_l"):TEXT("foot_r");
                        if(Avatar->GetMesh()->DoesSocketExist(FootName))
                        {
                            const FVector Foot=Avatar->GetMesh()->GetSocketLocation(FootName);
                            const FVector FootToBall=BallPosition-Foot;
                            const float FootDistance=FVector::Dist2D(Foot,BallPosition);
                            const bool ContactPhase=Avatar->ActionElapsed>=ShotContactTime*.55f;
                            PhysicalShotContact=ContactPhase&&FootDistance<=54.f&&FMath::Abs(FootToBall.Z)<=70.f;

                            // Hold the kick at the authored contact pose for a very short
                            // adjustment window instead of letting the foot swing through
                            // empty space and launching the ball remotely.
                            if(!PhysicalShotContact&&Avatar->ActionElapsed>=ShotContactTime)
                            {
                                if(auto* Sequence=Avatar->Animations.FindRef(Avatar->CurrentAnimation))
                                    Avatar->AnimationTime=FMath::Min(Avatar->AnimationTime,Sequence->GetPlayLength()*.43f);

                                const bool LateNearContact=Avatar->ActionElapsed>=ShotContactTime+.12f&&FootDistance<=68.f&&FMath::Abs(FootToBall.Z)<=75.f;
                                PhysicalShotContact|=LateNearContact;
                            }
                        }
                    }
                }
            }
        }
    }
    const bool DemoOrFlipFallback=PendingShot&&(ActiveScreen==EScreen::Main||ActiveScreen==EScreen::Credits||Avatar->Action==AFootballPlayer::EAction::Flip)&&Avatar->ActionElapsed>=ShotContactTime;
    if(PendingShot&&(PhysicalShotContact||DemoOrFlipFallback))
    {
        PendingShot=false;
        if(auto* Mode=GetWorld()->GetAuthGameMode<AFootballMode>())
        {
            if(PendingHasAim)
            {
                const FVector Delta=PendingAimTarget-Mode->Ball->GetComponentLocation();
                const float Flight=Delta.Size2D()/FMath::Max(1.f,PendingVelocity.Size2D());
                PendingVelocity=Delta.GetSafeNormal2D()*PendingVelocity.Size2D();
                PendingVelocity.Z=Delta.Z/Flight-.5f*GetWorld()->GetGravityZ()*Flight;
            }
            // Possession was validated when FireShot committed PendingShot. Do not
            // re-acquire possession here: a fast live ball may legitimately open the
            // gap during the wind-up, and pulling it back would reintroduce the visual
            // teleport. Launch from its actual current position instead.
            const bool Launched=Mode->LaunchShot(Avatar,PendingVelocity,true);
            if(!Launched)PendingTrip=false;
            const bool Demo=ActiveScreen==EScreen::Main||ActiveScreen==EScreen::Credits;
            if(Launched&&Demo)
            {
                Avatar->EntryVelocity=FVector::ZeroVector;
                Avatar->ActionVelocity=FVector::ZeroVector;
                Avatar->GetCharacterMovement()->StopMovementImmediately();
            }
        }
    }
    if(PendingShot&&ActiveScreen==EScreen::Game&&Avatar->Action==AFootballPlayer::EAction::Kick&&Avatar->ActionElapsed>=ShotContactTime+.34f)
    {
        // If a contextual normal kick somehow still cannot physically meet the ball,
        // fail the action rather than resurrecting the old remote/ghost kick.
        PendingShot=false;PendingTrip=false;
        if(auto* Mode=GetWorld()->GetAuthGameMode<AFootballMode>();Mode&&Mode->Ball&&!Mode->BallHidden&&!Mode->ShotInFlight)
            Mode->Ball->SetLinearDamping(.3f);
    }
    if(PendingTrip&&!PendingShot&&Avatar->ActionElapsed>=ShotRecoverTime)
    {
        PendingTrip=false;
        Avatar->StartAction(AFootballPlayer::EAction::Trip,TEXT("Trip_Roll_From_Run"),1.35f,.22f);
        Avatar->EntryVelocity=ShotEntryVelocity;
    }
}

void AFootballController::CycleAnimationPreview(int32 Direction)
{
    TArray<FString> Names;Avatar->Animations.GetKeys(Names);Names.Sort();
    PreviewIndex=(PreviewIndex+Direction+Names.Num()+1)%(Names.Num()+1);
    Avatar->ClearAction();Avatar->PhysicalJump=false;
    Avatar->PreviewAnimation=PreviewIndex<Names.Num();
    Avatar->CurrentAnimation.Empty();
    const FString Clip=Avatar->PreviewAnimation?Names[PreviewIndex]:TEXT("Idle_Breathe");
    const bool Loop=Clip.StartsWith(TEXT("Idle_"))||Clip==TEXT("Walk")||Clip==TEXT("Run")||Clip==TEXT("Sprint")||Clip==TEXT("Dance_Robot");
    Avatar->SetAnimation(Clip,Loop);
    Avatar->PlaybackRate=1;
}
FText AFootballController::PreviewAnimationText()const
{
    static const TMap<FString,FString> LabelsEn={
        {TEXT("Walk"),TEXT("Walk")},{TEXT("Run"),TEXT("Run")},{TEXT("Sprint"),TEXT("Sprint")},
        {TEXT("Kick_Right"),TEXT("Right-Foot Kick")},{TEXT("Kick_Left"),TEXT("Left-Foot Kick")},
        {TEXT("Jump"),TEXT("Jump")},{TEXT("Jump_Run"),TEXT("Running Jump")},
        {TEXT("JoyJump_Run"),TEXT("Running Celebration")},{TEXT("JoyJump_Standing"),TEXT("Jump for Joy")},
        {TEXT("Shot_Flip_Land"),TEXT("Flip Shot")},{TEXT("Slide_From_Run"),TEXT("Slide")},
        {TEXT("Trip_Roll_From_Run"),TEXT("Trip and Roll")},{TEXT("Celebrate_Victory"),TEXT("Victory")},
        {TEXT("Dance_Disco"),TEXT("Disco Dance")},{TEXT("Dance_Robot"),TEXT("Robot Dance")},
        {TEXT("Fall_Backward"),TEXT("Backward Fall")},{TEXT("GetUp_Back"),TEXT("Get Up from Back")},
        {TEXT("GetUp_Front"),TEXT("Get Up from Front")},{TEXT("Idle_Breathe"),TEXT("Breathing")},
        {TEXT("Idle_LookAround"),TEXT("Looking Around")},{TEXT("Idle_WeightShift"),TEXT("Weight Shift")},
        {TEXT("Meme_RageStomp"),TEXT("Rage Stomp")},{TEXT("Meme_Shrug"),TEXT("Shrug")},
        {TEXT("Turn_Step"),TEXT("Step Turn")}};
    static const TMap<FString,FString> LabelsPl={
        {TEXT("Walk"),TEXT("Chód")},{TEXT("Run"),TEXT("Bieg")},{TEXT("Sprint"),TEXT("Sprint")},
        {TEXT("Kick_Right"),TEXT("Strzał prawą nogą")},{TEXT("Kick_Left"),TEXT("Strzał lewą nogą")},
        {TEXT("Jump"),TEXT("Skok")},{TEXT("Jump_Run"),TEXT("Skok z biegu")},
        {TEXT("JoyJump_Run"),TEXT("Cieszynka w biegu")},{TEXT("JoyJump_Standing"),TEXT("Skok z radości")},
        {TEXT("Shot_Flip_Land"),TEXT("Strzał z saltem")},{TEXT("Slide_From_Run"),TEXT("Wślizg")},
        {TEXT("Trip_Roll_From_Run"),TEXT("Potknięcie i przewrót")},{TEXT("Celebrate_Victory"),TEXT("Zwycięstwo")},
        {TEXT("Dance_Disco"),TEXT("Taniec disco")},{TEXT("Dance_Robot"),TEXT("Taniec robota")},
        {TEXT("Fall_Backward"),TEXT("Upadek do tyłu")},{TEXT("GetUp_Back"),TEXT("Wstawanie z pleców")},
        {TEXT("GetUp_Front"),TEXT("Wstawanie z brzucha")},{TEXT("Idle_Breathe"),TEXT("Oddychanie")},
        {TEXT("Idle_LookAround"),TEXT("Rozglądanie się")},{TEXT("Idle_WeightShift"),TEXT("Przenoszenie ciężaru")},
        {TEXT("Meme_RageStomp"),TEXT("Wściekłe tupanie")},{TEXT("Meme_Shrug"),TEXT("Wzruszenie ramion")},
        {TEXT("Turn_Step"),TEXT("Obrót krokiem")}};
    if(!Avatar||!Avatar->PreviewAnimation)return FText::FromString(Localize(TEXT("Idle"),TEXT("Bezczynność")));
    const auto& Labels=Saved&&Saved->Language==1?LabelsPl:LabelsEn;
    return FText::FromString(Labels.FindRef(Avatar->CurrentAnimation));
}
void AFootballController::BallControlOn(){if(Screen!=EScreen::Game||!Avatar)return;auto* Mode=GetWorld()->GetAuthGameMode<AFootballMode>();if(!Mode||!Mode->HasBall(Avatar))return;BallControlHeld=true;if(!Sprint)Avatar->GetCharacterMovement()->MaxWalkSpeed=190;}
void AFootballController::BallControlOff(){BallControlHeld=false;if(Avatar)Avatar->GetCharacterMovement()->MaxWalkSpeed=Sprint?765:500;}
