#include "Football.h"
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
static const FLinearColor Ink(.015f,.025f,.045f,.96f),Mint(.27f,.9f,.68f),White(.94f,.97f,1);
static const TCHAR* ProfileSlot(){return FParse::Param(FCommandLine::Get(),TEXT("ErlingTest"))?TEXT("ErlingProfile_Test"):TEXT("ErlingProfile");}
static TSharedRef<STextBlock> Text(const FString& Value,int32 Size=20,FLinearColor Color=White) {
 return SNew(STextBlock).Text(FText::FromString(Value)).Font(FCoreStyle::GetDefaultFontStyle("Regular",Size)).ColorAndOpacity(Color);
}
static TSharedRef<SWidget> Button(const FString& Label,TFunction<void()> Click,bool Primary=false) {
 return SNew(SBox).MinDesiredHeight(48)[SNew(SButton).ContentPadding(FMargin(18,10)).ButtonColorAndOpacity(Primary?FLinearColor(.06f,.24f,.19f):FLinearColor(.075f,.11f,.16f)).OnClicked_Lambda([Click](){ClickSound();Click();return FReply::Handled();})[Text(Label,19,Primary?Mint:White)]];
}

AFootballPlayer::AFootballPlayer() {
 PrimaryActorTick.bCanEverTick=true; GetCapsuleComponent()->InitCapsuleSize(55,96);
 GetMesh()->SetRelativeLocation(FVector(0,0,-96));GetMesh()->SetRelativeRotation(FRotator(0,-90,0));GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 GetMesh()->SetForcedLOD(1);GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 Face=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Face"));Face->SetupAttachment(GetMesh());Face->SetRelativeLocation(FVector(0,.065871f,0));Face->SetCastShadow(false);Face->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 GetCharacterMovement()->MaxWalkSpeed=510;GetCharacterMovement()->bEnablePhysicsInteraction=false;GetCharacterMovement()->JumpZVelocity=420;GetCharacterMovement()->GravityScale=1.4f;GetCharacterMovement()->MaxAcceleration=1400;GetCharacterMovement()->BrakingDecelerationWalking=1800;GetCharacterMovement()->bOrientRotationToMovement=true;GetCharacterMovement()->RotationRate=FRotator(0,520,0);bUseControllerRotationYaw=false;
}
void AFootballPlayer::BeginPlay() { Super::BeginPlay();InitializeAssets(); }
void AFootballPlayer::InitializeAssets() {
 if(GetMesh()->GetSkeletalMeshAsset())return;
 GetMesh()->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Erling/Meshes/SK_body.SK_body")));
 Face->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Erling/Meshes/SK_face.SK_face")));Face->SetLeaderPoseComponent(GetMesh(),true);Face->SetForcedLOD(1);
 if(auto M=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Erling/Materials/M_Face.M_Face"))){FaceMaterial=UMaterialInstanceDynamic::Create(M,this);Face->SetMaterial(0,FaceMaterial);}
 for(auto N:{TEXT("Idle_Breathe"),TEXT("Idle_LookAround"),TEXT("Run_InPlace"),TEXT("Walk_InPlace"),TEXT("Kick_Standing"),TEXT("Jump")}) {
  FString Path=FString::Printf(TEXT("/Game/Erling/Animations/AN_%s.AN_%s"),N,N);if(auto A=LoadObject<UAnimSequence>(nullptr,*Path))Animations.Add(N,A);
 }
 SetAnimation(TEXT("Idle_Breathe"));
}
void AFootballPlayer::SetAnimation(const FString& Name,bool Loop) {
 if(CurrentAnimation==Name)return;if(auto A=Animations.FindRef(Name)){CurrentAnimation=Name;GetMesh()->PlayAnimation(A,Loop);}
}
void AFootballPlayer::Tick(float Dt) {
 Super::Tick(Dt);if(GetWorld()->GetTimeSeconds()<KickUntil)return;
 if(GetCharacterMovement()->IsFalling()){SetAnimation(TEXT("Jump"),false);return;}const float Speed=GetVelocity().Size2D();SetAnimation(Speed>15?TEXT("Run_InPlace"):TEXT("Idle_Breathe"));if(auto Instance=GetMesh()->GetSingleNodeInstance())Instance->SetPlayRate(Speed>600?1.5f:1.f);
}
void AFootballPlayer::ApplyKit(const TArray<int32>& Values,const TArray<FKitCategory>& Cats) {
 for(auto P:Pieces)if(P)P->DestroyComponent();Pieces.Empty();
 for(int32 C=0;C<Cats.Num();C++) {
  if(!Values.IsValidIndex(C)||!Cats[C].Options.IsValidIndex(Values[C]))continue;const auto& O=Cats[C].Options[Values[C]];
  if(C==1){if(FaceMaterial&&!O.Texture.IsEmpty())FaceMaterial->SetTextureParameterValue(TEXT("FaceTexture"),LoadObject<UTexture2D>(nullptr,*O.Texture));continue;}
  for(const auto& Path:O.Meshes)if(auto M=LoadObject<USkeletalMesh>(nullptr,*Path)){
   auto P=NewObject<USkeletalMeshComponent>(this);P->SetupAttachment(GetMesh());P->SetSkeletalMesh(M);P->SetCollisionEnabled(ECollisionEnabled::NoCollision);P->SetLeaderPoseComponent(GetMesh(),true);P->SetForcedLOD(1);P->RegisterComponent();Pieces.Add(P);
  }
 }
}
AFootballMode::AFootballMode(){PrimaryActorTick.bCanEverTick=true;DefaultPawnClass=AFootballPlayer::StaticClass();PlayerControllerClass=AFootballController::StaticClass();}
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
void AFootballMode::ResetBall(){if(!Ball)return;BallHidden=false;ShotInFlight=false;Ball->SetVisibility(true);Ball->SetSimulatePhysics(true);Ball->SetLinearDamping(.3f);Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);Ball->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);Ball->SetWorldLocation(FVector(0,0,28),false,nullptr,ETeleportType::TeleportPhysics);Scored=false;ResetAt=0;PreviousBall=Ball->GetComponentLocation();}
FVector AFootballMode::ShotVelocity(const FVector& Position,const FVector& Direction,float Seconds){
 FVector F=Direction.GetSafeNormal2D();float C=FMath::Clamp(Seconds*2.f,0.f,3.f);float Speed=C<=1?FMath::Lerp(850.f,2200.f,C):C<=2?FMath::Lerp(2200.f,2800.f,C-1):FMath::Lerp(2800.f,3200.f,C-2);
 if(C<.15f)return F*Speed;
 float GoalHeight=C<=1?FMath::Lerp(22.f,130.f,C):C<=2?FMath::Lerp(130.f,220.f,C-1):FMath::Lerp(220.f,420.f,C-2);
 float Distance=FMath::Abs(F.X)>.15f?FMath::Abs(((F.X>=0?2740.f:-2740.f)-Position.X)/F.X):1800.f;Distance=FMath::Clamp(Distance,200.f,5000.f);
 float T=Distance/Speed;float Z=FMath::Clamp((GoalHeight-Position.Z+.5f*980*T*T)/T,0.f,1300.f);return F*Speed+FVector(0,0,Z);
}
void AFootballMode::Kick(AFootballPlayer* P,float Seconds){if(!Ball||!P||BallHidden||Scored||FVector::Dist2D(Ball->GetComponentLocation(),P->GetActorLocation())>240)return;LastShot=GetWorld()->GetTimeSeconds();ShotInFlight=true;Ball->SetLinearDamping(.015f);Ball->SetPhysicsLinearVelocity(ShotVelocity(Ball->GetComponentLocation(),P->GetActorForwardVector(),Seconds));if(auto PC=Cast<AFootballController>(GetWorld()->GetFirstPlayerController()))PC->PlayEffect(TEXT("kick"));}
void AFootballMode::HideMiss(){if(BallHidden)return;if(auto PC=Cast<AFootballController>(GetWorld()->GetFirstPlayerController()))PC->PlayEffect(TEXT("fail"));BallHidden=true;Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);Ball->SetVisibility(false);Ball->SetSimulatePhysics(false);ResetAt=GetWorld()->GetTimeSeconds()+1.2f;}
void AFootballMode::Dribble(AFootballPlayer* Player,float Dt){
 if(!Player||Scored||BallHidden||GetWorld()->GetTimeSeconds()-LastShot<.85f||Player->GetCharacterMovement()->IsFalling())return;
 const FVector Pos=Ball->GetComponentLocation(),Base=Player->GetActorLocation();FVector To=Pos-Base;To.Z=0;
 const float Speed=Player->GetVelocity().Size2D();if(Speed<25||To.Size()>200||Pos.Z>Base.Z-35)return;
 const FVector Direction=Player->GetVelocity().GetSafeNormal2D();if(FVector::DotProduct(To.GetSafeNormal(),Direction)<-.25f)return;
 ShotInFlight=false;Ball->SetLinearDamping(.3f);const FVector Target=Base+Direction*(110+Speed*.035f);FVector Correction=Target-Pos;Correction.Z=0;
 FVector Desired=Player->GetVelocity()+Correction*8;Desired.Z=0;Desired=Desired.GetClampedToMaxSize(Speed+220);
 const FVector Current=Ball->GetPhysicsLinearVelocity();FVector V=FMath::VInterpTo(Current,Desired,Dt,18);V.Z=FMath::Clamp(Current.Z,-180.f,30.f);
 Ball->SetPhysicsLinearVelocity(V);Ball->SetPhysicsAngularVelocityInRadians(FVector(-V.Y/22,V.X/22,0));
}
void AFootballMode::Tick(float Dt){Super::Tick(Dt);if(!Ball)return;auto PC=Cast<AFootballController>(GetWorld()->GetFirstPlayerController());if(!PC||PC->Screen!=AFootballController::EScreen::Game)return;
 Dribble(PC->Avatar,Dt);auto P=Ball->GetComponentLocation();const FVector OldBall=PreviousBall;PreviousBall=P;float T=GetWorld()->GetTimeSeconds();if(ResetAt>0&&T>=ResetAt){ResetBall();return;}float CrossingY=P.Y,CrossingZ=P.Z;const float Plane=P.X>=0?2762.f:-2762.f;const bool Crossed=FMath::Abs(P.X)>2762&&FMath::Abs(OldBall.X)<=2762;if(Crossed&&!FMath::IsNearlyEqual(P.X,OldBall.X)){const FVector Crossing=FMath::Lerp(OldBall,P,(Plane-OldBall.X)/(P.X-OldBall.X));CrossingY=Crossing.Y;CrossingZ=Crossing.Z;}if(!Scored&&Crossed&&FMath::Abs(CrossingY)<320&&CrossingZ<238&&CrossingZ>0){Goals++;PC->PlayEffect(TEXT("goal"),1.5f);Scored=true;ResetAt=T+1.5f;PC->Toast=TEXT("GOOOL!  +1");PC->ToastUntil=T+2;}
 if(!Scored&&ShotInFlight&&!BallHidden&&((FMath::Abs(P.X)>2762)||(FMath::Abs(P.X)>2720&&(FMath::Abs(P.Y)>320||P.Z>238))||FMath::Abs(P.Y)>1750))HideMiss();if(P.Z<-200)ResetBall();}

