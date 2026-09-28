#include "ErlingAudioVideo.h"
#include "Football.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"

UErlingAudioVideo::UErlingAudioVideo()
{
	PrimaryComponentTick.bCanEverTick = false;
}
AFootballController* UErlingAudioVideo::Controller() const
{
	return CastChecked<AFootballController>(GetOwner());
}
void UErlingAudioVideo::LoadEffects()
{
	for (auto N : {TEXT("goal"), TEXT("jump"), TEXT("kick"), TEXT("fail"), TEXT("click")})
		Effects.Add(N, LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Erling/Audio/%s.%s"), N, N)));
}
void UErlingAudioVideo::StartMusic()
{
	// Outer is the controller so the audio component registers with that actor.
	Music = NewObject<UAudioComponent>(GetOwner());
	Music->bIsUISound = true;
	Music->bAllowSpatialization = false;
	Music->bAutoActivate = false;
	Music->bAutoDestroy = false;
	Music->RegisterComponent();
	if (auto Track = LoadObject<USoundWave>(nullptr, TEXT("/Game/Erling/Audio/background.background")))
	{
		Track->bLooping = true;
		Music->SetSound(Track);
		Music->SetVolumeMultiplier(Controller()->Saved->Volume);
		Music->Play();
	}
}
void UErlingAudioVideo::UpdateMusicVolume(float V)
{
	UFootballSave* Saved = Controller()->Saved;
	Saved->Volume = FMath::Clamp(V, 0.f, 1.f);
	if (Music)
		Music->SetVolumeMultiplier(Saved->Volume);
}
void UErlingAudioVideo::ApplyQuality()
{
	UFootballSave* Saved = Controller()->Saved;
	if (!Saved)
		return;
	Saved->Quality = FMath::Clamp(Saved->Quality, 0, 3);
	if (auto G = UGameUserSettings::GetGameUserSettings())
	{
		G->SetOverallScalabilityLevel(Saved->Quality);
		G->SetResolutionScaleValueEx(Saved->Quality == 0 ? 70 : 100);
		G->ApplySettings(false);
	}
	auto Set = [](const TCHAR* N, int V)
	{
		if (auto C = IConsoleManager::Get().FindConsoleVariable(N))
			C->Set(V, ECVF_SetByCode);
	};
	Set(TEXT("r.Shadow.MaxResolution"), Saved->Quality == 3 ? 4096 : Saved->Quality == 2 ? 2048 : 1024);
	Set(TEXT("r.Shadow.CSM.MaxCascades"), Saved->Quality == 3 ? 4 : 2);
	Set(TEXT("r.ContactShadows"), Saved->Quality >= 2);
	Set(TEXT("r.AmbientOcclusionLevels"), Saved->Quality == 3 ? 3 : Saved->Quality >= 1 ? 1 : 0);
	// The pitch has a deliberately flat storybook light rig. Keep GI
	// deterministic across quality presets so Ultra cannot turn the pastel look
	// back into a glossy/realistic Lumen render.
	Set(TEXT("r.DynamicGlobalIlluminationMethod"), 0);
	Set(TEXT("r.Lumen.DiffuseIndirect.Allow"), 0);
	Set(TEXT("r.Lumen.Reflections.Allow"), 0);
}
void UErlingAudioVideo::PlayEffect(const FString& Name, float Gain)
{
	UFootballSave* Saved = Controller()->Saved;
	if (!Saved)
		return;
	auto Sound = Effects.FindRef(Name);
	if (!Sound)
		return;
	ActiveEffects.RemoveAll(
	    [](UAudioComponent* C)
	    {
		    return !IsValid(C) || !C->IsPlaying();
	    });
	auto C = UGameplayStatics::SpawnSound2D(this, Sound, Saved->EffectsVolume * Gain);
	if (C)
	{
		C->ComponentTags.Add(FName(*FString::SanitizeFloat(Gain)));
		ActiveEffects.Add(C);
	}
}
void UErlingAudioVideo::UpdateEffectsVolume(float V)
{
	UFootballSave* Saved = Controller()->Saved;
	Saved->EffectsVolume = FMath::Clamp(V, 0.f, 1.f);
	for (auto C : ActiveEffects)
		if (IsValid(C) && C->IsPlaying())
		{
			float Gain = C->ComponentTags.IsEmpty() ? 1.f : FCString::Atof(*C->ComponentTags[0].ToString());
			C->SetVolumeMultiplier(Saved->EffectsVolume * Gain);
		}
}
