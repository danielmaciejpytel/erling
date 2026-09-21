#include "ErlingAnimation.h"
#include "Football.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"

struct FErlingAnimProxy final : public FAnimInstanceProxy
{
    explicit FErlingAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}
    UAnimSequence* Current=nullptr;
    UAnimSequence* Previous=nullptr;
    float Time=0,PreviousTime=0,Alpha=1;
    bool PhysicalJump=false;
    virtual void PreUpdate(UAnimInstance* Instance,float Dt) override
    {
        FAnimInstanceProxy::PreUpdate(Instance,Dt);
        if(const auto* P=Cast<AFootballPlayer>(Instance->GetOwningActor()))
        {
            Current=P->Animations.FindRef(P->CurrentAnimation);
            Previous=P->Animations.FindRef(P->PreviousAnimation);
            Time=P->AnimationTime;PreviousTime=P->PreviousAnimationTime;
            Alpha=FMath::SmoothStep(0.f,1.f,P->AnimationBlend);
            PhysicalJump=P->PhysicalJump;
        }
    }
    void Sample(UAnimSequence* Sequence,float At,FPoseContext& Pose) const
    {
        Pose.ResetToRefPose();
        if(!Sequence)return;
        FAnimationPoseData Data(Pose);
        Sequence->GetAnimationPose(Data,FAnimExtractContext(FMath::Clamp(double(At),0.0,double(Sequence->GetPlayLength())),false));
        // World movement belongs to CharacterMovement, not to both actor and mesh.
        const FBoneContainer& Bones=Pose.Pose.GetBoneContainer();
        for(FCompactPoseBoneIndex Index:Pose.Pose.ForEachBoneIndex())
        {
            const int32 MeshIndex=Bones.MakeMeshPoseIndex(Index).GetInt();
            if(Bones.GetReferenceSkeleton().GetBoneName(MeshIndex)!=TEXT("root"))continue;
            FVector Translation=Pose.Pose[Index].GetTranslation();
            const FVector Rest=Bones.GetRefPoseTransform(Index).GetTranslation();
            Translation.X=Rest.X;Translation.Y=Rest.Y;
            if(PhysicalJump && (Sequence->GetName()==TEXT("AN_Jump")||Sequence->GetName()==TEXT("AN_Jump_Run")))
            {
                // Curves are in component centimetres; the FBX armature parent scales
                // root's local metre coordinates by 100. Convert through the hierarchy.
                FTransform ParentToComponent=FTransform::Identity;
                for(FCompactPoseBoneIndex Parent=Bones.GetParentBoneIndex(Index);Parent.GetInt()!=INDEX_NONE;Parent=Bones.GetParentBoneIndex(Parent))
                    ParentToComponent=ParentToComponent*Pose.Pose[Parent];
                Translation-=ParentToComponent.InverseTransformVector(FVector(0,0,Pose.Curve.Get(TEXT("ErlingAirHeight"))));
            }
            Pose.Pose[Index].SetTranslation(Translation);
            if(Sequence->GetName()==TEXT("AN_Turn_Step"))
                Pose.Pose[Index].SetRotation(Bones.GetRefPoseTransform(Index).GetRotation());
            break;
        }
    }
    virtual bool Evaluate(FPoseContext& Output) override
    {
        Sample(Current,Time,Output);
        if(Previous&&Alpha<1.f)
        {
            FPoseContext Old(Output);Sample(Previous,PreviousTime,Old);
            FAnimationPoseData Result(Output),Outgoing(Old);
            FAnimationRuntime::BlendTwoPosesTogetherInPlace(Result,Outgoing,Alpha);
        }
        Output.Pose.NormalizeRotations();
        return true;
    }
};
FAnimInstanceProxy* UErlingAnimInstance::CreateAnimInstanceProxy(){return new FErlingAnimProxy(this);}
void UErlingAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy){delete Proxy;}

