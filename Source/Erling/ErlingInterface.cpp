#include "ErlingInterface.h"
#include "Football.h"
#include "Blueprint/WidgetTree.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Slider.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformProcess.h"
#include "InputCoreTypes.h"

void UErlingUIText::SetLanguage(bool bPolish)
{
    const FText& Translation = bPolish ? Polish : English;
    if (!GetText().EqualTo(Translation)) SetText(Translation);
}

void UErlingUIButton::Connect(UErlingInterface* InInterface)
{
    Interface = InInterface;
    UnfocusedStyle = GetStyle();
    OnClicked.AddUniqueDynamic(this, &UErlingUIButton::Dispatch);
}

void UErlingUIButton::Dispatch()
{
    if (Interface) Interface->HandleAction(Action, CategoryIndex);
}

void UErlingUIButton::UpdateFocusStyle()
{
    const bool Focused = HasAnyUserFocus();
    if (Focused == bFocusStyle) return;
    bFocusStyle = Focused;
    FButtonStyle Style = UnfocusedStyle;
    if (Focused) Style.SetNormal(UnfocusedStyle.Hovered);
    SetStyle(Style);
}

void UErlingInterface::NativeConstruct()
{
    Super::NativeConstruct();
    Controller = Cast<AFootballController>(GetOwningPlayer());
    SetIsFocusable(true);
    ScreenSwitcher = Cast<UWidgetSwitcher>(GetWidgetFromName(TEXT("Screens")));
    Buttons.Reset(); Labels.Reset(); Texts.Reset();
    TArray<UWidget*> Widgets;
    WidgetTree->GetAllWidgets(Widgets);
    for (UWidget* Widget : Widgets)
    {
        if (auto* Button = Cast<UErlingUIButton>(Widget)) { Button->Connect(this); Buttons.Add(Button); }
        if (auto* Label = Cast<UErlingUIText>(Widget)) Labels.Add(Label);
        if (auto* Text = Cast<UTextBlock>(Widget)) Texts.Add(Widget->GetFName(), Text);
    }
    if (auto* Slider = Cast<USlider>(GetWidgetFromName(TEXT("MusicSlider")))) Slider->OnValueChanged.AddUniqueDynamic(this, &UErlingInterface::MusicChanged);
    if (auto* Slider = Cast<USlider>(GetWidgetFromName(TEXT("EffectsSlider")))) Slider->OnValueChanged.AddUniqueDynamic(this, &UErlingInterface::EffectsChanged);
    if (auto* Slider = Cast<USlider>(GetWidgetFromName(TEXT("SensitivitySlider")))) Slider->OnValueChanged.AddUniqueDynamic(this, &UErlingInterface::SensitivityChanged);
    RefreshScreen();
}

void UErlingInterface::SetText(FName Name, const FString& Value)
{
    if (UTextBlock* Text = Texts.FindRef(Name))
        if (Text->GetText().ToString() != Value) Text->SetText(FText::FromString(Value));
}

void UErlingInterface::RefreshScreen()
{
    if (!Controller || !Controller->Saved || !ScreenSwitcher) return;
    const int32 Screen = static_cast<int32>(Controller->Screen);
    ScreenSwitcher->SetActiveWidgetIndex(Screen);
    const bool Changed = LastScreen != Screen;
    if (Changed)
    {
        LastScreen = Screen;
        Entrance = 0.f;
        // Focus a real button so keyboard and gamepad navigation work immediately.
        const FName First[] = {TEXT("PlayButton"), TEXT("AppearanceNext0"), NAME_None,
            TEXT("ResumeButton"), TEXT("LanguageButton"), TEXT("CreditsBackButton")};
        if (Screen != 2) if (UWidget* Button = GetWidgetFromName(First[Screen])) Button->SetUserFocus(Controller);
        const TCHAR* Names[] = {TEXT("MusicSlider"), TEXT("EffectsSlider"), TEXT("SensitivitySlider")};
        const float Values[] = {Controller->Saved->Volume, Controller->Saved->EffectsVolume, (Controller->Saved->Sensitivity-.3f)/2.7f};
        for (int32 I=0; I<3; ++I) if (auto* Slider=Cast<USlider>(GetWidgetFromName(Names[I]))) Slider->SetValue(Values[I]);
    }
    if (LastLanguage != Controller->Saved->Language)
    {
        LastLanguage = Controller->Saved->Language;
        for (const auto& Label : Labels) Label->SetLanguage(LastLanguage == 1);
    }
    UpdateValues();
}