AFootballController::AFootballController(){PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.bTickEvenWhenPaused=true;bAutoManageActiveCameraTarget=false;}
void AFootballController::LoadCatalog(){FString S; if(!FFileHelper::LoadFileToString(S,*(FPaths::ProjectContentDir()/TEXT("Data/wardrobe.json"))))return;TSharedPtr<FJsonObject> Root;auto Reader=TJsonReaderFactory<>::Create(S);if(!FJsonSerializer::Deserialize(Reader,Root)||!Root)return;
 for(auto CV:Root->GetArrayField(TEXT("categories"))){auto CO=CV->AsObject();FKitCategory C;C.Label=CO->GetStringField(TEXT("label"));for(auto OV:CO->GetArrayField(TEXT("options"))){auto OO=OV->AsObject();FKitOption O;O.Label=OO->GetStringField(TEXT("label"));OO->TryGetStringField(TEXT("texture"),O.Texture);const TArray<TSharedPtr<FJsonValue>>* Arr;if(OO->TryGetArrayField(TEXT("meshes"),Arr))for(auto V:*Arr)O.Meshes.Add(V->AsString());C.Options.Add(O);}Catalog.Add(C);}}
void AFootballController::BeginPlay(){Super::BeginPlay();Avatar=Cast<AFootballPlayer>(GetPawn());if(!Avatar){Avatar=GetWorld()->SpawnActor<AFootballPlayer>(FVector(-350,0,100),FRotator::ZeroRotator);Possess(Avatar);}AudioController=this;for(auto N:{TEXT("goal"),TEXT("jump"),TEXT("kick"),TEXT("fail"),TEXT("click")})Effects.Add(N,LoadObject<USoundBase>(nullptr,*FString::Printf(TEXT("/Game/Erling/Audio/%s.%s"),N,N)));Avatar->InitializeAssets();Avatar->SetActorLocation(FVector(-350,0,100));LoadCatalog();Saved=Cast<UFootballSave>(UGameplayStatics::LoadGameFromSlot(ProfileSlot(),0));if(!Saved)Saved=Cast<UFootballSave>(UGameplayStatics::CreateSaveGameObject(UFootballSave::StaticClass()));Selection=Saved->Appearance;
 Selection.SetNum(5);for(int I=0;I<Catalog.Num()&&I<5;I++)Selection[I]=FMath::Clamp(Selection[I],0,FMath::Max(0,Catalog[I].Options.Num()-1));Avatar->ApplyKit(Selection,Catalog);ViewCamera=GetWorld()->SpawnActor<ACameraActor>();ViewCamera->GetCameraComponent()->FieldOfView=48;SetViewTarget(ViewCamera);ChangeScreen(EScreen::Main);
 Music=NewObject<UAudioComponent>(this);Music->bIsUISound=true;Music->bAllowSpatialization=false;Music->bAutoActivate=false;Music->bAutoDestroy=false;Music->RegisterComponent();
 if(auto Track=LoadObject<USoundWave>(nullptr,TEXT("/Game/Erling/Audio/background.background"))){Track->bLooping=true;Music->SetSound(Track);Music->SetVolumeMultiplier(Saved->Volume);Music->Play();}ApplyQuality();}
