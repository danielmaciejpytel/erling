#include "Football.h"
#include "ErlingAnimation.h"
#include "ErlingInterface.h"
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
static const TCHAR* ProfileSlot(){return (FParse::Param(FCommandLine::Get(),TEXT("ErlingTest"))||FParse::Param(FCommandLine::Get(),TEXT("ErlingTest_Latest"))||FParse::Param(FCommandLine::Get(),TEXT("ErlingUITest")))?TEXT("ErlingProfile_Test"):TEXT("ErlingProfile");}
AFootballPlayer::AFootballPlayer(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer.SetDefaultSubobjectClass<UErlingMovement>(ACharacter::CharacterMovementComponentName)) {
 PrimaryActorTick.bCanEverTick=true; GetCapsuleComponent()->InitCapsuleSize(55,96);
 GetMesh()->SetRelativeLocation(FVector(0,0,-96));GetMesh()->SetRelativeRotation(FRotator(0,-90,0));GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 GetMesh()->SetForcedLOD(1);GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 Face=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Face"));Face->SetupAttachment(GetMesh());Face->SetRelativeLocation(FVector(0,.065871f,0));Face->SetCastShadow(false);Face->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 GetCharacterMovement()->MaxWalkSpeed=510;GetCharacterMovement()->bEnablePhysicsInteraction=false;GetCharacterMovement()->JumpZVelocity=420;GetCharacterMovement()->GravityScale=1.4f;GetCharacterMovement()->MaxAcceleration=1400;GetCharacterMovement()->BrakingDecelerationWalking=1800;GetCharacterMovement()->bOrientRotationToMovement=true;GetCharacterMovement()->RotationRate=FRotator(0,520,0);bUseControllerRotationYaw=false;
}
void AFootballPlayer::BeginPlay() { Super::BeginPlay();InitializeAssets(); }
void AFootballPlayer::InitializeAssets() {
 if(GetMesh()->GetSkeletalMeshAsset()&&Face->GetSkeletalMeshAsset()&&FaceMaterial&&Animations.Num()==24)return;
 if(!GetMesh()->GetSkeletalMeshAsset())GetMesh()->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Erling/Meshes/SK_body.SK_body")));
 ConfigurePiece(GetMesh());
 if(!Face->GetSkeletalMeshAsset())Face->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Erling/Meshes/SK_face.SK_face")));
 Face->SetRelativeLocation(FVector::ZeroVector);ConfigurePiece(Face);
 if(!FaceMaterial){if(auto M=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Erling/Materials/M_Face.M_Face"))){FaceMaterial=UMaterialInstanceDynamic::Create(M,this);Face->SetMaterial(0,FaceMaterial);}}
 for(auto N:{TEXT("Idle_Breathe"),TEXT("Idle_LookAround"),TEXT("Idle_WeightShift"),TEXT("Walk"),TEXT("Run"),TEXT("Sprint"),TEXT("Kick_Right"),TEXT("Kick_Left"),TEXT("Jump"),TEXT("Jump_Run"),TEXT("JoyJump_Run"),TEXT("JoyJump_Standing"),TEXT("Shot_Flip_Land"),TEXT("Slide_From_Run"),TEXT("Trip_Roll_From_Run"),TEXT("Celebrate_Victory"),TEXT("Dance_Disco"),TEXT("Dance_Robot"),TEXT("Fall_Backward"),TEXT("GetUp_Back"),TEXT("GetUp_Front"),TEXT("Meme_RageStomp"),TEXT("Meme_Shrug"),TEXT("Turn_Step")}) {
  const FString AssetName=FString(TEXT("AN_"))+N;FString Path=FString::Printf(TEXT("/Game/Erling/Animations/%s.%s"),*AssetName,*AssetName);if(auto A=LoadObject<UAnimSequence>(nullptr,*Path))Animations.Add(N,A);else UE_LOG(LogTemp,Error,TEXT("Missing animation %s"),N);
 }
 SetAnimation(TEXT("Idle_Breathe"));
}
void AFootballPlayer::SetAnimation(const FString& Name,bool Loop) {
 if(CurrentAnimation==Name)return;
 if(auto A=Animations.FindRef(Name)){
  const bool Gait=(Name==TEXT("Walk")||Name==TEXT("Run")||Name==TEXT("Sprint"));
  const bool WasGait=(CurrentAnimation==TEXT("Walk")||CurrentAnimation==TEXT("Run")||CurrentAnimation==TEXT("Sprint"));
  float Phase=0;if(auto Old=Animations.FindRef(CurrentAnimation))Phase=AnimationTime/FMath::Max(.01f,Old->GetPlayLength());
  PreviousAnimation=CurrentAnimation;PreviousAnimationTime=AnimationTime;PreviousLoops=AnimationLoops;PreviousRate=PlaybackRate;
  CurrentAnimation=Name;AnimationTime=Gait&&WasGait?Phase*A->GetPlayLength():0;AnimationLoops=Loop;AnimationBlend=PreviousAnimation.IsEmpty()?1:0;
 }
}
void AFootballPlayer::Tick(float Dt) {
 Super::Tick(Dt);UpdateAction(Dt);
}
void AFootballPlayer::ApplyKit(const TArray<int32>& Values,const TArray<FKitCategory>& Cats) {
 for(auto P:Pieces)if(P)P->DestroyComponent();Pieces.Empty();
 for(int32 C=0;C<Cats.Num();C++) {
  if(!Values.IsValidIndex(C)||!Cats[C].Options.IsValidIndex(Values[C]))continue;const auto& O=Cats[C].Options[Values[C]];
  if(C==1){if(FaceMaterial&&!O.Texture.IsEmpty())FaceMaterial->SetTextureParameterValue(TEXT("FaceTexture"),LoadObject<UTexture2D>(nullptr,*O.Texture));continue;}
  for(const auto& Path:O.Meshes)if(auto M=LoadObject<USkeletalMesh>(nullptr,*Path)){
   auto P=NewObject<USkeletalMeshComponent>(this);P->SetupAttachment(GetMesh());P->SetSkeletalMesh(M);P->SetCollisionEnabled(ECollisionEnabled::NoCollision);P->RegisterComponent();ConfigurePiece(P);Pieces.Add(P);
  }
 }
}
AFootballMode::AFootballMode(){PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostPhysics;DefaultPawnClass=AFootballPlayer::StaticClass();PlayerControllerClass=AFootballController::StaticClass();}
UStaticMeshComponent* AFootballMode::Box(const FVector& P,const FVector& Size,const FLinearColor& Color,bool Collision) {
 auto A=GetWorld()->SpawnActor<AStaticMeshActor>(P,FRotator::ZeroRotator);auto C=A->GetStaticMeshComponent();C->SetMobility(EComponentMobility::Movable);C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));C->SetWorldScale3D(Size/100);
 C->SetCollisionEnabled(Collision?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
 if(auto M=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Erling/Materials/M_World.M_World"))){auto D=UMaterialInstanceDynamic::Create(M,this);D->SetVectorParameterValue(TEXT("Tint"),Color);C->SetMaterial(0,D);}return C;
}
void AFootballMode::CreateField() {
 TArray<AActor*> Existing;UGameplayStatics::GetAllActorsWithTag(this,TEXT("PitchBuilt"),Existing);if(!Existing.IsEmpty())return;
 Box(FVector(0,0,-35),FVector(6800,4800,70),FLinearColor(.025f,.1f,.07f));
 for(int32 I=0;I<12;I++)Box(FVector(-2475+I*450,0,-2),FVector(450,3600,5),I%2?FLinearColor(.055f,.25f,.125f):FLinearColor(.04f,.21f,.10f),false);
 auto Line=[this](FVector P,FVector S){Box(P,S,FLinearColor(.83f,.91f,.85f),false);};
 for(int S:{-1,1}){Line(FVector(0,S*1750,2),FVector(5400,8,3));Line(FVector(S*2700,0,2),FVector(8,3500,3));}
 Line(FVector(0,0,2),FVector(8,3500,3));
 for(int I=0;I<64;I++){float A=I*2*PI/64;auto C=Box(FVector(500*FMath::Cos(A),500*FMath::Sin(A),2),FVector(50,8,3),FLinearColor(.83f,.91f,.85f),false);C->SetWorldRotation(FRotator(0,FMath::RadiansToDegrees(A)+90,0));}
 for(int S:{-1,1}){
  for(int Y:{-1,1}){Line(FVector(S*2400,Y*650,2),FVector(600,8,3));Box(FVector(S*2740,Y*350,130),FVector(18,18,260),FLinearColor(.9f,.95f,1));}
  Line(FVector(S*2100,0,2),FVector(8,1300,3));Box(FVector(S*2740,0,260),FVector(18,720,18),FLinearColor(.9f,.95f,1));
  for(int Y=-350;Y<=350;Y+=50)Box(FVector(S*2990,Y,130),FVector(5,3,260),FLinearColor(.48f,.58f,.62f),false);
  for(int Z=20;Z<=260;Z+=40)Box(FVector(S*2990,0,Z),FVector(5,700,3),FLinearColor(.48f,.58f,.62f),false);
 }
 // Simple stands leave the gameplay area clear.
 for(int S:{-1,1})for(int Row=0;Row<3;Row++)Box(FVector(0,S*(2050+Row*130),Row*80+25),FVector(5400,110,50+Row*100),FLinearColor(.05f,.10f,.16f));
 auto Sun=GetWorld()->SpawnActor<ADirectionalLight>(FVector(0,0,1000),FRotator(-55,-30,0));Sun->GetLightComponent()->SetIntensity(3.2f);Cast<UDirectionalLightComponent>(Sun->GetLightComponent())->bAtmosphereSunLight=true;
 auto Sky=GetWorld()->SpawnActor<ASkyLight>();Sky->GetLightComponent()->SetIntensity(.8f);Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);Sky->GetLightComponent()->bRealTimeCapture=true;
 GetWorld()->SpawnActor<ASkyAtmosphere>();
}
void AFootballMode::BeginPlay(){Super::BeginPlay();CreateField();CreateNetCollision();Box(FVector(0,0,-55),FVector(20000,20000,100),FLinearColor(.04f,.18f,.08f));GoalSound=LoadObject<USoundBase>(nullptr,TEXT("/Game/Erling/Audio/goal.goal"));auto A=GetWorld()->SpawnActor<AStaticMeshActor>(FVector(0,0,40),FRotator::ZeroRotator);Ball=A->GetStaticMeshComponent();Ball->SetMobility(EComponentMobility::Movable);Ball->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));Ball->SetWorldScale3D(FVector(.44));Ball->SetCollisionProfileName(TEXT("PhysicsActor"));Ball->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);Ball->SetSimulatePhysics(true);Ball->SetMassOverrideInKg(NAME_None,.43f);Ball->SetLinearDamping(.3f);Ball->SetAngularDamping(.2f);Ball->BodyInstance.bUseCCD=true;Ball->SetNotifyRigidBodyCollision(true);Ball->OnComponentHit.AddDynamic(this,&AFootballMode::NetHit);
 auto Dome=GetWorld()->SpawnActor<AStaticMeshActor>();Dome->SetActorEnableCollision(false);auto DC=Dome->GetStaticMeshComponent();DC->SetMobility(EComponentMobility::Movable);DC->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));DC->SetWorldScale3D(FVector(400));DC->SetCollisionEnabled(ECollisionEnabled::NoCollision);DC->SetCastShadow(false);DC->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Erling/Materials/M_Sky.M_Sky")));
 if(auto M=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Erling/Materials/M_Ball.M_Ball")))Ball->SetMaterial(0,M);
 auto PM=NewObject<UPhysicalMaterial>(this);PM->Restitution=.22f;PM->Friction=.45f;Ball->SetPhysMaterialOverride(PM);ResetBall();}
