#include "ErlingAnimation.h"
#include "Football.h"
#include "ErlingHairSettings.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
    constexpr int32 HairStrandCount=14;
    constexpr int32 HairJointCount=3;
    constexpr int32 HairSpringCount=HairStrandCount*HairJointCount;
}

struct FErlingAnimProxy final : public FAnimInstanceProxy
{
    explicit FErlingAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}
    UAnimSequence* Current=nullptr;
    UAnimSequence* Previous=nullptr;
    float Time=0,PreviousTime=0,Alpha=1;
    bool PhysicalJump=false;
    bool BackHair=false;
    bool BodyUnderShorts=false;
    bool Shirt=false;
    bool HairInitialized=false;
    double HairPhase=0.0;
    float LastHairYaw=0.f;
    FVector LastHairVelocity=FVector::ZeroVector,LastHairLocation=FVector::ZeroVector;
    FVector HairPitchAxis=FVector::RightVector;
    FVector HairTurnAxis=FVector::UpVector;
    TArray<float> HairJointSwing,HairJointSpeed;
    TArray<float> HairJointTurn,HairJointTurnSpeed;
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
            const auto* Component=Instance->GetOwningComponent();
            BackHair=Component&&Component->GetSkeletalMeshAsset()&&
                Component->GetSkeletalMeshAsset()->GetFName()==TEXT("SK_hair_back");
            BodyUnderShorts=Component&&Component->GetSkeletalMeshAsset()&&
                Component->GetSkeletalMeshAsset()->GetFName()==TEXT("SK_body");
            Shirt=Component&&Component->GetSkeletalMeshAsset()&&
                Component->GetSkeletalMeshAsset()->GetFName()==TEXT("SK_shirt");
            if(BackHair)
            {
                const auto* Settings=GetDefault<UErlingHairSettings>();
                const float Strength=FMath::Clamp(Settings->HairStrength,0.f,5.f);
                const float TurnStrength=FMath::Clamp(Settings->HairTurnStrength,0.f,5.f);
                const float MaxAngle=FMath::Clamp(Settings->HairMaxAngle,0.f,30.f);
                const float Stiffness=FMath::Clamp(Settings->HairStiffness,1.f,1000.f);
                const float Damping=FMath::Clamp(Settings->HairDamping,0.f,100.f);
                if(HairJointSwing.Num()!=HairSpringCount||HairJointTurn.Num()!=HairSpringCount)
                {
                    HairJointSwing.Init(0.f,HairSpringCount);
                    HairJointSpeed.Init(0.f,HairSpringCount);
                    HairJointTurn.Init(0.f,HairSpringCount);
                    HairJointTurnSpeed.Init(0.f,HairSpringCount);
                }
                if(Strength==0.f||MaxAngle==0.f)
                {
                    HairJointSwing.Init(0.f,HairSpringCount);
                    HairJointSpeed.Init(0.f,HairSpringCount);
                }
                const FVector Velocity=P->GetVelocity();
                const FVector Location=P->GetActorLocation();
                const float Yaw=P->GetActorRotation().Yaw;
                if(!HairInitialized||Dt>.1f||FVector::DistSquared(Location,LastHairLocation)>FMath::Square(200.f))
                {
                    HairJointSwing.Init(0.f,HairSpringCount);
                    HairJointSpeed.Init(0.f,HairSpringCount);
                    LastHairVelocity=Velocity;
                    LastHairYaw=Yaw;
                    HairJointTurn.Init(0.f,HairSpringCount);
                    HairJointTurnSpeed.Init(0.f,HairSpringCount);
                    HairPhase=0.0;
                    HairInitialized=true;
                }
                const FVector Acceleration=P->GetActorTransform().InverseTransformVectorNoScale(
                    (Velocity-LastHairVelocity)/FMath::Max(Dt,.001f)).GetClampedToMaxSize(3000.f);
                LastHairVelocity=Velocity;LastHairLocation=Location;
                // Shortest angular delta avoids an impulse when yaw wraps at +/-180.
                const float TurnRate=FMath::Clamp(FMath::FindDeltaAngleDegrees(LastHairYaw,Yaw)/
                    FMath::Max(Dt,.001f),-720.f,720.f);
                LastHairYaw=Yaw;
                if(TurnStrength==0.f||Strength==0.f||MaxAngle==0.f)
                {
                    HairJointTurn.Init(0.f,HairSpringCount);
                    HairJointTurnSpeed.Init(0.f,HairSpringCount);
                }
                const bool Gait=P->CurrentAnimation==TEXT("Run")||P->CurrentAnimation==TEXT("Sprint")||P->CurrentAnimation==TEXT("Walk");
                const float Amount=Gait?FMath::Clamp(Velocity.Size2D()/765.f,0.f,1.f):0.f;
                // Keep the oscillator continuous across animation loops and transitions.
                if(Gait&&Current)
                    HairPhase=FMath::Fmod(HairPhase+2.0*PI*FMath::Clamp(Dt,0.f,.1f)/
                        FMath::Max(Current->GetPlayLength(),.01f),2.0*PI*1000.0);
                constexpr float SegmentGain[HairJointCount]={.35f,.65f,1.f};
                for(int32 Strand=0;Strand<HairStrandCount;++Strand)
                {
                    // Deterministic strand variation also separates their response to
                    // acceleration and their settling time after the player stops.
                    const float Variation=FMath::Frac((Strand+1)*.618033989f);
                    const float StrandGain=FMath::Lerp(.65f,1.25f,Variation);
                    const float StrandStiffness=Stiffness*FMath::Lerp(.65f,1.35f,Variation);
                    const float StrandDamping=Damping*FMath::Lerp(1.2f,.7f,Variation);
                    for(int32 Segment=0;Segment<HairJointCount;++Segment)
                    {
                        const int32 Spring=Strand*HairJointCount+Segment;
                        // Each joint has its own phase, gain, and spring state. The
                        // increasing gain down the chain widens the tip's arc.
                        const double JointPhase=HairPhase*(1.0+.001*(Strand*17+Segment*7))
                            +Strand*2.39996323-Segment*.73;
                        const float Target=FMath::Clamp(Strength*StrandGain*SegmentGain[Segment]*(
                            Amount*(3.4f+1.8f*static_cast<float>(FMath::Sin(JointPhase*2.0)))+
                            FMath::Clamp(Acceleration.X*.0012f,-3.f,3.f)),0.f,MaxAngle);
                        const float JointStiffness=StrandStiffness*(1.f-.15f*Segment);
                        const float JointDamping=StrandDamping*(1.f-.10f*Segment);
                        const float TurnLimit=MaxAngle*.65f;
                        const float TurnTarget=Strength==0.f?0.f:FMath::Clamp(
                            -TurnRate*.012f*TurnStrength*StrandGain*SegmentGain[Segment],-TurnLimit,TurnLimit);
                        for(float Remaining=FMath::Clamp(Dt,0.f,.1f);Remaining>KINDA_SMALL_NUMBER;)
                        {
                            const float Step=FMath::Min(Remaining,1.f/120.f);
                            HairJointSpeed[Spring]+=((Target-HairJointSwing[Spring])*JointStiffness-HairJointSpeed[Spring]*JointDamping)*Step;
                            HairJointSwing[Spring]+=HairJointSpeed[Spring]*Step;
                            HairJointTurnSpeed[Spring]+=((TurnTarget-HairJointTurn[Spring])*JointStiffness-
                                HairJointTurnSpeed[Spring]*JointDamping)*Step;
                            HairJointTurn[Spring]+=HairJointTurnSpeed[Spring]*Step;
                            if(FMath::Abs(HairJointTurn[Spring])>TurnLimit)
                            {
                                HairJointTurn[Spring]=FMath::Clamp(HairJointTurn[Spring],-TurnLimit,TurnLimit);
                                if(HairJointTurn[Spring]*HairJointTurnSpeed[Spring]>0.f)
                                    HairJointTurnSpeed[Spring]=0.f;
                            }
                            if(HairJointSwing[Spring]<0.f)
                            {
                                HairJointSwing[Spring]=0.f;
                                HairJointSpeed[Spring]=FMath::Max(HairJointSpeed[Spring],0.f);
                            }
                            if(HairJointSwing[Spring]>MaxAngle)
                            {
                                HairJointSwing[Spring]=MaxAngle;
                                HairJointSpeed[Spring]=FMath::Min(HairJointSpeed[Spring],0.f);
                            }
                            Remaining-=Step;
                        }
                    }
                }
                HairPitchAxis=Component->GetComponentTransform().InverseTransformVectorNoScale(P->GetActorRightVector());
                HairTurnAxis=Component->GetComponentTransform().InverseTransformVectorNoScale(P->GetActorUpVector());
            }
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
        const FName ClipName=Sequence->GetFName();
        if(ClipName==TEXT("AN_Idle_Breathe")||ClipName==TEXT("AN_Idle_WeightShift")||
            ClipName==TEXT("AN_Idle_Relaxed"))
        {
            // These idle clips lean the short arms behind the torso. Correct the
            // limb direction in mesh space before blending, on every modular piece.
            const FName ArmNames[]={TEXT("upperarm_l"),TEXT("lowerarm_l"),
                TEXT("upperarm_r"),TEXT("lowerarm_r")};
            const FName ChildNames[]={TEXT("lowerarm_l"),TEXT("hand_l"),
                TEXT("lowerarm_r"),TEXT("hand_r")};
            for(int32 Arm=0;Arm<4;++Arm)
            {
                FCompactPoseBoneIndex Joint(INDEX_NONE),Child(INDEX_NONE);
                for(FCompactPoseBoneIndex Index:Pose.Pose.ForEachBoneIndex())
                {
                    const FName Name=Bones.GetReferenceSkeleton().GetBoneName(Bones.MakeMeshPoseIndex(Index).GetInt());
                    if(Name==ArmNames[Arm])Joint=Index;
                    if(Name==ChildNames[Arm])Child=Index;
                }
                if(Joint.GetInt()==INDEX_NONE||Child.GetInt()==INDEX_NONE||Bones.GetParentBoneIndex(Child)!=Joint)continue;
                FTransform ParentToComponent=FTransform::Identity;
                for(FCompactPoseBoneIndex Parent=Bones.GetParentBoneIndex(Joint);
                    Parent.GetInt()!=INDEX_NONE;Parent=Bones.GetParentBoneIndex(Parent))
                    ParentToComponent=ParentToComponent*Pose.Pose[Parent];
                const FTransform JointToComponent=Pose.Pose[Joint]*ParentToComponent;
                const FVector Direction=JointToComponent.TransformVectorNoScale(Pose.Pose[Child].GetTranslation()).GetSafeNormal();
                if(Direction.IsNearlyZero())continue;
                // Mesh forward is +Y (the character mesh has a -90 degree yaw).
                // Retain gentle lateral sway while removing the backward lean.
                const FVector Down=FVector(FMath::Clamp(Direction.X,-.25,.25),0.,-1.).GetSafeNormal();
                const FQuat Correction=FQuat::FindBetweenNormals(
                    ParentToComponent.InverseTransformVectorNoScale(Direction).GetSafeNormal(),
                    ParentToComponent.InverseTransformVectorNoScale(Down).GetSafeNormal());
                Pose.Pose[Joint].SetRotation((Correction*Pose.Pose[Joint].GetRotation()).GetNormalized());
            }
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
        if(Shirt)
        {
            const FBoneContainer& Bones=Output.Pose.GetBoneContainer();
            // The front hem has thigh weights while the back mostly follows the
            // pelvis. Reduce leg-driven distortion only on the shirt's own pose.
            for(FCompactPoseBoneIndex Index:Output.Pose.ForEachBoneIndex())
            {
                const FName Name=Bones.GetReferenceSkeleton().GetBoneName(Bones.MakeMeshPoseIndex(Index).GetInt());
                if(Name!=TEXT("thigh_l")&&Name!=TEXT("thigh_r"))continue;
                const FTransform& Rest=Bones.GetRefPoseTransform(Index);
                Output.Pose[Index].SetRotation(FQuat::Slerp(Rest.GetRotation(),Output.Pose[Index].GetRotation(),.2f).GetNormalized());
                Output.Pose[Index].SetTranslation(FMath::Lerp(Rest.GetTranslation(),Output.Pose[Index].GetTranslation(),.2f));
            }
        }
        if(BodyUnderShorts)
        {
            const FBoneContainer& Bones=Output.Pose.GetBoneContainer();
            // Give the skin clearance below the shorts without removing geometry.
            // Compensate direct children so knee/foot positions and the upper body
            // retain their animation transforms; only vertices weighted to these
            // bones receive the smaller skin envelope.
            for(FCompactPoseBoneIndex Index:Output.Pose.ForEachBoneIndex())
            {
                const FName Name=Bones.GetReferenceSkeleton().GetBoneName(Bones.MakeMeshPoseIndex(Index).GetInt());
                if(Name!=TEXT("pelvis")&&Name!=TEXT("thigh_l")&&Name!=TEXT("thigh_r"))continue;
                const float ClearanceScale=Name==TEXT("pelvis")?.90f:.92f;
                Output.Pose[Index].SetScale3D(Output.Pose[Index].GetScale3D()*ClearanceScale);
                for(FCompactPoseBoneIndex Child:Output.Pose.ForEachBoneIndex())
                {
                    if(Bones.GetParentBoneIndex(Child)!=Index)continue;
                    Output.Pose[Child].SetTranslation(Output.Pose[Child].GetTranslation()/ClearanceScale);
                    Output.Pose[Child].SetScale3D(Output.Pose[Child].GetScale3D()/ClearanceScale);
                }
            }
        }
        if(BackHair&&HairJointSwing.Num()==HairSpringCount&&HairJointTurn.Num()==HairSpringCount)
        {
            const FBoneContainer& Bones=Output.Pose.GetBoneContainer();
            for(FCompactPoseBoneIndex Index:Output.Pose.ForEachBoneIndex())
            {
                const FString BoneName=Bones.GetReferenceSkeleton().GetBoneName(
                    Bones.MakeMeshPoseIndex(Index).GetInt()).ToString();
                if(!BoneName.StartsWith(TEXT("hair_back_")))continue;
                TArray<FString> Tokens;
                BoneName.ParseIntoArray(Tokens,TEXT("_"));
                if(Tokens.Num()!=5||Tokens[3]!=TEXT("seg"))continue;
                const int32 Strand=FCString::Atoi(*Tokens[2])-1;
                const int32 Segment=FCString::Atoi(*Tokens[4])-1;
                if(Strand<0||Strand>=HairStrandCount||Segment<0||Segment>=HairJointCount)continue;
                const int32 Spring=Strand*HairJointCount+Segment;
                FTransform ParentToComponent=FTransform::Identity;
                for(FCompactPoseBoneIndex Parent=Bones.GetParentBoneIndex(Index);
                    Parent.GetInt()!=INDEX_NONE;Parent=Bones.GetParentBoneIndex(Parent))
                    ParentToComponent=ParentToComponent*Output.Pose[Parent];
                const FQuat Pitch(
                    ParentToComponent.InverseTransformVectorNoScale(HairPitchAxis).GetSafeNormal(),
                    FMath::DegreesToRadians(HairJointSwing[Spring]));
                const FQuat Turn(
                    ParentToComponent.InverseTransformVectorNoScale(HairTurnAxis).GetSafeNormal(),
                    FMath::DegreesToRadians(HairJointTurn[Spring]));
                Output.Pose[Index].SetRotation((Turn*Pitch*Output.Pose[Index].GetRotation()).GetNormalized());
            }
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
            // Releasing sprint does not cancel the physical recovery of its last touch.
            const bool RecoverWhileSteering=!Mode->Possession->PossessionActive&&!RawInputDirection.IsNearlyZero();
            const bool SprintReleaseChase=SprintController&&(SprintController->Sprint||RecoverWhileSteering)&&!Mode->Possession->BallStopRequested&&!Mode->Possession->BallStopped&&!Mode->ShotInFlight&&!Mode->Possession->SprintKickPending&&Now>=Mode->Possession->SprintContactUntil&&Mode->IsRecoverableSprintTouch(P);
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
                        Mode->Possession->DribbleDirection=ChaseDirection;
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
        if(const auto* Mode=P->GetWorld()?P->GetWorld()->GetAuthGameMode<AFootballMode>():nullptr;Mode&&Mode->Possession->BallStopRequested&&!Mode->Possession->BallStopped&&!Mode->Possession->SprintReleaseDirection.IsNearlyZero()&&Mode->Ball&&Speed>120.f)
        {
            const FVector Gap=Mode->Ball->GetComponentLocation()-CharacterOwner->GetActorLocation();
            const FVector RecoveryDirection=Mode->Possession->LastPossessionDirection.GetSafeNormal2D();
            const float Ahead=FVector::DotProduct(Gap,RecoveryDirection),GapDistance=Gap.Size2D();
            const float RecoveryAge=P->GetWorld()->GetTimeSeconds()-Mode->Possession->BallStopStarted;
            const bool BallAtPlayableHeight=Gap.Z<=-35.f;
            if(!RecoveryDirection.IsNearlyZero()&&BallAtPlayableHeight&&Ahead>20.f&&!Mode->Possession->PossessionActive&&GapDistance>48.f&&GapDistance<420.f&&RecoveryAge<1.35f)
            {
                // Keep the runner physically committed to the sprint touch for a few
                // recovery steps. The ball remains free; the player catches it instead
                // of an invisible leash pulling it backwards.
                const bool StillReleased=P->GetWorld()->GetTimeSeconds()<Mode->Possession->SprintReleaseUntil;
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
            const FVector LimitedDirection=Mode->Possession->DribbleDirection.GetSafeNormal2D();
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
                if(Sprinting&&Mode->Possession->PossessionActive&&RawAgainstMomentum<.3f)
                {
                    // Plant before reversing: shorten the player's arc, never redirect
                    // the free ball. The release chase above always has priority.
                    const float PlantedSpeed=FMath::Max(240.f,Speed-4000.f*Dt);
                    Velocity.X*=PlantedSpeed/Speed;Velocity.Y*=PlantedSpeed/Speed;
                }
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