void AFootballController::SetupInputComponent(){Super::SetupInputComponent();InputComponent->BindAction(TEXT("Jump"),IE_Pressed,this,&AFootballController::JumpPressed);InputComponent->BindAction(TEXT("Jump"),IE_Released,this,&AFootballController::JumpReleased);InputComponent->BindAxis(TEXT("EditorTurn"),this,&AFootballController::TurnEditor);InputComponent->BindAxis(TEXT("EditorZoom"),this,&AFootballController::ZoomEditor);InputComponent->BindAxis(TEXT("Forward"),this,&AFootballController::Forward);InputComponent->BindAxis(TEXT("Right"),this,&AFootballController::Right);InputComponent->BindAxis(TEXT("LookX"),this,&AFootballController::LookX);InputComponent->BindAxis(TEXT("LookY"),this,&AFootballController::LookY);InputComponent->BindAction(TEXT("Sprint"),IE_Pressed,this,&AFootballController::SprintOn);InputComponent->BindAction(TEXT("Sprint"),IE_Released,this,&AFootballController::SprintOff);InputComponent->BindAction(TEXT("Kick"),IE_Pressed,this,&AFootballController::StartCharge);InputComponent->BindAction(TEXT("Kick"),IE_Released,this,&AFootballController::ReleaseCharge);InputComponent->BindAction(TEXT("ResetBall"),IE_Pressed,this,&AFootballController::Reset);auto& B=InputComponent->BindAction(TEXT("Pause"),IE_Pressed,this,&AFootballController::PauseToggle);B.bExecuteWhenPaused=true;}
void AFootballController::Forward(float V){if(Screen==EScreen::Game&&Avatar)Avatar->AddMovementInput(FRotator(0,Yaw,0).Vector(),V);}
void AFootballController::Right(float V){if(Screen==EScreen::Game&&Avatar)Avatar->AddMovementInput(FRotationMatrix(FRotator(0,Yaw,0)).GetUnitAxis(EAxis::Y),V);}
void AFootballController::LookX(float V){if(Screen==EScreen::Game)Yaw+=V*Saved->Sensitivity*1.7f;}
void AFootballController::LookY(float V){if(Screen==EScreen::Game)Pitch=FMath::Clamp(Pitch-V*Saved->Sensitivity,-40.f,5.f);}
void AFootballController::SprintOn(){if(Screen!=EScreen::Game)return;Sprint=true;if(Avatar)Avatar->GetCharacterMovement()->MaxWalkSpeed=765;}
void AFootballController::SprintOff(){Sprint=false;if(Avatar)Avatar->GetCharacterMovement()->MaxWalkSpeed=510;}
void AFootballController::StartCharge(){if(Screen!=EScreen::Game||GetWorld()->GetTimeSeconds()<KickCooldown)return;Charging=true;ChargeStarted=GetWorld()->GetTimeSeconds();}
void AFootballController::ReleaseCharge(){if(!Charging)return;Charging=false;if(Screen==EScreen::Game)FireShot(FMath::Min(1.5f,GetWorld()->GetTimeSeconds()-ChargeStarted));}
void AFootballController::Kick(){FireShot(.5f);}
void AFootballController::FireShot(float Seconds){if(Screen!=EScreen::Game||!Avatar)return;float T=GetWorld()->GetTimeSeconds();if(T<KickCooldown)return;KickCooldown=T+.9f;Avatar->KickUntil=T+1.2f;Avatar->CurrentAnimation.Empty();Avatar->SetAnimation(TEXT("Kick_Standing"),false);FTimerHandle H;GetWorldTimerManager().SetTimer(H,[this,Seconds](){if(Screen==EScreen::Game)if(auto M=GetWorld()->GetAuthGameMode<AFootballMode>())M->Kick(Avatar,Seconds);},.15f,false);}
void AFootballController::Reset(){if(auto M=GetWorld()->GetAuthGameMode<AFootballMode>())M->ResetBall();}
void AFootballController::PauseToggle(){if(Screen==EScreen::Game)ChangeScreen(EScreen::Pause);else if(Screen==EScreen::Pause)ChangeScreen(EScreen::Game);else if(Screen==EScreen::Settings)ChangeScreen(SettingsReturn);else if(Screen==EScreen::Editor)BackFromEditor();else if(Screen==EScreen::Credits)ChangeScreen(EScreen::Main);}
void AFootballController::ChangeScreen(EScreen Next){const auto Old=Screen;Screen=Next;Charging=false;SetPause(Next==EScreen::Pause||(Next==EScreen::Settings&&SettingsReturn==EScreen::Pause));SprintOff();
 if(Next==EScreen::Editor){EditorZoom=0;Dragging=false;DraftBefore=Selection;Avatar->GetCharacterMovement()->StopMovementImmediately();Avatar->SetActorLocation(FVector(-350,0,100));Avatar->SetActorRotation(FRotator(0,-35,0));Avatar->KickUntil=0;Avatar->SetAnimation(TEXT("Idle_Breathe"));Reset();}
 if(Next==EScreen::Game&&Old!=EScreen::Pause){Avatar->SetActorLocation(FVector(-350,0,100));Avatar->SetActorRotation(FRotator::ZeroRotator);Yaw=0;Pitch=-15;KickCooldown=0;Reset();}
 if(Next==EScreen::Main&&Old!=EScreen::Credits){const float X=Avatar->GetActorLocation().X;DemoPhase=FMath::Abs(X)<240?1:0;DemoDirection=DemoPhase==1?(X>=0?1:-1):(X>0?-1:1);Reset();}bShowMouseCursor=Next!=EScreen::Game;if(bShowMouseCursor){FInputModeGameAndUI Mode;Mode.SetHideCursorDuringCapture(false);SetInputMode(Mode);}else{FInputModeGameOnly Mode;SetInputMode(Mode);}BuildUI();}