void AFootballMode::ResetBall(){if(!Ball)return;LastShooter.Reset();BallHidden=false;ShotInFlight=false;SprintDribbleFoot=NAME_None;SprintLeadFoot=NAME_None;CarryDribbleFoot=NAME_None;BallStopFoot=NAME_None;SprintContactUntil=0;SprintReleaseUntil=0;SprintNextTouchAt=0;CarryFootLockUntil=0;BallStopStarted=0;SprintKickPending=false;SprintDribbleTouchCount=0;CarryFootSwitchCount=0;DribbleDirection=FVector::ZeroVector;SprintReleaseDirection=FVector::ZeroVector;LastPossessionDirection=FVector::ForwardVector;BallStopAnchor=FVector::ZeroVector;PossessionActive=false;HadControlInput=false;BallStopRequested=false;BallStopped=false;BallStopGesturePlayed=false;LastPossessionWasDigital=false;DribbleMinFootClearance=MAX_flt;SprintDribbleMinDistance=MAX_flt;SprintDribbleMaxDistance=0;DribbleMinBodyAhead=MAX_flt;DribbleMinDirectionAhead=MAX_flt;if(auto PC=Cast<AFootballController>(GetWorld()->GetFirstPlayerController());PC&&PC->Avatar)PC->Avatar->CancelBallTrap();Ball->SetVisibility(true);Ball->SetSimulatePhysics(true);Ball->SetLinearDamping(.3f);Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);Ball->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);Ball->SetWorldLocation(FVector(0,0,28),false,nullptr,ETeleportType::TeleportPhysics);Scored=false;ResetAt=0;PreviousBall=Ball->GetComponentLocation();}
FVector AFootballMode::ShotVelocity(const FVector& Position,const FVector& Direction,float Seconds){
 FVector F=Direction.GetSafeNormal2D();float C=FMath::Clamp(Seconds*2.f,0.f,3.f);float Speed=C<=1?FMath::Lerp(850.f,2200.f,C):C<=2?FMath::Lerp(2200.f,2800.f,C-1):FMath::Lerp(2800.f,3200.f,C-2);
 if(C<.15f)return F*Speed;
 float GoalHeight=C<=1?FMath::Lerp(22.f,130.f,C):C<=2?FMath::Lerp(130.f,220.f,C-1):FMath::Lerp(220.f,420.f,C-2);
 float Distance=FMath::Abs(F.X)>.15f?FMath::Abs(((F.X>=0?2740.f:-2740.f)-Position.X)/F.X):1800.f;Distance=FMath::Clamp(Distance,200.f,5000.f);
 float T=Distance/Speed;float Z=FMath::Clamp((GoalHeight-Position.Z+.5f*980*T*T)/T,0.f,1300.f);return F*Speed+FVector(0,0,Z);
}
void AFootballMode::Kick(AFootballPlayer* P,float Seconds){if(!Ball||!P||BallHidden||Scored||FVector::Dist2D(Ball->GetComponentLocation(),P->GetActorLocation())>240)return;LastShooter=P;LastShot=GetWorld()->GetTimeSeconds();ShotInFlight=true;Ball->SetLinearDamping(.015f);Ball->SetPhysicsLinearVelocity(ShotVelocity(Ball->GetComponentLocation(),P->GetActorForwardVector(),Seconds));if(auto PC=Cast<AFootballController>(GetWorld()->GetFirstPlayerController()))PC->PlayEffect(TEXT("kick"));}
void AFootballMode::HideMiss(){if(BallHidden)return;if(auto PC=Cast<AFootballController>(GetWorld()->GetFirstPlayerController()))PC->PlayEffect(TEXT("fail"));BallHidden=true;Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);Ball->SetVisibility(false);Ball->SetSimulatePhysics(false);ResetAt=GetWorld()->GetTimeSeconds()+1.2f;}
void AFootballMode::Dribble(AFootballPlayer* Player,float Dt){
 if(!Player||Player->IsMovementLocked()||Scored||BallHidden||GetWorld()->GetTimeSeconds()-LastShot<.85f||Player->GetCharacterMovement()->IsFalling())return;
 const float Now=GetWorld()->GetTimeSeconds();
 FVector Pos=Ball->GetComponentLocation();const FVector Base=Player->GetActorLocation();FVector To=Pos-Base;To.Z=0;
 if(SprintKickPending)
 {
  // SprintKickPending represents a physical foot contact that is still completing.
  // It must not inherit the wider 420 cm sprint-stop recovery envelope. If the ball
  // has already left any plausible foot-contact radius, that contact no longer exists.
  bool PendingFootContact=To.Size2D()<=150.f;
  if(Player->GetMesh()&&Player->GetMesh()->DoesSocketExist(TEXT("foot_l"))&&Player->GetMesh()->DoesSocketExist(TEXT("foot_r")))
  {
   const FVector LeftFoot=Player->GetMesh()->GetSocketLocation(TEXT("foot_l"));
   const FVector RightFoot=Player->GetMesh()->GetSocketLocation(TEXT("foot_r"));
   PendingFootContact=FMath::Min(FVector::Dist2D(Pos,LeftFoot),FVector::Dist2D(Pos,RightFoot))<=104.f;
  }
  if(!PendingFootContact)ClearSprintReleaseRecovery();
 }
 if(!SprintReleaseDirection.IsNearlyZero()&&!SprintKickPending&&Now>=SprintContactUntil&&!IsRecoverableSprintTouch(Player))
 {
  // Sprint-release recovery is a short-lived physical context, not permanent
  // possession metadata. Once its age, distance or playable-height envelope expires,
  // clear it so an old touch cannot resurrect chase/facing later.
  ClearSprintReleaseRecovery();
 }
 // A pending sprint-stop is deliberately allowed to live outside the normal dribble
 // radius. The ball is still free there; keeping the state alive only lets the runner
 // finish physically catching the touch instead of silently cancelling the requested
 // trap as soon as the gap crosses 270 cm.
 const bool PendingSprintStopRecovery=BallStopRequested&&!SprintReleaseDirection.IsNearlyZero();
 const float RecoveryAbandonDistance=420.f;
 const float Speed=Player->GetVelocity().Size2D();if(To.Size()>(PendingSprintStopRecovery?RecoveryAbandonDistance:270.f)||Pos.Z>Base.Z-35){if(SprintKickPending||!IsRecoverableSprintTouch(Player))ClearSprintReleaseRecovery();CarryDribbleFoot=NAME_None;BallStopFoot=NAME_None;DribbleDirection=FVector::ZeroVector;PossessionActive=false;HadControlInput=false;BallStopRequested=false;BallStopped=false;BallStopGesturePlayed=false;Player->CancelBallTrap();return;}
 const bool SprintReleaseActive=Now<SprintReleaseUntil;
 const auto* PC=Cast<AFootballController>(Player->GetController());
 const bool SprintTouchActive=Now<SprintContactUntil||SprintKickPending;
 const bool Sprinting=SprintReleaseActive||SprintTouchActive||(PC&&PC->Sprint&&Speed>560.f);
 const bool BallControl=PC&&PC->BallControlHeld&&!Sprinting;
 const FVector VelocityDirection=Player->GetVelocity().GetSafeNormal2D();
 FVector DesiredDirection=VelocityDirection;
 bool HasControlInput=false;
 float TurnAmount=0.f;
 const bool PlayerControlled=PC&&(PC->Screen==AFootballController::EScreen::Game||(PC->Screen==AFootballController::EScreen::Settings&&PC->SettingsReturn==AFootballController::EScreen::Game&&!PC->IsPaused()));
 if(PlayerControlled)
 {
  FVector Input=PC->GetMoveIntentWorld();Input.Z=0;
  if(!Input.IsNearlyZero())
  {
   HasControlInput=true;Input.Normalize();
   const float InputDotVelocity=VelocityDirection.IsNearlyZero()?1.f:FVector::DotProduct(Input,VelocityDirection);
   TurnAmount=FMath::Clamp((1.f-InputDotVelocity)*.5f,0.f,1.f);
   DesiredDirection=Input;
  }
 }
 const bool ControlledEnvelope=HasDribbleControl(Player);
 if(ControlledEnvelope)PossessionActive=true;
 if(HasControlInput)
 {
  BallStopRequested=false;BallStopped=false;BallStopGesturePlayed=false;BallStopFoot=NAME_None;BallStopAnchor=FVector::ZeroVector;Player->CancelBallTrap();
  LastPossessionWasDigital=PC&&PC->HasDigitalMoveIntent();
 }
 else if(PlayerControlled&&HadControlInput&&ControlledEnvelope)
 {
  BallStopRequested=true;BallStopStarted=Now;BallStopped=false;BallStopGesturePlayed=false;
 }
 HadControlInput=HasControlInput;
 const float SpeedAlpha=FMath::Clamp((Speed-180.f)/(765.f-180.f),0.f,1.f);
 const float PossessionTurnRate=BallControl?720.f:FMath::Lerp(480.f,200.f,SpeedAlpha);
 if(DribbleDirection.IsNearlyZero())DribbleDirection=!VelocityDirection.IsNearlyZero()?VelocityDirection:!LastPossessionDirection.IsNearlyZero()?LastPossessionDirection:Player->GetActorForwardVector();
 if(HasControlInput&&!DesiredDirection.IsNearlyZero())
 {
  const float NewYaw=FMath::FixedTurn(DribbleDirection.Rotation().Yaw,DesiredDirection.Rotation().Yaw,PossessionTurnRate*Dt);
  DribbleDirection=FRotator(0,NewYaw,0).Vector();
 }
 else if(!PlayerControlled&&!DesiredDirection.IsNearlyZero())DribbleDirection=DesiredDirection;
 FVector Direction=DribbleDirection.GetSafeNormal2D();
 if(!BallStopRequested&&!BallStopped&&!Direction.IsNearlyZero()&&ControlledEnvelope)LastPossessionDirection=Direction;
 if(Direction.IsNearlyZero())Direction=LastPossessionDirection.GetSafeNormal2D();
 if(Direction.IsNearlyZero()||(!BallControl&&!ControlledEnvelope&&FVector::DotProduct(To.GetSafeNormal(),Direction)<-.25f))return;
 const FVector BodyForward=Player->GetActorForwardVector().GetSafeNormal2D();
 const FVector BodyRight=Player->GetActorRightVector().GetSafeNormal2D();

 const bool HasFeet=Player->GetMesh()&&Player->GetMesh()->DoesSocketExist(TEXT("foot_l"))&&Player->GetMesh()->DoesSocketExist(TEXT("foot_r"));
 FVector Left=Base,Right=Base;constexpr float FootClearance=38.f;
 if(HasFeet)
 {
  Left=Player->GetMesh()->GetSocketLocation(TEXT("foot_l"));Right=Player->GetMesh()->GetSocketLocation(TEXT("foot_r"));
  if(!SprintReleaseActive)
  {
   bool Corrected=false;
   for(const FVector Foot:{Left,Right}){FVector D=Pos-Foot;D.Z=0;const float L=D.Size();if(L<FootClearance){Pos+=(L>1.f?D/L:Direction)*(FootClearance-L);Corrected=true;}}
   // The two foot circles alone leave a hole between the legs. Keep a controlled
   // ball out of that central corridor as well, otherwise a tight turn can visually
   // put the sphere underneath the pelvis while technically clearing both sockets.
   FVector Relative=Pos-Base;Relative.Z=0;const FVector Side=FVector::CrossProduct(FVector::UpVector,Direction).GetSafeNormal();
   const float Forward=FVector::DotProduct(Relative,Direction),Lateral=FVector::DotProduct(Relative,Side);
   if(ControlledEnvelope&&Forward>-18.f&&Forward<38.f&&FMath::Abs(Lateral)<44.f){Pos+=Direction*(38.f-Forward);Corrected=true;}
   if(ControlledEnvelope&&HasControlInput&&Relative.Size2D()<100.f)
   {
    const float DirectionAhead=FVector::DotProduct(Pos-Base,Direction);if(DirectionAhead<18.f){Pos+=Direction*(18.f-DirectionAhead);Corrected=true;}
    if(!BodyForward.IsNearlyZero()){const float BodyAhead=FVector::DotProduct(Pos-Base,BodyForward);if(BodyAhead<8.f){Pos+=BodyForward*(8.f-BodyAhead);Corrected=true;}}
   }
   if(Corrected)Ball->SetWorldLocation(FVector(Pos.X,Pos.Y,Ball->GetComponentLocation().Z),false,nullptr,ETeleportType::TeleportPhysics);
  }
  DribbleMinFootClearance=FMath::Min(DribbleMinFootClearance,FMath::Min(FVector::Dist2D(Pos,Left),FVector::Dist2D(Pos,Right)));
 }

 ShotInFlight=false;
 auto KeepTargetOutsideFeet=[&](FVector Target,float Radius)
 {
  if(!HasFeet)return Target;
  for(const FVector Foot:{Left,Right})
  {
   FVector D=Target-Foot;D.Z=0;const float L=D.Size();
   if(L<Radius)Target+=(L>1.f?D/L:Direction)*(Radius-L);
  }
  return Target;
 };
 auto KeepTargetOutsideBody=[&](FVector Target)
 {
  FVector Relative=Target-Base;Relative.Z=0;
  const float Forward=FVector::DotProduct(Relative,Direction);if(Forward<52.f)Target+=Direction*(52.f-Forward);
  if(!BodyForward.IsNearlyZero())
  {
   Relative=Target-Base;Relative.Z=0;const float BodyAhead=FVector::DotProduct(Relative,BodyForward);if(BodyAhead<26.f)Target+=BodyForward*(26.f-BodyAhead);
  }
  return Target;
 };
 auto AvoidFeet=[&](FVector V,float MaxPlanarSpeed)
 {
  if(!HasFeet||Dt<=SMALL_NUMBER)return V;
  FVector Next=Pos+V*Dt;
  for(const FVector Foot:{Left,Right})
  {
   FVector D=Next-Foot;D.Z=0;const float L=D.Size();
   if(L<FootClearance+1.f)Next+=(L>1.f?D/L:Direction)*(FootClearance+1.f-L);
  }
  FVector Relative=Next-Base;Relative.Z=0;
  if(ControlledEnvelope&&!SprintReleaseActive&&Relative.Size2D()<150.f)
  {
   const float Forward=FVector::DotProduct(Relative,Direction);if(Forward<30.f)Next+=Direction*(30.f-Forward);
   if(!BodyForward.IsNearlyZero()){Relative=Next-Base;Relative.Z=0;const float BodyAhead=FVector::DotProduct(Relative,BodyForward);if(BodyAhead<16.f)Next+=BodyForward*(16.f-BodyAhead);}
  }
  FVector Planar=(Next-Pos)/Dt;Planar.Z=0;Planar=Planar.GetClampedToMaxSize(MaxPlanarSpeed);
  V.X=Planar.X;V.Y=Planar.Y;return V;
 };
 const float PlayerGap=FVector::Dist2D(Pos,Base);

 // Releasing the stick/key may request a stop while the ball is still in a genuine
 // sprint release. Remember that request, but do not recall or curve the free ball.
 // Trapping may begin only after the release expires or the player has really caught
 // back up into close-touch range.
 const bool SprintStopRecovery=PlayerControlled&&!HasControlInput&&BallStopRequested&&!SprintReleaseDirection.IsNearlyZero();
 if(SprintStopRecovery&&PlayerGap>125.f)
 {
  const FVector V=Ball->GetPhysicsLinearVelocity();
  // While the touch is genuinely released keep its original low damping and heading.
  // Once that release window ends, natural rolling resistance can slow the free ball
  // enough for the still-coasting player to catch it. No XY steering is introduced.
  Ball->SetLinearDamping(SprintReleaseActive?.08f:1.65f);Ball->SetPhysicsAngularVelocityInRadians(FVector(-V.Y/22,V.X/22,0));return;
 }

 // Releasing movement while still in possession is an explicit trap, not a point
 // where physics is abandoned. The player catches the rolling ball with one foot,
 // kills its momentum, then holds it stationary until movement or a shot resumes.
 if(PlayerControlled&&!HasControlInput&&ControlledEnvelope&&(BallStopRequested||BallStopped))
 {
  SprintReleaseUntil=0;SprintContactUntil=0;SprintKickPending=false;SprintDribbleFoot=NAME_None;SprintLeadFoot=NAME_None;
  if(BallStopFoot==NAME_None&&HasFeet)BallStopFoot=FVector::Dist2D(Pos,Left)<=FVector::Dist2D(Pos,Right)?TEXT("foot_l"):TEXT("foot_r");
  const FVector Foot=BallStopFoot==TEXT("foot_l")?Left:Right;
  float StopSide=0.f;if(HasFeet&&!BodyRight.IsNearlyZero())StopSide=FMath::Clamp(FVector::DotProduct(Foot-Base,BodyRight),-22.f,22.f);
  FVector Target=Base+Direction*56.f+BodyRight*StopSide*.72f;Target.Z=Pos.Z;Target=KeepTargetOutsideFeet(KeepTargetOutsideBody(Target),FootClearance+2.f);
  if(BallStopped)
  {
   // Once trapped, BallStopAnchor is stored in player-local space. This makes the
   // held ball orbit with a Turn_Step instead of remaining nailed to its old world
   // position and ending up behind the player after an in-place camera turn.
   if(BallStopAnchor.IsNearlyZero())BallStopAnchor=Player->GetActorTransform().InverseTransformPosition(Target);
   const FVector HoldWorld=Player->GetActorTransform().TransformPosition(BallStopAnchor);
   if(!BodyForward.IsNearlyZero())DribbleDirection=BodyForward;
   Ball->SetWorldLocation(HoldWorld,false,nullptr,ETeleportType::TeleportPhysics);
   Ball->SetLinearDamping(2.f);Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);Ball->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);return;
  }
  FVector C=Target-Pos;C.Z=0;const float TargetDistance=C.Size();
  if(!BallStopGesturePlayed&&HasFeet&&TargetDistance<92.f&&Speed<220.f)
  {
   Player->StartBallTrap(BallStopFoot);BallStopGesturePlayed=Player->BallTrapActive;
  }
  const bool SprintRecoveryTrap=!SprintReleaseDirection.IsNearlyZero();
  const float CatchGain=TargetDistance>90.f?(SprintRecoveryTrap?7.f:10.f):(SprintRecoveryTrap?12.f:24.f);
  const float PlayerCarry= SprintRecoveryTrap?.88f:.28f;
  // A sprint trap starts while the runner still has meaningful forward speed. Match
  // most of that speed first, then bleed the remaining target error away. Pulling only
  // toward the anchor here can reverse the ball through the player's legs and create
  // the exact snap the trap is supposed to remove.
  FVector Desired=(Player->GetVelocity()*PlayerCarry+C*CatchGain).GetClampedToMaxSize(FMath::Max(180.f,Speed+120.f));
  const float CatchInterp=TargetDistance>90.f?(SprintRecoveryTrap?14.f:18.f):(SprintRecoveryTrap?18.f:32.f);
  FVector V=FMath::VInterpTo(Ball->GetPhysicsLinearVelocity(),Desired,Dt,CatchInterp);V.Z=FMath::Clamp(V.Z,-120.f,20.f);V=AvoidFeet(V,FMath::Max(180.f,Speed+120.f));
  Ball->SetLinearDamping(.9f);Ball->SetPhysicsLinearVelocity(V);Ball->SetPhysicsAngularVelocityInRadians(FVector(-V.Y/22,V.X/22,0));
  const bool GestureContact=!HasFeet||(BallStopGesturePlayed&&(!Player->BallTrapActive||Player->BallTrapElapsed>.07f));
  if(TargetDistance<10.f&&Speed<38.f&&V.Size2D()<125.f&&GestureContact)
  {
   BallStopped=true;BallStopRequested=false;BallStopAnchor=Player->GetActorTransform().InverseTransformPosition(Target);
   Ball->SetWorldLocation(Target,false,nullptr,ETeleportType::TeleportPhysics);Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);Ball->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);Ball->SetLinearDamping(2.f);
  }
  return;
 }
 if(Speed<25.f&&!(BallControl&&HasControlInput))return;
 if(Sprinting&&HasFeet)
 {
  CarryDribbleFoot=NAME_None;CarryFootLockUntil=0;
  SprintDribbleMinDistance=FMath::Min(SprintDribbleMinDistance,PlayerGap);SprintDribbleMaxDistance=FMath::Max(SprintDribbleMaxDistance,PlayerGap);
  const float LeftFootDistance=FVector::Dist2D(Pos,Left),RightFootDistance=FVector::Dist2D(Pos,Right);
  const FName NearestFoot=LeftFootDistance<=RightFootDistance?TEXT("foot_l"):TEXT("foot_r");
  const float NearestFootDistance=FMath::Min(LeftFootDistance,RightFootDistance);
  const bool StrongTurn=HasControlInput&&ControlledEnvelope&&TurnAmount>.5f&&PlayerGap<120.f;
  const float SprintFootContactRadius=FootClearance+14.f;
  const bool ReleaseFootRecontact=SprintReleaseActive&&HasControlInput&&ControlledEnvelope&&PlayerGap<118.f&&NearestFootDistance<SprintFootContactRadius;
  // A released sprint touch stays genuinely free unless the runner physically catches
  // it again. On a hard cut, a ball that has returned to a real foot-contact envelope
  // can be planted immediately instead of being allowed to pass behind the silhouette
  // until an arbitrary release timer expires.
  const bool PhysicalTurnContact=(StrongTurn&&NearestFootDistance<SprintFootContactRadius||ReleaseFootRecontact)&&Now>=SprintNextTouchAt;
  if(PhysicalTurnContact&&Now<SprintReleaseUntil)
  {
   SprintReleaseUntil=0.f;SprintDribbleFoot=NearestFoot;SprintLeadFoot=NearestFoot;
   SprintContactUntil=Now+.095f;SprintKickPending=true;SprintNextTouchAt=Now+.16f;
  }
  if(Now<SprintReleaseUntil)
  {
   // Once the ball has been pushed ahead it is physically free. Player input may
   // curve the runner, but must never bend the already-released ball in mid-roll.
   FVector V=Ball->GetPhysicsLinearVelocity();
   Ball->SetLinearDamping(.08f);Ball->SetPhysicsAngularVelocityInRadians(FVector(-V.Y/22,V.X/22,0));return;
  }
  if(PhysicalTurnContact&&!SprintKickPending&&Now>=SprintContactUntil)
  {
   SprintDribbleFoot=NearestFoot;SprintLeadFoot=NearestFoot;SprintContactUntil=Now+.095f;SprintKickPending=true;SprintNextTouchAt=Now+.16f;
  }
  const FName Lead=FVector::DotProduct(Left-Base,Direction)>=FVector::DotProduct(Right-Base,Direction)?TEXT("foot_l"):TEXT("foot_r");
  const FVector LeadFoot=Lead==TEXT("foot_l")?Left:Right;const bool LeadChanged=Lead!=SprintLeadFoot;SprintLeadFoot=Lead;
  const float TouchInterval=FMath::Lerp(.24f,.16f,TurnAmount),ReleaseDuration=FMath::Lerp(.25f,.17f,TurnAmount),ReleasePush=FMath::Lerp(225.f,180.f,TurnAmount);
  if(Now>=SprintNextTouchAt&&Now>=SprintReleaseUntil&&LeadChanged&&FVector::Dist2D(Pos,LeadFoot)<92.f){SprintDribbleFoot=Lead;SprintContactUntil=Now+.085f;SprintKickPending=true;SprintNextTouchAt=Now+TouchInterval;}
  if(Now<SprintContactUntil&&SprintDribbleFoot!=NAME_None)
  {
   const FVector Foot=SprintDribbleFoot==TEXT("foot_l")?Left:Right;FVector Target=KeepTargetOutsideFeet(KeepTargetOutsideBody(Foot+Direction*FootClearance),FootClearance+2.f);Target.Z=Pos.Z;FVector C=Target-Pos;C.Z=0;
   FVector Desired=(Player->GetVelocity()+C*18.f).GetClampedToMaxSize(Speed+180.f);FVector V=FMath::VInterpTo(Ball->GetPhysicsLinearVelocity(),Desired,Dt,30.f);V.Z=FMath::Clamp(Ball->GetPhysicsLinearVelocity().Z,-180.f,30.f);
   V=AvoidFeet(V,Speed+180.f);
   Ball->SetLinearDamping(.35f);Ball->SetPhysicsLinearVelocity(V);Ball->SetPhysicsAngularVelocityInRadians(FVector(-V.Y/22,V.X/22,0));return;
  }
  if(SprintKickPending&&Now>=SprintContactUntil){SprintKickPending=false;SprintReleaseUntil=Now+ReleaseDuration;SprintDribbleTouchCount++;SprintReleaseDirection=Direction;const float Along=FMath::Max(0.f,FVector::DotProduct(Player->GetVelocity(),SprintReleaseDirection));FVector V=SprintReleaseDirection*(Along+ReleasePush);V.Z=0;Ball->SetLinearDamping(.08f);Ball->SetPhysicsLinearVelocity(V);Ball->SetPhysicsAngularVelocityInRadians(FVector(-V.Y/22,V.X/22,0));return;}
  const bool ReleasedBallStillFar=!SprintReleaseDirection.IsNearlyZero()&&!SprintKickPending&&PlayerGap>125.f;
  if(ReleasedBallStillFar)
  {
   // Expiring the release timer is not a physical touch. If the ball is still well
   // ahead of the runner, keep it rolling on its own trajectory until the player
   // actually catches up. Steering it toward Base+Direction here was the hidden
   // "magnet" that could make a ball disappear behind the player and then snap back
   // into a plausible dribble position during rapid sprint turns.
   const FVector V=Ball->GetPhysicsLinearVelocity();
   Ball->SetLinearDamping(.62f);Ball->SetPhysicsAngularVelocityInRadians(FVector(-V.Y/22,V.X/22,0));return;
  }
  const FVector Target=KeepTargetOutsideFeet(KeepTargetOutsideBody(Base+Direction*72.f),FootClearance+2.f);FVector C=Target-Pos;C.Z=0;FVector Desired=(Direction*Speed+C*6.5f).GetClampedToMaxSize(Speed+120.f);FVector V=FMath::VInterpTo(Ball->GetPhysicsLinearVelocity(),Desired,Dt,12.f);V.Z=FMath::Clamp(Ball->GetPhysicsLinearVelocity().Z,-180.f,30.f);V=AvoidFeet(V,Speed+120.f);
  Ball->SetLinearDamping(.22f);Ball->SetPhysicsLinearVelocity(V);Ball->SetPhysicsAngularVelocityInRadians(FVector(-V.Y/22,V.X/22,0));return;
 }

 SprintDribbleFoot=NAME_None;SprintLeadFoot=NAME_None;SprintContactUntil=0;SprintReleaseUntil=0;SprintReleaseDirection=FVector::ZeroVector;SprintKickPending=false;
 // The menu background is intentionally a softer, more cinematic carry. Gameplay
 // tuning became much tighter to solve under-body/behind-player defects, but applying
 // that same high-gain controller to the autonomous demo makes the ball look glued to
 // the runner's boots. Keep the old rolling run feel here while retaining predictive
 // foot avoidance.
 if(!PlayerControlled)
 {
  CarryDribbleFoot=NAME_None;CarryFootLockUntil=0;
  FVector Target=Base+Direction*90.f;
  if(HasFeet)
  {
   const FVector Foot=FVector::Dist2D(Pos,Left)<=FVector::Dist2D(Pos,Right)?Left:Right;
   Target=FMath::Lerp(Target,Foot+Direction*FootClearance,.55f);
   Target=KeepTargetOutsideFeet(KeepTargetOutsideBody(Target),FootClearance+1.f);
  }
  FVector C=Target-Pos;C.Z=0;
  FVector Desired=(Player->GetVelocity()+C*11.5f).GetClampedToMaxSize(Speed+190.f);
  FVector V=FMath::VInterpTo(Ball->GetPhysicsLinearVelocity(),Desired,Dt,20.f);
  V.Z=FMath::Clamp(Ball->GetPhysicsLinearVelocity().Z,-180.f,30.f);V=AvoidFeet(V,Speed+190.f);
  Ball->SetLinearDamping(.3f);Ball->SetPhysicsLinearVelocity(V);Ball->SetPhysicsAngularVelocityInRadians(FVector(-V.Y/22,V.X/22,0));return;
 }
 const float PlayerDistance=BallControl?50.f:72.f;
 FVector Target=Base+Direction*PlayerDistance;
 if(HasFeet)
 {
  const float BallSide=BodyRight.IsNearlyZero()?0.f:FVector::DotProduct(Pos-Base,BodyRight);
  FName Preferred=CarryDribbleFoot;
  if(FMath::Abs(BallSide)>10.f)
  {
   const float LeftSide=FVector::DotProduct(Left-Base,BodyRight),RightSide=FVector::DotProduct(Right-Base,BodyRight);
   Preferred=FMath::Abs(BallSide-LeftSide)<=FMath::Abs(BallSide-RightSide)?TEXT("foot_l"):TEXT("foot_r");
  }
  if(CarryDribbleFoot==NAME_None){CarryDribbleFoot=FVector::Dist2D(Pos,Left)<=FVector::Dist2D(Pos,Right)?TEXT("foot_l"):TEXT("foot_r");CarryFootLockUntil=Now+.24f;}
  if(Now>=CarryFootLockUntil&&Preferred!=CarryDribbleFoot)
  {
   const FVector Current=CarryDribbleFoot==TEXT("foot_l")?Left:Right,Other=Preferred==TEXT("foot_l")?Left:Right;
   const float Ahead=FVector::DotProduct(Pos-Base,Direction),OtherDistance=FVector::Dist2D(Pos,Other),CurrentDistance=FVector::Dist2D(Pos,Current);
   if(Ahead>24.f&&OtherDistance<72.f&&OtherDistance+12.f<CurrentDistance){CarryDribbleFoot=Preferred;CarryFootLockUntil=Now+(BallControl?.22f:.30f);CarryFootSwitchCount++;}
  }
  const FVector Foot=CarryDribbleFoot==TEXT("foot_l")?Left:Right;
  const float FootSide=BodyRight.IsNearlyZero()?0.f:FMath::Clamp(FVector::DotProduct(Foot-Base,BodyRight),-26.f,26.f);
  const FVector StableTarget=Base+Direction*PlayerDistance+BodyRight*FootSide*.62f;
  const FVector FootTarget=Foot+Direction*(FootClearance+4.f);
  Target=FMath::Lerp(StableTarget,FootTarget,BallControl?.48f:.30f);
  Target=KeepTargetOutsideFeet(KeepTargetOutsideBody(Target),FootClearance+2.f);
 }
 FVector C=Target-Pos;C.Z=0;
 const FVector CarryVelocity=BallControl?Direction*FMath::Max(150.f,Speed):FMath::Lerp(Player->GetVelocity(),Direction*Speed,.45f);
 const float DirectionAhead=FVector::DotProduct(Pos-Base,Direction);const float BehindAlpha=FMath::Clamp((34.f-DirectionAhead)/70.f,0.f,1.f);
 const float CorrectionGain=(BallControl?30.f:18.f)+TurnAmount*8.f+BehindAlpha*12.f;
 const float ExtraSpeed=(BallControl?240.f:220.f)+BehindAlpha*100.f;
 const float FollowSpeed=(BallControl?42.f:31.f)+TurnAmount*7.f+BehindAlpha*9.f;
 FVector Desired=(CarryVelocity+C*CorrectionGain).GetClampedToMaxSize(Speed+ExtraSpeed);FVector V=FMath::VInterpTo(Ball->GetPhysicsLinearVelocity(),Desired,Dt,FollowSpeed);V.Z=FMath::Clamp(Ball->GetPhysicsLinearVelocity().Z,-180.f,30.f);V=AvoidFeet(V,Speed+ExtraSpeed);
 if(ControlledEnvelope&&!SprintReleaseActive)
 {
  const FVector Relative=Pos-Base;DribbleMinDirectionAhead=FMath::Min(DribbleMinDirectionAhead,FVector::DotProduct(Relative,Direction));
  if(!BodyForward.IsNearlyZero())DribbleMinBodyAhead=FMath::Min(DribbleMinBodyAhead,FVector::DotProduct(Relative,BodyForward));
 }
 Ball->SetLinearDamping(BallControl?.42f:.3f);Ball->SetPhysicsLinearVelocity(V);Ball->SetPhysicsAngularVelocityInRadians(FVector(-V.Y/22,V.X/22,0));
}
void AFootballMode::Tick(float Dt){Super::Tick(Dt);if(!Ball)return;auto PC=Cast<AFootballController>(GetWorld()->GetFirstPlayerController());if(!PC)return;const bool GameplayActive=PC->Screen==AFootballController::EScreen::Game||(PC->Screen==AFootballController::EScreen::Settings&&PC->SettingsReturn==AFootballController::EScreen::Game&&!PC->IsPaused());if(!GameplayActive)return;
 Dribble(PC->Avatar,Dt);auto P=Ball->GetComponentLocation();const FVector OldBall=PreviousBall;PreviousBall=P;float T=GetWorld()->GetTimeSeconds();if(ResetAt>0&&T>=ResetAt){ResetBall();return;}float CrossingY=P.Y,CrossingZ=P.Z;const float Plane=P.X>=0?2762.f:-2762.f;const bool Crossed=FMath::Abs(P.X)>2762&&FMath::Abs(OldBall.X)<=2762;if(Crossed&&!FMath::IsNearlyEqual(P.X,OldBall.X)){const FVector Crossing=FMath::Lerp(OldBall,P,(Plane-OldBall.X)/(P.X-OldBall.X));CrossingY=Crossing.Y;CrossingZ=Crossing.Z;}if(!Scored&&Crossed&&FMath::Abs(CrossingY)<320&&CrossingZ<238&&CrossingZ>0){Goals++;PC->OnGoal(LastShooter.Get());PC->PlayEffect(TEXT("goal"),1.5f);Scored=true;ResetAt=T+1.5f;PC->Toast=PC->Localize(TEXT("GOAL!  +1"),TEXT("GOL!  +1"));PC->ToastUntil=T+2;}
 if(!Scored&&ShotInFlight&&!BallHidden&&((FMath::Abs(P.X)>2762)||(FMath::Abs(P.X)>2720&&(FMath::Abs(P.Y)>320||P.Z>238))||FMath::Abs(P.Y)>1750))HideMiss();if(P.Z<-200)ResetBall();}

