#include "Football.h"
#include "ErlingAnimation.h"
#include "ErlingInterface.h"
#include "ErlingTuning.h"
#include "ErlingStylizedGoal.h"
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
#include "ErlingProfile.h"
void AFootballController::LoadCatalog()
{
	FString S;
	if (!FFileHelper::LoadFileToString(S, *(FPaths::ProjectContentDir() / TEXT("Data/wardrobe.json"))))
		return;
	TSharedPtr<FJsonObject> Root;
	auto Reader = TJsonReaderFactory<>::Create(S);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root)
		return;
	for (auto CV : Root->GetArrayField(TEXT("categories")))
	{
		auto CO = CV->AsObject();
		FKitCategory C;
		C.Label = CO->GetStringField(TEXT("label"));
		for (auto OV : CO->GetArrayField(TEXT("options")))
		{
			auto OO = OV->AsObject();
			FKitOption O;
			O.Label = OO->GetStringField(TEXT("label"));
			OO->TryGetStringField(TEXT("texture"), O.Texture);
			const TArray<TSharedPtr<FJsonValue>>* Arr;
			if (OO->TryGetArrayField(TEXT("meshes"), Arr))
				for (auto V : *Arr)
					O.Meshes.Add(V->AsString());
			C.Options.Add(O);
		}
		Catalog.Add(C);
	}
}
void AFootballController::PauseToggle()
{
	if (Screen == EScreen::Game)
		ChangeScreen(EScreen::Pause);
	else if (Screen == EScreen::Pause)
		ChangeScreen(EScreen::Game);
	else if (Screen == EScreen::Settings)
		ChangeScreen(SettingsReturn);
	else if (Screen == EScreen::Editor)
		BackFromEditor();
	else if (Screen == EScreen::Credits)
		ChangeScreen(EScreen::Main);
}
void AFootballController::ChangeScreen(EScreen Next)
{
	const auto Old = Screen;
	const bool ResumeFromSettings = Old == EScreen::Settings && Next == SettingsReturn;
	BallControlOff();
	if (Avatar)
	{
		Avatar->PreviewAnimation = false;
		PreviewIndex = -1;
	}
	Screen = Next;
	Charging = false;
	ShotChargeArmed = false;
	ShotBufferActive = false;
	ShotChargeAimDirection = FVector::ZeroVector;
	ShotBufferAimDirection = FVector::ZeroVector;
	GoalSpacePending = false;
	SpaceHeld = false;
	if ((Next == EScreen::Editor || Next == EScreen::Main) && !ResumeFromSettings)
	{
		CancelPendingActions();
		if (Avatar)
		{
			Avatar->ClearAction();
			Avatar->PhysicalJump = false;
		}
		GoalCelebrationUsed = true;
	}
	const bool PauseForSettings = Next == EScreen::Settings && (SettingsReturn == EScreen::Pause || (Saved && Saved->PauseInSettings));
	SetPause(Next == EScreen::Pause || PauseForSettings);
	SprintOff();
	if (Next == EScreen::Editor)
	{
		EditorZoom = 0;
		Dragging = false;
		DraftBefore = Selection;
		Avatar->GetCharacterMovement()->StopMovementImmediately();
		Avatar->SetActorLocation(FVector(-350, 0, 100));
		Avatar->SetActorRotation(FRotator(0, -35, 0));
		Avatar->KickUntil = 0;
		Avatar->SetAnimation(TEXT("Idle_Breathe"));
		Reset();
	}
	if (Next == EScreen::Game && Old != EScreen::Pause && !ResumeFromSettings)
	{
		CancelPendingActions();
		Avatar->ClearAction();
		Avatar->PhysicalJump = false;
		GoalCelebrationUsed = true;
		Avatar->SetActorLocation(FVector(-350, 0, 100));
		Avatar->SetActorRotation(FRotator::ZeroRotator);
		Yaw = 0;
		Pitch = -15;
		KickCooldown = 0;
		CameraRig->CameraPivotInitialized = false;
		CameraRig->PitchCameraLeadX = 0;
		CameraRig->PitchViewInitialized = false;
		CameraRig->PitchShotTracking = false;
		CameraRig->PitchShotHoldRemaining = 0.f;
		CameraRig->PitchShotOffset = FVector::ZeroVector;
		Reset();
	}
	if (Next == EScreen::Main && Old != EScreen::Credits && !ResumeFromSettings)
	{
		const float X = Avatar->GetActorLocation().X;
		DemoPhase = FMath::Abs(X) < 240 ? 4 : 0;
		DemoDirection = DemoPhase == 4 ? (X >= 0 ? 1 : -1) : (X > 0 ? -1 : 1);
		DemoShotTargetX = 0.f;
		Reset();
	}
	bShowMouseCursor = Next != EScreen::Game;
	if (bShowMouseCursor)
	{
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
	}
	else
	{
		FInputModeGameOnly Mode;
		SetInputMode(Mode);
	}
	BuildUI();
}
void AFootballController::Cycle(int32 C, int32 Direction)
{
	if (!Catalog.IsValidIndex(C) || Catalog[C].Options.IsEmpty())
		return;
	Selection[C] = (Selection[C] + Direction + Catalog[C].Options.Num()) % Catalog[C].Options.Num();
	Avatar->ApplyKit(Selection, Catalog);
	Toast.Empty();
}
void AFootballController::SaveAppearance()
{
	Saved->Appearance = Selection;
	const bool Ok = UGameplayStatics::SaveGameToSlot(Saved, ProfileSlot(), 0);
	if (Ok)
		DraftBefore = Selection;
	Toast = Ok ? Localize(TEXT("Appearance saved"), TEXT("Wygląd zapisany"))
	           : Localize(TEXT("Failed to save appearance"), TEXT("Nie udało się zapisać wyglądu"));
	ToastUntil = GetWorld()->GetTimeSeconds() + 4;
}
void AFootballController::BackFromEditor()
{
	Selection = DraftBefore;
	Avatar->ApplyKit(Selection, Catalog);
	ChangeScreen(EScreen::Main);
}
void AFootballController::SaveSettings()
{
	UGameplayStatics::SaveGameToSlot(Saved, ProfileSlot(), 0);
}
void AFootballController::Quit()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}
FString AFootballController::Localize(const TCHAR* English, const TCHAR* Polish) const
{
	return Saved && Saved->Language == 1 ? FString(Polish) : FString(English);
}
FString AFootballController::LocalizeCatalogLabel(const FString& Value) const
{
	if (!Saved || Saved->Language != 1)
		return Value;
	static const TMap<FString, FString> Pl = {{TEXT("Hairstyle"), TEXT("Fryzura")}, {TEXT("Face"), TEXT("Twarz")},
	    {TEXT("Top"), TEXT("Góra")}, {TEXT("Bottom"), TEXT("Dół")}, {TEXT("Footwear"), TEXT("Obuwie")}, {TEXT("Bald"), TEXT("Łysy")},
	    {TEXT("Ponytail"), TEXT("Kucyk")}, {TEXT("Smile"), TEXT("Uśmiech")}, {TEXT("Joy"), TEXT("Radość")}, {TEXT("Anger"), TEXT("Złość")},
	    {TEXT("Surprise"), TEXT("Zaskoczenie")}, {TEXT("Sleepy"), TEXT("Senny")}, {TEXT("Tired"), TEXT("Zmęczony")},
	    {TEXT("Shirtless"), TEXT("Bez koszulki")}, {TEXT("Norway"), TEXT("Norwegia")}, {TEXT("Underwear"), TEXT("Bielizna")},
	    {TEXT("White Shorts"), TEXT("Białe spodenki")}, {TEXT("Barefoot"), TEXT("Boso")}, {TEXT("Football Boots"), TEXT("Korki")}};
	if (const FString* Found = Pl.Find(Value))
		return *Found;
	return Value;
}
FText AFootballController::OptionText(int32 I) const
{
	if (Catalog.IsValidIndex(I) && Selection.IsValidIndex(I) && Catalog[I].Options.IsValidIndex(Selection[I]))
		return FText::FromString(LocalizeCatalogLabel(Catalog[I].Options[Selection[I]].Label));
	return FText::FromString(Localize(TEXT("No items"), TEXT("Brak opcji")));
}
void AFootballController::BuildUI()
{
	if (!IsLocalController())
		return;
	if (!UI)
	{
		UClass* WidgetClass = LoadClass<UErlingInterface>(nullptr, TEXT("/Game/Erling/UI/WBP_ErlingInterface.WBP_ErlingInterface_C"));
		if (!WidgetClass)
		{
			UE_LOG(LogTemp, Error, TEXT("Missing WBP_ErlingInterface. Run the UI import and authoring tools."));
			return;
		}
		UI = CreateWidget<UErlingInterface>(this, WidgetClass);
		if (UI)
			UI->AddToViewport(10);
	}
	if (UI)
		UI->RefreshScreen();
}
void AFootballController::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UI)
	{
		UI->RemoveFromParent();
		UI = nullptr;
	}
	Super::EndPlay(Reason);
}