void AFootballController::Cycle(int32 C,int32 Direction){if(!Catalog.IsValidIndex(C)||Catalog[C].Options.IsEmpty())return;Selection[C]=(Selection[C]+Direction+Catalog[C].Options.Num())%Catalog[C].Options.Num();Avatar->ApplyKit(Selection,Catalog);Toast.Empty();}
void AFootballController::SaveAppearance(){Saved->Appearance=Selection;const bool Ok=UGameplayStatics::SaveGameToSlot(Saved,ProfileSlot(),0);if(Ok)DraftBefore=Selection;Toast=Ok?TEXT("Wygląd zapisany"):TEXT("Nie udało się zapisać wyglądu");ToastUntil=GetWorld()->GetTimeSeconds()+4;}
void AFootballController::BackFromEditor(){Selection=DraftBefore;Avatar->ApplyKit(Selection,Catalog);ChangeScreen(EScreen::Main);}
void AFootballController::SaveSettings(){UGameplayStatics::SaveGameToSlot(Saved,ProfileSlot(),0);ApplyQuality();}
void AFootballController::Quit(){UKismetSystemLibrary::QuitGame(this,this,EQuitPreference::Quit,false);}
FText AFootballController::OptionText(int32 I)const{if(Catalog.IsValidIndex(I)&&Selection.IsValidIndex(I)&&Catalog[I].Options.IsValidIndex(Selection[I]))return FText::FromString(Catalog[I].Options[Selection[I]].Label);return FText::FromString(TEXT("Brak elementów"));}
void AFootballController::Tick(float Dt){Super::Tick(Dt);if(!Avatar||!ViewCamera)return;UpdateEditorInput(Dt);if(Avatar->GetActorLocation().Z < -250){Avatar->GetCharacterMovement()->StopMovementImmediately();Avatar->SetActorLocation(FVector(-350,0,100));Reset();}if(FParse::Param(FCommandLine::Get(),TEXT("ErlingTest")))RunProjectChecks();auto P=Avatar->GetActorLocation();
 if(Screen==EScreen::Main||Screen==EScreen::Credits)UpdateDemo(Dt);
 FVector Cam,Target;float Fov=48;
 if(Screen==EScreen::Game||Screen==EScreen::Pause||(Screen==EScreen::Settings&&SettingsReturn==EScreen::Pause)){Target=P+FRotator(0,Yaw,0).Vector()*350+FVector(0,0,40);Cam=P-FRotator(Pitch,Yaw,0).Vector()*750+FVector(0,0,300);Fov=65;}
 else{bool Close=Screen==EScreen::Editor;Target=P+FVector(0,0,0);if(Close)Fov=FMath::Lerp(48.f,34.f,EditorZoom);FVector Side=FVector(.65f,.76f,0);Target+=Side*(Close?170*Fov/48:220);Cam=P+FVector(Close?470:760,Close?-620:-1000,Close?190:310);}
 float Speed=Saved->ReducedMotion?10000:4;auto New=FMath::VInterpTo(ViewCamera->GetActorLocation(),Cam,Dt,Speed);auto R=FMath::RInterpTo(ViewCamera->GetActorRotation(),(Target-New).Rotation(),Dt,Speed);ViewCamera->SetActorLocationAndRotation(New,R);ViewCamera->GetCameraComponent()->SetFieldOfView(FMath::FInterpTo(ViewCamera->GetCameraComponent()->FieldOfView,Fov,Dt,Speed));}

