#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "ErlingInterface.generated.h"

class AFootballController;
class UWidgetSwitcher;
class USlider;
class UProgressBar;

UENUM(BlueprintType)
enum class EErlingUIAction : uint8
{
    Play, Character, Settings, Credits, Quit, MainMenu, Back,
    SaveAppearance, AppearancePrevious, AppearanceNext,
    PreviewPrevious, PreviewNext, Language, Camera, Quality, PauseInSettings,
    SaveSettings, ArtStation, GitHub, LinkedIn
};

/** Bilingual designer text. Both translations remain editable in the Widget Designer. */
UCLASS(meta=(DisplayName="Erling Localized Text"))
class ERLING_API UErlingUIText : public UTextBlock
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category="Erling|Translation") FText English;
    UPROPERTY(EditAnywhere, Category="Erling|Translation") FText Polish;
    void SetLanguage(bool bPolish);
};

/** A normal UMG button with a designer-selectable game action. */
UCLASS(meta=(DisplayName="Erling Action Button"))
class ERLING_API UErlingUIButton : public UButton
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category="Erling|Action") EErlingUIAction Action = EErlingUIAction::Play;
    UPROPERTY(EditAnywhere, Category="Erling|Action", meta=(ClampMin="0")) int32 CategoryIndex = 0;
    void Connect(class UErlingInterface* InInterface);
    void UpdateFocusStyle();
private:
    UPROPERTY(Transient) TObjectPtr<class UErlingInterface> Interface;
    FButtonStyle UnfocusedStyle;
    bool bFocusStyle = false;
    UFUNCTION() void Dispatch();
};

/** Behavior only. The complete editable layout lives in WBP_ErlingInterface. */
UCLASS()
class ERLING_API UErlingInterface : public UUserWidget
{
    GENERATED_BODY()
public:
    void RefreshScreen();
    void HandleAction(EErlingUIAction Action, int32 CategoryIndex);
    bool IsPointerOverPanel() const;
#if !UE_BUILD_SHIPPING
    // Called before Slate paints: input tests must use the completed previous hit grid.
    void RunUIChecks();
#endif
protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Key) override;
private:
    UPROPERTY(Transient) TObjectPtr<AFootballController> Controller;
    UPROPERTY(Transient) TObjectPtr<UWidgetSwitcher> ScreenSwitcher;
    UPROPERTY(Transient) TArray<TObjectPtr<UErlingUIButton>> Buttons;
    UPROPERTY(Transient) TArray<TObjectPtr<UErlingUIText>> Labels;
    UPROPERTY(Transient) TMap<FName,TObjectPtr<UTextBlock>> Texts;
    int32 LastScreen = -1;
    int32 LastLanguage = -1;
    float Entrance = 1.f;
    void UpdateValues();
    void SetText(FName Name, const FString& Value);
    UFUNCTION() void MusicChanged(float Value);
    UFUNCTION() void EffectsChanged(float Value);
    UFUNCTION() void SensitivityChanged(float Value);
#if !UE_BUILD_SHIPPING
    int32 CheckStage = 0;
    double NextCheck = 0;
    FString CheckReport;
    TArray<int32> CheckAppearance;
    bool CheckFailed = false;
#endif
};
