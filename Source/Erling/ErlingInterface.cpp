#include "ErlingInterface.h"
#include "Football.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Slider.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Framework/Application/SlateApplication.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "HAL/PlatformProcess.h"
#include "InputCoreTypes.h"
#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

void UErlingUIText::SetLanguage(bool bPolish)
{
    const FText& Translation = bPolish ? Polish : English;
    const FText UppercaseTranslation = Translation.ToUpper();
    if (!GetText().EqualTo(UppercaseTranslation)) SetText(UppercaseTranslation);
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
    BuildControllerHints();
    RefreshScreen();
}

void UErlingInterface::NativePreConstruct()
{
    Super::NativePreConstruct();
    if (IsDesignTime()) ApplyDesignerPreview();
}

void UErlingInterface::ApplyDesignerPreview()
{
    if (!WidgetTree) return;
    const bool bPolish = DesignerPreviewLanguage == EErlingDesignerPreviewLanguage::Polish;
    TArray<UWidget*> Widgets;
    WidgetTree->GetAllWidgets(Widgets);
    for (UWidget* Widget : Widgets)
        if (UErlingUIText* Text = Cast<UErlingUIText>(Widget)) Text->SetLanguage(bPolish);
}

#if WITH_EDITOR
void UErlingInterface::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UErlingInterface, DesignerPreviewLanguage))
        ApplyDesignerPreview();
}
#endif

void UErlingInterface::SetText(FName Name, const FString& Value)
{
    if (UTextBlock* Text = Texts.FindRef(Name))
    {
        const FText UppercaseValue = FText::FromString(Value).ToUpper();
        if (!Text->GetText().EqualTo(UppercaseValue)) Text->SetText(UppercaseValue);
    }
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
    UpdateControllerHints();
    UpdateValues();
}