void UErlingInterface::HandleAction(EErlingUIAction Action, int32 Index)
{
    if (!Controller || !Controller->Saved) return;
    auto* C=Controller.Get();
    auto* S=C->Saved;
    using Screen=AFootballController::EScreen;
    C->PlayEffect(TEXT("click"));
    switch (Action)
    {
    case EErlingUIAction::Play: C->ChangeScreen(Screen::Game); break;
    case EErlingUIAction::Character: C->ChangeScreen(Screen::Editor); break;
    case EErlingUIAction::Settings: C->SettingsReturn=C->Screen; C->ChangeScreen(Screen::Settings); break;
    case EErlingUIAction::Credits: C->ChangeScreen(Screen::Credits); break;
    case EErlingUIAction::Quit: C->Quit(); break;
    case EErlingUIAction::MainMenu: C->ChangeScreen(Screen::Main); break;
    case EErlingUIAction::Back: C->PauseToggle(); break;
    case EErlingUIAction::SaveAppearance: C->SaveAppearance(); break;
    case EErlingUIAction::AppearancePrevious: C->Cycle(Index,-1); break;
    case EErlingUIAction::AppearanceNext: C->Cycle(Index,1); break;
    case EErlingUIAction::PreviewPrevious: C->CycleAnimationPreview(-1); break;
    case EErlingUIAction::PreviewNext: C->CycleAnimationPreview(1); break;
    case EErlingUIAction::Language: S->Language=1-S->Language; C->SaveSettings(); break;
    case EErlingUIAction::Camera: S->CameraMode=(S->CameraMode+1)%3; S->ReducedMotion=S->CameraMode==1; C->CameraPivotInitialized=false; C->PitchCameraLeadX=0; break;
    case EErlingUIAction::Quality: S->Quality=(S->Quality+1)%4; C->ApplyQuality(); break;
    case EErlingUIAction::PauseInSettings: S->PauseInSettings=!S->PauseInSettings; C->SetPause(C->SettingsReturn==Screen::Pause || S->PauseInSettings); break;
    case EErlingUIAction::SaveSettings: C->SaveSettings(); C->ChangeScreen(C->SettingsReturn); break;
    case EErlingUIAction::ArtStation: FPlatformProcess::LaunchURL(TEXT("https://www.artstation.com/danielmaciejpytel"),nullptr,nullptr); break;
    case EErlingUIAction::GitHub: FPlatformProcess::LaunchURL(TEXT("https://github.com/danielmaciejpytel"),nullptr,nullptr); break;
    case EErlingUIAction::LinkedIn: FPlatformProcess::LaunchURL(TEXT("https://www.linkedin.com/in/danielmaciejpytel/"),nullptr,nullptr); break;
    }
    RefreshScreen();
}

void UErlingInterface::MusicChanged(float Value) { if (Controller) Controller->UpdateMusicVolume(Value); UpdateValues(); }
void UErlingInterface::EffectsChanged(float Value) { if (Controller) Controller->UpdateEffectsVolume(Value); UpdateValues(); }
void UErlingInterface::SensitivityChanged(float Value) { if (Controller && Controller->Saved) Controller->Saved->Sensitivity=.3f+Value*2.7f; UpdateValues(); }

