#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/SaveGame.h"
#include "Football.generated.h"

class UStaticMeshComponent; class USkeletalMeshComponent; class UMaterialInstanceDynamic; class UAnimSequence; class ACameraActor; class SWidget; class SVerticalBox;
class UAudioComponent;
struct FKitOption { FString Label; TArray<FString> Meshes; FString Texture; };
struct FKitCategory { FString Label; TArray<FKitOption> Options; };
UCLASS() class ERLINGFOOTBALL_API UFootballSave : public USaveGame {
 GENERATED_BODY()
public:
 UPROPERTY() TArray<int32> Appearance={1,0,1,1,1};
 UPROPERTY() float Sensitivity=1.f;
 UPROPERTY() float Volume=.7f;
 UPROPERTY() float EffectsVolume=1.f;
 UPROPERTY() bool ReducedMotion=false;
 UPROPERTY() int32 Quality=2;
};
UCLASS() class ERLINGFOOTBALL_API AFootballPlayer : public ACharacter {
 GENERATED_BODY()
public:
 AFootballPlayer(); virtual void BeginPlay() override; virtual void Tick(float Dt) override;
 virtual void Landed(const FHitResult& Hit) override; void InitializeAssets(); void SetAnimation(const FString& Name,bool Loop=true); void ApplyKit(const TArray<int32>& Values,const TArray<FKitCategory>& Catalog);
 UPROPERTY() TArray<USkeletalMeshComponent*> Pieces;
 UPROPERTY() USkeletalMeshComponent* Face;
 UPROPERTY() UMaterialInstanceDynamic* FaceMaterial;
 UPROPERTY() TMap<FString,UAnimSequence*> Animations;
 FString CurrentAnimation; float KickUntil=0;
};
UCLASS() class ERLINGFOOTBALL_API AFootballMode : public AGameModeBase {
 GENERATED_BODY()
public:
 AFootballMode(); virtual void BeginPlay() override; virtual void Tick(float Dt) override;
 UPROPERTY() UStaticMeshComponent* Ball=nullptr;
 UPROPERTY() class USoundBase* GoalSound=nullptr;
 bool ShotInFlight=false,BallHidden=false;
 UFUNCTION() void NetHit(UPrimitiveComponent* HitComponent,AActor* OtherActor,UPrimitiveComponent* OtherComp,FVector NormalImpulse,const FHitResult& Hit);
 void CreateNetCollision(); void HideMiss(); static FVector ShotVelocity(const FVector& Position,const FVector& Direction,float Seconds);
 int32 Goals=0; float ResetAt=0; bool Scored=false; FVector PreviousBall=FVector::ZeroVector; float LastShot=-10;
 void Dribble(AFootballPlayer* Player,float Dt);
 void ResetBall(); void Kick(AFootballPlayer* Avatar,float Seconds=1.f); void CreateField();
 UStaticMeshComponent* Box(const FVector& P,const FVector& Size,const FLinearColor& Color,bool Collision=true);
};
UCLASS() class ERLINGFOOTBALL_API AFootballController : public APlayerController {
 GENERATED_BODY()
public:
 AFootballController(); virtual void BeginPlay() override; virtual void SetupInputComponent() override; virtual void Tick(float Dt) override;
 enum class EScreen:uint8 { Main,Editor,Game,Pause,Settings,Credits };
 EScreen Screen=EScreen::Main,SettingsReturn=EScreen::Main;
 UPROPERTY() AFootballPlayer* Avatar=nullptr;
 UPROPERTY() ACameraActor* ViewCamera=nullptr;
 UPROPERTY() UFootballSave* Saved=nullptr;
 UPROPERTY() UAudioComponent* Music=nullptr;
 UPROPERTY() TMap<FString,class USoundBase*> Effects;
 UPROPERTY() TArray<UAudioComponent*> ActiveEffects;
 void PlayEffect(const FString& Name,float Gain=1.f); void UpdateEffectsVolume(float V);
 float EditorZoom=0; bool Dragging=false,MouseWasDown=false; float LastMouseX=0;
 bool Charging=false; float ChargeStarted=0; void StartCharge(); void ReleaseCharge(); void FireShot(float Seconds); void UpdateDemo(float Dt);
 int DemoDirection=1,DemoPhase=0; float DemoShotAt=0;
 void JumpPressed(); void JumpReleased(); void TurnEditor(float V); void ZoomEditor(float V); void UpdateEditorInput(float Dt); void ApplyQuality(); void UpdateMusicVolume(float V);
 TArray<FKitCategory> Catalog; TArray<int32> Selection,DraftBefore;
 TSharedPtr<SWidget> UI; float Yaw=0,Pitch=-15; bool Sprint=false; float DemoTime=0; float KickCooldown=0; FString Toast; float ToastUntil=0;
 void LoadCatalog(); void BuildUI(); void ChangeScreen(EScreen Next); void Cycle(int32 Category,int32 Direction); void SaveAppearance(); void BackFromEditor(); void PauseToggle(); void Kick(); void Reset(); void Forward(float V); void Right(float V); void LookX(float V); void LookY(float V); void SprintOn(); void SprintOff(); void SaveSettings(); void Quit();
 FText OptionText(int32 Index) const;
 void RunProjectChecks(); int32 TestStage=0; double TestStarted=0; FString TestReport; FVector TestPosition; bool TestJumpObserved=false;
};

