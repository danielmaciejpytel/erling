#include "Football.h"
#include "ErlingAnimation.h"
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
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Styling/CoreStyle.h"

static TWeakObjectPtr<AFootballController> AudioController;
static void ClickSound(){if(AudioController.IsValid())AudioController->PlayEffect(TEXT("click"));}
static const FLinearColor Ink(.018f,.022f,.03f,.955f),Panel(.035f,.043f,.055f,.92f),PanelSoft(.055f,.064f,.078f,.86f),Pink(1.f,.16f,.49f,1.f),PinkSoft(.42f,.055f,.19f,.92f),White(.97f,.975f,.985f),Muted(.70f,.73f,.78f);
static const TCHAR* ProfileSlot(){return (FParse::Param(FCommandLine::Get(),TEXT("ErlingTest"))||FParse::Param(FCommandLine::Get(),TEXT("ErlingTest_Latest")))?TEXT("ErlingProfile_Test"):TEXT("ErlingProfile");}
static TSharedRef<STextBlock> Text(const FString& Value,int32 Size=20,FLinearColor Color=White) {
 return SNew(STextBlock).Text(FText::FromString(Value)).Font(FCoreStyle::GetDefaultFontStyle("Regular",Size)).ColorAndOpacity(Color);
}
static TSharedRef<STextBlock> BoldText(const FString& Value,int32 Size=20,FLinearColor Color=White) {
 return SNew(STextBlock).Text(FText::FromString(Value)).Font(FCoreStyle::GetDefaultFontStyle("Bold",Size)).ColorAndOpacity(Color);
}
static TSharedRef<SWidget> Button(const FString& Label,TFunction<void()> Click,bool Primary=false,const FString& Prefix=TEXT("")) {
 const FLinearColor Tint=Primary?PinkSoft:PanelSoft;
 return SNew(SBox).MinDesiredHeight(62)
 [SNew(SButton).ContentPadding(FMargin(16,10)).ButtonColorAndOpacity(Tint).ForegroundColor(White).OnClicked_Lambda([Click](){ClickSound();Click();return FReply::Handled();})
  [SNew(SHorizontalBox)
   +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,14,0)[BoldText(Prefix,Primary?22:18,Primary?Pink:Muted)]
   +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[BoldText(Label,19,White)]]];
}
static TSharedRef<SWidget> KeyCap(const FString& Key,const FString& Label) {
 return SNew(SHorizontalBox)
  +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.04f,.055f,.055f,.82f)).Padding(FMargin(8,5))[BoldText(Key,14,Pink)]]
  +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10,0,22,0)[Text(Label,15,White)];
}

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
void AFootballMode::ResetBall(){if(!Ball)return;LastShooter.Reset();BallHidden=false;ShotInFlight=false;SprintDribbleFoot=NAME_None;SprintLeadFoot=NAME_None;SprintContactUntil=0;SprintReleaseUntil=0;SprintNextTouchAt=0;SprintKickPending=false;SprintDribbleTouchCount=0;DribbleMinFootClearance=MAX_flt;SprintDribbleMinDistance=MAX_flt;SprintDribbleMaxDistance=0;Ball->SetVisibility(true);Ball->SetSimulatePhysics(true);Ball->SetLinearDamping(.3f);Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);Ball->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);Ball->SetWorldLocation(FVector(0,0,28),false,nullptr,ETeleportType::TeleportPhysics);Scored=false;ResetAt=0;PreviousBall=Ball->GetComponentLocation();}
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
 FVector Pos=Ball->GetComponentLocation();const FVector Base=Player->GetActorLocation();FVector To=Pos-Base;To.Z=0;
 const float Speed=Player->GetVelocity().Size2D();if(To.Size()>230||Pos.Z>Base.Z-35)return;
 const auto* PC=Cast<AFootballController>(Player->GetController());
 const bool Sprinting=PC&&PC->Sprint&&Speed>560.f;
 const bool BallControl=PC&&PC->BallControlHeld&&!Sprinting;
 FVector Direction=Player->GetVelocity().GetSafeNormal2D();
 bool HasControlInput=false;
 if(PC)
 {
  const float MoveYaw=PC->Saved&&PC->Saved->CameraMode==2?-90.f:PC->Yaw;
  const FVector F=FRotator(0,MoveYaw,0).Vector(),R=FRotationMatrix(FRotator(0,MoveYaw,0)).GetUnitAxis(EAxis::Y);
  FVector Input=F*PC->MoveForward+R*PC->MoveRight;Input.Z=0;
  if(!Input.IsNearlyZero()){HasControlInput=true;Input.Normalize();Direction=BallControl?Input:(Direction*(Sprinting?.9f:.72f)+Input*(Sprinting?.1f:.28f)).GetSafeNormal2D();}
 }
 if(Speed<25.f&&!(BallControl&&HasControlInput))return;
 if(Direction.IsNearlyZero()||(!BallControl&&FVector::DotProduct(To.GetSafeNormal(),Direction)<-.25f))return;

 const bool HasFeet=Player->GetMesh()&&Player->GetMesh()->DoesSocketExist(TEXT("foot_l"))&&Player->GetMesh()->DoesSocketExist(TEXT("foot_r"));
 FVector Left=Base,Right=Base;constexpr float FootClearance=32.f;
 if(HasFeet)
 {
  Left=Player->GetMesh()->GetSocketLocation(TEXT("foot_l"));Right=Player->GetMesh()->GetSocketLocation(TEXT("foot_r"));bool Corrected=false;
  for(const FVector Foot:{Left,Right}){FVector D=Pos-Foot;D.Z=0;const float L=D.Size();if(L<FootClearance){Pos+=(L>1.f?D/L:Direction)*(FootClearance-L);Corrected=true;}}
  if(Corrected)Ball->SetWorldLocation(FVector(Pos.X,Pos.Y,Ball->GetComponentLocation().Z),false,nullptr,ETeleportType::TeleportPhysics);
  DribbleMinFootClearance=FMath::Min(DribbleMinFootClearance,FMath::Min(FVector::Dist2D(Pos,Left),FVector::Dist2D(Pos,Right)));
 }

 ShotInFlight=false;const float Now=GetWorld()->GetTimeSeconds();
 if(Sprinting&&HasFeet)
 {
  const float PlayerGap=FVector::Dist2D(Pos,Base);SprintDribbleMinDistance=FMath::Min(SprintDribbleMinDistance,PlayerGap);SprintDribbleMaxDistance=FMath::Max(SprintDribbleMaxDistance,PlayerGap);
  const FName Lead=FVector::DotProduct(Left-Base,Direction)>=FVector::DotProduct(Right-Base,Direction)?TEXT("foot_l"):TEXT("foot_r");
  const FVector LeadFoot=Lead==TEXT("foot_l")?Left:Right;const bool LeadChanged=Lead!=SprintLeadFoot;SprintLeadFoot=Lead;
  if(Now>=SprintNextTouchAt&&Now>=SprintReleaseUntil&&LeadChanged&&FVector::Dist2D(Pos,LeadFoot)<92.f){SprintDribbleFoot=Lead;SprintContactUntil=Now+.085f;SprintKickPending=true;SprintNextTouchAt=Now+.24f;}
  if(Now<SprintContactUntil&&SprintDribbleFoot!=NAME_None)
  {
   const FVector Foot=SprintDribbleFoot==TEXT("foot_l")?Left:Right;FVector Target=Foot+Direction*FootClearance;Target.Z=Pos.Z;FVector C=Target-Pos;C.Z=0;
   FVector Desired=(Player->GetVelocity()+C*18.f).GetClampedToMaxSize(Speed+180.f);FVector V=FMath::VInterpTo(Ball->GetPhysicsLinearVelocity(),Desired,Dt,30.f);V.Z=FMath::Clamp(Ball->GetPhysicsLinearVelocity().Z,-180.f,30.f);
   Ball->SetLinearDamping(.35f);Ball->SetPhysicsLinearVelocity(V);Ball->SetPhysicsAngularVelocityInRadians(FVector(-V.Y/22,V.X/22,0));return;
  }
  if(SprintKickPending&&Now>=SprintContactUntil){SprintKickPending=false;SprintReleaseUntil=Now+.29f;SprintDribbleTouchCount++;FVector V=Player->GetVelocity()+Direction*360.f;V.Z=0;Ball->SetLinearDamping(.08f);Ball->SetPhysicsLinearVelocity(V);Ball->SetPhysicsAngularVelocityInRadians(FVector(-V.Y/22,V.X/22,0));return;}
  if(Now<SprintReleaseUntil)
  {
   const FVector Side=FVector::CrossProduct(FVector::UpVector,Direction).GetSafeNormal();const float SideError=FVector::DotProduct(Base+Direction*120.f-Pos,Side);FVector V=Ball->GetPhysicsLinearVelocity();V+=Side*SideError*FMath::Min(1.f,Dt*7.f);V.Z=FMath::Clamp(V.Z,-180.f,30.f);
   Ball->SetLinearDamping(.08f);Ball->SetPhysicsLinearVelocity(V);Ball->SetPhysicsAngularVelocityInRadians(FVector(-V.Y/22,V.X/22,0));return;
  }
  const FVector Target=Base+Direction*78.f;FVector C=Target-Pos;C.Z=0;FVector Desired=(Player->GetVelocity()+C*4.2f).GetClampedToMaxSize(Speed+100.f);FVector V=FMath::VInterpTo(Ball->GetPhysicsLinearVelocity(),Desired,Dt,8.f);V.Z=FMath::Clamp(Ball->GetPhysicsLinearVelocity().Z,-180.f,30.f);
  Ball->SetLinearDamping(.22f);Ball->SetPhysicsLinearVelocity(V);Ball->SetPhysicsAngularVelocityInRadians(FVector(-V.Y/22,V.X/22,0));return;
 }

 SprintDribbleFoot=NAME_None;SprintLeadFoot=NAME_None;SprintContactUntil=0;SprintReleaseUntil=0;SprintKickPending=false;
 const float PlayerDistance=BallControl?52.f:88.f,CorrectionGain=BallControl?24.f:9.5f,ExtraSpeed=BallControl?220.f:190.f,FollowSpeed=BallControl?36.f:18.f;
 FVector Target=Base+Direction*PlayerDistance;
 if(HasFeet)
 {
  const FVector Foot=FVector::Dist2D(Pos,Left)<=FVector::Dist2D(Pos,Right)?Left:Right;
  const FVector FootTarget=Foot+Direction*FootClearance;
  Target=BallControl?FootTarget:FMath::Lerp(Target,FootTarget,.76f);
 }
 FVector C=Target-Pos;C.Z=0;
 const FVector CarryVelocity=BallControl?Direction*FMath::Max(150.f,Speed):Player->GetVelocity();
 FVector Desired=(CarryVelocity+C*CorrectionGain).GetClampedToMaxSize(Speed+ExtraSpeed);FVector V=FMath::VInterpTo(Ball->GetPhysicsLinearVelocity(),Desired,Dt,FollowSpeed);V.Z=FMath::Clamp(Ball->GetPhysicsLinearVelocity().Z,-180.f,30.f);
 Ball->SetLinearDamping(BallControl?.42f:.3f);Ball->SetPhysicsLinearVelocity(V);Ball->SetPhysicsAngularVelocityInRadians(FVector(-V.Y/22,V.X/22,0));
}
void AFootballMode::Tick(float Dt){Super::Tick(Dt);if(!Ball)return;auto PC=Cast<AFootballController>(GetWorld()->GetFirstPlayerController());if(!PC)return;const bool GameplayActive=PC->Screen==AFootballController::EScreen::Game||(PC->Screen==AFootballController::EScreen::Settings&&PC->SettingsReturn==AFootballController::EScreen::Game&&!PC->IsPaused());if(!GameplayActive)return;
 Dribble(PC->Avatar,Dt);auto P=Ball->GetComponentLocation();const FVector OldBall=PreviousBall;PreviousBall=P;float T=GetWorld()->GetTimeSeconds();if(ResetAt>0&&T>=ResetAt){ResetBall();return;}float CrossingY=P.Y,CrossingZ=P.Z;const float Plane=P.X>=0?2762.f:-2762.f;const bool Crossed=FMath::Abs(P.X)>2762&&FMath::Abs(OldBall.X)<=2762;if(Crossed&&!FMath::IsNearlyEqual(P.X,OldBall.X)){const FVector Crossing=FMath::Lerp(OldBall,P,(Plane-OldBall.X)/(P.X-OldBall.X));CrossingY=Crossing.Y;CrossingZ=Crossing.Z;}if(!Scored&&Crossed&&FMath::Abs(CrossingY)<320&&CrossingZ<238&&CrossingZ>0){Goals++;PC->OnGoal(LastShooter.Get());PC->PlayEffect(TEXT("goal"),1.5f);Scored=true;ResetAt=T+1.5f;PC->Toast=PC->Localize(TEXT("GOAL!  +1"),TEXT("GOL!  +1"));PC->ToastUntil=T+2;}
 if(!Scored&&ShotInFlight&&!BallHidden&&((FMath::Abs(P.X)>2762)||(FMath::Abs(P.X)>2720&&(FMath::Abs(P.Y)>320||P.Z>238))||FMath::Abs(P.Y)>1750))HideMiss();if(P.Z<-200)ResetBall();}