AFootballController::AFootballController(){PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.bTickEvenWhenPaused=true;bAutoManageActiveCameraTarget=false;}
void AFootballController::LoadCatalog(){FString S; if(!FFileHelper::LoadFileToString(S,*(FPaths::ProjectContentDir()/TEXT("Data/wardrobe.json"))))return;TSharedPtr<FJsonObject> Root;auto Reader=TJsonReaderFactory<>::Create(S);if(!FJsonSerializer::Deserialize(Reader,Root)||!Root)return;
 for(auto CV:Root->GetArrayField(TEXT("categories"))){auto CO=CV->AsObject();FKitCategory C;C.Label=CO->GetStringField(TEXT("label"));for(auto OV:CO->GetArrayField(TEXT("options"))){auto OO=OV->AsObject();FKitOption O;O.Label=OO->GetStringField(TEXT("label"));OO->TryGetStringField(TEXT("texture"),O.Texture);const TArray<TSharedPtr<FJsonValue>>* Arr;if(OO->TryGetArrayField(TEXT("meshes"),Arr))for(auto V:*Arr)O.Meshes.Add(V->AsString());C.Options.Add(O);}Catalog.Add(C);}}
void AFootballController::BeginPlay(){Super::BeginPlay();Avatar=Cast<AFootballPlayer>(GetPawn());if(!Avatar){Avatar=GetWorld()->SpawnActor<AFootballPlayer>(FVector(-350,0,100),FRotator::ZeroRotator);Possess(Avatar);}for(auto N:{TEXT("goal"),TEXT("jump"),TEXT("kick"),TEXT("fail"),TEXT("click")})Effects.Add(N,LoadObject<USoundBase>(nullptr,*FString::Printf(TEXT("/Game/Erling/Audio/%s.%s"),N,N)));Avatar->InitializeAssets();Avatar->SetActorLocation(FVector(-350,0,100));LoadCatalog();Saved=Cast<UFootballSave>(UGameplayStatics::LoadGameFromSlot(ProfileSlot(),0));if(!Saved)Saved=Cast<UFootballSave>(UGameplayStatics::CreateSaveGameObject(UFootballSave::StaticClass()));Saved->Language=FMath::Clamp(Saved->Language,0,1);if(Saved->CameraMode<0||Saved->CameraMode>2)Saved->CameraMode=Saved->ReducedMotion?1:0;Saved->ReducedMotion=Saved->CameraMode==1;Selection=Saved->Appearance;
 if(!Catalog.IsEmpty()){Selection.SetNum(Catalog.Num());for(int I=0;I<Catalog.Num();I++)Selection[I]=FMath::Clamp(Selection[I],0,FMath::Max(0,Catalog[I].Options.Num()-1));}else UE_LOG(LogTemp,Error,TEXT("Wardrobe catalog is empty; keeping the saved appearance unchanged"));Avatar->ApplyKit(Selection,Catalog);ViewCamera=GetWorld()->SpawnActor<ACameraActor>();ViewCamera->GetCameraComponent()->FieldOfView=48;SetViewTarget(ViewCamera);ChangeScreen(EScreen::Main);
 Music=NewObject<UAudioComponent>(this);Music->bIsUISound=true;Music->bAllowSpatialization=false;Music->bAutoActivate=false;Music->bAutoDestroy=false;Music->RegisterComponent();
 if(auto Track=LoadObject<USoundWave>(nullptr,TEXT("/Game/Erling/Audio/background.background"))){Track->bLooping=true;Music->SetSound(Track);Music->SetVolumeMultiplier(Saved->Volume);Music->Play();}ApplyQuality();}