void UErlingInterface::BuildControllerHints()
{
    UCanvasPanel* Hints=Cast<UCanvasPanel>(GetWidgetFromName(TEXT("ControlHints")));
    if(!Hints) return;

    KeyboardHintWidgets.Reset();
    const int32 Language=Controller&&Controller->Saved?Controller->Saved->Language:0;
    TSet<UWidget*> KeyboardWidgets;
    for(UWidget* Child:Hints->GetAllChildren())
    {
        KeyboardHintWidgets.Add(Child);
        KeyboardWidgets.Add(Child);
    }

    // Some saved widget versions kept the mouse-button prompt in a sibling
    // canvas instead of under ControlHints. Capture that complete prompt group
    // too, so it cannot remain visible alongside the Xbox row.
    TArray<UWidget*> AllWidgets;
    if(WidgetTree) WidgetTree->GetAllWidgets(AllWidgets);
    for(UWidget* Widget:AllWidgets)
        if(UImage* Image=Cast<UImage>(Widget))
            if(const UObject* Resource=Image->GetBrush().GetResourceObject();Resource&&Resource->GetName()==TEXT("T_UI_Mouse_Outline"))
                if(UPanelWidget* Parent=Image->GetParent())
                    for(UWidget* Sibling:Parent->GetAllChildren())
                        if(Sibling&&!KeyboardWidgets.Contains(Sibling))
                        {
                            KeyboardWidgets.Add(Sibling);
                            KeyboardHintWidgets.Add(Sibling);
                        }

    UTextBlock* StyleText=nullptr;
    for(UWidget* Child:KeyboardHintWidgets)
        if((StyleText=Cast<UTextBlock>(Child))) break;
    const FSlateFontInfo HintFont=StyleText?StyleText->GetFont():FSlateFontInfo();
    const FSlateColor HintColor=StyleText?StyleText->GetColorAndOpacity():FSlateColor(FLinearColor::White);

    struct FControllerHint { const TCHAR* File; const TCHAR* En; const TCHAR* Pl; float X; float LabelWidth; };
    const FControllerHint HintsToBuild[]={
        {TEXT("analog.png"),TEXT("move"),TEXT("ruch"),18,140},
        {TEXT("lt.png"),TEXT("control"),TEXT("kontrola"),270,135},
        {TEXT("rt.png"),TEXT("sprint"),TEXT("sprint"),526,120},
        {TEXT("a.png"),TEXT("jump"),TEXT("skok"),770,105},
        {TEXT("x.png"),TEXT("shoot / pass"),TEXT("strzał / podanie"),996,240},
        {TEXT("b.png"),TEXT("slide"),TEXT("wślizg"),1345,115},
        {TEXT("menu.png"),TEXT("menu"),TEXT("menu"),1570,112}
    };
    const FString IconDirectory=FPaths::ProjectContentDir()/TEXT("Erling/UI/SourceArt/XboxController");
    IImageWrapperModule& ImageModule=FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    for(const FControllerHint& Hint:HintsToBuild)
    {
        TArray<uint8> Compressed,Pixels;
        if(!FFileHelper::LoadFileToArray(Compressed,*(IconDirectory/Hint.File))) continue;
        TSharedPtr<IImageWrapper> Wrapper=ImageModule.CreateImageWrapper(EImageFormat::PNG);
        if(!Wrapper.IsValid()||!Wrapper->SetCompressed(Compressed.GetData(),Compressed.Num())||!Wrapper->GetRaw(ERGBFormat::BGRA,8,Pixels)) continue;
        UTexture2D* Texture=UTexture2D::CreateTransient(Wrapper->GetWidth(),Wrapper->GetHeight(),PF_B8G8R8A8);
        if(!Texture) continue;
        FTexture2DMipMap& Mip=Texture->GetPlatformData()->Mips[0];
        void* Dest=Mip.BulkData.Lock(LOCK_READ_WRITE);
        FMemory::Memcpy(Dest,Pixels.GetData(),Pixels.Num());
        Mip.BulkData.Unlock();
        Texture->SRGB=true;
        Texture->UpdateResource();
        ControllerHintTextures.Add(Texture);

        UImage* Icon=NewObject<UImage>(Hints);
        Icon->SetBrushFromTexture(Texture,true);
        Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
        if(UCanvasPanelSlot* CanvasSlot=Hints->AddChildToCanvas(Icon))
        {
            CanvasSlot->SetPosition(FVector2D(Hint.X,-5));
            CanvasSlot->SetSize(FVector2D(64,64));
        }
        ControllerHintWidgets.Add(Icon);

        UTextBlock* Label=NewObject<UTextBlock>(Hints);
        Label->SetText(FText::FromString(Language==1?Hint.Pl:Hint.En));
        Label->SetFont(HintFont);
        Label->SetColorAndOpacity(HintColor);
        Label->SetVisibility(ESlateVisibility::HitTestInvisible);
        ControllerHintLabels.Add(Label);
        if(UCanvasPanelSlot* CanvasSlot=Hints->AddChildToCanvas(Label))
        {
            // The face's visible capital height sits below the center of its slot.
            // Lift the label so it centers optically in the HUD strip.
            CanvasSlot->SetPosition(FVector2D(Hint.X+80,-1));
            CanvasSlot->SetSize(FVector2D(Hint.LabelWidth,38));
        }
        ControllerHintWidgets.Add(Label);
    }
    bLastGamepadHint=!Controller||!Controller->bUsingGamepadInput;
    UpdateControllerHints();
}