void AFootballController::BuildUI(){if(!GEngine||!GEngine->GameViewport)return;if(UI.IsValid())GEngine->GameViewport->RemoveViewportWidgetContent(UI.ToSharedRef());
 TSharedRef<SOverlay> Root=SNew(SOverlay);TSharedRef<SVerticalBox> Panel=SNew(SVerticalBox);
 auto Add=[&Panel](TSharedRef<SWidget> W){Panel->AddSlot().AutoHeight().Padding(0,0,0,12)[W];};
 auto Settings=[this](){SettingsReturn=Screen;ChangeScreen(EScreen::Settings);};
 if(Screen==EScreen::Main){Add(Text(TEXT("ERLING"),44));Add(Text(TEXT("FOOTBALL CLUB"),16,Mint));Add(Text(TEXT("Twój zawodnik. Twoje boisko."),18));Add(Button(TEXT("Graj"),[this](){ChangeScreen(EScreen::Game);},true));Add(Button(TEXT("Edytor postaci"),[this](){ChangeScreen(EScreen::Editor);}));Add(Button(TEXT("Ustawienia"),Settings));Add(Button(TEXT("Twórcy"),[this](){ChangeScreen(EScreen::Credits);}));Add(Button(TEXT("Wyjdź"),[this](){Quit();}));}
 else if(Screen==EScreen::Editor){Add(Text(TEXT("TWÓJ ZAWODNIK"),30));Add(Text(TEXT("Q / E lub przeciągnij postać"),16,Mint));Add(Text(TEXT("Kółko myszy: przybliżenie"),14,Mint));
  for(int32 I=0;I<Catalog.Num();I++){Add(Text(Catalog[I].Label,16));Add(SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth()[Button(TEXT("‹"),[this,I](){Cycle(I,-1);})]+SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).HAlign(HAlign_Center)[SNew(STextBlock).Text_Lambda([this,I](){return OptionText(I);}).Font(FCoreStyle::GetDefaultFontStyle("Regular",17)).ColorAndOpacity(White)]+SHorizontalBox::Slot().AutoWidth()[Button(TEXT("›"),[this,I](){Cycle(I,1);})]);}
  Root->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(36)[Button(TEXT("← Wstecz"),[this](){BackFromEditor();})];Root->AddSlot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(36)[Button(TEXT("Zapisz wygląd ✓"),[this](){SaveAppearance();},true)];
 }
 else if(Screen==EScreen::Pause){Add(Text(TEXT("PRZERWA"),36));Add(Button(TEXT("Wznów"),[this](){ChangeScreen(EScreen::Game);},true));Add(Button(TEXT("Ustawienia"),Settings));Add(Button(TEXT("Wyjdź do menu"),[this](){ChangeScreen(EScreen::Main);}));Add(Button(TEXT("Wyjdź z gry"),[this](){Quit();}));}
 else if(Screen==EScreen::Credits){Add(Text(TEXT("TWÓRCY"),32));Add(Text(TEXT("Daniel Maciej Pytel"),23,Mint));Add(Text(TEXT("Projekt przygotowany przeze mnie"),17));Add(Text(TEXT("specjalnie do portfolio w 2026 roku."),17));
 auto Link=[](const TCHAR* Label,const TCHAR* Url,FLinearColor Color){return SNew(SBox).WidthOverride(106).HeightOverride(106)[SNew(SButton).ContentPadding(FMargin(4)).ButtonColorAndOpacity(Color).HAlign(HAlign_Center).VAlign(VAlign_Center).OnClicked_Lambda([Url](){ClickSound();FPlatformProcess::LaunchURL(Url,nullptr,nullptr);return FReply::Handled();})[Text(Label,13)]];};
 Add(SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().Padding(0,8,8,16)[Link(TEXT("ArtStation"),TEXT("https://www.artstation.com/danielmaciejpytel"),FLinearColor(.02f,.3f,.55f))]+SHorizontalBox::Slot().AutoWidth().Padding(0,8,8,16)[Link(TEXT("GitHub"),TEXT("https://github.com/danielmaciejpytel"),FLinearColor(.3f,.13f,.5f))]+SHorizontalBox::Slot().AutoWidth().Padding(0,8,0,16)[Link(TEXT("LinkedIn"),TEXT("https://www.linkedin.com/in/danielmaciejpytel/"),FLinearColor(.015f,.2f,.4f))]);Add(Button(TEXT("← Wstecz"),[this](){ChangeScreen(EScreen::Main);}));}
 else if(Screen==EScreen::Settings){Add(Text(TEXT("USTAWIENIA"),32));Add(Text(TEXT("Głośność muzyki"),18));Add(SNew(SSlider).Value(Saved->Volume).OnValueChanged_Lambda([this](float V){UpdateMusicVolume(V);}));Add(Text(TEXT("Głośność efektów"),18));Add(SNew(SSlider).Value(Saved->EffectsVolume).OnValueChanged_Lambda([this](float V){UpdateEffectsVolume(V);}));Add(Text(TEXT("Czułość myszy"),18));Add(SNew(SSlider).Value((Saved->Sensitivity-.3f)/2.7f).OnValueChanged_Lambda([this](float V){Saved->Sensitivity=.3f+V*2.7f;}));Add(Button(Saved->ReducedMotion?TEXT("Ruch kamery: ograniczony"):TEXT("Ruch kamery: płynny"),[this](){Saved->ReducedMotion=!Saved->ReducedMotion;BuildUI();}));Add(Button(FString::Printf(TEXT("Jakość grafiki: %d / 4"),Saved->Quality+1),[this](){Saved->Quality=(Saved->Quality+1)%4;ApplyQuality();BuildUI();}));Add(Button(TEXT("Zapisz i wróć"),[this](){SaveSettings();ChangeScreen(SettingsReturn);},true));}
 if(Screen!=EScreen::Game){Root->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(FMargin(36,20,0,90))[SNew(SBox).WidthOverride(420).MaxDesiredHeight(780)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Ink).Padding(28)[SNew(SScrollBox)+SScrollBox::Slot()[Panel]]]];}
 else{Root->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(24)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Ink).Padding(16)[SNew(STextBlock).Text_Lambda([this](){auto M=GetWorld()->GetAuthGameMode<AFootballMode>();return FText::FromString(FString::Printf(TEXT("GOLE  %02d"),M?M->Goals:0));}).Font(FCoreStyle::GetDefaultFontStyle("Bold",26)).ColorAndOpacity(Mint)]];Root->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(20)[Text(TEXT("WASD  ruch     SHIFT  sprint     SPACJA  skok     LPM przytrzymaj / puść  strzał     R  reset piłki     ESC  pauza"),16)];}
 Root->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0,95,0,0)[SNew(STextBlock).Text_Lambda([this](){if(Screen==EScreen::Game&&Charging){float C=FMath::Min(1.5f,GetWorld()->GetTimeSeconds()-ChargeStarted);return FText::FromString(FString::Printf(TEXT("STRZAŁ  %.1f s  ·  %s"),C,C>=1.5f?TEXT("KIKS!"):C>=.85f?TEXT("POD POPRZECZKĘ"):C>=.35f?TEXT("MOCNY"):TEXT("PODANIE")));}return FText::FromString(GetWorld()->GetTimeSeconds()<ToastUntil?Toast:TEXT(""));}).Font(FCoreStyle::GetDefaultFontStyle("Bold",24)).ColorAndOpacity(Mint)];UI=Root;GEngine->GameViewport->AddViewportWidgetContent(UI.ToSharedRef(),10);
}
void AFootballController::RunProjectChecks(){
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
 case 0:Selection={1,0,1,1,1};Avatar->ApplyKit(Selection,Catalog);Check(TEXT("catalog_5_categories"),Catalog.Num()==5);Check(TEXT("body_loaded"),Avatar->GetMesh()->GetSkeletalMeshAsset()!=nullptr);Check(TEXT("animations_loaded"),Avatar->Animations.Num()==6);Shot(TEXT("01_Main"));break;
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
 case 12:Check(TEXT("walking_input"),FVector::Dist2D(TestPosition,Avatar->GetActorLocation())>200&&Avatar->CurrentAnimation==TEXT("Run_InPlace"));TestPosition=Avatar->GetActorLocation();SprintOn();break;
 case 13:Check(TEXT("sprint_input"),FVector::Dist2D(TestPosition,Avatar->GetActorLocation())>600&&Avatar->CurrentAnimation==TEXT("Run_InPlace"));SprintOff();Avatar->GetCharacterMovement()->StopMovementImmediately();if(Mode&&Mode->Ball){Mode->ResetBall();Mode->Ball->SetWorldLocation(Avatar->GetActorLocation()+Avatar->GetActorForwardVector()*140-FVector(0,0,65),false,nullptr,ETeleportType::TeleportPhysics);Mode->Ball->SetPhysicsLinearVelocity(FVector::ZeroVector);}Kick();break;
 case 14:Check(TEXT("kick_action"),Mode&&Mode->Ball&&Mode->Ball->GetPhysicsLinearVelocity().Size()>100);Shot(TEXT("07_Action"));break;
 case 15:Mode->ResetBall();Mode->LastShot=-10;Avatar->SetActorLocation(FVector(-350,0,100));Avatar->GetCharacterMovement()->StopMovementImmediately();Mode->Ball->SetWorldLocation(FVector(-240,0,22));SprintOn();break;
 case 16:Check(TEXT("sprint_dribble_controlled"),Mode->Ball->GetPhysicsLinearVelocity().Size()<1000&&Avatar->GetVelocity().Size2D()>700&&Mode->Ball->GetComponentLocation().Z<60&&FVector::Dist2D(Mode->Ball->GetComponentLocation(),Avatar->GetActorLocation())<175);JumpPressed();break;
 case 17:Check(TEXT("jump_and_land"),TestJumpObserved&&!Avatar->GetCharacterMovement()->IsFalling());JumpReleased();SprintOff();ChangeScreen(EScreen::Editor);Selection[1]=3;Avatar->ApplyKit(Selection,Catalog);ZoomEditor(20);Check(TEXT("zoom_limit"),EditorZoom==1);{float Before=Avatar->GetActorRotation().Yaw;TurnEditor(1);Check(TEXT("editor_rotation"),!FMath::IsNearlyEqual(Before,Avatar->GetActorRotation().Yaw));}break;
 case 18:Shot(TEXT("08_Editor_Zoom"));Check(TEXT("music_loaded_playing"),Music&&Music->Sound&&Music->IsPlaying());UpdateMusicVolume(0);Check(TEXT("music_mute"),Music&&Music->VolumeMultiplier==0);UpdateMusicVolume(.5f);Saved->Quality=0;ApplyQuality();break;
 case 19:Shot(TEXT("09_Low"));break;
 case 20:Saved->Quality=3;ApplyQuality();break;
 case 21:Shot(TEXT("10_Ultra"));Check(TEXT("graphics_high"),UGameUserSettings::GetGameUserSettings()->GetShadowQuality()==3);Check(TEXT("music_continuity"),Music&&Music->IsPlaying());break;
 case 22:ChangeScreen(EScreen::Game);PauseToggle();Check(TEXT("music_pause_continuity"),Music&&Music->IsPlaying()&&Music->bIsUISound);break;
 case 23:{ChangeScreen(EScreen::Game);Mode->ResetBall();Mode->Ball->SetWorldLocation(FVector(2800,1000,22));Mode->PreviousBall=FVector(2650,1000,22);Mode->ShotInFlight=true;Mode->Tick(.01f);Check(TEXT("miss_disappears"),Mode->BallHidden);Mode->ResetBall();Mode->Ball->SetWorldLocation(FVector(2800,1000,22));Mode->Tick(.01f);Check(TEXT("carried_ball_outside_stays"),!Mode->BallHidden);Check(TEXT("goal_sound_loaded"),Mode->GoalSound!=nullptr);
 FVector Pos(0,0,22),Dir(1,0,0);auto Pass=AFootballMode::ShotVelocity(Pos,Dir,0);Check(TEXT("tap_is_ground_pass"),Pass.Z==0);
 for(int C=1;C<=3;C++){auto V=AFootballMode::ShotVelocity(Pos,Dir,C*.5f);float T=2740/V.X;float Height=22+V.Z*T-490*T*T;Check(FString::Printf(TEXT("charge_%d_height"),C),FMath::Abs(Height-(C==1?130:C==2?220:420))<2);}
 StartCharge();ChargeStarted=GetWorld()->GetTimeSeconds()-2;ReleaseCharge();Check(TEXT("release_fires_charge"),!Charging&&Avatar->CurrentAnimation==TEXT("Kick_Standing"));break;}
 case 24:ChangeScreen(EScreen::Credits);break;
 case 25:Shot(TEXT("11_Credits"));break;
 case 26:ChangeScreen(EScreen::Main);DemoDirection=1;DemoPhase=0;Mode->ResetBall();Avatar->GetCharacterMovement()->StopMovementImmediately();Avatar->SetActorRotation(FRotator::ZeroRotator);Avatar->SetActorLocation(FVector(-350,0,98));break;
 case 27:Shot(TEXT("12_Demo_Right"));break;
 case 28:Check(TEXT("demo_alternates"),DemoPhase==1&&Avatar->GetActorLocation().X*DemoDirection>0);Check(TEXT("demo_faces_shot"),Avatar->GetActorForwardVector().X*DemoDirection>.9f);Shot(TEXT("13_Demo_Left"));break;
 case 29:ChangeScreen(EScreen::Game);Mode->ResetBall();Mode->Scored=true;Mode->Ball->SetWorldLocation(FVector(2800,0,160),false,nullptr,ETeleportType::TeleportPhysics);Mode->Ball->SetPhysicsLinearVelocity(FVector(3200,0,0));break;
 case 30:Check(TEXT("net_stops_ball"),Mode->Ball->GetComponentLocation().X<2980&&Mode->Ball->GetComponentLocation().X>2800&&Mode->Ball->GetPhysicsLinearVelocity().Size2D()<100);Check(TEXT("net_ball_lands"),Mode->Ball->GetComponentLocation().Z<40);Check(TEXT("all_effects_loaded"),Effects.Num()==5&&Effects.FindRef(TEXT("jump"))&&Effects.FindRef(TEXT("kick"))&&Effects.FindRef(TEXT("fail"))&&Effects.FindRef(TEXT("click")));UpdateEffectsVolume(.5f);PlayEffect(TEXT("goal"),1.5f);Check(TEXT("goal_gain_150_percent"),!ActiveEffects.IsEmpty()&&FMath::IsNearlyEqual(ActiveEffects.Last()->VolumeMultiplier,.75f));UpdateEffectsVolume(0);Check(TEXT("effects_mute"),!ActiveEffects.IsEmpty()&&ActiveEffects.Last()->VolumeMultiplier==0);Check(TEXT("effects_do_not_mute_music"),Music&&Music->VolumeMultiplier>0&&Music->IsPlaying());UpdateEffectsVolume(.8f);SaveSettings();{auto Loaded=Cast<UFootballSave>(UGameplayStatics::LoadGameFromSlot(ProfileSlot(),0));Check(TEXT("effects_volume_saved"),Loaded&&FMath::IsNearlyEqual(Loaded->EffectsVolume,.8f));}SettingsReturn=EScreen::Game;ChangeScreen(EScreen::Settings);break;
 case 31:Shot(TEXT("14_Settings_Effects"));break;
 default:FFileHelper::SaveStringToFile(TestReport,*(FPaths::ProjectSavedDir()/TEXT("runtime_checks.txt")));FPlatformMisc::RequestExit(false);break;
 }TestStage++;TestStarted=FPlatformTime::Seconds()+((TestStage==14||TestStage==5)?.35:2.0);
}