void AFootballController::SetupInputComponent(){Super::SetupInputComponent();InputComponent->BindAction(TEXT("BallControl"),IE_Pressed,this,&AFootballController::BallControlOn);InputComponent->BindAction(TEXT("BallControl"),IE_Released,this,&AFootballController::BallControlOff);InputComponent->BindAction(TEXT("Slide"),IE_Pressed,this,&AFootballController::SlidePressed);InputComponent->BindAction(TEXT("Jump"),IE_Pressed,this,&AFootballController::JumpPressed);InputComponent->BindAction(TEXT("Jump"),IE_Released,this,&AFootballController::JumpReleased);InputComponent->BindAxis(TEXT("EditorTurn"),this,&AFootballController::TurnEditor);InputComponent->BindAxis(TEXT("EditorZoom"),this,&AFootballController::ZoomEditor);InputComponent->BindAxis(TEXT("EditorGamepadTurn"),this,&AFootballController::EditorGamepadTurn);InputComponent->BindAxis(TEXT("EditorGamepadZoom"),this,&AFootballController::EditorGamepadZoom);InputComponent->BindAxis(TEXT("Forward"),this,&AFootballController::Forward);InputComponent->BindAxis(TEXT("Right"),this,&AFootballController::Right);InputComponent->BindAxis(TEXT("LookX"),this,&AFootballController::LookX);InputComponent->BindAxis(TEXT("LookY"),this,&AFootballController::LookY);InputComponent->BindAxis(TEXT("GamepadLookX"),this,&AFootballController::GamepadLookX);InputComponent->BindAxis(TEXT("GamepadLookY"),this,&AFootballController::GamepadLookY);InputComponent->BindAction(TEXT("Sprint"),IE_Pressed,this,&AFootballController::SprintOn);InputComponent->BindAction(TEXT("Sprint"),IE_Released,this,&AFootballController::SprintOff);InputComponent->BindAction(TEXT("Kick"),IE_Pressed,this,&AFootballController::StartCharge);InputComponent->BindAction(TEXT("Kick"),IE_Released,this,&AFootballController::ReleaseCharge);InputComponent->BindAction(TEXT("ResetBall"),IE_Pressed,this,&AFootballController::Reset);auto& B=InputComponent->BindAction(TEXT("Pause"),IE_Pressed,this,&AFootballController::PauseToggle);B.bExecuteWhenPaused=true;}
void AFootballController::Forward(float V){const bool NewPress=!FMath::IsNearlyZero(V)&&!FMath::IsNearlyEqual(V,MoveForward);MoveForward=V;if(Screen==EScreen::Game&&Avatar){if(NewPress){Avatar->CancelBallTrap();if(Avatar->ActionInterruptible)Avatar->ClearAction();}if(Avatar->IsMovementLocked())return;const float MoveYaw=Saved&&Saved->CameraMode==2?-90.f:Yaw;Avatar->AddMovementInput(FRotator(0,MoveYaw,0).Vector(),V);}}
void AFootballController::Right(float V){const bool NewPress=!FMath::IsNearlyZero(V)&&!FMath::IsNearlyEqual(V,MoveRight);MoveRight=V;if(Screen==EScreen::Game&&Avatar){if(NewPress){Avatar->CancelBallTrap();if(Avatar->ActionInterruptible)Avatar->ClearAction();}if(Avatar->IsMovementLocked())return;const float MoveYaw=Saved&&Saved->CameraMode==2?-90.f:Yaw;Avatar->AddMovementInput(FRotationMatrix(FRotator(0,MoveYaw,0)).GetUnitAxis(EAxis::Y),V);}}
FVector AFootballController::GetMoveIntentWorld()const{const float MoveYaw=Saved&&Saved->CameraMode==2?-90.f:Yaw;const FVector F=FRotator(0,MoveYaw,0).Vector(),R=FRotationMatrix(FRotator(0,MoveYaw,0)).GetUnitAxis(EAxis::Y);FVector D=F*MoveForward+R*MoveRight;D.Z=0;return D.GetClampedToMaxSize(1.f);}
bool AFootballController::HasDigitalMoveIntent()const
{
 return TestDigitalMoveIntent||IsInputKeyDown(EKeys::W)||IsInputKeyDown(EKeys::A)||IsInputKeyDown(EKeys::S)||IsInputKeyDown(EKeys::D);
}
void AFootballController::LookX(float V){if(Screen==EScreen::Game&&Saved&&Saved->CameraMode!=2){Yaw+=V*Saved->Sensitivity*1.7f;if(Avatar&&!FMath::IsNearlyZero(V)&&FMath::Abs(FMath::FindDeltaAngleDegrees(Avatar->GetActorRotation().Yaw,Yaw))>65)Avatar->BeginTurn(Yaw);}}
void AFootballController::LookY(float V){if(Screen==EScreen::Game&&Saved&&Saved->CameraMode!=2)Pitch=FMath::Clamp(Pitch+V*Saved->Sensitivity,-40.f,5.f);}
void AFootballController::GamepadLookX(float V){if(Screen==EScreen::Game&&Saved&&Saved->CameraMode!=2&&!FMath::IsNearlyZero(V)){Yaw+=V*Saved->Sensitivity*120.f*GetWorld()->GetDeltaSeconds();if(Avatar&&FMath::Abs(FMath::FindDeltaAngleDegrees(Avatar->GetActorRotation().Yaw,Yaw))>65)Avatar->BeginTurn(Yaw);}}
void AFootballController::GamepadLookY(float V){if(Screen==EScreen::Game&&Saved&&Saved->CameraMode!=2&&!FMath::IsNearlyZero(V))Pitch=FMath::Clamp(Pitch+V*Saved->Sensitivity*90.f*GetWorld()->GetDeltaSeconds(),-40.f,5.f);}
void AFootballController::SprintOn(){if(Screen!=EScreen::Game)return;Sprint=true;if(Avatar)Avatar->GetCharacterMovement()->MaxWalkSpeed=765;}
void AFootballController::SprintOff(){Sprint=false;if(!Avatar)return;auto* Mode=GetWorld()->GetAuthGameMode<AFootballMode>();if(BallControlHeld&&(!Mode||!Mode->HasDribbleControl(Avatar)))BallControlHeld=false;Avatar->GetCharacterMovement()->MaxWalkSpeed=BallControlHeld?180:510;}
void AFootballController::StartCharge()
{
 if(Screen!=EScreen::Game||!Avatar||!Avatar->CanAct()||GetWorld()->GetTimeSeconds()<KickCooldown)return;
 auto* M=GetWorld()->GetAuthGameMode<AFootballMode>();if(!M||!M->Ball||M->BallHidden||M->Scored||M->ShotInFlight)return;
 const float Now=GetWorld()->GetTimeSeconds();
 const bool RecoverableSprintTouch=Sprint&&M->IsRecoverableSprintTouch(Avatar);
 if(!M->HasBall(Avatar)&&!M->HasDribbleControl(Avatar)&&!RecoverableSprintTouch)return;
 ShotBufferActive=false;ShotChargeArmed=true;Charging=true;ChargeStarted=Now;ShotChargeAimDirection=Avatar->GetActorForwardVector().GetSafeNormal2D();
}
void AFootballController::ReleaseCharge()
{
 if(!Charging)return;
 const float Now=GetWorld()->GetTimeSeconds();
 const float Seconds=FMath::Min(1.5f,Now-ChargeStarted);
 const FVector ChargedAim=ShotChargeAimDirection;
 Charging=false;
 if(Screen!=EScreen::Game||!ShotChargeArmed){ShotChargeArmed=false;return;}
 ShotChargeArmed=false;
 auto* M=GetWorld()->GetAuthGameMode<AFootballMode>();
 if(M&&M->HasBall(Avatar)&&Avatar->CanAct()&&Now>=KickCooldown)
 {
  FireShot(Seconds,ChargedAim);
  if(PendingShot)return;
 }
 const bool RecoverableAtRelease=M&&Avatar->CanAct()&&(M->HasDribbleControl(Avatar)||(Sprint&&M->IsRecoverableSprintTouch(Avatar)));
 if(!RecoverableAtRelease){ShotBufferActive=false;ShotBufferAimDirection=FVector::ZeroVector;return;}
 // The user already committed the shot input. A temporary sprint touch / loose-ball
 // phase must not eat that command. Keep the selected power and execute at the next
 // genuine ball contact, but only for a short football-action window.
 ShotBufferActive=true;ShotBufferStarted=Now;ShotBufferSeconds=Seconds;ShotBufferAimDirection=ChargedAim;
}
void AFootballController::Kick(){FireShot(.5f);}
void AFootballController::FireShot(float Seconds,FVector AimOverride)
{
 auto* M=GetWorld()->GetAuthGameMode<AFootballMode>();
 const EScreen ActiveScreen=Screen==EScreen::Settings?SettingsReturn:Screen;
 const bool Demo=ActiveScreen==EScreen::Main||ActiveScreen==EScreen::Credits;
 if((ActiveScreen!=EScreen::Game&&!Demo)||!Avatar||!M||!Avatar->CanAct()||!M->HasBall(Avatar))return;
 const float Now=GetWorld()->GetTimeSeconds();if(Now<KickCooldown)return;
 const FVector BallPosition=M->Ball->GetComponentLocation();
 FVector ShotDirection=!AimOverride.IsNearlyZero()?AimOverride.GetSafeNormal2D():Avatar->GetActorForwardVector();
 const FVector CurrentIntent=!Demo?GetMoveIntentWorld():FVector::ZeroVector;
 const bool CurrentDigital=!Demo&&HasDigitalMoveIntent();
 const bool UseRememberedDirection=!Demo&&M->BallStopped&&CurrentIntent.IsNearlyZero()&&M->HasDribbleControl(Avatar)&&!M->LastPossessionDirection.IsNearlyZero();
 const bool DigitalShotIntent=CurrentDigital||(UseRememberedDirection&&M->LastPossessionWasDigital);
 if(CurrentDigital)
 {
  if(!CurrentIntent.IsNearlyZero())ShotDirection=CurrentIntent;
 }
 else if(UseRememberedDirection)ShotDirection=M->LastPossessionDirection.GetSafeNormal2D();
 PendingVelocity=AFootballMode::ShotVelocity(BallPosition,ShotDirection,Seconds);
 // Digital WASD has only eight directions. Treat those directions as shot intent,
 // not literal 45-degree ballistics: preserve the selected side but compress it
 // into the mouth of the goal, similar to an assisted football-game shot model.
 if(!Demo&&DigitalShotIntent&&FMath::Abs(PendingVelocity.X)>1.f)
 {
  const float GoalX=PendingVelocity.X>0?2740.f:-2740.f;
  const FVector GoalDirection=(FVector(GoalX,0,BallPosition.Z)-BallPosition).GetSafeNormal2D();
  const FVector RawDirection=PendingVelocity.GetSafeNormal2D();
  if(FVector::DotProduct(RawDirection,GoalDirection)>.25f)
  {
   const float RawFlight=(GoalX-BallPosition.X)/PendingVelocity.X;
   if(RawFlight>0.f)
   {
    const float RawSide=BallPosition.Y+PendingVelocity.Y*RawFlight;
    constexpr float AimMargin=292.f;
    const float AssistedSide=RawSide*AimMargin/FMath::Sqrt(RawSide*RawSide+AimMargin*AimMargin);
    const FVector AssistedDirection=(FVector(GoalX,AssistedSide,BallPosition.Z)-BallPosition).GetSafeNormal2D();
    PendingVelocity=AFootballMode::ShotVelocity(BallPosition,AssistedDirection,Seconds);
   }
  }
 }
 float CrossingHeight=-1,CrossingSide=MAX_flt;
 if(FMath::Abs(PendingVelocity.X)>1)
 {
  const float GoalX=PendingVelocity.X>0?2740.f:-2740.f;
  const float Flight=(GoalX-BallPosition.X)/PendingVelocity.X;
  if(Flight>0){CrossingHeight=BallPosition.Z+PendingVelocity.Z*Flight+.5f*GetWorld()->GetGravityZ()*Flight*Flight;CrossingSide=BallPosition.Y+PendingVelocity.Y*Flight;}
 }
 PendingHasAim=CrossingHeight>=0&&FMath::Abs(CrossingSide)<5000;
 PendingAimTarget=FVector(PendingVelocity.X>=0?2740.f:-2740.f,CrossingSide,CrossingHeight);
 const bool Aimed=FMath::Abs(CrossingSide)<319;
 const bool UnderBar=PendingVelocity.Size2D()>=2600&&CrossingHeight>=180&&CrossingHeight<=228;
 const bool AboveBar=Aimed&&Seconds>=1.35f&&CrossingHeight-22>269;
 // Perfect under-bar timing immediately selects the flip, regardless of whether the shot eventually scores.
 const bool Flip=!Demo&&UnderBar;
 PendingTrip=!Demo&&AboveBar&&FMath::FRand()<TripChance;
 const FVector ShotPlanarDirection=PendingVelocity.GetSafeNormal2D();
 const FVector ShotRight=ShotPlanarDirection.IsNearlyZero()?Avatar->GetActorRightVector():FRotationMatrix(ShotPlanarDirection.Rotation()).GetUnitAxis(EAxis::Y);
 const bool Left=FVector::DotProduct(BallPosition-Avatar->GetActorLocation(),ShotRight)<0;
 const FString Clip=Flip?TEXT("Shot_Flip_Land"):Left?TEXT("Kick_Left"):TEXT("Kick_Right");
 ShotEntryVelocity=Avatar->GetVelocity();ShotEntryVelocity.Z=0;
 const float Rate=Flip?2.6f:2.2f;
 if(!Avatar->StartAction(Flip?AFootballPlayer::EAction::Flip:AFootballPlayer::EAction::Kick,Clip,Rate))return;
 if(Flip)
 {
  const float Speed=FMath::Clamp(ShotEntryVelocity.Size2D(),320.f,765.f);
  Avatar->EntryVelocity=(ShotEntryVelocity.IsNearlyZero()?Avatar->GetActorForwardVector():ShotEntryVelocity.GetSafeNormal())*Speed;
  Avatar->ActionVelocity=Avatar->EntryVelocity*1.15f;
 }
 ShotBallStart=BallPosition;ShotActorStart=Avatar->GetActorLocation();
 const float Length=Avatar->Animations.FindRef(Clip)->GetPlayLength();
 ShotContactTime=Length*(Flip?.36f*.43f:.43f)/Rate;
 ShotRecoverTime=ShotContactTime+.1f;PendingShot=true;KickCooldown=Now+.3f;
 GoalSpacePending=false;SpaceHeld=false;
}
void AFootballController::Reset(){if(Screen==EScreen::Game&&Avatar&&Avatar->IsMovementLocked())return;Charging=false;ShotChargeArmed=false;ShotBufferActive=false;ShotChargeAimDirection=FVector::ZeroVector;ShotBufferAimDirection=FVector::ZeroVector;if(auto M=GetWorld()->GetAuthGameMode<AFootballMode>())M->ResetBall();}
void AFootballController::PauseToggle(){if(Screen==EScreen::Game)ChangeScreen(EScreen::Pause);else if(Screen==EScreen::Pause)ChangeScreen(EScreen::Game);else if(Screen==EScreen::Settings)ChangeScreen(SettingsReturn);else if(Screen==EScreen::Editor)BackFromEditor();else if(Screen==EScreen::Credits)ChangeScreen(EScreen::Main);}
void AFootballController::ChangeScreen(EScreen Next){const auto Old=Screen;const bool ResumeFromSettings=Old==EScreen::Settings&&Next==SettingsReturn;BallControlOff();if(Avatar){Avatar->PreviewAnimation=false;PreviewIndex=-1;}Screen=Next;Charging=false;ShotChargeArmed=false;ShotBufferActive=false;ShotChargeAimDirection=FVector::ZeroVector;ShotBufferAimDirection=FVector::ZeroVector;GoalSpacePending=false;SpaceHeld=false;if((Next==EScreen::Editor||Next==EScreen::Main)&&!ResumeFromSettings){CancelPendingActions();if(Avatar){Avatar->ClearAction();Avatar->PhysicalJump=false;}GoalCelebrationUsed=true;}const bool PauseForSettings=Next==EScreen::Settings&&(SettingsReturn==EScreen::Pause||(Saved&&Saved->PauseInSettings));SetPause(Next==EScreen::Pause||PauseForSettings);SprintOff();
 if(Next==EScreen::Editor){EditorZoom=0;Dragging=false;DraftBefore=Selection;Avatar->GetCharacterMovement()->StopMovementImmediately();Avatar->SetActorLocation(FVector(-350,0,100));Avatar->SetActorRotation(FRotator(0,-35,0));Avatar->KickUntil=0;Avatar->SetAnimation(TEXT("Idle_Breathe"));Reset();}
 if(Next==EScreen::Game&&Old!=EScreen::Pause&&!ResumeFromSettings){CancelPendingActions();Avatar->ClearAction();Avatar->PhysicalJump=false;GoalCelebrationUsed=true;Avatar->SetActorLocation(FVector(-350,0,100));Avatar->SetActorRotation(FRotator::ZeroRotator);Yaw=0;Pitch=-15;KickCooldown=0;CameraPivotInitialized=false;PitchCameraLeadX=0;Reset();}
 if(Next==EScreen::Main&&Old!=EScreen::Credits&&!ResumeFromSettings){const float X=Avatar->GetActorLocation().X;DemoPhase=FMath::Abs(X)<240?4:0;DemoDirection=DemoPhase==4?(X>=0?1:-1):(X>0?-1:1);DemoShotTargetX=0.f;Reset();}bShowMouseCursor=Next!=EScreen::Game;if(bShowMouseCursor){FInputModeGameAndUI Mode;Mode.SetHideCursorDuringCapture(false);SetInputMode(Mode);}else{FInputModeGameOnly Mode;SetInputMode(Mode);}BuildUI();}
