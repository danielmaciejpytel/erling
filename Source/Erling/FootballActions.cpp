#include "Football.h"
#include "ErlingAnimation.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

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
}
bool AFootballPlayer::CanAct()const
{
    return Action==EAction::None&&!GetCharacterMovement()->IsFalling()&&!PhysicalJump;
}
bool AFootballPlayer::StartAction(EAction Kind,const FString& Clip,float Rate,float StartFraction,bool Interruptible)
{
    UAnimSequence* Sequence=Animations.FindRef(Clip);
    if(!Sequence)return false;
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
        if(!PhysicalJump){PhysicalJump=true;JumpElapsed=0;SetAnimation(Speed>80?TEXT("Jump_Run"):TEXT("Jump"),false);}
        JumpElapsed+=Dt;
        // Hold the flight pose; landing advances the recovery rather than replaying a second takeoff.
        if(auto Sequence=Animations.FindRef(CurrentAnimation))
        {
            const float Phase=GetVelocity().Z>40?.44f:GetVelocity().Z< -40?.65f:.55f;
            const float Desired=Sequence->GetPlayLength()*Phase;
            AnimationTime=FMath::FInterpTo(AnimationTime,Desired,Dt,12.f);
        }
        return;
    }
    if(PhysicalJump)
    {
        if(auto Sequence=Animations.FindRef(CurrentAnimation);Sequence&&AnimationTime<Sequence->GetPlayLength()-.01f)return;
        PhysicalJump=false;
    }
    if(Speed>15)
    {
        IdleElapsed=0;
        // Hysteresis and shared normalized phase avoid chatter and swapped supporting feet.
        const float RunThreshold=CurrentAnimation==TEXT("Walk")?215:180;
        const float SprintThreshold=CurrentAnimation==TEXT("Sprint")?595:635;
        const FString Clip=Speed>SprintThreshold?TEXT("Sprint"):Speed>RunThreshold?TEXT("Run"):TEXT("Walk");
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
    PhysicalJump=true;JumpElapsed=0;PlaybackRate=2.f;
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
    return Delta.Size2D()<=230.f&&Delta.Z<=-35.f;
}
bool AFootballMode::LaunchShot(AFootballPlayer* P,const FVector& Velocity)
{
    if(!P||!Ball||BallHidden||Scored||FVector::Dist2D(Ball->GetComponentLocation(),P->GetActorLocation())>240)return false;
    LastShot=GetWorld()->GetTimeSeconds();LastShooter=P;ShotInFlight=true;
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
    if(Avatar->StartAction(AFootballPlayer::EAction::Emote,Clip,1.15f,StartFraction,false))
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
    Charging=false;PendingShot=false;PendingTrip=false;GoalSpacePending=false;SpaceHeld=false;GoalHoldRequested=false;
}
void AFootballController::UpdateActions(float Dt)
{
    const EScreen ActiveScreen=Screen==EScreen::Settings?SettingsReturn:Screen;
    if((ActiveScreen!=EScreen::Game&&ActiveScreen!=EScreen::Main&&ActiveScreen!=EScreen::Credits)||!Avatar)return;
    const float Now=GetWorld()->GetTimeSeconds();
    if(GoalSpacePending)
    {
        GoalHoldRequested|=SpaceHeld&&Now-SpaceStarted>=1.f;
        if(GoalHoldRequested||!SpaceHeld)BeginCelebration(GoalHoldRequested);
    }
    if(PendingShot)
    {
        if(auto* Mode=GetWorld()->GetAuthGameMode<AFootballMode>();Mode&&Mode->Ball&&!Mode->BallHidden)
        {
            // Keep the possessed ball at the striking foot during the short wind-up.
            const float Side=Avatar->CurrentAnimation==TEXT("Kick_Left")?-18.f:18.f;
            const FVector Contact=Avatar->GetActorLocation()+Avatar->GetActorForwardVector()*82+Avatar->GetActorRightVector()*Side-FVector(0,0,74);
            const bool Demo=ActiveScreen==EScreen::Main||ActiveScreen==EScreen::Credits;
            const float ContactBlend=FMath::SmoothStep(0.f,1.f,FMath::Clamp(Avatar->ActionElapsed/FMath::Max(.01f,ShotContactTime),0.f,1.f));
            const FVector Position=Demo?FMath::Lerp(ShotBallStart+Avatar->GetActorLocation()-ShotActorStart,Contact,ContactBlend):Contact;
            Mode->Ball->SetWorldLocation(Position,false,nullptr,ETeleportType::TeleportPhysics);
            Mode->Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);
        }
    }
    if(PendingShot&&Avatar->ActionElapsed>=ShotContactTime)
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
            if(!Mode->LaunchShot(Avatar,PendingVelocity))PendingTrip=false;
        }
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
    static const TMap<FString,FString> Labels={
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
    return FText::FromString(Avatar&&Avatar->PreviewAnimation?Labels.FindRef(Avatar->CurrentAnimation):TEXT("Idle"));
}
void AFootballController::BallControlOn(){if(Screen!=EScreen::Game||!Avatar)return;auto* Mode=GetWorld()->GetAuthGameMode<AFootballMode>();if(!Mode||!Mode->HasBall(Avatar))return;BallControlHeld=true;if(!Sprint)Avatar->GetCharacterMovement()->MaxWalkSpeed=180;}
void AFootballController::BallControlOff(){BallControlHeld=false;if(Avatar)Avatar->GetCharacterMovement()->MaxWalkSpeed=Sprint?765:510;}