void UErlingMovement::CalcVelocity(float Dt,float Friction,bool Fluid,float Braking)
{
    if(const auto* P=Cast<AFootballPlayer>(CharacterOwner);P&&P->IsMovementLocked())
    {
        TurnSkidRemaining=0.f;
        LastMoveInputDirection=FVector::ZeroVector;
        if(P->Action==AFootballPlayer::EAction::Slide&&P->SlideDistance>0)
        {
            const FVector Direction=P->EntryVelocity.GetSafeNormal2D();
            const float Travel=FMath::Max(0.f,FVector::DotProduct(CharacterOwner->GetActorLocation()-P->ActionStartLocation,Direction));
            const float Remaining=FMath::Max(0.f,P->SlideDistance-Travel);
            const float Speed=FMath::Min(P->ActionVelocity.Size2D(),Remaining/FMath::Max(Dt,SMALL_NUMBER));
            Velocity.X=Direction.X*Speed;Velocity.Y=Direction.Y*Speed;
        }
        else {Velocity.X=P->ActionVelocity.X;Velocity.Y=P->ActionVelocity.Y;}
        Acceleration=FVector::ZeroVector;
        return;
    }
    if(!IsMovingOnGround())
    {
        TurnSkidRemaining=0.f;
        LastMoveInputDirection=FVector::ZeroVector;
        Super::CalcVelocity(Dt,Friction,Fluid,Braking);
        return;
    }
    const FVector RawInputDirection=Acceleration.GetSafeNormal2D();
    FVector InputDirection=RawInputDirection;
    const float Speed=Velocity.Size2D();
    if(const auto* P=Cast<AFootballPlayer>(CharacterOwner))
    {
        if(auto* Mode=P->GetWorld()?P->GetWorld()->GetAuthGameMode<AFootballMode>():nullptr;Mode&&Mode->Ball)
        {
            const auto* SprintController=Cast<AFootballController>(P->GetController());
            const float Now=P->GetWorld()->GetTimeSeconds();
            const bool SprintReleaseChase=SprintController&&SprintController->Sprint&&!Mode->BallStopRequested&&!Mode->BallStopped&&!Mode->ShotInFlight&&!Mode->SprintKickPending&&Now>=Mode->SprintContactUntil&&Mode->IsRecoverableSprintTouch(P);
            if(SprintReleaseChase)
            {
                FVector Gap=Mode->Ball->GetComponentLocation()-CharacterOwner->GetActorLocation();
                const float GapDistance=Gap.Size2D();
                const bool BallAtPlayableHeight=Gap.Z<=-35.f;
                if(BallAtPlayableHeight&&GapDistance>48.f&&GapDistance<420.f)
                {
                    // EA-FC-style released-ball chase: once a sprint touch is out, the
                    // runner is committed to recovering that ball. Ignore steering input
                    // until a real contact starts and run directly through the ball's
                    // current center instead of travelling on a parallel/off-axis path.
                    if(GapDistance>80.f)
                    {
                        const FVector ChaseDirection=Gap.GetSafeNormal2D();
                        const float TargetSpeed=FMath::Max(Speed,765.f);
                        const float NewSpeed=FMath::FInterpTo(Speed,FMath::Min(TargetSpeed,765.f),Dt,12.f);
                        Velocity.X=ChaseDirection.X*NewSpeed;Velocity.Y=ChaseDirection.Y*NewSpeed;
                        Mode->DribbleDirection=ChaseDirection;
                        LastMoveInputDirection=ChaseDirection;
                    }
                    Acceleration=FVector::ZeroVector;
                    TurnSkidRemaining=0.f;
                    return;
                }
            }
        }
    }
    if(const auto* P=Cast<AFootballPlayer>(CharacterOwner))
    {
        if(const auto* PC=Cast<AFootballController>(P->GetController());PC&&PC->Charging)
        {
            // Charging suppresses sharp-turn skid, but only after the released-ball
            // recovery path above had first chance to keep the runner on the live ball.
            TurnSkidRemaining=0.f;LastMoveInputDirection=FVector::ZeroVector;
            Super::CalcVelocity(Dt,Friction,Fluid,Braking);
            return;
        }
    }
    if(const auto* P=Cast<AFootballPlayer>(CharacterOwner);P&&RawInputDirection.IsNearlyZero())
    {
        if(const auto* Mode=P->GetWorld()?P->GetWorld()->GetAuthGameMode<AFootballMode>():nullptr;Mode&&Mode->BallStopRequested&&!Mode->BallStopped&&!Mode->SprintReleaseDirection.IsNearlyZero()&&Mode->Ball&&Speed>120.f)
        {
            const FVector Gap=Mode->Ball->GetComponentLocation()-CharacterOwner->GetActorLocation();
            const FVector RecoveryDirection=Mode->LastPossessionDirection.GetSafeNormal2D();
            const float Ahead=FVector::DotProduct(Gap,RecoveryDirection),GapDistance=Gap.Size2D();
            const float RecoveryAge=P->GetWorld()->GetTimeSeconds()-Mode->BallStopStarted;
            const bool BallAtPlayableHeight=Gap.Z<=-35.f;
            if(!RecoveryDirection.IsNearlyZero()&&BallAtPlayableHeight&&Ahead>60.f&&GapDistance>122.f&&GapDistance<420.f&&RecoveryAge<1.35f)
            {
                // Keep the runner physically committed to the sprint touch for a few
                // recovery steps. The ball remains free; the player catches it instead
                // of an invisible leash pulling it backwards.
                const bool StillReleased=P->GetWorld()->GetTimeSeconds()<Mode->SprintReleaseUntil;
                const float TargetSpeed=StillReleased?FMath::Max(Speed,720.f):(GapDistance>145.f?745.f:590.f);
                const float NewSpeed=FMath::FInterpTo(Speed,FMath::Min(TargetSpeed,765.f),Dt,10.f);
                Velocity.X=RecoveryDirection.X*NewSpeed;Velocity.Y=RecoveryDirection.Y*NewSpeed;
                Acceleration=FVector::ZeroVector;TurnSkidRemaining=0.f;LastMoveInputDirection=FVector::ZeroVector;
                return;
            }
        }
    }
    if(const auto* P=Cast<AFootballPlayer>(CharacterOwner);P&&!RawInputDirection.IsNearlyZero())
    {
        if(auto* Mode=P->GetWorld()?P->GetWorld()->GetAuthGameMode<AFootballMode>():nullptr;Mode)
        {
            if(Mode->HasDribbleControl(P))
            {
            const FVector LimitedDirection=Mode->DribbleDirection.GetSafeNormal2D();
            if(!LimitedDirection.IsNearlyZero())
            {
                const float AccelSize=Acceleration.Size2D();
                Acceleration.X=LimitedDirection.X*AccelSize;Acceleration.Y=LimitedDirection.Y*AccelSize;
                InputDirection=LimitedDirection;
            }
            const float RawAgainstMomentum=Speed>1.f?FVector::DotProduct(RawInputDirection,Velocity.GetSafeNormal2D()):1.f;
            TurnSkidRemaining=0.f;LastMoveInputDirection=InputDirection;
            if(Speed>260.f&&RawAgainstMomentum<.35f)
            {
                // A full reversal with the ball is a plant-and-cut: brake the old
                // momentum first while the limited dribble heading rotates toward input.
                const auto* PC=Cast<AFootballController>(P->GetController());
                const bool Sprinting=PC&&PC->Sprint;
                Super::CalcVelocity(Dt,Friction*(Sprinting?1.45f:1.7f),Fluid,Braking*(Sprinting?1.25f:1.5f));
                return;
            }
            }
        }
    }
    if(!InputDirection.IsNearlyZero())
    {
        if(Speed>300.f&&!LastMoveInputDirection.IsNearlyZero())
        {
            const float InputChange=FVector::DotProduct(InputDirection,LastMoveInputDirection);
            const float AgainstMomentum=FVector::DotProduct(InputDirection,Velocity.GetSafeNormal2D());
            if(InputChange<.45f&&AgainstMomentum<.55f)
                TurnSkidRemaining=Speed>600.f?.28f:.18f;
        }
        LastMoveInputDirection=InputDirection;
    }
    else if(Speed<60.f)LastMoveInputDirection=FVector::ZeroVector;

    if(TurnSkidRemaining>0.f)TurnSkidRemaining=FMath::Max(0.f,TurnSkidRemaining-Dt);
    if(TurnSkidRemaining>0.f&&Speed>250.f)
    {
        const float SprintAlpha=FMath::Clamp((Speed-510.f)/255.f,0.f,1.f);
        const float FrictionScale=FMath::Lerp(.24f,.14f,SprintAlpha);
        Super::CalcVelocity(Dt,Friction*FrictionScale,Fluid,Braking*.55f);
        return;
    }
    Super::CalcVelocity(Dt,Friction,Fluid,Braking);
}