void AFootballController::Cycle(int32 C,int32 Direction){if(!Catalog.IsValidIndex(C)||Catalog[C].Options.IsEmpty())return;Selection[C]=(Selection[C]+Direction+Catalog[C].Options.Num())%Catalog[C].Options.Num();Avatar->ApplyKit(Selection,Catalog);Toast.Empty();}
void AFootballController::SaveAppearance(){Saved->Appearance=Selection;const bool Ok=UGameplayStatics::SaveGameToSlot(Saved,ProfileSlot(),0);if(Ok)DraftBefore=Selection;Toast=Ok?Localize(TEXT("Appearance saved"),TEXT("Wygląd zapisany")):Localize(TEXT("Failed to save appearance"),TEXT("Nie udało się zapisać wyglądu"));ToastUntil=GetWorld()->GetTimeSeconds()+4;}
void AFootballController::BackFromEditor(){Selection=DraftBefore;Avatar->ApplyKit(Selection,Catalog);ChangeScreen(EScreen::Main);}
void AFootballController::SaveSettings(){UGameplayStatics::SaveGameToSlot(Saved,ProfileSlot(),0);}
void AFootballController::Quit(){UKismetSystemLibrary::QuitGame(this,this,EQuitPreference::Quit,false);}
FString AFootballController::Localize(const TCHAR* English,const TCHAR* Polish)const{return Saved&&Saved->Language==1?FString(Polish):FString(English);}
FString AFootballController::LocalizeCatalogLabel(const FString& Value)const{
 if(!Saved||Saved->Language!=1)return Value;
 static const TMap<FString,FString> Pl={
  {TEXT("Hairstyle"),TEXT("Fryzura")},{TEXT("Face"),TEXT("Twarz")},{TEXT("Top"),TEXT("Góra")},{TEXT("Bottom"),TEXT("Dół")},{TEXT("Footwear"),TEXT("Obuwie")},
  {TEXT("Bald"),TEXT("Łysy")},{TEXT("Ponytail"),TEXT("Kucyk")},{TEXT("Smile"),TEXT("Uśmiech")},{TEXT("Joy"),TEXT("Radość")},{TEXT("Anger"),TEXT("Złość")},{TEXT("Surprise"),TEXT("Zaskoczenie")},{TEXT("Sleepy"),TEXT("Senny")},{TEXT("Tired"),TEXT("Zmęczony")},
  {TEXT("Shirtless"),TEXT("Bez koszulki")},{TEXT("Norway"),TEXT("Norwegia")},{TEXT("Underwear"),TEXT("Bielizna")},{TEXT("White Shorts"),TEXT("Białe spodenki")},{TEXT("Barefoot"),TEXT("Boso")},{TEXT("Football Boots"),TEXT("Korki")}
 };
 if(const FString* Found=Pl.Find(Value))return *Found;return Value;
}
FText AFootballController::OptionText(int32 I)const{if(Catalog.IsValidIndex(I)&&Selection.IsValidIndex(I)&&Catalog[I].Options.IsValidIndex(Selection[I]))return FText::FromString(LocalizeCatalogLabel(Catalog[I].Options[Selection[I]].Label));return FText::FromString(Localize(TEXT("No items"),TEXT("Brak opcji")));}
void AFootballController::Tick(float Dt){Super::Tick(Dt);
#if !UE_BUILD_SHIPPING
 if(UI)UI->RunUIChecks();
#endif
 UpdateActions(Dt);auto* GameplayMode=GetWorld()->GetAuthGameMode<AFootballMode>();if(BallControlHeld){if(!GameplayMode||!GameplayMode->HasDribbleControl(Avatar))BallControlOff();}if(FParse::Param(FCommandLine::Get(),TEXT("ErlingTest_Latest")))RunChecks();if(!Avatar||!ViewCamera)return;if(Screen==EScreen::Settings&&IsPaused())return;const EScreen ActiveScreen=Screen==EScreen::Settings?SettingsReturn:Screen;UpdateEditorInput(Dt);if(Avatar->GetActorLocation().Z < -250){Avatar->GetCharacterMovement()->StopMovementImmediately();Avatar->SetActorLocation(FVector(-350,0,100));Reset();}if(FParse::Param(FCommandLine::Get(),TEXT("ErlingTest")))RunProjectChecks();
 if(auto* Movement=Avatar->GetCharacterMovement())
 {
  const bool OnBall=ActiveScreen==EScreen::Game&&GameplayMode&&GameplayMode->HasDribbleControl(Avatar)&&Avatar->CanAct()&&!Charging;
  const float Now=GetWorld()->GetTimeSeconds();
  bool SprintReleaseLocked=false,SprintReleaseChase=false;FVector SprintChaseFacing=FVector::ZeroVector;
  if(ActiveScreen==EScreen::Game&&GameplayMode&&GameplayMode->Ball&&Sprint&&Avatar->CanAct()&&!GameplayMode->BallStopRequested&&!GameplayMode->BallStopped&&!GameplayMode->ShotInFlight&&!GameplayMode->SprintKickPending&&Now>=GameplayMode->SprintContactUntil&&GameplayMode->IsRecoverableSprintTouch(Avatar))
  {
   FVector Gap=GameplayMode->Ball->GetComponentLocation()-Avatar->GetActorLocation();
   const float GapDistance=Gap.Size2D();
   SprintReleaseLocked=Gap.Z<=-35.f&&GapDistance>48.f&&GapDistance<420.f;
   SprintReleaseChase=SprintReleaseLocked&&GapDistance>80.f;
   if(SprintReleaseChase)SprintChaseFacing=Gap.GetSafeNormal2D();
  }
  const float Speed=Avatar->GetVelocity().Size2D();
  const float SpeedAlpha=FMath::Clamp((Speed-180.f)/(765.f-180.f),0.f,1.f);
  const float DesiredYawRate=SprintReleaseChase?360.f:OnBall?(BallControlHeld?720.f:FMath::Lerp(480.f,200.f,SpeedAlpha)):520.f;
  Movement->RotationRate.Yaw=FMath::FInterpTo(Movement->RotationRate.Yaw,DesiredYawRate,Dt,10.f);
  if(SprintReleaseChase)
  {
   Movement->bOrientRotationToMovement=false;
   if(!SprintChaseFacing.IsNearlyZero())
   {
    const float CurrentYaw=Avatar->GetActorRotation().Yaw;
    const float TargetYaw=SprintChaseFacing.Rotation().Yaw;
    const float DeltaYaw=FMath::FindDeltaAngleDegrees(CurrentYaw,TargetYaw);
    const float EaseAlpha=1.f-FMath::Exp(-8.f*Dt);
    const float MaxStep=DesiredYawRate*Dt;
    const float Step=FMath::Clamp(DeltaYaw*EaseAlpha,-MaxStep,MaxStep);
    const float NewYaw=FMath::UnwindDegrees(CurrentYaw+Step);
    Avatar->SetActorRotation(FRotator(0,NewYaw,0));
   }
  }
  else if(SprintReleaseLocked)
  {
   // Final approach: keep the last body orientation stable so changing input cannot
   // rotate the capsule/foot socket away from a contact that is only a few centimetres
   // away. Full steering returns when SprintKickPending marks real touch.
   Movement->bOrientRotationToMovement=false;
  }
  else if(OnBall)
  {
   Movement->bOrientRotationToMovement=false;
   // A trapped ball is already secure. Do not rotate the player back toward the
   // previous dribble heading after a deliberate standing Turn_Step.
   if(!GameplayMode->BallStopRequested&&!GameplayMode->BallStopped&&!Avatar->TurningInPlace)
   {
    FVector Facing=GameplayMode->DribbleDirection.GetSafeNormal2D();
    if(Facing.IsNearlyZero())Facing=Avatar->GetVelocity().GetSafeNormal2D();
    if(!Facing.IsNearlyZero())
    {
     const float CurrentYaw=Avatar->GetActorRotation().Yaw;
     const float NewYaw=FMath::FixedTurn(CurrentYaw,Facing.Rotation().Yaw,DesiredYawRate*Dt);
     Avatar->SetActorRotation(FRotator(0,NewYaw,0));
    }
   }
  }
  else if(Avatar->CanAct()&&!Avatar->TurningInPlace&&!Avatar->PreviewAnimation)
   Movement->bOrientRotationToMovement=true;
 }
 auto P=Avatar->GetActorLocation();
 if(ActiveScreen==EScreen::Main||ActiveScreen==EScreen::Credits)UpdateDemo(Dt);
 FVector Cam,Target;float Fov=48;
 const bool GameplayView=ActiveScreen==EScreen::Game||ActiveScreen==EScreen::Pause;
 if(GameplayView){
  const int32 Mode=Saved?FMath::Clamp(Saved->CameraMode,0,2):0;
  if(!CameraPivotInitialized){CameraPivot=P;CameraPivotInitialized=true;}
  if(Mode==0)CameraPivot=FMath::VInterpTo(CameraPivot,P,Dt,8.f);
  else if(Mode==1)CameraPivot=P;
  else CameraPivot=FMath::VInterpTo(CameraPivot,P,Dt,5.5f);
  if(Mode==2){
   UpdatePitchCameraLead(Dt);
   const FVector Center=CameraPivot+FVector(PitchCameraLeadX,0,0);
   Cam=Center+FVector(0,1500,1050);Target=Center+FVector(0,-280,70);Fov=68;
  }else{
   const float TopDown=FMath::Clamp((-Pitch-15.f)/25.f,0.f,1.f);const float LookAhead=FMath::Lerp(350.f,180.f,TopDown);const FVector ForwardDir=FRotator(0,Yaw,0).Vector();
   Target=CameraPivot+ForwardDir*LookAhead+FVector(0,0,40);Cam=CameraPivot-FRotator(Pitch,Yaw,0).Vector()*750+FVector(0,0,300);Fov=65;
  }
  ViewCamera->SetActorLocationAndRotation(Cam,(Target-Cam).Rotation());ViewCamera->GetCameraComponent()->SetFieldOfView(FMath::FInterpTo(ViewCamera->GetCameraComponent()->FieldOfView,Fov,Dt,8.f));
 }else{bool Close=ActiveScreen==EScreen::Editor;Target=P+FVector(0,0,0);if(Close)Fov=FMath::Lerp(48.f,34.f,EditorZoom);FVector Side=FVector(.65f,.76f,0);Target+=Side*(Close?170*Fov/48:220);Cam=P+FVector(Close?470:760,Close?-620:-1000,Close?190:310);float Speed=4;auto New=FMath::VInterpTo(ViewCamera->GetActorLocation(),Cam,Dt,Speed);auto R=FMath::RInterpTo(ViewCamera->GetActorRotation(),(Target-Cam).Rotation(),Dt,Speed);ViewCamera->SetActorLocationAndRotation(New,R);ViewCamera->GetCameraComponent()->SetFieldOfView(FMath::FInterpTo(ViewCamera->GetCameraComponent()->FieldOfView,Fov,Dt,Speed));}
}