void UErlingInterface::UpdateControllerHints()
{
    if(ControllerHintWidgets.IsEmpty()) return;
    const int32 Language=Controller&&Controller->Saved?Controller->Saved->Language:0;
    if(LastControllerHintLanguage!=Language)
    {
        LastControllerHintLanguage=Language;
        const TCHAR* English[]={TEXT("move"),TEXT("control"),TEXT("sprint"),TEXT("jump"),TEXT("shoot / pass"),TEXT("slide"),TEXT("menu")};
        const TCHAR* Polish[]={TEXT("ruch"),TEXT("kontrola"),TEXT("sprint"),TEXT("skok"),TEXT("strzał / podanie"),TEXT("wślizg"),TEXT("menu")};
        for(int32 Index=0;Index<ControllerHintLabels.Num();++Index)
            if(ControllerHintLabels[Index]) ControllerHintLabels[Index]->SetText(FText::FromString(Language==1?Polish[Index]:English[Index]));
    }
    const bool bGamepad=Controller&&Controller->bUsingGamepadInput;
    bLastGamepadHint=bGamepad;
    const ESlateVisibility KeyboardVisibility=bGamepad?ESlateVisibility::Collapsed:ESlateVisibility::HitTestInvisible;
    const ESlateVisibility ControllerVisibility=bGamepad?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed;
    for(const TObjectPtr<UWidget>& Widget:KeyboardHintWidgets)
        if(Widget&&Widget->GetVisibility()!=KeyboardVisibility) Widget->SetVisibility(KeyboardVisibility);
    for(const TObjectPtr<UWidget>& Widget:ControllerHintWidgets)
        if(Widget&&Widget->GetVisibility()!=ControllerVisibility) Widget->SetVisibility(ControllerVisibility);
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
    if (auto* Panel=GetWidgetFromName(TEXT("PlayerShotPanel")))
    {
        FVector2D Position;
        bool Visible=false;
        if (Shot && C->Avatar && C->Avatar->Face)
        {
            const FBoxSphereBounds& Bounds=C->Avatar->Face->Bounds;
            // Rendering bounds are enlarged for flips; remove that culling padding.
            const FVector AboveHead=Bounds.Origin+FVector(0,0,Bounds.BoxExtent.Z/FMath::Max(C->Avatar->Face->BoundsScale,0.01f)+12.f);
            if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(C,AboveHead,Position,false))
                if (auto* PlayerSlot=Cast<UCanvasPanelSlot>(Panel->Slot))
                {
                    constexpr float ShotPanelWidth=150.f;
                    constexpr float ShotPanelHeight=26.f;
                    PlayerSlot->SetAutoSize(false);
                    PlayerSlot->SetAnchors(FAnchors(0.f,0.f));
                    PlayerSlot->SetAlignment(FVector2D::ZeroVector);
                    PlayerSlot->SetSize(FVector2D(ShotPanelWidth,ShotPanelHeight));
                    const auto Viewport=UWidgetLayoutLibrary::GetViewportWidgetGeometry(C);
                    Position=Panel->GetParent()->GetCachedGeometry().AbsoluteToLocal(Viewport.LocalToAbsolute(Position));
                    PlayerSlot->SetPosition(Position-FVector2D(ShotPanelWidth*.5f,ShotPanelHeight));

                    // Keep the authored layers compact as well. A previous UI
                    // restyle left one of these child slots serialized at a much
                    // larger size, which produced a giant dark rectangle while
                    // the charge fill itself remained correctly sized.
                    if (auto* ShotCanvas=Cast<UCanvasPanel>(Panel))
                    {
                        for (UWidget* Child:ShotCanvas->GetAllChildren())
                            if (auto* ChildSlot=Cast<UCanvasPanelSlot>(Child->Slot))
                            {
                                ChildSlot->SetAutoSize(false);
                                ChildSlot->SetAnchors(FAnchors(0.f,0.f));
                                ChildSlot->SetAlignment(FVector2D::ZeroVector);
                                if (Child->GetFName()==TEXT("PlayerShotFill"))
                                {
                                    ChildSlot->SetPosition(FVector2D(10.f,9.f));
                                    ChildSlot->SetSize(FVector2D(130.f,8.f));
                                }
                                else
                                {
                                    ChildSlot->SetPosition(FVector2D::ZeroVector);
                                    ChildSlot->SetSize(FVector2D(ShotPanelWidth,ShotPanelHeight));
                                }
                            }
                    }
                    Visible=true;
                }
        }
        Panel->SetVisibility(Visible?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
    }
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
            if (auto* Bar=Cast<UProgressBar>(GetWidgetFromName(TEXT("PlayerShotFill")))) Bar->SetPercent(Charge/1.5f);
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
