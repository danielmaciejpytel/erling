#include "ErlingInterface.h"

#if !UE_BUILD_SHIPPING
#include "Football.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Slider.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "UnrealClient.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "InputCoreTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Input/HittestGrid.h"
#include "Layout/WidgetPath.h"

// Opt-in end-to-end checks run against ErlingProfile_Test, never the player's save.
void UErlingInterface::RunUIChecks()
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("ErlingUITest"))) return;
    const double Now=FPlatformTime::Seconds();
    if(NextCheck==0) { NextCheck=Now+6;return; }
    if(Now<NextCheck) return;
    auto* C=Controller.Get();
    if(!C || !C->Saved) return;
    FString Directory=FPaths::ProjectSavedDir()/TEXT("UIChecks");
    FParse::Value(FCommandLine::Get(),TEXT("ErlingUIReport="),Directory);
    IFileManager::Get().MakeDirectory(*Directory,true);
    auto Check=[this](const TCHAR* Name,bool Passed){CheckReport+=FString::Printf(TEXT("%s %s\n"),Passed?TEXT("PASS"):TEXT("FAIL"),Name);CheckFailed|=!Passed;UE_LOG(LogTemp,Display,TEXT("ERLING_UI_CHECK %s %s"),Passed?TEXT("PASS"):TEXT("FAIL"),Name);};
    auto Click=[this,&Check](FName Name){auto* B=Cast<UErlingUIButton>(GetWidgetFromName(Name));Check(*FString::Printf(TEXT("button_%s"),*Name.ToString()),B&&B->OnClicked.IsBound());if(B)B->OnClicked.Broadcast();};
    auto Key=[](FKey K){auto& App=FSlateApplication::Get();App.ProcessKeyDownEvent(FKeyEvent(K,FModifierKeysState(),0,false,0,0));App.ProcessKeyUpEvent(FKeyEvent(K,FModifierKeysState(),0,false,0,0));};
    // Offscreen windows have no visible OS window; route through their real UMG hit grid.
    auto MouseClick=[this,&Check](FName Name)
    {
        auto* B=GetWidgetFromName(Name);if(!B)return;
        auto& App=FSlateApplication::Get();const auto G=B->GetCachedGeometry();
        const FVector2D P=G.LocalToAbsolute(G.GetLocalSize()*.5f);
        auto Window=App.FindWidgetWindow(B->TakeWidget());if(!Window)return;
        auto Hits=Window->GetHittestGrid().GetBubblePath(P,0,false,0);FWidgetPath Path(MakeArrayView(Hits));
        Check(TEXT("mouse_hit_grid_finds_button"),Path.ContainsWidget(B->GetCachedWidget().Get()));
        TSet<FKey> Pressed;const FModifierKeysState Mod;
        App.RoutePointerMoveEvent(Path,FPointerEvent(0,P,P,Pressed,EKeys::Invalid,0,Mod),true);
        Pressed.Add(EKeys::LeftMouseButton);App.RoutePointerDownEvent(Path,FPointerEvent(0,P,P,Pressed,EKeys::LeftMouseButton,0,Mod));
        Pressed.Empty();App.RoutePointerUpEvent(Path,FPointerEvent(0,P,P,Pressed,EKeys::LeftMouseButton,0,Mod));
    };
    auto Shot=[&](const TCHAR* Name){FScreenshotRequest::RequestScreenshot(Directory/(FString(Name)+TEXT(".png")),true,false);};
    auto Bounds=[this,&Check](const TCHAR* Name){
        const auto Root=GetCachedGeometry();const FVector2D Size=Root.GetLocalSize();int32 Count=0;bool Valid=true;
        for(const auto& B:Buttons){UWidget* W=B;bool Visible=true;while(W){if(W->GetVisibility()==ESlateVisibility::Collapsed||W->GetVisibility()==ESlateVisibility::Hidden){Visible=false;break;}if(W->GetParent()==ScreenSwitcher && ScreenSwitcher->GetActiveWidget()!=W){Visible=false;break;}W=W->GetParent();}if(!Visible)continue;
            const auto G=B->GetCachedGeometry();const FVector2D A=Root.AbsoluteToLocal(G.LocalToAbsolute(FVector2D::ZeroVector));const FVector2D Z=Root.AbsoluteToLocal(G.LocalToAbsolute(G.GetLocalSize()));Valid&=A.X>=-1&&A.Y>=-1&&Z.X<=Size.X+1&&Z.Y<=Size.Y+1&&G.GetLocalSize().X>0&&G.GetLocalSize().Y>0;++Count;}
        Check(Name,Valid&&Count>0);
    };
    using S=AFootballController::EScreen;
    // Optional fixed-pose visual evidence for both collar sides after clothing edits.
    if(FParse::Param(FCommandLine::Get(),TEXT("CollarReview"))&&CheckStage>=24&&CheckStage<=29)
    {
        if(CheckStage==24){C->ChangeScreen(S::Editor);C->EditorZoom=1;C->Avatar->PreviewAnimation=true;C->Avatar->SetAnimation(TEXT("Idle_WeightShift"));C->Avatar->PlaybackRate=0;C->Avatar->AnimationTime=1.f;}
        if(CheckStage==25)Shot(TEXT("12_Collar_Left"));
        if(CheckStage==26)C->Avatar->AnimationTime=3.f;
        if(CheckStage==27)Shot(TEXT("13_Collar_Right"));
        if(CheckStage==28)C->Avatar->SetActorRotation(FRotator(0,145,0));
        if(CheckStage==29)Shot(TEXT("14_Collar_Back"));
        ++CheckStage;NextCheck=Now+1.1;return;
    }
    switch(CheckStage)
    {
    case 0:{int32 Width=1920,Height=1080;FParse::Value(FCommandLine::Get(),TEXT("ResX="),Width);FParse::Value(FCommandLine::Get(),TEXT("ResY="),Height);C->ConsoleCommand(FString::Printf(TEXT("r.SetRes %dx%dw"),Width,Height));}C->Saved->Language=1;C->Saved->PauseInSettings=false;C->Saved->Appearance={1,0,1,1,1};C->Selection=C->Saved->Appearance;C->Avatar->ApplyKit(C->Selection,C->Catalog);C->ChangeScreen(S::Main);RefreshScreen();Check(TEXT("designer_blueprint"),GetClass()->GetPathName().Contains(TEXT("WBP_ErlingInterface_C")));Check(TEXT("six_pages"),ScreenSwitcher&&ScreenSwitcher->GetNumWidgets()==6);break;
    case 1:Bounds(TEXT("main_bounds"));Check(TEXT("main_keyboard_focus"),GetWidgetFromName(TEXT("PlayButton"))->HasAnyUserFocus());Shot(TEXT("01_Main_PL"));break;
    case 2:MouseClick(TEXT("CharacterButton"));CheckAppearance=C->Selection;break;
    case 3:Check(TEXT("mouse_opens_character_screen"),C->Screen==S::Editor);Bounds(TEXT("character_bounds"));Shot(TEXT("02_Character_PL"));break;
    case 4:for(int32 I=0;I<C->Catalog.Num();++I){const int32 Old=C->Selection[I];Click(*FString::Printf(TEXT("AppearanceNext%d"),I));Check(*FString::Printf(TEXT("category_%d_next"),I),C->Selection[I]==(Old+1)%C->Catalog[I].Options.Num());Click(*FString::Printf(TEXT("AppearancePrevious%d"),I));Check(*FString::Printf(TEXT("category_%d_previous"),I),C->Selection[I]==Old);}Click(TEXT("PreviewNext"));Check(TEXT("preview_animation"),C->PreviewIndex>=0&&C->Avatar->PreviewAnimation);Click(TEXT("AppearanceNext2"));Click(TEXT("CharacterBackButton"));Check(TEXT("cancel_draft"),C->Screen==S::Main&&C->Selection==CheckAppearance);break;
    case 5:GetWidgetFromName(TEXT("CharacterButton"))->SetUserFocus(C);Key(EKeys::Gamepad_FaceButton_Bottom);Check(TEXT("gamepad_opens_character_screen"),C->Screen==S::Editor);Click(TEXT("AppearanceNext0"));Click(TEXT("SaveAppearanceButton"));{auto* Loaded=Cast<UFootballSave>(UGameplayStatics::LoadGameFromSlot(TEXT("ErlingProfile_Test"),0));Check(TEXT("appearance_saved"),Loaded&&Loaded->Appearance==C->Selection);}break;
    case 6:Shot(TEXT("03_Appearance_Saved"));break;
    case 7:Click(TEXT("CharacterBackButton"));Click(TEXT("SettingsButton"));break;
    case 8:{
        Bounds(TEXT("settings_bounds"));
        const TCHAR* Names[]={TEXT("MusicSlider"),TEXT("EffectsSlider"),TEXT("SensitivitySlider")};const float Values[]={.25f,.55f,.5f};
        for(int32 I=0;I<3;++I){auto* Slider=CastChecked<USlider>(GetWidgetFromName(Names[I]));Slider->SetValue(Values[I]);Slider->OnValueChanged.Broadcast(Values[I]);}
        Check(TEXT("music_slider"),FMath::IsNearlyEqual(C->Saved->Volume,.25f)&&C->Music&&FMath::IsNearlyEqual(C->Music->VolumeMultiplier,.25f));
        Check(TEXT("effects_slider"),FMath::IsNearlyEqual(C->Saved->EffectsVolume,.55f));Check(TEXT("sensitivity_slider"),FMath::IsNearlyEqual(C->Saved->Sensitivity,1.65f,1.e-4f));
        Click(TEXT("LanguageButton"));Check(TEXT("english_translation"),C->Saved->Language==0&&Texts.FindRef(TEXT("PlayButtonLabel"))->GetText().ToString()==TEXT("Play"));Click(TEXT("LanguageButton"));
        const int32 Camera=C->Saved->CameraMode;Click(TEXT("CameraButton"));Check(TEXT("camera_cycle"),C->Saved->CameraMode==(Camera+1)%3);C->Saved->CameraMode=Camera;C->Saved->ReducedMotion=Camera==1;
        Click(TEXT("PauseSettingButton"));Check(TEXT("pause_setting_on"),C->Saved->PauseInSettings&&C->IsPaused());Click(TEXT("PauseSettingButton"));Check(TEXT("pause_setting_off"),!C->Saved->PauseInSettings&&!C->IsPaused());RefreshScreen();break;}
    case 9:Shot(TEXT("04_Settings_PL"));break;
    case 10:Click(TEXT("SaveSettingsButton"));{auto* Loaded=Cast<UFootballSave>(UGameplayStatics::LoadGameFromSlot(TEXT("ErlingProfile_Test"),0));Check(TEXT("settings_persisted"),Loaded&&FMath::IsNearlyEqual(Loaded->Volume,.25f)&&FMath::IsNearlyEqual(Loaded->EffectsVolume,.55f)&&FMath::IsNearlyEqual(Loaded->Sensitivity,1.65f,1.e-4f));}Click(TEXT("CreditsButton"));break;
    case 11:Check(TEXT("credits_screen"),C->Screen==S::Credits);Bounds(TEXT("credits_bounds"));Shot(TEXT("05_Credits_PL"));break;
    case 12:Click(TEXT("CreditsBackButton"));Click(TEXT("PlayButton"));Check(TEXT("game_input_restored"),C->Screen==S::Game&&!C->IsPaused()&&!C->bShowMouseCursor);GetWorld()->GetAuthGameMode<AFootballMode>()->Goals=7;break;
    case 13:Check(TEXT("live_score"),Texts.FindRef(TEXT("GoalsValue"))->GetText().ToString()==TEXT("07"));Shot(TEXT("06_HUD_PL"));break;
    case 14:C->Charging=true;C->ChargeStarted=GetWorld()->GetTimeSeconds()-.9f;UpdateValues();Shot(TEXT("07_Shot_PL"));break;
    case 15:C->Charging=false;C->PauseToggle();break;
    case 16:Check(TEXT("pause_screen"),C->Screen==S::Pause&&C->IsPaused()&&C->bShowMouseCursor);Bounds(TEXT("pause_bounds"));Shot(TEXT("08_Pause_PL"));break;
    case 17:Key(EKeys::Tab);Check(TEXT("keyboard_tab_navigation"),GetWidgetFromName(TEXT("PauseSettingsButton"))->HasAnyUserFocus());Click(TEXT("PauseSettingsButton"));Check(TEXT("settings_from_pause"),C->Screen==S::Settings&&C->SettingsReturn==S::Pause&&C->IsPaused());Click(TEXT("SaveSettingsButton"));Check(TEXT("return_to_pause"),C->Screen==S::Pause&&C->IsPaused());Click(TEXT("ResumeButton"));Check(TEXT("resume_game"),C->Screen==S::Game&&!C->IsPaused());break;
    case 18:C->ChangeScreen(S::Editor);C->Saved->Language=0;RefreshScreen();break;
    case 19:Shot(TEXT("09_Character_EN"));break;
    case 20:Key(EKeys::Escape);Check(TEXT("escape_returns_from_character"),C->Screen==S::Main);break;
    case 21:Shot(TEXT("10_Main_EN"));break;
    case 22:C->ChangeScreen(S::Settings);break;
    case 23:Shot(TEXT("11_Settings_EN"));break;
    default:
        FFileHelper::SaveStringToFile(CheckReport,*(Directory/TEXT("checks.txt")));
        UE_LOG(LogTemp,Display,TEXT("ERLING_UI_CHECKS_COMPLETE: %s"),CheckFailed?TEXT("FAIL"):TEXT("PASS"));
        FPlatformMisc::RequestExitWithStatus(false,CheckFailed?1:0);break;
    }
    ++CheckStage;NextCheck=Now+1.1;
}
#endif