void AFootballController::BuildUI()
{
 if(!IsLocalController())return;
 if(!UI)
 {
  UClass* WidgetClass=LoadClass<UErlingInterface>(nullptr,TEXT("/Game/Erling/UI/WBP_ErlingInterface.WBP_ErlingInterface_C"));
  if(!WidgetClass){UE_LOG(LogTemp,Error,TEXT("Missing WBP_ErlingInterface. Run the UI import and authoring tools."));return;}
  UI=CreateWidget<UErlingInterface>(this,WidgetClass);
  if(UI)UI->AddToViewport(10);
 }
 if(UI)UI->RefreshScreen();
}
void AFootballController::EndPlay(const EEndPlayReason::Type Reason)
{
 if(UI){UI->RemoveFromParent();UI=nullptr;}
 Super::EndPlay(Reason);
}

void AFootballController::RunProjectChecks(){
 static bool DemoReturned=false,DemoShotFacedGoal=false;
 if(TestStage==27||TestStage==28){DemoReturned|=DemoDirection==-1;if(PendingShot)DemoShotFacedGoal|=Avatar->GetActorForwardVector().X*DemoDirection>.9f;}
#if WITH_EDITOR
 if(FAssetCompilingManager::Get().GetNumRemainingAssets()>0)return;
#endif
 if(TestStarted==0)TestStarted=FPlatformTime::Seconds()+3;
 if(TestStage==12||TestStage==13||TestStage==16||TestStage==17)Forward(1);if(TestStage==17&&Avatar->GetCharacterMovement()->IsFalling())TestJumpObserved=true;
 if(FPlatformTime::Seconds()<TestStarted)return;
 auto Check=[this](const FString& Name,bool Ok){FString Line=FString::Printf(TEXT("%s: %s\n"),*Name,Ok?TEXT("PASS"):TEXT("FAIL"));TestReport+=Line;UE_LOG(LogTemp,Display,TEXT("ERLING_CHECK %s"),*Line);};
 auto Shot=[](const FString& Name){FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/(Name+TEXT(".png")),true,false);};
 auto Mode=GetWorld()->GetAuthGameMode<AFootballMode>();
 UE_LOG(LogTemp,Display,TEXT("CHECK_POSITION stage=%d avatar=%s ball=%s"),TestStage,*Avatar->GetActorLocation().ToString(),Mode&&Mode->Ball?*Mode->Ball->GetComponentLocation().ToString():TEXT("none")); switch(TestStage){
 case 0:Selection={1,0,1,1,1};Avatar->ApplyKit(Selection,Catalog);Check(TEXT("catalog_5_categories"),Catalog.Num()==5);Check(TEXT("body_loaded"),Avatar->GetMesh()->GetSkeletalMeshAsset()!=nullptr);Check(TEXT("animations_loaded"),Avatar->Animations.Num()==24);Shot(TEXT("01_Main"));break;
 case 1:ChangeScreen(EScreen::Editor);Cycle(0,-1);Cycle(1,1);Check(TEXT("live_bald"),Avatar->Pieces.Num()==3);Check(TEXT("face_material"),Avatar->FaceMaterial!=nullptr);break;
 case 2:{TArray<FFinalSkinVertex> Vertices;Avatar->GetMesh()->GetCPUSkinnedVertices(Vertices,0);float MinZ=MAX_flt;for(const auto& V:Vertices)MinZ=FMath::Min(MinZ,static_cast<float>(Avatar->GetMesh()->GetComponentTransform().TransformPosition(FVector(V.Position)).Z));UE_LOG(LogTemp,Display,TEXT("FOOT_MIN_Z %f"),MinZ);}Shot(TEXT("02_Editor"));SaveAppearance();{auto Loaded=Cast<UFootballSave>(UGameplayStatics::LoadGameFromSlot(ProfileSlot(),0));Check(TEXT("appearance_persistence"),Loaded&&Loaded->Appearance==Selection);}break;
 case 3:Cycle(2,-1);BackFromEditor();Check(TEXT("cancel_draft"),Selection==Saved->Appearance);ChangeScreen(EScreen::Game);Check(TEXT("game_mode"),Screen==EScreen::Game&&!IsPaused());break;
 case 4:Shot(TEXT("03_Play"));{FTimerHandle ShotDelay;GetWorldTimerManager().SetTimer(ShotDelay,[this,Mode](){if(Mode&&Mode->Ball){Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+FVector(140,0,-65),false,nullptr,ETeleportType::TeleportPhysics);Mode->Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);Mode->Kick(Avatar);}},.1f,false);}break;
 case 5:if(Mode&&Mode->Ball){Check(TEXT("ball_kick_impulse"),Mode->Ball->GetPhysicsLinearVelocity().Size()>100);Mode->ResetBall();Mode->PreviousBall=FVector(2600,0,90);Mode->Ball->SetWorldLocation(FVector(2800,0,90),false,nullptr,ETeleportType::TeleportPhysics);int32 Before=Mode->Goals;Mode->Tick(.01f);Check(TEXT("goal_count"),Mode->Goals==Before+1);Mode->Tick(.01f);Check(TEXT("goal_debounce"),Mode->Goals==Before+1);}PauseToggle();Check(TEXT("pause"),IsPaused());break;
 case 6:Shot(TEXT("04_Pause"));break;
 case 7:PauseToggle();Check(TEXT("resume"),!IsPaused());ChangeScreen(EScreen::Editor);Cycle(0,1);Cycle(1,1);break;
 case 8:Shot(TEXT("05_Editor_Hair"));break;
 case 9:for(int C=2;C<5;C++)Cycle(C,-1);Check(TEXT("remove_all_clothes"),Avatar->Pieces.Num()==4);break;
 case 10:Shot(TEXT("06_Body"));break;
 case 11:for(int C=2;C<5;C++)Cycle(C,1);Check(TEXT("restore_all_clothes"),Avatar->Pieces.Num()==7);ChangeScreen(EScreen::Game);TestPosition=Avatar->GetActorLocation();break;
 case 12:Check(TEXT("walking_input"),FVector::Dist2D(TestPosition,Avatar->GetActorLocation())>200&&Avatar->CurrentAnimation==TEXT("Run"));TestPosition=Avatar->GetActorLocation();SprintOn();break;
 case 13:Check(TEXT("sprint_input"),FVector::Dist2D(TestPosition,Avatar->GetActorLocation())>600&&Avatar->CurrentAnimation==TEXT("Sprint"));SprintOff();Avatar->GetCharacterMovement()->StopMovementImmediately();if(Mode&&Mode->Ball){Mode->ResetBall();Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+Avatar->GetActorForwardVector()*140-FVector(0,0,65),false,nullptr,ETeleportType::TeleportPhysics);Mode->Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);}Kick();break;
 case 14:Check(TEXT("kick_action"),Mode&&Mode->Ball&&Mode->Ball->GetPhysicsLinearVelocity().Size()>100);Shot(TEXT("07_Action"));break;
 case 15:Mode->ResetBall();Mode->LastShot=-10;Avatar->SetActorLocation(FVector(-350,0,100));Avatar->GetCharacterMovement()->StopMovementImmediately();Mode->Ball->SetWorldLocation(FVector(-240,0,22));SprintOn();break;
 case 16:{const float BallSpeed=Mode->Ball->GetPhysicsLinearVelocity().Size(),PlayerSpeed=Avatar->GetVelocity().Size2D(),BallZ=Mode->Ball->GetComponentLocation().Z,Gap=FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetActorLocation());UE_LOG(LogTemp,Display,TEXT("BASELINE_SPRINT_DRIBBLE ball_speed=%f player_speed=%f ball_z=%f gap=%f"),BallSpeed,PlayerSpeed,BallZ,Gap);Check(TEXT("sprint_dribble_controlled"),BallSpeed<1000&&PlayerSpeed>700&&BallZ<60&&Gap<175);}GoalCelebrationUsed=true;JumpPressed();break;
 case 17:Check(TEXT("jump_and_land"),TestJumpObserved&&!Avatar->GetCharacterMovement()->IsFalling());JumpReleased();SprintOff();ChangeScreen(EScreen::Editor);Selection[1]=3;Avatar->ApplyKit(Selection,Catalog);ZoomEditor(20);Check(TEXT("zoom_limit"),EditorZoom==1);{float Before=Avatar->GetActorRotation().Yaw;TurnEditor(1);Check(TEXT("editor_rotation"),!FMath::IsNearlyEqual(Before,Avatar->GetActorRotation().Yaw));}break;
 case 18:Shot(TEXT("08_Editor_Zoom"));Check(TEXT("music_loaded_playing"),Music&&Music->Sound&&Music->IsPlaying());UpdateMusicVolume(0);Check(TEXT("music_mute"),Music&&Music->VolumeMultiplier==0);UpdateMusicVolume(.5f);Saved->Quality=0;ApplyQuality();break;
 case 19:Shot(TEXT("09_Low"));break;
 case 20:Saved->Quality=3;ApplyQuality();break;
 case 21:Shot(TEXT("10_Ultra"));Check(TEXT("graphics_high"),UGameUserSettings::GetGameUserSettings()->GetShadowQuality()==3);Check(TEXT("music_continuity"),Music&&Music->IsPlaying());break;
 case 22:ChangeScreen(EScreen::Game);PauseToggle();Check(TEXT("music_pause_continuity"),Music&&Music->IsPlaying()&&Music->bIsUISound);break;
 case 23:{ChangeScreen(EScreen::Game);Mode->ResetBall();Mode->Ball->SetWorldLocation(FVector(2800,1000,22));Mode->PreviousBall=FVector(2650,1000,22);Mode->ShotInFlight=true;Mode->Tick(.01f);Check(TEXT("miss_disappears"),Mode->BallHidden);Mode->ResetBall();Mode->Ball->SetWorldLocation(FVector(2800,1000,22));Mode->Tick(.01f);Check(TEXT("carried_ball_outside_stays"),!Mode->BallHidden);Check(TEXT("goal_sound_loaded"),Mode->GoalSound!=nullptr);
 FVector Pos(0,0,22),Dir(1,0,0);auto Pass=AFootballMode::ShotVelocity(Pos,Dir,0);Check(TEXT("tap_is_ground_pass"),Pass.Z==0);
 for(int C=1;C<=3;C++){auto V=AFootballMode::ShotVelocity(Pos,Dir,C*.5f);float T=2740/V.X;float Height=22+V.Z*T-490*T*T;Check(FString::Printf(TEXT("charge_%d_height"),C),FMath::Abs(Height-(C==1?130:C==2?220:420))<2);}
 Mode->ResetBall();Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+Avatar->GetActorForwardVector()*125-FVector(0,0,74),false,nullptr,ETeleportType::TeleportPhysics);StartCharge();ChargeStarted=GetWorld()->GetTimeSeconds()-2;ReleaseCharge();Check(TEXT("release_fires_charge"),!Charging&&Avatar->Action==AFootballPlayer::EAction::Kick);break;}
 case 24:ChangeScreen(EScreen::Credits);break;
 case 25:Shot(TEXT("11_Credits"));break;
 case 26:ChangeScreen(EScreen::Main);DemoDirection=1;DemoPhase=0;DemoShotTargetX=500.f;Mode->ResetBall();Avatar->GetCharacterMovement()->StopMovementImmediately();Avatar->SetActorRotation(FRotator::ZeroRotator);Avatar->SetActorLocation(FVector(-350,0,98));break;
 case 27:Shot(TEXT("12_Demo_Right"));break;
 case 28:Check(TEXT("demo_alternates"),DemoReturned);Check(TEXT("demo_faces_shot"),DemoShotFacedGoal);Shot(TEXT("13_Demo_Left"));break;
 case 29:ChangeScreen(EScreen::Game);Mode->ResetBall();Mode->Scored=true;Mode->Ball->SetWorldLocation(FVector(2800,0,160),false,nullptr,ETeleportType::TeleportPhysics);Mode->Ball->SetPhysicsLinearVelocity(FVector(3200,0,0));break;
 case 30:Check(TEXT("net_stops_ball"),Mode->Ball->GetComponentLocation().X<2980&&Mode->Ball->GetComponentLocation().X>2800&&Mode->Ball->GetPhysicsLinearVelocity().Size2D()<100);Check(TEXT("net_ball_lands"),Mode->Ball->GetComponentLocation().Z<40);Check(TEXT("all_effects_loaded"),Effects.Num()==5&&Effects.FindRef(TEXT("jump"))&&Effects.FindRef(TEXT("kick"))&&Effects.FindRef(TEXT("fail"))&&Effects.FindRef(TEXT("click")));UpdateEffectsVolume(.5f);PlayEffect(TEXT("goal"),1.5f);Check(TEXT("goal_gain_150_percent"),!ActiveEffects.IsEmpty()&&FMath::IsNearlyEqual(ActiveEffects.Last()->VolumeMultiplier,.75f));UpdateEffectsVolume(0);Check(TEXT("effects_mute"),!ActiveEffects.IsEmpty()&&ActiveEffects.Last()->VolumeMultiplier==0);Check(TEXT("effects_do_not_mute_music"),Music&&Music->VolumeMultiplier>0&&Music->IsPlaying());UpdateEffectsVolume(.8f);SaveSettings();{auto Loaded=Cast<UFootballSave>(UGameplayStatics::LoadGameFromSlot(ProfileSlot(),0));Check(TEXT("effects_volume_saved"),Loaded&&FMath::IsNearlyEqual(Loaded->EffectsVolume,.8f));}SettingsReturn=EScreen::Game;ChangeScreen(EScreen::Settings);break;
 case 31:Shot(TEXT("14_Settings_Effects"));break;
 default:FFileHelper::SaveStringToFile(TestReport,*(FPaths::ProjectSavedDir()/TEXT("runtime_checks.txt")));FPlatformMisc::RequestExit(false);break;
 }TestStage++;TestStarted=FPlatformTime::Seconds()+(TestStage==5?.35:TestStage==14?.5:2.0);
}













