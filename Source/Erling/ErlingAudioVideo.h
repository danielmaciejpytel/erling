#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ErlingAudioVideo.generated.h"

class AFootballController;
class UAudioComponent;
class USoundBase;

/**
 * Music, sound effects and graphics quality. Owned by AFootballController, which
 * keeps the saved profile (volumes, quality); this component loads and plays the
 * audio and applies those settings to the engine.
 */
UCLASS(ClassGroup = (Erling))
class ERLING_API UErlingAudioVideo : public UActorComponent
{
	GENERATED_BODY()
public:
	UErlingAudioVideo();

	void LoadEffects();
	void StartMusic();
	void PlayEffect(const FString& Name, float Gain = 1.f);
	void UpdateEffectsVolume(float V);
	void UpdateMusicVolume(float V);
	void ApplyQuality();

	UPROPERTY() UAudioComponent* Music = nullptr;
	UPROPERTY() TMap<FString, USoundBase*> Effects;
	UPROPERTY() TArray<UAudioComponent*> ActiveEffects;

private:
	AFootballController* Controller() const;
};