AFootballController::AFootballController(){PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.bTickEvenWhenPaused=true;bAutoManageActiveCameraTarget=false;}
void AFootballController::LoadCatalog(){FString S; if(!FFileHelper::LoadFileToString(S,*(FPaths::ProjectContentDir()/TEXT("Data/wardrobe.json"))))return;TSharedPtr<FJsonObject> Root;auto Reader=TJsonReaderFactory<>::Create(S);if(!FJsonSerializer::Deserialize(Reader,Root)||!Root)return;
 for(auto CV:Root->GetArrayField(TEXT("categories"))){auto CO=CV->AsObject();FKitCategory C;C.Label=CO->GetStringField(TEXT("label"));for(auto OV:CO->GetArrayField(TEXT("options"))){auto OO=OV->AsObject();FKitOption O;O.Label=OO->GetStringField(TEXT("label"));OO->TryGetStringField(TEXT("texture"),O.Texture);const TArray<TSharedPtr<FJsonValue>>* Arr;if(OO->TryGetArrayField(TEXT("meshes"),Arr))for(auto V:*Arr)O.Meshes.Add(V->AsString());C.Options.Add(O);}Catalog.Add(C);}}
void AFootballController::BeginPlay(){Super::BeginPlay();Avatar=Cast<AFootballPlayer>(GetPawn());if(!Avatar){Avatar=GetWorld()->SpawnActor<AFootballPlayer>(FVector(-350,0,100),FRotator::ZeroRotator);Possess(Avatar);}AudioController=this;for(auto N:{TEXT("goal"),TEXT("jump"),TEXT("kick"),TEXT("fail"),TEXT("click")})Effects.Add(N,LoadObject<USoundBase>(nullptr,*FString::Printf(TEXT("/Game/Erling/Audio/%s.%s"),N,N)));Avatar->InitializeAssets();Avatar->SetActorLocation(FVector(-350,0,100));LoadCatalog();Saved=Cast<UFootballSave>(UGameplayStatics::LoadGameFromSlot(ProfileSlot(),0));if(!Saved)Saved=Cast<UFootballSave>(UGameplayStatics::CreateSaveGameObject(UFootballSave::StaticClass()));Saved->Language=FMath::Clamp(Saved->Language,0,1);if(Saved->CameraMode<0||Saved->CameraMode>2)Saved->CameraMode=Saved->ReducedMotion?1:0;Saved->ReducedMotion=Saved->CameraMode==1;Selection=Saved->Appearance;
 if(!Catalog.IsEmpty()){Selection.SetNum(Catalog.Num());for(int I=0;I<Catalog.Num();I++)Selection[I]=FMath::Clamp(Selection[I],0,FMath::Max(0,Catalog[I].Options.Num()-1));}else UE_LOG(LogTemp,Error,TEXT("Wardrobe catalog is empty; keeping the saved appearance unchanged"));Avatar->ApplyKit(Selection,Catalog);ViewCamera=GetWorld()->SpawnActor<ACameraActor>();ViewCamera->GetCameraComponent()->FieldOfView=48;SetViewTarget(ViewCamera);ChangeScreen(EScreen::Main);
 Music=NewObject<UAudioComponent>(this);Music->bIsUISound=true;Music->bAllowSpatialization=false;Music->bAutoActivate=false;Music->bAutoDestroy=false;Music->RegisterComponent();
 if(auto Track=LoadObject<USoundWave>(nullptr,TEXT("/Game/Erling/Audio/background.background"))){Track->bLooping=true;Music->SetSound(Track);Music->SetVolumeMultiplier(Saved->Volume);Music->Play();}ApplyQuality();}