void AFootballController::JumpPressed()
{
 if(Screen!=EScreen::Game||!Avatar||SpaceHeld)return;
 SpaceHeld=true;SpaceStarted=GetWorld()->GetTimeSeconds();GoalHoldRequested=false;
 if(Avatar->CanAct()&&WasInputKeyJustPressed(EKeys::RightMouseButton)){SlidePressed();if(!Avatar->CanAct())return;}
 if(!GoalCelebrationUsed&&SpaceStarted<GoalUntil){GoalSpacePending=true;return;}
 if(!Avatar->CanAct())return;
 Avatar->StartPhysicalJump();
}
void AFootballController::JumpReleased()
{
 if(GoalSpacePending&&SpaceHeld&&Screen==EScreen::Game)
 {
  GoalHoldRequested|=GetWorld()->GetTimeSeconds()-SpaceStarted>=1.f;
  BeginCelebration(GoalHoldRequested);
 }
 SpaceHeld=false;if(Avatar)Avatar->StopJumping();
}
void AFootballController::TurnEditor(float V){if(Screen==EScreen::Editor&&Avatar&&!FMath::IsNearlyZero(V))Avatar->AddActorWorldRotation(FRotator(0,-V*90*GetWorld()->GetDeltaSeconds(),0));}
void AFootballController::ZoomEditor(float V){if(Screen==EScreen::Editor)EditorZoom=FMath::Clamp(EditorZoom+V*.15f,0.f,1.f);}
void AFootballController::EditorGamepadTurn(float V){if(Screen==EScreen::Editor&&Avatar&&!FMath::IsNearlyZero(V))Avatar->AddActorWorldRotation(FRotator(0,-V*120.f*GetWorld()->GetDeltaSeconds(),0));}
void AFootballController::EditorGamepadZoom(float V){if(Screen==EScreen::Editor&&!FMath::IsNearlyZero(V))EditorZoom=FMath::Clamp(EditorZoom+V*.9f*GetWorld()->GetDeltaSeconds(),0.f,1.f);}
void AFootballController::UpdateEditorInput(float Dt){
 if(Screen!=EScreen::Editor){Dragging=false;MouseWasDown=false;return;}
 float X,Y;if(!GetMousePosition(X,Y))return;bool Down=IsInputKeyDown(EKeys::LeftMouseButton);
 if(Down&&!MouseWasDown&&!(UI&&UI->IsPointerOverPanel())){FVector Center=Avatar->GetActorLocation()+FVector(0,0,2);FVector2D Lo(1e9,1e9),Hi(-1e9,-1e9);
  for(int I=0;I<8;I++){FVector2D Pixel;ProjectWorldLocationToScreen(Center+FVector(I&1?95:-95,I&2?65:-65,I&4?100:-96),Pixel);Lo.X=FMath::Min(Lo.X,Pixel.X);Lo.Y=FMath::Min(Lo.Y,Pixel.Y);Hi.X=FMath::Max(Hi.X,Pixel.X);Hi.Y=FMath::Max(Hi.Y,Pixel.Y);}
  int W,H;GetViewportSize(W,H);Dragging=X>W*.40f&&Y<H*.86f&&X>=Lo.X&&X<=Hi.X&&Y>=Lo.Y&&Y<=Hi.Y;
 }
 if(Down&&Dragging)Avatar->AddActorWorldRotation(FRotator(0,-(X-LastMouseX)*.45f,0));if(!Down)Dragging=false;LastMouseX=X;MouseWasDown=Down;
}
void AFootballController::UpdateMusicVolume(float V){Saved->Volume=FMath::Clamp(V,0.f,1.f);if(Music)Music->SetVolumeMultiplier(Saved->Volume);}
void AFootballController::ApplyQuality(){
 if(!Saved)return;Saved->Quality=FMath::Clamp(Saved->Quality,0,3);
 if(auto G=UGameUserSettings::GetGameUserSettings()){G->SetOverallScalabilityLevel(Saved->Quality);G->SetResolutionScaleValueEx(Saved->Quality==0?70:100);G->ApplySettings(false);}
 auto Set=[](const TCHAR* N,int V){if(auto C=IConsoleManager::Get().FindConsoleVariable(N))C->Set(V,ECVF_SetByCode);};
 Set(TEXT("r.Shadow.MaxResolution"),Saved->Quality==3?4096:Saved->Quality==2?2048:1024);Set(TEXT("r.Shadow.CSM.MaxCascades"),Saved->Quality==3?4:2);
 Set(TEXT("r.ContactShadows"),Saved->Quality>=2);Set(TEXT("r.AmbientOcclusionLevels"),Saved->Quality==3?3:Saved->Quality>=1?1:0);
 Set(TEXT("r.DynamicGlobalIlluminationMethod"),Saved->Quality==3?1:0);Set(TEXT("r.Lumen.DiffuseIndirect.Allow"),Saved->Quality==3);Set(TEXT("r.Lumen.Reflections.Allow"),0);
}