FReply UErlingInterface::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Key)
{
    if (Controller && (Key.GetKey()==EKeys::Escape || Key.GetKey()==EKeys::Gamepad_FaceButton_Right || Key.GetKey()==EKeys::Gamepad_Special_Right))
    {
        Controller->PauseToggle();
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(Geometry, Key);
}

bool UErlingInterface::IsPointerOverPanel() const
{
    if (!Controller || Controller->Screen!=AFootballController::EScreen::Editor || !FSlateApplication::IsInitialized()) return false;
    const FVector2D PointerPosition=FSlateApplication::Get().GetCursorPos();
    for (const TCHAR* Name : {TEXT("CharacterPanel"),TEXT("SaveAppearanceButton")})
        if (const UWidget* Panel=GetWidgetFromName(Name)) if (Panel->GetCachedGeometry().IsUnderLocation(PointerPosition)) return true;
    return false;
}

void UErlingInterface::NativeTick(const FGeometry& Geometry, float Dt)
{
    Super::NativeTick(Geometry, Dt);
    if (!Controller) return;
    Entrance=FMath::Min(1.f, Entrance+Dt/0.18f);
    if (ScreenSwitcher) ScreenSwitcher->SetRenderOpacity(Entrance);
    for (const auto& Button : Buttons) Button->UpdateFocusStyle();
    UpdateValues();
}

void UErlingInterface::UpdateValues()
{
    if (!Controller || !Controller->Saved) return;
    auto* C=Controller.Get();
    auto* S=C->Saved;
    auto L=[C](const TCHAR* En,const TCHAR* Pl){return C->Localize(En,Pl);};
    if (C->Screen==AFootballController::EScreen::Editor)
    {
        for (int32 I=0; I<C->Catalog.Num(); ++I) SetText(*FString::Printf(TEXT("AppearanceValue%d"),I), C->OptionText(I).ToString());
        SetText(TEXT("PreviewValue"), C->PreviewAnimationText().ToString());
    }
    if (C->Screen==AFootballController::EScreen::Settings)
    {
        const TCHAR* FillNames[]={TEXT("MusicSliderFill"),TEXT("EffectsSliderFill"),TEXT("SensitivitySliderFill")};
        const float FillValues[]={S->Volume,S->EffectsVolume,(S->Sensitivity-.3f)/2.7f};
        for(int32 I=0;I<3;++I) if(auto* Fill=Cast<UProgressBar>(GetWidgetFromName(FillNames[I]))) Fill->SetPercent(FillValues[I]);
        SetText(TEXT("LanguageValue"),S->Language==1?TEXT("Polski"):TEXT("English"));
        SetText(TEXT("MusicValue"),FString::Printf(TEXT("%.0f%%"),S->Volume*100));
        SetText(TEXT("EffectsValue"),FString::Printf(TEXT("%.0f%%"),S->EffectsVolume*100));
        SetText(TEXT("SensitivityValue"),FString::Printf(TEXT("%.1f"),S->Sensitivity));
        SetText(TEXT("CameraValue"),S->CameraMode==1?L(TEXT("Reduced"),TEXT("Ograniczony")):S->CameraMode==2?L(TEXT("Pitch"),TEXT("Boiskowy")):L(TEXT("Smooth"),TEXT("Płynny")));
        const TCHAR* En[]={TEXT("Low"),TEXT("Medium"),TEXT("High"),TEXT("Ultra")};
        const TCHAR* Pl[]={TEXT("Niska"),TEXT("Średnia"),TEXT("Wysoka"),TEXT("Ultra")};
        SetText(TEXT("QualityValue"),L(En[FMath::Clamp(S->Quality,0,3)],Pl[FMath::Clamp(S->Quality,0,3)]));
        SetText(TEXT("PauseValue"),S->PauseInSettings?L(TEXT("On"),TEXT("Wł.")):L(TEXT("Off"),TEXT("Wył.")));
    }
    const bool Game=C->Screen==AFootballController::EScreen::Game;
    const bool Shot=Game&&(C->Charging||C->ShotBufferActive);
    if (auto* Panel=GetWidgetFromName(TEXT("ShotPanel"))) Panel->SetVisibility(Shot?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
    if (Game)
    {
        const auto* Mode=GetWorld()->GetAuthGameMode<AFootballMode>();
        SetText(TEXT("GoalsValue"),FString::Printf(TEXT("%02d"),Mode?Mode->Goals:0));
        if (Shot)
        {
            const float Charge=C->Charging?FMath::Min(1.5f,GetWorld()->GetTimeSeconds()-C->ChargeStarted):C->ShotBufferSeconds;
            FString State=Charge>=1.35f?L(TEXT("OVER THE BAR!"),TEXT("ZA WYSOKO!")):Charge>=.85f&&Charge<=1.02f?L(TEXT("UNDER THE BAR"),TEXT("POD POPRZECZKĘ")):Charge>=.35f?L(TEXT("POWER SHOT"),TEXT("MOCNY STRZAŁ")):L(TEXT("PASS"),TEXT("PODANIE"));
            if (C->ShotBufferActive) State+=L(TEXT(" · QUEUED"),TEXT(" · OCZEKUJE"));
            SetText(TEXT("ShotValue"),State);
            if (auto* Bar=Cast<UProgressBar>(GetWidgetFromName(TEXT("ShotFill")))) Bar->SetPercent(Charge/1.5f);
        }
    }
    FString Message;
    if (Game&&!Shot&&!C->GoalCelebrationUsed&&GetWorld()->GetTimeSeconds()<C->GoalUntil)
        Message=L(TEXT("SPACE: jump  ·  hold 1 s: celebrate"),TEXT("SPACJA: skok  ·  przytrzymaj 1 s: cieszynka"));
    else if (!Shot && GetWorld()->GetTimeSeconds()<C->ToastUntil) Message=C->Toast;
    SetText(TEXT("ToastValue"),Message);
    if (auto* Panel=GetWidgetFromName(TEXT("ToastPanel")))
    {
        Panel->SetVisibility(Message.IsEmpty()?ESlateVisibility::Collapsed:ESlateVisibility::HitTestInvisible);
        if(auto* CanvasSlot=Cast<UCanvasPanelSlot>(Panel->Slot)) CanvasSlot->SetPosition(Game?FVector2D(545,148):FVector2D(980,62));
    }
}