void AFootballController::SetupInputComponent(){Super::SetupInputComponent();InputComponent->BindAction(TEXT("BallControl"),IE_Pressed,this,&AFootballController::BallControlOn);InputComponent->BindAction(TEXT("BallControl"),IE_Released,this,&AFootballController::BallControlOff);InputComponent->BindAction(TEXT("Slide"),IE_Pressed,this,&AFootballController::SlidePressed);InputComponent->BindAction(TEXT("Jump"),IE_Pressed,this,&AFootballController::JumpPressed);InputComponent->BindAction(TEXT("Jump"),IE_Released,this,&AFootballController::JumpReleased);InputComponent->BindAxis(TEXT("EditorTurn"),this,&AFootballController::TurnEditor);InputComponent->BindAxis(TEXT("EditorZoom"),this,&AFootballController::ZoomEditor);InputComponent->BindAxis(TEXT("EditorGamepadTurn"),this,&AFootballController::EditorGamepadTurn);InputComponent->BindAxis(TEXT("EditorGamepadZoom"),this,&AFootballController::EditorGamepadZoom);InputComponent->BindAxis(TEXT("Forward"),this,&AFootballController::Forward);InputComponent->BindAxis(TEXT("Right"),this,&AFootballController::Right);InputComponent->BindAxis(TEXT("LookX"),this,&AFootballController::LookX);InputComponent->BindAxis(TEXT("LookY"),this,&AFootballController::LookY);InputComponent->BindAxis(TEXT("GamepadLookX"),this,&AFootballController::GamepadLookX);InputComponent->BindAxis(TEXT("GamepadLookY"),this,&AFootballController::GamepadLookY);InputComponent->BindAction(TEXT("Sprint"),IE_Pressed,this,&AFootballController::SprintOn);InputComponent->BindAction(TEXT("Sprint"),IE_Released,this,&AFootballController::SprintOff);InputComponent->BindAction(TEXT("Kick"),IE_Pressed,this,&AFootballController::StartCharge);InputComponent->BindAction(TEXT("Kick"),IE_Released,this,&AFootballController::ReleaseCharge);InputComponent->BindAction(TEXT("ResetBall"),IE_Pressed,this,&AFootballController::Reset);auto& B=InputComponent->BindAction(TEXT("Pause"),IE_Pressed,this,&AFootballController::PauseToggle);B.bExecuteWhenPaused=true;}
void AFootballController::Forward(float V){const bool NewPress=!FMath::IsNearlyZero(V)&&!FMath::IsNearlyEqual(V,MoveForward);MoveForward=V;if(Screen==EScreen::Game&&Avatar){if(NewPress&&Avatar->ActionInterruptible)Avatar->ClearAction();if(Avatar->IsMovementLocked())return;const float MoveYaw=Saved&&Saved->CameraMode==2?-90.f:Yaw;Avatar->AddMovementInput(FRotator(0,MoveYaw,0).Vector(),V);}}
void AFootballController::Right(float V){const bool NewPress=!FMath::IsNearlyZero(V)&&!FMath::IsNearlyEqual(V,MoveRight);MoveRight=V;if(Screen==EScreen::Game&&Avatar){if(NewPress&&Avatar->ActionInterruptible)Avatar->ClearAction();if(Avatar->IsMovementLocked())return;const float MoveYaw=Saved&&Saved->CameraMode==2?-90.f:Yaw;Avatar->AddMovementInput(FRotationMatrix(FRotator(0,MoveYaw,0)).GetUnitAxis(EAxis::Y),V);}}
void AFootballController::LookX(float V){if(Screen==EScreen::Game&&Saved&&Saved->CameraMode!=2){Yaw+=V*Saved->Sensitivity*1.7f;if(Avatar&&!FMath::IsNearlyZero(V)&&FMath::Abs(FMath::FindDeltaAngleDegrees(Avatar->GetActorRotation().Yaw,Yaw))>65)Avatar->BeginTurn(Yaw);}}
void AFootballController::LookY(float V){if(Screen==EScreen::Game&&Saved&&Saved->CameraMode!=2)Pitch=FMath::Clamp(Pitch+V*Saved->Sensitivity,-40.f,5.f);}
void AFootballController::GamepadLookX(float V){if(Screen==EScreen::Game&&Saved&&Saved->CameraMode!=2&&!FMath::IsNearlyZero(V)){Yaw+=V*Saved->Sensitivity*120.f*GetWorld()->GetDeltaSeconds();if(Avatar&&FMath::Abs(FMath::FindDeltaAngleDegrees(Avatar->GetActorRotation().Yaw,Yaw))>65)Avatar->BeginTurn(Yaw);}}
void AFootballController::GamepadLookY(float V){if(Screen==EScreen::Game&&Saved&&Saved->CameraMode!=2&&!FMath::IsNearlyZero(V))Pitch=FMath::Clamp(Pitch+V*Saved->Sensitivity*90.f*GetWorld()->GetDeltaSeconds(),-40.f,5.f);}
void AFootballController::SprintOn(){if(Screen!=EScreen::Game)return;Sprint=true;if(Avatar)Avatar->GetCharacterMovement()->MaxWalkSpeed=765;}
void AFootballController::SprintOff(){Sprint=false;if(!Avatar)return;auto* Mode=GetWorld()->GetAuthGameMode<AFootballMode>();if(BallControlHeld&&(!Mode||!Mode->HasDribbleControl(Avatar)))BallControlHeld=false;Avatar->GetCharacterMovement()->MaxWalkSpeed=BallControlHeld?180:510;}
void AFootballController::StartCharge(){if(Screen!=EScreen::Game||!Avatar||!Avatar->CanAct()||GetWorld()->GetTimeSeconds()<KickCooldown)return;Charging=true;ChargeStarted=GetWorld()->GetTimeSeconds();}
void AFootballController::ReleaseCharge(){if(!Charging)return;Charging=false;if(Screen==EScreen::Game)FireShot(FMath::Min(1.5f,GetWorld()->GetTimeSeconds()-ChargeStarted));}
void AFootballController::Kick(){FireShot(.5f);}
void AFootballController::FireShot(float Seconds)
{
 auto* M=GetWorld()->GetAuthGameMode<AFootballMode>();
 const EScreen ActiveScreen=Screen==EScreen::Settings?SettingsReturn:Screen;
 const bool Demo=ActiveScreen==EScreen::Main||ActiveScreen==EScreen::Credits;
 if((ActiveScreen!=EScreen::Game&&!Demo)||!Avatar||!M||!Avatar->CanAct()||!M->HasBall(Avatar))return;
 const float Now=GetWorld()->GetTimeSeconds();if(Now<KickCooldown)return;
 const FVector BallPosition=M->Ball->GetComponentLocation();
 PendingVelocity=AFootballMode::ShotVelocity(BallPosition,Avatar->GetActorForwardVector(),Seconds);
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
 const bool Left=FVector::DotProduct(BallPosition-Avatar->GetActorLocation(),Avatar->GetActorRightVector())<0;
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
void AFootballController::Reset(){if(Screen==EScreen::Game&&Avatar&&Avatar->IsMovementLocked())return;if(auto M=GetWorld()->GetAuthGameMode<AFootballMode>())M->ResetBall();}
void AFootballController::PauseToggle(){if(Screen==EScreen::Game)ChangeScreen(EScreen::Pause);else if(Screen==EScreen::Pause)ChangeScreen(EScreen::Game);else if(Screen==EScreen::Settings)ChangeScreen(SettingsReturn);else if(Screen==EScreen::Editor)BackFromEditor();else if(Screen==EScreen::Credits)ChangeScreen(EScreen::Main);}
void AFootballController::ChangeScreen(EScreen Next){const auto Old=Screen;const bool ResumeFromSettings=Old==EScreen::Settings&&Next==SettingsReturn;BallControlOff();if(Avatar){Avatar->PreviewAnimation=false;PreviewIndex=-1;}Screen=Next;Charging=false;GoalSpacePending=false;SpaceHeld=false;if((Next==EScreen::Editor||Next==EScreen::Main)&&!ResumeFromSettings){CancelPendingActions();if(Avatar){Avatar->ClearAction();Avatar->PhysicalJump=false;}GoalCelebrationUsed=true;}const bool PauseForSettings=Next==EScreen::Settings&&(SettingsReturn==EScreen::Pause||(Saved&&Saved->PauseInSettings));SetPause(Next==EScreen::Pause||PauseForSettings);SprintOff();
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
void AFootballController::Tick(float Dt){Super::Tick(Dt);UpdateActions(Dt);if(BallControlHeld){auto* Mode=GetWorld()->GetAuthGameMode<AFootballMode>();if(!Mode||!Mode->HasDribbleControl(Avatar))BallControlOff();}if(FParse::Param(FCommandLine::Get(),TEXT("ErlingTest_Latest")))RunChecks();if(!Avatar||!ViewCamera)return;if(Screen==EScreen::Settings&&IsPaused())return;const EScreen ActiveScreen=Screen==EScreen::Settings?SettingsReturn:Screen;UpdateEditorInput(Dt);if(Avatar->GetActorLocation().Z < -250){Avatar->GetCharacterMovement()->StopMovementImmediately();Avatar->SetActorLocation(FVector(-350,0,100));Reset();}if(FParse::Param(FCommandLine::Get(),TEXT("ErlingTest")))RunProjectChecks();auto P=Avatar->GetActorLocation();
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

void AFootballController::BuildUI(){if(!GEngine||!GEngine->GameViewport)return;if(UI.IsValid())GEngine->GameViewport->RemoveViewportWidgetContent(UI.ToSharedRef());
 TSharedRef<SOverlay> Root=SNew(SOverlay);TSharedRef<SVerticalBox> Content=SNew(SVerticalBox);
 auto Add=[&Content](TSharedRef<SWidget> W,float Bottom=10.f){Content->AddSlot().AutoHeight().Padding(0,0,0,Bottom)[W];};
 auto Settings=[this](){SettingsReturn=Screen;ChangeScreen(EScreen::Settings);};
 auto L=[this](const TCHAR* En,const TCHAR* Pl){return Localize(En,Pl);};
 auto AccentRule=[](){return SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(46).HeightOverride(3)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Pink)]];};
 auto SectionTitle=[&](const FString& Title,const FString& Subtitle){Add(BoldText(Title,38));Add(AccentRule(),12);if(!Subtitle.IsEmpty())Add(Text(Subtitle,17,Muted),22);};
 auto EditorRow=[this](int32 I)->TSharedRef<SWidget>{
  FString Category=LocalizeCatalogLabel(Catalog[I].Label);
  return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(PanelSoft).Padding(FMargin(14,9))
   [SNew(SHorizontalBox)
    +SHorizontalBox::Slot().FillWidth(.34f).VAlign(VAlign_Center)[Text(Category,16,Muted)]
    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SButton).ContentPadding(FMargin(12,7)).ButtonColorAndOpacity(Panel).OnClicked_Lambda([this,I](){ClickSound();Cycle(I,-1);return FReply::Handled();})[BoldText(TEXT("‹"),23,Muted)]]
    +SHorizontalBox::Slot().FillWidth(.45f).VAlign(VAlign_Center).HAlign(HAlign_Center).Padding(10,0)[SNew(STextBlock).Text_Lambda([this,I](){return OptionText(I);}).Font(FCoreStyle::GetDefaultFontStyle("Bold",17)).ColorAndOpacity(White)]
    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SButton).ContentPadding(FMargin(12,7)).ButtonColorAndOpacity(PinkSoft).OnClicked_Lambda([this,I](){ClickSound();Cycle(I,1);return FReply::Handled();})[BoldText(TEXT("›"),23,Pink)]]];
 };
 if(Screen==EScreen::Main){
  Add(BoldText(TEXT("ERLING"),50));Add(BoldText(L(TEXT("F E E L   T H E   G A M E"),TEXT("P O C Z U J   G R Ę")),15,Pink),10);Add(AccentRule(),14);Add(Text(L(TEXT("Your player. Your pitch."),TEXT("Twój zawodnik. Twoje boisko.")),18,Muted),26);
  Add(Button(L(TEXT("Play"),TEXT("Graj")),[this](){ChangeScreen(EScreen::Game);},true,TEXT("▶")),10);
  Add(Button(L(TEXT("Character Creator"),TEXT("Edytor postaci")),[this](){ChangeScreen(EScreen::Editor);},false,TEXT("●")),10);
  Add(Button(L(TEXT("Settings"),TEXT("Ustawienia")),Settings,false,TEXT("O")),10);
  Add(Button(L(TEXT("Credits"),TEXT("Twórcy")),[this](){ChangeScreen(EScreen::Credits);},false,TEXT("••")),10);
  Add(Button(L(TEXT("Quit"),TEXT("Wyjdź")),[this](){Quit();},false,TEXT("□")),0);
 }
 else if(Screen==EScreen::Editor){
  SectionTitle(L(TEXT("YOUR PLAYER"),TEXT("TWÓJ ZAWODNIK")),L(TEXT("Create your player. Your pitch. Your rules."),TEXT("Stwórz swojego zawodnika. Twoje boisko. Twoje zasady.")));
  for(int32 I=0;I<Catalog.Num();I++)Add(EditorRow(I),10);
  Add(SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(PanelSoft).Padding(FMargin(14,9))
   [SNew(SHorizontalBox)
    +SHorizontalBox::Slot().FillWidth(.34f).VAlign(VAlign_Center)[Text(L(TEXT("Animation"),TEXT("Animacja")),16,Muted)]
    +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).ContentPadding(FMargin(12,7)).ButtonColorAndOpacity(Panel).OnClicked_Lambda([this](){ClickSound();CycleAnimationPreview(-1);return FReply::Handled();})[BoldText(TEXT("‹"),23,Muted)]]
    +SHorizontalBox::Slot().FillWidth(.45f).VAlign(VAlign_Center).HAlign(HAlign_Center).Padding(10,0)[SNew(STextBlock).Text_Lambda([this](){return PreviewAnimationText();}).Font(FCoreStyle::GetDefaultFontStyle("Regular",14)).ColorAndOpacity(White)]
    +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).ContentPadding(FMargin(12,7)).ButtonColorAndOpacity(PinkSoft).OnClicked_Lambda([this](){ClickSound();CycleAnimationPreview(1);return FReply::Handled();})[BoldText(TEXT("›"),23,Pink)]]],0);
  Root->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(FMargin(54,0,0,42))[SNew(SBox).WidthOverride(250)[Button(L(TEXT("Back"),TEXT("Wstecz")),[this](){BackFromEditor();},false,TEXT("←"))]];
  Root->AddSlot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(0,0,54,42))[SNew(SBox).WidthOverride(310)[Button(L(TEXT("Save Appearance"),TEXT("Zapisz wygląd")),[this](){SaveAppearance();},true,TEXT("✓"))]];
 }
 else if(Screen==EScreen::Pause){SectionTitle(L(TEXT("PAUSED"),TEXT("PAUZA")),L(TEXT("The match is waiting for your return."),TEXT("Mecz czeka na Twój powrót.")));Add(Button(L(TEXT("Resume"),TEXT("Wznów")),[this](){ChangeScreen(EScreen::Game);},true,TEXT("▶")));Add(Button(L(TEXT("Settings"),TEXT("Ustawienia")),Settings,false,TEXT("O")));Add(Button(L(TEXT("Main Menu"),TEXT("Menu główne")),[this](){ChangeScreen(EScreen::Main);},false,TEXT("←")));Add(Button(L(TEXT("Quit Game"),TEXT("Wyjdź z gry")),[this](){Quit();},false,TEXT("□")));}
 else if(Screen==EScreen::Credits){SectionTitle(L(TEXT("CREDITS"),TEXT("TWÓRCY")),L(TEXT("Project created by"),TEXT("Projekt stworzony przez")));Add(BoldText(TEXT("Daniel Maciej Pytel"),22,Pink));Add(Text(TEXT("2026"),16,Muted),18);
  auto Link=[](const TCHAR* Label,const TCHAR* Url){return SNew(SBox).MinDesiredHeight(52)[SNew(SButton).ContentPadding(FMargin(14,9)).ButtonColorAndOpacity(PanelSoft).OnClicked_Lambda([Url](){ClickSound();FPlatformProcess::LaunchURL(Url,nullptr,nullptr);return FReply::Handled();})[BoldText(Label,13,White)]];};
  Add(SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1).Padding(0,0,8,0)[Link(TEXT("ArtStation"),TEXT("https://www.artstation.com/danielmaciejpytel"))]+SHorizontalBox::Slot().FillWidth(1).Padding(0,0,8,0)[Link(TEXT("GitHub"),TEXT("https://github.com/danielmaciejpytel"))]+SHorizontalBox::Slot().FillWidth(1)[Link(TEXT("LinkedIn"),TEXT("https://www.linkedin.com/in/danielmaciejpytel/"))]);Add(Button(L(TEXT("Back"),TEXT("Wstecz")),[this](){ChangeScreen(EScreen::Main);},false,TEXT("←")));}
 else if(Screen==EScreen::Settings){SectionTitle(L(TEXT("SETTINGS"),TEXT("USTAWIENIA")),L(TEXT("Make the game yours."),TEXT("Dopasuj grę do siebie.")));
  Add(Button(FString::Printf(TEXT("%s: %s"),*L(TEXT("Language"),TEXT("Język")),Saved->Language==1?TEXT("Polski"):TEXT("English")),[this](){Saved->Language=Saved->Language==1?0:1;UGameplayStatics::SaveGameToSlot(Saved,ProfileSlot(),0);BuildUI();},false,TEXT("文")),12);
  Add(Text(L(TEXT("Music Volume"),TEXT("Głośność muzyki")),16,Muted),5);Add(SNew(SSlider).Value(Saved->Volume).OnValueChanged_Lambda([this](float V){UpdateMusicVolume(V);}),14);
  Add(Text(L(TEXT("Effects Volume"),TEXT("Głośność efektów")),16,Muted),5);Add(SNew(SSlider).Value(Saved->EffectsVolume).OnValueChanged_Lambda([this](float V){UpdateEffectsVolume(V);}),14);
  Add(Text(L(TEXT("Mouse Sensitivity"),TEXT("Czułość myszy")),16,Muted),5);Add(SNew(SSlider).Value((Saved->Sensitivity-.3f)/2.7f).OnValueChanged_Lambda([this](float V){Saved->Sensitivity=.3f+V*2.7f;}),16);
  const FString CameraLabel=Saved->CameraMode==1?L(TEXT("Reduced"),TEXT("Ograniczony")):Saved->CameraMode==2?L(TEXT("Pitch"),TEXT("Boiskowy")):L(TEXT("Smooth"),TEXT("Płynny"));
 Add(Button(FString::Printf(TEXT("%s: %s"),*L(TEXT("Camera Movement"),TEXT("Ruch kamery")),*CameraLabel),[this](){Saved->CameraMode=(Saved->CameraMode+1)%3;Saved->ReducedMotion=Saved->CameraMode==1;CameraPivotInitialized=false;PitchCameraLeadX=0;BuildUI();},false,TEXT("◉")));
 Add(Button(FString::Printf(TEXT("%s: %d / 4"),*L(TEXT("Graphics Quality"),TEXT("Jakość grafiki")),Saved->Quality+1),[this](){Saved->Quality=(Saved->Quality+1)%4;ApplyQuality();BuildUI();},false,TEXT("▣")));
 Add(Button(FString::Printf(TEXT("%s: %s"),*L(TEXT("Pause in settings"),TEXT("Pauza w ustawieniach")),Saved->PauseInSettings?*L(TEXT("On"),TEXT("Wł.")):*L(TEXT("Off"),TEXT("Wył."))),[this](){Saved->PauseInSettings=!Saved->PauseInSettings;const bool ShouldPause=SettingsReturn==EScreen::Pause||Saved->PauseInSettings;SetPause(ShouldPause);BuildUI();},false,TEXT("Ⅱ")));
 Add(Button(L(TEXT("Save and Back"),TEXT("Zapisz i wróć")),[this](){SaveSettings();ChangeScreen(SettingsReturn);},true,TEXT("✓")),0);
 }
 if(Screen!=EScreen::Game){
  const float Width=Screen==EScreen::Editor?650.f:520.f;
  Root->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(FMargin(64,32,0,88))[SNew(SBox).WidthOverride(Width).MaxDesiredHeight(830)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Ink).Padding(FMargin(Screen==EScreen::Editor?36:42,32))[SNew(SScrollBox)+SScrollBox::Slot()[Content]]]];
 }
 else{
  Root->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(0,28,0,0))[SNew(SBox).WidthOverride(300).MinDesiredHeight(72)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Ink).Padding(FMargin(22,12))[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).HAlign(HAlign_Right).Padding(0,0,16,0)[BoldText(L(TEXT("GOALS"),TEXT("GOLE")),24,White)]
   +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Text_Lambda([this](){auto M=GetWorld()->GetAuthGameMode<AFootballMode>();return FText::FromString(FString::Printf(TEXT("%02d"),M?M->Goals:0));}).Font(FCoreStyle::GetDefaultFontStyle("Bold",30)).ColorAndOpacity(Pink)]]]];
  Root->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(18,0,18,24))[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.015f,.05f,.045f,.72f)).Padding(FMargin(18,10))[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().AutoWidth()[KeyCap(TEXT("WASD"),L(TEXT("MOVE"),TEXT("RUCH")))]
   +SHorizontalBox::Slot().AutoWidth()[KeyCap(TEXT("CTRL"),L(TEXT("BALL CONTROL"),TEXT("KONTROLA PIŁKI")))]
   +SHorizontalBox::Slot().AutoWidth()[KeyCap(TEXT("SHIFT"),L(TEXT("SPRINT"),TEXT("SPRINT")))]
   +SHorizontalBox::Slot().AutoWidth()[KeyCap(TEXT("SPACE"),L(TEXT("JUMP"),TEXT("SKOK")))]
   +SHorizontalBox::Slot().AutoWidth()[KeyCap(TEXT("LMB"),L(TEXT("HOLD / RELEASE SHOT"),TEXT("PRZYTRZYMAJ / PUŚĆ STRZAŁ")))]
   +SHorizontalBox::Slot().AutoWidth()[KeyCap(TEXT("RMB"),L(TEXT("SLIDE"),TEXT("WŚLIZG")))]
   +SHorizontalBox::Slot().AutoWidth()[KeyCap(TEXT("R"),L(TEXT("RESET BALL"),TEXT("RESET PIŁKI")))]
   +SHorizontalBox::Slot().AutoWidth()[KeyCap(TEXT("ESC"),L(TEXT("PAUSE"),TEXT("PAUZA")))]]];
 }
 Root->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0,112,0,0)[SNew(STextBlock).Text_Lambda([this](){if(Screen==EScreen::Game&&Charging){float C=FMath::Min(1.5f,GetWorld()->GetTimeSeconds()-ChargeStarted);const FString State=C>=1.35f?Localize(TEXT("OVER THE BAR!"),TEXT("ZA WYSOKO!")):C>=.85f&&C<=1.02f?Localize(TEXT("UNDER THE BAR"),TEXT("POD POPRZECZKĘ")):C>=.35f?Localize(TEXT("POWER SHOT"),TEXT("MOCNY STRZAŁ")):Localize(TEXT("PASS"),TEXT("PODANIE"));return FText::FromString(FString::Printf(TEXT("%s  %.1f s  ·  %s"),*Localize(TEXT("SHOT"),TEXT("STRZAŁ")),C,*State));}if(Screen==EScreen::Game&&!GoalCelebrationUsed&&GetWorld()->GetTimeSeconds()<GoalUntil)return FText::FromString(FString::Printf(TEXT("%s  ·  %.0f s"),*Localize(TEXT("SPACE: hop  |  hold 1 s: celebrate"),TEXT("SPACJA: skok  |  przytrzymaj 1 s: cieszynka")),FMath::CeilToFloat(GoalUntil-GetWorld()->GetTimeSeconds())));return FText::FromString(GetWorld()->GetTimeSeconds()<ToastUntil?Toast:TEXT(""));}).Font(FCoreStyle::GetDefaultFontStyle("Bold",21)).ColorAndOpacity(Pink)];
 UI=Root;GEngine->GameViewport->AddViewportWidgetContent(UI.ToSharedRef(),10);
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
 case 16:Check(TEXT("sprint_dribble_controlled"),Mode->Ball->GetPhysicsLinearVelocity().Size()<1000&&Avatar->GetVelocity().Size2D()>700&&Mode->Ball->GetComponentLocation().Z<60&&FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetActorLocation())<175);GoalCelebrationUsed=true;JumpPressed();break;
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
 case 26:ChangeScreen(EScreen::Main);DemoDirection=1;DemoPhase=0;Mode->ResetBall();Avatar->GetCharacterMovement()->StopMovementImmediately();Avatar->SetActorRotation(FRotator::ZeroRotator);Avatar->SetActorLocation(FVector(-350,0,98));break;
 case 27:Shot(TEXT("12_Demo_Right"));break;
 case 28:Check(TEXT("demo_alternates"),DemoReturned);Check(TEXT("demo_faces_shot"),DemoShotFacedGoal);Shot(TEXT("13_Demo_Left"));break;
 case 29:ChangeScreen(EScreen::Game);Mode->ResetBall();Mode->Scored=true;Mode->Ball->SetWorldLocation(FVector(2800,0,160),false,nullptr,ETeleportType::TeleportPhysics);Mode->Ball->SetPhysicsLinearVelocity(FVector(3200,0,0));break;
 case 30:Check(TEXT("net_stops_ball"),Mode->Ball->GetComponentLocation().X<2980&&Mode->Ball->GetComponentLocation().X>2800&&Mode->Ball->GetPhysicsLinearVelocity().Size2D()<100);Check(TEXT("net_ball_lands"),Mode->Ball->GetComponentLocation().Z<40);Check(TEXT("all_effects_loaded"),Effects.Num()==5&&Effects.FindRef(TEXT("jump"))&&Effects.FindRef(TEXT("kick"))&&Effects.FindRef(TEXT("fail"))&&Effects.FindRef(TEXT("click")));UpdateEffectsVolume(.5f);PlayEffect(TEXT("goal"),1.5f);Check(TEXT("goal_gain_150_percent"),!ActiveEffects.IsEmpty()&&FMath::IsNearlyEqual(ActiveEffects.Last()->VolumeMultiplier,.75f));UpdateEffectsVolume(0);Check(TEXT("effects_mute"),!ActiveEffects.IsEmpty()&&ActiveEffects.Last()->VolumeMultiplier==0);Check(TEXT("effects_do_not_mute_music"),Music&&Music->VolumeMultiplier>0&&Music->IsPlaying());UpdateEffectsVolume(.8f);SaveSettings();{auto Loaded=Cast<UFootballSave>(UGameplayStatics::LoadGameFromSlot(ProfileSlot(),0));Check(TEXT("effects_volume_saved"),Loaded&&FMath::IsNearlyEqual(Loaded->EffectsVolume,.8f));}SettingsReturn=EScreen::Game;ChangeScreen(EScreen::Settings);break;
 case 31:Shot(TEXT("14_Settings_Effects"));break;
 default:FFileHelper::SaveStringToFile(TestReport,*(FPaths::ProjectSavedDir()/TEXT("runtime_checks.txt")));FPlatformMisc::RequestExit(false);break;
 }TestStage++;TestStarted=FPlatformTime::Seconds()+((TestStage==14||TestStage==5)?.35:2.0);
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
 if(Down&&!MouseWasDown){FVector Center=Avatar->GetActorLocation()+FVector(0,0,2);FVector2D Lo(1e9,1e9),Hi(-1e9,-1e9);
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
 if(Avatar->CanAct()&&DemoPhase==0&&To.X*DemoDirection>70&&To.X*DemoDirection<210&&FMath::Abs(To.Y)<80&&Avatar->GetActorForwardVector().X*DemoDirection>.95f){
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
  M->Ball->SetPhysicsLinearVelocity(BallVelocity+Right*(SideError*8.f-FVector::DotProduct(BallVelocity,Right)));
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
