#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/SaveGame.h"
#include "ErlingBallPossession.h"
#include "ErlingCameraRig.h"
#include "ErlingAudioVideo.h"
#include "ErlingReferee.h"
#include "ErlingShot.h"
#include "Football.generated.h"

class UStaticMeshComponent; class USkeletalMeshComponent; class UMaterialInstanceDynamic; class UAnimSequence; class ACameraActor; class UErlingInterface;
class UAudioComponent;
struct FKitOption { FString Label; TArray<FString> Meshes; FString Texture; };
struct FKitCategory { FString Label; TArray<FKitOption> Options; };
UCLASS() class ERLING_API UFootballSave : public USaveGame {
 GENERATED_BODY()
public:
 UPROPERTY() TArray<int32> Appearance={1,0,1,1,1};
 UPROPERTY() float Sensitivity=1.f;
 UPROPERTY() float Volume=.7f;
 UPROPERTY() float EffectsVolume=1.f;
 UPROPERTY() bool ReducedMotion=false;
 UPROPERTY() bool PauseInSettings=false;
 UPROPERTY() int32 Language=0;
 UPROPERTY() int32 CameraMode=-1;
 UPROPERTY() int32 Quality=2;
};
UCLASS() class ERLING_API AFootballPlayer : public ACharacter {
 GENERATED_BODY()
public:
 AFootballPlayer(const FObjectInitializer& ObjectInitializer=FObjectInitializer::Get()); virtual void BeginPlay() override; virtual void Tick(float Dt) override;
 virtual void Landed(const FHitResult& Hit) override; void InitializeAssets(); void SetAnimation(const FString& Name,bool Loop=true); void ApplyKit(const TArray<int32>& Values,const TArray<FKitCategory>& Catalog);
 UPROPERTY() TArray<USkeletalMeshComponent*> Pieces;
 UPROPERTY() USkeletalMeshComponent* Face;
 UPROPERTY() UMaterialInstanceDynamic* FaceMaterial;
 UPROPERTY() TMap<FString,UAnimSequence*> Animations;
 FString CurrentAnimation; float KickUntil=0;
 enum class EAction:uint8 { None,Kick,Flip,Slide,Trip,Emote,Fall,GetUp };
 EAction Action=EAction::None;
 FString PreviousAnimation;
 float AnimationTime=0,PreviousAnimationTime=0,AnimationBlend=1,BlendSeconds=.16f,PlaybackRate=1,PreviousRate=1;
 float JumpAnimationScale=1.f;
 bool AnimationLoops=true,PreviousLoops=true,PhysicalJump=false,ActionInterruptible=false,ActionAllowsMovement=false;
 bool PreviewAnimation=false;
 bool BallTrapActive=false; float BallTrapElapsed=0.f; FName BallTrapFoot=NAME_None; int32 BallTrapCount=0;
 bool TurningInPlace=false; float TurnElapsed=0,TurnStartYaw=0,TurnTargetYaw=0;
 void BeginTurn(float TargetYaw);
 float ActionElapsed=0,ActionStartTime=0,ActionDuration=0,SlideDistance=0,IdleElapsed=0,JumpElapsed=0;
 FVector ActionVelocity=FVector::ZeroVector,EntryVelocity=FVector::ZeroVector,ActionStartLocation=FVector::ZeroVector;
 bool IsMovementLocked()const{return Action!=EAction::None&&!ActionAllowsMovement;}
 bool CanAct()const;
 bool StartAction(EAction Kind,const FString& Clip,float Rate=1.f,float StartFraction=0.f,bool Interruptible=false);
 void ClearAction(); void UpdateAction(float Dt); void StartPhysicalJump(); void StartBallTrap(FName Foot); void CancelBallTrap();
 void ConfigurePiece(USkeletalMeshComponent* Piece);
};
UCLASS() class ERLING_API AFootballMode : public AGameModeBase {
 GENERATED_BODY()
public:
 AFootballMode(); virtual void BeginPlay() override; virtual void Tick(float Dt) override;
 /** Live ball mesh, owned by BallActor (see ErlingBall.h). */
 UPROPERTY() UStaticMeshComponent* Ball=nullptr;
 UPROPERTY() TObjectPtr<class AErlingBall> BallActor;
 UPROPERTY() class USoundBase* GoalSound=nullptr;
 void NetHit(UPrimitiveComponent* HitComponent,AActor* OtherActor,UPrimitiveComponent* OtherComp,FVector NormalImpulse,const FHitResult& Hit);
 void HideMiss(){Referee->HideMiss();} static FVector ShotVelocity(const FVector& Position,const FVector& Direction,float Seconds);
 /** Shot state, goals, misses and the score (see ErlingReferee.h). */
 UPROPERTY(VisibleAnywhere) TObjectPtr<UErlingReferee> Referee;
 UFUNCTION(BlueprintPure, Category="Football|Validation") int32 GetGoalCountForValidation() const { return Referee->Goals; }
 /** Possession / dribbling state and logic (see ErlingBallPossession.h). */
 UPROPERTY(VisibleAnywhere) TObjectPtr<UErlingBallPossession> Possession;
 void Dribble(AFootballPlayer* Player,float Dt){Possession->Dribble(Player,Dt);}
 UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> FinishedBalls;
 void PreserveFinishedBall();
 void ResetBall(); void Kick(AFootballPlayer* Avatar,float Seconds=1.f);
 bool HasBall(const AFootballPlayer* Player)const{return Possession->HasBall(Player);}
 bool HasDribbleControl(const AFootballPlayer* Player)const{return Possession->HasDribbleControl(Player);}
 bool IsRecoverableSprintTouch(const AFootballPlayer* Player,float MaxGap=ErlingPossession::RecoveryAbandonDistance)const{return Possession->IsRecoverableSprintTouch(Player,MaxGap);}
 void ClearSprintReleaseRecovery(){Possession->ClearSprintReleaseRecovery();}
 bool LaunchShot(AFootballPlayer* Player,const FVector& Velocity,bool CommittedShot=false);
 UStaticMeshComponent* Box(const FVector& P,const FVector& Size,const FLinearColor& Color,bool Collision=true);
};
UCLASS() class ERLING_API AFootballController : public APlayerController {
 GENERATED_BODY()
public:
 AFootballController(); virtual void BeginPlay() override; virtual void EndPlay(const EEndPlayReason::Type Reason) override; virtual void SetupInputComponent() override; virtual void Tick(float Dt) override;
 virtual bool InputKey(const FInputKeyEventArgs& Params) override;
 enum class EScreen:uint8 { Main,Editor,Game,Pause,Settings,Credits };
 EScreen Screen=EScreen::Main,SettingsReturn=EScreen::Main;
 UPROPERTY() AFootballPlayer* Avatar=nullptr;
 UPROPERTY() ACameraActor* ViewCamera=nullptr;
 UPROPERTY() UFootballSave* Saved=nullptr;
 /** Music, sound effects and graphics quality (see ErlingAudioVideo.h). */
 UPROPERTY(VisibleAnywhere) TObjectPtr<UErlingAudioVideo> AudioVideo;
 void PlayEffect(const FString& Name,float Gain=1.f){AudioVideo->PlayEffect(Name,Gain);}
 void UpdateEffectsVolume(float V){AudioVideo->UpdateEffectsVolume(V);}
 void UpdateMusicVolume(float V){AudioVideo->UpdateMusicVolume(V);}
 void ApplyQuality(){AudioVideo->ApplyQuality();}
 float EditorZoom=0; bool Dragging=false,MouseWasDown=false; float LastMouseX=0;
 bool Charging=false,ShotChargeArmed=false,ShotBufferActive=false;
 // Buffer power stays in the ballistic curve's 0..1.5 range, independent of button hold duration.
 float ChargeStarted=0,ShotBufferStarted=0,ShotBufferSeconds=0;
 FVector ShotChargeAimDirection=FVector::ZeroVector,ShotBufferAimDirection=FVector::ZeroVector;
 void StartCharge(); void ReleaseCharge(); void FireShot(float Seconds,FVector AimOverride=FVector::ZeroVector); void UpdateDemo(float Dt);
 float GetShotChargePower()const;
 FErlingShotEvaluation EvaluateShot(float PowerSeconds,FVector AimOverride=FVector::ZeroVector)const;
 int DemoDirection=1,DemoPhase=0,DemoShotDistanceIndex=-1; float DemoShotAt=0,DemoShotTargetX=0;
 float DemoReceiveAt=0,DemoFootSide=18; FVector DemoReceivePosition=FVector::ZeroVector;
 void JumpPressed(); void JumpReleased(); void TurnEditor(float V); void ZoomEditor(float V); void GamepadLookX(float V); void GamepadLookY(float V); void EditorGamepadTurn(float V); void EditorGamepadZoom(float V); void UpdateEditorInput(float Dt);
 void SlidePressed(); void UpdateActions(float Dt); void OnGoal(AFootballPlayer* Scorer); void BeginCelebration(bool Held); void CancelPendingActions();
 bool SpaceHeld=false,GoalSpacePending=false,GoalCelebrationUsed=true,PendingShot=false,PendingTrip=false;
	bool GoalHoldRequested=false; float MoveForward=0,MoveRight=0;
	FVector SmoothedGamepadMoveInput=FVector::ZeroVector;
 FVector ShotBallStart=FVector::ZeroVector,ShotActorStart=FVector::ZeroVector;
 float SpaceStarted=0,GoalUntil=-1,ShotContactTime=0,ShotRecoverTime=0;
 FVector PendingVelocity=FVector::ZeroVector,PendingAimTarget=FVector::ZeroVector,ShotEntryVelocity=FVector::ZeroVector;
 bool PendingHasAim=false;
 UPROPERTY(Config) float SlideRunDistance=520;
 UPROPERTY(Config) float SlideSprintMultiplier=1.25f;
 UPROPERTY(Config) float TripChance=.33f;
 UPROPERTY(Config) float ShotBufferWindow=1.4f;
 int32 PreviewIndex=-1; void CycleAnimationPreview(int32 Direction); FText PreviewAnimationText()const;
 bool BallControlHeld=false; void BallControlOn(); void BallControlOff();
 bool bUsingGamepadInput=false;
 bool HasDigitalMoveIntent()const; FVector GetMoveIntentWorld()const;
 #if !UE_BUILD_SHIPPING
 void RunChecks(); bool bRunLatestChecks=false; bool bRunProjectChecks=false;
 #endif
 int32 TestStage_Latest=0; double TestAt=0; FString Report; bool TestDigitalMoveIntent=false;
 TArray<FKitCategory> Catalog; TArray<int32> Selection,DraftBefore;
 UPROPERTY(Transient) TObjectPtr<UErlingInterface> UI; float Yaw=0,Pitch=-15; bool Sprint=false; float DemoTime=0; float KickCooldown=0; FString Toast; float ToastUntil=0;
 /** Camera placement and shot tracking (see ErlingCameraRig.h). */
 UPROPERTY(VisibleAnywhere) TObjectPtr<UErlingCameraRig> CameraRig;
 void UpdateAvatarFacing(float Dt,EScreen ActiveScreen,AFootballMode* GameplayMode);
 void UpdateCamera(float Dt,EScreen ActiveScreen,AFootballMode* GameplayMode,const FVector& P){CameraRig->Update(Dt,ActiveScreen==EScreen::Game||ActiveScreen==EScreen::Pause,ActiveScreen==EScreen::Editor,GameplayMode,P);}
 void LoadCatalog(); void BuildUI(); void ChangeScreen(EScreen Next); void Cycle(int32 Category,int32 Direction); void SaveAppearance(); void BackFromEditor(); void PauseToggle(); void Kick(); void Reset(); void Forward(float V); void Right(float V); void LookX(float V); void LookY(float V); void SprintOn(); void SprintOff(); void SaveSettings(); void Quit();
 FString Localize(const TCHAR* English,const TCHAR* Polish) const;
 FString LocalizeCatalogLabel(const FString& Value) const;
 FText OptionText(int32 Index) const;
 #if !UE_BUILD_SHIPPING
 void RunProjectChecks();
 #endif
 int32 TestStage=0; double TestStarted=0; FString TestReport; FVector TestPosition; bool TestJumpObserved=false;
};
