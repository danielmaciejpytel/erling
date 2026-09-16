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
    if(const auto* P=Cast<AFootballPlayer>(CharacterOwner))
    {
        if(const auto* PC=Cast<AFootballController>(P->GetController());PC&&PC->Charging)
        {
            // Charging/striking must not inherit the sharp-turn glide.
            TurnSkidRemaining=0.f;LastMoveInputDirection=FVector::ZeroVector;
            Super::CalcVelocity(Dt,Friction,Fluid,Braking);
            return;
        }
    }
    if(!IsMovingOnGround())
    {
        TurnSkidRemaining=0.f;
        LastMoveInputDirection=FVector::ZeroVector;
        Super::CalcVelocity(Dt,Friction,Fluid,Braking);
        return;
    }
    const FVector InputDirection=Acceleration.GetSafeNormal2D();
    const float Speed=Velocity.Size2D();
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