void AFootballController::JumpPressed(){if(Screen==EScreen::Game&&Avatar)Avatar->Jump();}
void AFootballController::JumpReleased(){if(Avatar)Avatar->StopJumping();}
void AFootballController::TurnEditor(float V){if(Screen==EScreen::Editor&&Avatar&&!FMath::IsNearlyZero(V))Avatar->AddActorWorldRotation(FRotator(0,V*90*GetWorld()->GetDeltaSeconds(),0));}
void AFootballController::ZoomEditor(float V){if(Screen==EScreen::Editor)EditorZoom=FMath::Clamp(EditorZoom+V*.15f,0.f,1.f);}
void AFootballController::UpdateEditorInput(float Dt){
 if(Screen!=EScreen::Editor){Dragging=false;MouseWasDown=false;return;}
 float X,Y;if(!GetMousePosition(X,Y))return;bool Down=IsInputKeyDown(EKeys::LeftMouseButton);
 if(Down&&!MouseWasDown){FVector Center=Avatar->GetActorLocation()+FVector(0,0,2);FVector2D Lo(1e9,1e9),Hi(-1e9,-1e9);
  for(int I=0;I<8;I++){FVector2D Pixel;ProjectWorldLocationToScreen(Center+FVector(I&1?95:-95,I&2?65:-65,I&4?100:-96),Pixel);Lo.X=FMath::Min(Lo.X,Pixel.X);Lo.Y=FMath::Min(Lo.Y,Pixel.Y);Hi.X=FMath::Max(Hi.X,Pixel.X);Hi.Y=FMath::Max(Hi.Y,Pixel.Y);}
  int W,H;GetViewportSize(W,H);Dragging=X>W*.40f&&Y<H*.86f&&X>=Lo.X&&X<=Hi.X&&Y>=Lo.Y&&Y<=Hi.Y;
 }
 if(Down&&Dragging)Avatar->AddActorWorldRotation(FRotator(0,(X-LastMouseX)*.45f,0));if(!Down)Dragging=false;LastMouseX=X;MouseWasDown=Down;
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
 // Run through the kick, continue beyond midfield, then turn before resetting the ball.
 if(DemoPhase==1&&X*DemoDirection>=650.f){DemoDirection=-DemoDirection;DemoPhase=0;M->ResetBall();}
 Avatar->GetCharacterMovement()->MaxWalkSpeed=510;
 FVector Direction(DemoDirection,0,0);Direction.Y=FMath::Clamp(-Avatar->GetActorLocation().Y*.01f,-.3f,.3f);
 Avatar->AddMovementInput(Direction.GetSafeNormal(),1);
 FVector To=M->Ball->GetComponentLocation()-Avatar->GetActorLocation();To.Z=0;
 if(DemoPhase==0&&To.X*DemoDirection>70&&To.X*DemoDirection<210&&FMath::Abs(To.Y)<80&&Avatar->GetActorForwardVector().X*DemoDirection>.95f){
  Avatar->KickUntil=T+.65f;Avatar->CurrentAnimation.Empty();Avatar->SetAnimation(TEXT("Kick_Standing"),false);
  M->Kick(Avatar,1.f);DemoPhase=1;DemoShotAt=T;
 }
}

void AFootballPlayer::Landed(const FHitResult& Hit){
 const bool Audible=GetVelocity().Z < -150;Super::Landed(Hit);
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