void AFootballController::UpdateDemo(float Dt){
 auto M=GetWorld()->GetAuthGameMode<AFootballMode>();if(!M||!M->Ball)return;
 const float T=GetWorld()->GetTimeSeconds();const float X=Avatar->GetActorLocation().X;
 // Turn as soon as the kick finishes; let the released ball complete its flight.
 if(DemoPhase==1&&!PendingShot&&Avatar->CanAct()){DemoDirection=-DemoDirection;DemoPhase=3;}
 if(DemoPhase==3&&T-DemoShotAt>=1.25f){DemoPhase=0;M->ResetBall();}
 // Entering the menu near midfield needs a short approach, not a completed-shot state.
 if(DemoPhase==4&&X*DemoDirection>=300.f&&Avatar->CanAct()){DemoDirection=-DemoDirection;DemoPhase=0;M->ResetBall();}
 Avatar->GetCharacterMovement()->MaxWalkSpeed=510;
 FVector Direction(DemoDirection,0,0);Direction.Y=FMath::Clamp(-Avatar->GetActorLocation().Y*.01f,-.3f,.3f);
 if(!Avatar->IsMovementLocked())Avatar->AddMovementInput(Direction.GetSafeNormal(),1);
 FVector To=M->Ball->GetComponentLocation()-Avatar->GetActorLocation();To.Z=0;
 if(DemoPhase==0)
 {
  // The midfield ball is a waiting ball. It must not anticipate the runner or slide
  // toward a future receive point. The first movement comes from a real foot contact.
  M->Ball->SetLinearDamping(2.f);M->Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);M->Ball->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
 }
 const bool HasDemoFeet=Avatar->GetMesh()&&Avatar->GetMesh()->DoesSocketExist(TEXT("foot_l"))&&Avatar->GetMesh()->DoesSocketExist(TEXT("foot_r"));
 float DemoFootDistance=MAX_flt;
 if(HasDemoFeet)DemoFootDistance=FMath::Min(FVector::Dist2D(M->Ball->GetComponentLocation(),Avatar->GetMesh()->GetSocketLocation(TEXT("foot_l"))),FVector::Dist2D(M->Ball->GetComponentLocation(),Avatar->GetMesh()->GetSocketLocation(TEXT("foot_r"))));
 const float DemoPlayerGap=To.Size2D();
 const bool RealDemoReceive=DemoPlayerGap<118.f&&((HasDemoFeet&&DemoFootDistance<=62.f)||(!HasDemoFeet&&DemoPlayerGap<88.f));
 if(Avatar->CanAct()&&DemoPhase==0&&RealDemoReceive&&To.X*DemoDirection>18&&FMath::Abs(To.Y)<82&&Avatar->GetActorForwardVector().X*DemoDirection>.9f){
  DemoPhase=2;DemoReceiveAt=T;DemoReceivePosition=Avatar->GetActorLocation();
  const float Side=FVector::DotProduct(To,Avatar->GetActorRightVector());
  DemoFootSide=FMath::Abs(Side)>8?FMath::Sign(Side)*18.f:DemoDirection*18.f;
 }
 if(DemoPhase==2&&Avatar->CanAct())
 {
  M->Dribble(Avatar,Dt);
  // Ease the received ball toward the available foot without teleporting it.
  const FVector SideTarget=Avatar->GetActorLocation()+Avatar->GetActorRightVector()*DemoFootSide;
  const float SideError=FVector::DotProduct(SideTarget-M->Ball->GetComponentLocation(),Avatar->GetActorRightVector());
  const FVector BallVelocity=M->Ball->GetPhysicsLinearVelocity();
  const FVector Right=Avatar->GetActorRightVector();
  M->Ball->SetPhysicsLinearVelocity(BallVelocity+Right*(SideError*5.f-FVector::DotProduct(BallVelocity,Right)*.72f));
  if(DemoShotTargetX==0.f)
  {
   static const float Distances[]={500.f,800.f,1100.f,1400.f,1700.f,2100.f};
   int32 Choice=FMath::RandRange(0,UE_ARRAY_COUNT(Distances)-1);
   if(Choice==DemoShotDistanceIndex)Choice=(Choice+FMath::RandRange(1,UE_ARRAY_COUNT(Distances)-1))%UE_ARRAY_COUNT(Distances);
   DemoShotDistanceIndex=Choice;DemoShotTargetX=Distances[Choice]*DemoDirection;
  }
  const bool ReachedShotPoint=DemoDirection>0?Avatar->GetActorLocation().X>=DemoShotTargetX:Avatar->GetActorLocation().X<=DemoShotTargetX;
  if(T-DemoReceiveAt>=.35f&&ReachedShotPoint&&M->HasBall(Avatar))
  {
   FireShot(1.f);if(PendingShot){DemoPhase=1;DemoShotAt=T;DemoShotTargetX=0.f;}
  }
 }
}

float AFootballController::UpdatePitchCameraLead(float Dt)
{
 if(!Avatar)return PitchCameraLeadX;
 const bool ShotAction=Avatar->Action==AFootballPlayer::EAction::Kick||Avatar->Action==AFootballPlayer::EAction::Flip;
 const float LeadVelocityX=ShotAction?Avatar->EntryVelocity.X:Avatar->GetVelocity().X;
 const float DesiredLeadX=FMath::Clamp(LeadVelocityX*.22f,-260.f,260.f);
 PitchCameraLeadX=FMath::FInterpTo(PitchCameraLeadX,DesiredLeadX,Dt,4.5f);
 return PitchCameraLeadX;
}

void AFootballPlayer::Landed(const FHitResult& Hit){
 const bool Audible=GetVelocity().Z < -150;Super::Landed(Hit);
 if(PhysicalJump)if(auto Sequence=Animations.FindRef(CurrentAnimation)){AnimationTime=Sequence->GetPlayLength()*.76f;PlaybackRate=2.6f;}
 if(Audible)if(auto PC=Cast<AFootballController>(GetController()))if(PC->Screen==AFootballController::EScreen::Game)PC->PlayEffect(TEXT("jump"));
}
void AFootballController::PlayEffect(const FString& Name,float Gain){
 if(!Saved)return;auto Sound=Effects.FindRef(Name);if(!Sound)return;
 ActiveEffects.RemoveAll([](UAudioComponent* C){return !IsValid(C)||!C->IsPlaying();});
 auto C=UGameplayStatics::SpawnSound2D(this,Sound,Saved->EffectsVolume*Gain);
 if(C){C->ComponentTags.Add(FName(*FString::SanitizeFloat(Gain)));ActiveEffects.Add(C);}
}
void AFootballController::UpdateEffectsVolume(float V){
 Saved->EffectsVolume=FMath::Clamp(V,0.f,1.f);
 for(auto C:ActiveEffects)if(IsValid(C)&&C->IsPlaying()){float Gain=C->ComponentTags.IsEmpty()?1.f:FCString::Atof(*C->ComponentTags[0].ToString());C->SetVolumeMultiplier(Saved->EffectsVolume*Gain);}
}
void AFootballMode::CreateNetCollision(){
 auto Net=[this](FVector P,FVector Size){auto C=Box(P,Size,FLinearColor::White);C->SetVisibility(false);C->SetCastShadow(false);C->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);C->ComponentTags.Add(TEXT("GoalNet"));auto PM=NewObject<UPhysicalMaterial>(C);PM->Restitution=0;PM->bOverrideRestitutionCombineMode=true;PM->RestitutionCombineMode=EFrictionCombineMode::Min;PM->Friction=.9f;C->SetPhysMaterialOverride(PM);};
 for(int S:{-1,1}){Net(FVector(S*2990,0,130),FVector(12,700,260));for(int Y:{-1,1})Net(FVector(S*2870,Y*350,130),FVector(250,12,260));Net(FVector(S*2870,0,260),FVector(250,700,12));}
}
void AFootballMode::NetHit(UPrimitiveComponent* HitComponent,AActor* OtherActor,UPrimitiveComponent* OtherComp,FVector NormalImpulse,const FHitResult& Hit){
 if(Ball&&OtherComp&&OtherComp->ComponentHasTag(TEXT("GoalNet"))){auto V=Ball->GetPhysicsLinearVelocity();Ball->SetPhysicsLinearVelocity(FVector(0,0,FMath::Min(V.Z,0.f)));Ball->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);}
}
