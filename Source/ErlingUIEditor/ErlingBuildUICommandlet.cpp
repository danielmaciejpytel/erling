#include "ErlingBuildUICommandlet.h"
#include "ErlingInterface.h"
#include "Modules/ModuleManager.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Image.h"
#include "Components/Slider.h"
#include "Components/ProgressBar.h"
#include "Components/ButtonSlot.h"
#include "Engine/Texture2D.h"
#include "Engine/Font.h"
#include "Brushes/SlateNoResource.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SavePackage.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, ErlingUIEditor)

namespace ErlingUIAuthoring
{
const FLinearColor White=FLinearColor::FromSRGBColor(FColor(246,247,250));
const FLinearColor Muted=FLinearColor::FromSRGBColor(FColor(183,190,204));
const FLinearColor Pink=FLinearColor::FromSRGBColor(FColor(246,44,142));

class FLayout
{
public:
    UWidgetTree* Tree;
    TMap<FString,TSharedPtr<FJsonObject>> Assets;
    int32 Serial=0;
    int32 Missing=0;
    explicit FLayout(UWidgetTree* InTree):Tree(InTree) {}
    template<class T> T* Make(const FString& Name=TEXT(""))
    {
        auto* Widget=Tree->ConstructWidget<T>(T::StaticClass(), *(!Name.IsEmpty()?Name:FString::Printf(TEXT("Element_%03d"),++Serial)));
        Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        return Widget;
    }
    FSlateBrush Brush(const FString& Name)
    {
        FSlateBrush B;
        auto* Texture=LoadObject<UTexture2D>(nullptr,*FString::Printf(TEXT("/Game/Erling/UI/Textures/%s.%s"),*Name,*Name));
        if (!Texture) { UE_LOG(LogTemp,Error,TEXT("Missing UI texture: %s"),*Name); ++Missing; return B; }
        B.SetResourceObject(Texture);
        B.TintColor=FLinearColor::White;
        B.DrawAs=ESlateBrushDrawType::Image;
        B.ImageSize=FVector2D(Texture->GetSizeX(),Texture->GetSizeY());
        if (auto* Entry=Assets.Find(Name))
        {
            const auto& M=(*Entry)->GetArrayField(TEXT("margin_LTRB"));
            const auto& S=(*Entry)->GetArrayField(TEXT("suggested_slate_size"));
            B.Margin=FMargin(M[0]->AsNumber(),M[1]->AsNumber(),M[2]->AsNumber(),M[3]->AsNumber());
            B.ImageSize=FVector2D(S[0]->AsNumber(),S[1]->AsNumber());
            if ((*Entry)->GetStringField(TEXT("draw_as"))==TEXT("Box")) B.DrawAs=ESlateBrushDrawType::Box;
        }
        return B;
    }
    void At(UCanvasPanel* Parent,UWidget* Widget,float X,float Y,float W,float H)
    {
        auto* Slot=Parent->AddChildToCanvas(Widget);
        Slot->SetPosition(FVector2D(X,Y)); Slot->SetSize(FVector2D(W,H));
    }
    UCanvasPanel* Canvas(UCanvasPanel* Parent,const FString& Name,float X,float Y,float W,float H)
    {
        auto* C=Make<UCanvasPanel>(Name); At(Parent,C,X,Y,W,H); return C;
    }
    UImage* Image(UCanvasPanel* Parent,const FString& Asset,float X,float Y,float W,float H,float Opacity=1.f)
    {
        auto* I=Make<UImage>(); I->SetBrush(Brush(Asset)); I->SetVisibility(ESlateVisibility::HitTestInvisible); I->SetRenderOpacity(Opacity); At(Parent,I,X,Y,W,H); return I;
    }
    UTextBlock* Text(UCanvasPanel* Parent,const FString& Name,const FString& En,const FString& Pl,
        float X,float Y,float W,float H,int32 Size=22,FLinearColor Color=White,bool Bold=false,bool Center=false)
    {
        auto* T=Make<UErlingUIText>(Name); T->English=FText::FromString(En); T->Polish=FText::FromString(Pl); T->SetLanguage(true);
        FSlateFontInfo Font(LoadObject<UFont>(nullptr,TEXT("/Engine/EngineFonts/Roboto.Roboto")),Size,Bold?FName(TEXT("Bold")):FName(TEXT("Regular")));
        T->SetFont(Font); T->SetColorAndOpacity(Color); T->SetJustification(Center?ETextJustify::Center:ETextJustify::Left);
        T->SetVisibility(ESlateVisibility::HitTestInvisible); At(Parent,T,X,Y,W,H); return T;
    }
    UCanvasPanel* Panel(UCanvasPanel* Parent,const FString& Name,float X,float Y,float W,float H,bool Accent=false)
    {
        auto* C=Canvas(Parent,Name,X,Y,W,H);
        Image(C,TEXT("T_UI_Panel_Dark"),0,0,W,H,.96f);
        Image(C,TEXT("T_UI_Panel_SheenOverlay"),0,0,W,H,.11f);
        Image(C,Accent?TEXT("T_UI_Panel_Frame_Pink"):TEXT("T_UI_Panel_Frame_Gray"),0,0,W,H,Accent?.5f:.32f);
        return C;
    }
    UErlingUIButton* Button(UCanvasPanel* Parent,const FString& Name,EErlingUIAction Action,
        const FString& En,const FString& Pl,const FString& Icon,float X,float Y,float W,float H,bool Primary=false,int32 Index=0)
    {
        if(Primary) Image(Parent,TEXT("T_UI_Glow_Button_Pink"),X-7,Y-2,W+14,H+4,.45f);
        auto* B=Make<UErlingUIButton>(Name); B->Action=Action; B->CategoryIndex=Index; B->SetVisibility(ESlateVisibility::Visible);
        FButtonStyle Style; const FString Base=Primary?TEXT("T_UI_Button_Primary_"):TEXT("T_UI_Button_Secondary_");
        Style.SetNormal(Brush(Base+TEXT("Normal"))).SetHovered(Brush(Base+TEXT("Hovered"))).SetPressed(Brush(Base+TEXT("Pressed"))).SetDisabled(Brush(Base+TEXT("Disabled")));
        Style.NormalPadding=FMargin(0); Style.PressedPadding=FMargin(0,1,0,-1); B->SetStyle(Style);
        auto* Body=Make<UCanvasPanel>(); B->AddChild(Body); auto* ButtonSlot=CastChecked<UButtonSlot>(Body->Slot); ButtonSlot->SetPadding(FMargin(0)); ButtonSlot->SetHorizontalAlignment(HAlign_Fill); ButtonSlot->SetVerticalAlignment(VAlign_Fill);
        if (!Icon.IsEmpty()) Image(Body,Icon,28,(H-36)/2,36,36);
        Text(Body,Name+TEXT("Label"),En,Pl,Icon.IsEmpty()?26:98,(H-34)/2,W-(Icon.IsEmpty()?80:150),38,22,White,Primary);
        Image(Body,TEXT("T_UI_Icon_ChevronRight_White"),W-48,(H-30)/2,30,30,.8f);
        At(Parent,B,X,Y,W,H); return B;
    }
    void Arrow(UCanvasPanel* Parent,const FString& Name,EErlingUIAction Action,int32 Index,float X,float Y,bool Right)
    {
        auto* B=Make<UErlingUIButton>(Name); B->Action=Action; B->CategoryIndex=Index; B->SetVisibility(ESlateVisibility::Visible);
        FButtonStyle S;
        S.SetNormal(Brush(TEXT("T_UI_Button_Square_Normal"))).SetHovered(Brush(TEXT("T_UI_Button_Square_Hovered"))).SetPressed(Brush(TEXT("T_UI_Button_Square_Pressed"))).SetDisabled(Brush(TEXT("T_UI_Button_Square_Disabled")));
        S.NormalPadding=FMargin(14); S.PressedPadding=FMargin(14,15,14,13); B->SetStyle(S);
        auto* I=Make<UImage>(); I->SetBrush(Brush(Right?TEXT("T_UI_Icon_ChevronRight_White"):TEXT("T_UI_Icon_ChevronLeft_White"))); I->SetColorAndOpacity(Right?Pink:Muted); I->SetVisibility(ESlateVisibility::HitTestInvisible); B->AddChild(I);
        At(Parent,B,X,Y,56,56);
    }
    void Heading(UCanvasPanel* C,const FString& En,const FString& Pl,const FString& SubEn,const FString& SubPl,float W,int32 Size=40)
    {
        Text(C,TEXT(""),En,Pl,48,34,W-96,78,Size,White,true);
        Image(C,TEXT("T_UI_Title_Underline"),48,113,62,6);
        Text(C,TEXT(""),SubEn,SubPl,48,139,W-96,64,19,Muted);
    }
    void SettingRow(UCanvasPanel* C,const FString& Name,EErlingUIAction Action,const FString& En,const FString& Pl,const FString& Value,float Y)
    {
        auto* B=Button(C,Name,Action,En,Pl,TEXT(""),38,Y,636,68);
        auto* Body=CastChecked<UCanvasPanel>(B->GetContent());
        Text(Body,Value,TEXT(""),TEXT(""),326,20,245,38,20,Pink,false,true);
        if(auto* L=Cast<UCanvasPanelSlot>(Body->GetChildAt(0)->Slot)) L->SetSize(FVector2D(308,38));
    }
    void Slider(UCanvasPanel* C,const FString& Name,const FString& Value,const FString& En,const FString& Pl,float Y)
    {
        Text(C,TEXT(""),En,Pl,48,Y,330,34,20,Muted);
        Text(C,Value,TEXT("70%"),TEXT("70%"),560,Y,98,34,20,White,true,true);
        auto* Fill=Make<UProgressBar>(Name+TEXT("Fill")); FProgressBarStyle FillStyle; FillStyle.SetBackgroundImage(Brush(TEXT("T_UI_Slider_Track"))).SetFillImage(Brush(TEXT("T_UI_Slider_Fill_White")));Fill->SetWidgetStyle(FillStyle);Fill->SetFillColorAndOpacity(Pink);Fill->SetPercent(.7f);At(C,Fill,66,Y+53,572,7);
        auto* S=Make<USlider>(Name); S->SetVisibility(ESlateVisibility::Visible);
        FSliderStyle Style; Style.SetNormalBarImage(FSlateNoResource()).SetHoveredBarImage(FSlateNoResource()).SetDisabledBarImage(FSlateNoResource());
        Style.SetNormalThumbImage(Brush(TEXT("T_UI_Slider_Thumb_Normal"))).SetHoveredThumbImage(Brush(TEXT("T_UI_Slider_Thumb_Hovered"))).SetDisabledThumbImage(Brush(TEXT("T_UI_Slider_Thumb_Disabled"))).SetBarThickness(7.f);
        Style.NormalThumbImage.ImageSize=FVector2D(36,36);Style.HoveredThumbImage.ImageSize=FVector2D(38,38);Style.DisabledThumbImage.ImageSize=FVector2D(36,36);
        S->SetWidgetStyle(Style);S->SetStepSize(.05f);S->SetValue(.7f);At(C,S,48,Y+40,608,32);
    }
    void Hint(UCanvasPanel* C,float X,float W,const FString& Key,const FString& En,const FString& Pl,bool Mouse=false)
    {
        const float KeyW=Mouse?38:Key==TEXT("W A S D")?112:Key.Len()>4?76:Key.Len()>1?64:34;
        Image(C,Mouse?TEXT("T_UI_Mouse_Outline"):TEXT("T_UI_Keycap_Wide_Normal"),X,-5,KeyW,46);
        if(!Mouse) Text(C,TEXT(""),Key,Key==TEXT("SPACE")?TEXT("SPACJA"):Key,X,6,KeyW,27,13,White,true,true);
        Text(C,TEXT(""),En,Pl,X+KeyW+12,6,W-KeyW-12,30,15,White);
    }
    void Build()
    {
        auto* Scale=Make<UScaleBox>(TEXT("ResponsiveScale")); Scale->SetStretch(EStretch::ScaleToFit); Tree->RootWidget=Scale;
        auto* Size=Make<USizeBox>(TEXT("DesignResolution")); Size->SetWidthOverride(1920);Size->SetHeightOverride(1080); Scale->AddChild(Size);
        auto* ScaleSlot=CastChecked<UScaleBoxSlot>(Size->Slot);ScaleSlot->SetHorizontalAlignment(HAlign_Center);ScaleSlot->SetVerticalAlignment(VAlign_Center);
        auto* Root=Make<UCanvasPanel>(TEXT("Root")); Size->AddChild(Root);
        auto* Switch=Make<UWidgetSwitcher>(TEXT("Screens")); At(Root,Switch,0,0,1920,1080);
        const TCHAR* PageNames[]={TEXT("MainMenu"),TEXT("CharacterCreator"),TEXT("GameHUD"),TEXT("PauseMenu"),TEXT("SettingsMenu"),TEXT("CreditsMenu")};
        UCanvasPanel* Pages[6];for(int32 I=0;I<6;++I){Pages[I]=Make<UCanvasPanel>(PageNames[I]);Switch->AddChild(Pages[I]);}
        auto* Main=Panel(Pages[0],TEXT("MainPanel"),76,108,580,808,true);
        auto* Logo=Text(Main,TEXT("BrandTitle"),TEXT("ERLING"),TEXT("ERLING"),52,32,486,132,90,White,true);
        FSlateFontInfo LogoFont=Logo->GetFont();LogoFont.TypefaceFontName=TEXT("Bold");Logo->SetFont(LogoFont);
        auto* Club=Text(Main,TEXT(""),TEXT("F O O T B A L L   C L U B"),TEXT("F O O T B A L L   C L U B"),60,159,468,40,20,Pink,true);
        Image(Main,TEXT("T_UI_Title_Underline"),60,208,58,5);
        Text(Main,TEXT(""),TEXT("Your player. Your pitch."),TEXT("Twój zawodnik. Twoje boisko."),60,236,480,42,22,Muted);
        Button(Main,TEXT("PlayButton"),EErlingUIAction::Play,TEXT("Play"),TEXT("Graj"),TEXT("T_UI_Icon_Play_Accent"),38,310,504,86,true);
        Button(Main,TEXT("CharacterButton"),EErlingUIAction::Character,TEXT("Character Creator"),TEXT("Edytor postaci"),TEXT("T_UI_Icon_Person_Silver"),38,406,504,86);
        Button(Main,TEXT("SettingsButton"),EErlingUIAction::Settings,TEXT("Settings"),TEXT("Ustawienia"),TEXT("T_UI_Icon_Settings_Silver"),38,502,504,86);
        Button(Main,TEXT("CreditsButton"),EErlingUIAction::Credits,TEXT("Credits"),TEXT("Twórcy"),TEXT("T_UI_Icon_Group_Silver"),38,598,504,86);
        Button(Main,TEXT("QuitButton"),EErlingUIAction::Quit,TEXT("Quit"),TEXT("Wyjdź"),TEXT("T_UI_Icon_Exit_Silver"),38,694,504,86);

        auto* Character=Panel(Pages[1],TEXT("CharacterPanel"),64,66,770,950);
        Heading(Character,TEXT("YOUR PLAYER"),TEXT("TWÓJ ZAWODNIK"),TEXT("Create your player. Your pitch. Your rules."),TEXT("Stwórz swojego zawodnika. Twoje boisko. Twoje zasady."),770,43);
        const TCHAR* Icons[]={TEXT("Hair"),TEXT("Face"),TEXT("Shirt"),TEXT("Shorts"),TEXT("Boot")};
        const TCHAR* En[]={TEXT("Hairstyle"),TEXT("Face"),TEXT("Top"),TEXT("Bottom"),TEXT("Footwear")};
        const TCHAR* Pl[]={TEXT("Fryzura"),TEXT("Twarz"),TEXT("Góra"),TEXT("Dół"),TEXT("Buty")};
        for(int32 I=0;I<5;++I)
        {
            const float Y=215+I*106;
            Image(Character,TEXT("T_UI_OptionRow_Normal"),36,Y,698,94);
            Image(Character,FString::Printf(TEXT("T_UI_Icon_%s_Silver"),Icons[I]),64,Y+25,44,44);
            Text(Character,TEXT(""),En[I],Pl[I],138,Y+32,160,40,20,Muted);
            Arrow(Character,FString::Printf(TEXT("AppearancePrevious%d"),I),EErlingUIAction::AppearancePrevious,I,306,Y+19,false);
            Text(Character,FString::Printf(TEXT("AppearanceValue%d"),I),TEXT(""),TEXT(""),370,Y+31,282,40,21,White,false,true);
            Arrow(Character,FString::Printf(TEXT("AppearanceNext%d"),I),EErlingUIAction::AppearanceNext,I,660,Y+19,true);
        }
        Text(Character,TEXT(""),TEXT("Animation preview"),TEXT("Podgląd animacji"),50,768,250,34,17,Muted);
        Arrow(Character,TEXT("PreviewPrevious"),EErlingUIAction::PreviewPrevious,0,306,756,false);
        Text(Character,TEXT("PreviewValue"),TEXT("Idle"),TEXT("Bezczynność"),370,773,282,38,15,Muted,false,true);
        Arrow(Character,TEXT("PreviewNext"),EErlingUIAction::PreviewNext,0,660,756,true);
        Button(Character,TEXT("CharacterBackButton"),EErlingUIAction::Back,TEXT("Back"),TEXT("Wstecz"),TEXT("T_UI_Icon_ArrowLeft_White"),36,842,298,74);
        Button(Pages[1],TEXT("SaveAppearanceButton"),EErlingUIAction::SaveAppearance,TEXT("Save appearance"),TEXT("Zapisz wygląd"),TEXT(""),1502,928,350,82,true);
        Image(Pages[1],TEXT("T_UI_Panel_Dark"),990,1024,800,42,.8f);
        Text(Pages[1],TEXT(""),TEXT("Drag to rotate  ·  Scroll to zoom"),TEXT("Przeciągnij, aby obrócić  ·  Kółko myszy: zoom"),912,1026,920,36,16,Muted,false,true);

        auto* Goals=Canvas(Pages[2],TEXT("GoalsPanel"),782,32,356,90);
        Image(Goals,TEXT("T_UI_HUD_Goals_Backplate"),0,0,356,90);
        Text(Goals,TEXT(""),TEXT("GOALS"),TEXT("GOLE"),40,24,180,52,27,White,true);
        Text(Goals,TEXT("GoalsValue"),TEXT("00"),TEXT("00"),233,16,90,66,39,Pink,true,true);
        Image(Pages[2],TEXT("T_UI_Panel_Dark"),42,975,1810,90,.78f);
        auto* Hints=Canvas(Pages[2],TEXT("ControlHints"),70,1009,1780,42);
        Hint(Hints,0,206,TEXT("W A S D"),TEXT("move"),TEXT("ruch"));
        Hint(Hints,216,178,TEXT("CTRL"),TEXT("control"),TEXT("kontrola"));
        Hint(Hints,398,170,TEXT("SHIFT"),TEXT("sprint"),TEXT("sprint"));
        Hint(Hints,570,166,TEXT("SPACE"),TEXT("jump"),TEXT("skok"));
        Hint(Hints,744,338,TEXT(""),TEXT("hold / release shot"),TEXT("przytrzymaj / puść strzał"),true);
        Hint(Hints,1088,196,TEXT("RMB"),TEXT("slide"),TEXT("wślizg"));
        Hint(Hints,1290,215,TEXT("R"),TEXT("reset ball"),TEXT("reset piłki"));
        Hint(Hints,1520,176,TEXT("ESC"),TEXT("pause"),TEXT("pauza"));
        auto* Shot=Panel(Pages[2],TEXT("ShotPanel"),735,155,450,91);
        Text(Shot,TEXT("ShotValue"),TEXT("POWER SHOT"),TEXT("MOCNY STRZAŁ"),18,16,414,36,18,White,true,true);
        auto* Bar=Make<UProgressBar>(TEXT("ShotFill"));FProgressBarStyle BS;BS.SetBackgroundImage(Brush(TEXT("T_UI_Slider_Track"))).SetFillImage(Brush(TEXT("T_UI_Slider_Fill_White")));Bar->SetWidgetStyle(BS);Bar->SetFillColorAndOpacity(Pink);At(Shot,Bar,28,64,394,8);Shot->SetVisibility(ESlateVisibility::Collapsed);

        auto* Pause=Panel(Pages[3],TEXT("PausePanel"),76,180,580,686);
        Heading(Pause,TEXT("PAUSED"),TEXT("PAUZA"),TEXT("The pitch is waiting for you."),TEXT("Boisko czeka na Twój powrót."),580,48);
        Button(Pause,TEXT("ResumeButton"),EErlingUIAction::Play,TEXT("Resume"),TEXT("Wznów"),TEXT("T_UI_Icon_Play_Accent"),38,242,504,82,true);
        Button(Pause,TEXT("PauseSettingsButton"),EErlingUIAction::Settings,TEXT("Settings"),TEXT("Ustawienia"),TEXT("T_UI_Icon_Settings_Silver"),38,338,504,82);
        Button(Pause,TEXT("PauseMainButton"),EErlingUIAction::MainMenu,TEXT("Main menu"),TEXT("Menu główne"),TEXT("T_UI_Icon_Home_Silver"),38,434,504,82);
        Button(Pause,TEXT("PauseQuitButton"),EErlingUIAction::Quit,TEXT("Quit game"),TEXT("Wyjdź z gry"),TEXT("T_UI_Icon_Exit_Silver"),38,530,504,82);

        auto* Settings=Panel(Pages[4],TEXT("SettingsPanel"),76,66,712,950);
        Heading(Settings,TEXT("SETTINGS"),TEXT("USTAWIENIA"),TEXT("Make the game yours."),TEXT("Dopasuj grę do siebie."),712,43);
        SettingRow(Settings,TEXT("LanguageButton"),EErlingUIAction::Language,TEXT("Language"),TEXT("Język"),TEXT("LanguageValue"),211);
        Slider(Settings,TEXT("MusicSlider"),TEXT("MusicValue"),TEXT("Music volume"),TEXT("Głośność muzyki"),301);
        Slider(Settings,TEXT("EffectsSlider"),TEXT("EffectsValue"),TEXT("Effects volume"),TEXT("Głośność efektów"),401);
        Slider(Settings,TEXT("SensitivitySlider"),TEXT("SensitivityValue"),TEXT("Mouse sensitivity"),TEXT("Czułość myszy"),501);
        SettingRow(Settings,TEXT("CameraButton"),EErlingUIAction::Camera,TEXT("Camera movement"),TEXT("Ruch kamery"),TEXT("CameraValue"),611);
        SettingRow(Settings,TEXT("QualityButton"),EErlingUIAction::Quality,TEXT("Graphics quality"),TEXT("Jakość grafiki"),TEXT("QualityValue"),687);
        SettingRow(Settings,TEXT("PauseSettingButton"),EErlingUIAction::PauseInSettings,TEXT("Pause in settings"),TEXT("Pauza w ustawieniach"),TEXT("PauseValue"),763);
        Button(Settings,TEXT("SaveSettingsButton"),EErlingUIAction::SaveSettings,TEXT("Save and back"),TEXT("Zapisz i wróć"),TEXT("T_UI_Icon_Check_Accent"),38,853,636,70,true);

        auto* Credits=Panel(Pages[5],TEXT("CreditsPanel"),76,168,666,712);
        Heading(Credits,TEXT("CREDITS"),TEXT("TWÓRCY"),TEXT("Project created by"),TEXT("Projekt stworzony przez"),666,48);
        Text(Credits,TEXT(""),TEXT("Daniel Maciej Pytel"),TEXT("Daniel Maciej Pytel"),48,234,570,50,29,Pink,true);
        Text(Credits,TEXT(""),TEXT("2026  ·  ERLING FOOTBALL CLUB"),TEXT("2026  ·  ERLING FOOTBALL CLUB"),48,300,570,34,16,Muted);
        Button(Credits,TEXT("ArtStationButton"),EErlingUIAction::ArtStation,TEXT("ArtStation"),TEXT("ArtStation"),TEXT(""),38,378,280,70);
        Button(Credits,TEXT("GitHubButton"),EErlingUIAction::GitHub,TEXT("GitHub"),TEXT("GitHub"),TEXT(""),338,378,290,70);
        Button(Credits,TEXT("LinkedInButton"),EErlingUIAction::LinkedIn,TEXT("LinkedIn"),TEXT("LinkedIn"),TEXT(""),38,462,590,70);
        Button(Credits,TEXT("CreditsBackButton"),EErlingUIAction::Back,TEXT("Back"),TEXT("Wstecz"),TEXT("T_UI_Icon_ArrowLeft_White"),38,592,300,74);

        auto* Toast=Canvas(Root,TEXT("ToastPanel"),980,62,830,72);
        Image(Toast,TEXT("T_UI_Toast_Backplate"),0,0,830,72);
        Image(Toast,TEXT("T_UI_Icon_Check_Accent"),24,22,28,28);
        Text(Toast,TEXT("ToastValue"),TEXT(""),TEXT(""),72,21,728,38,18,White);
        Toast->SetVisibility(ESlateVisibility::Collapsed);
        Switch->SetActiveWidgetIndex(0);
    }
};
}

UErlingBuildUICommandlet::UErlingBuildUICommandlet() { IsClient=false;IsServer=false;IsEditor=true;LogToConsole=true; }

int32 UErlingBuildUICommandlet::Main(const FString& Params)
{
    const FString PackageName=TEXT("/Game/Erling/UI/WBP_ErlingInterface");
    UWidgetBlueprint* Blueprint=FPackageName::DoesPackageExist(PackageName)?LoadObject<UWidgetBlueprint>(nullptr,*(PackageName+TEXT(".WBP_ErlingInterface"))):nullptr;
    if (Blueprint && !FParse::Param(*Params,TEXT("Replace"))) { UE_LOG(LogTemp,Error,TEXT("Designer asset exists. Refusing to overwrite; archive it and use -Replace explicitly."));return 1; }
    FString ManifestPath;
    if(!FParse::Value(*Params,TEXT("Manifest="),ManifestPath)) { UE_LOG(LogTemp,Error,TEXT("Pass -Manifest=<asset_manifest.json>"));return 1; }
    FString Json;TSharedPtr<FJsonObject> Manifest;
    if(!FFileHelper::LoadFileToString(Json,*ManifestPath)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Manifest)) return 1;
    if(!Blueprint)
    {
        auto* Factory=NewObject<UWidgetBlueprintFactory>(); Factory->ParentClass=UErlingInterface::StaticClass();
        Blueprint=CastChecked<UWidgetBlueprint>(Factory->FactoryCreateNew(UWidgetBlueprint::StaticClass(),CreatePackage(*PackageName),TEXT("WBP_ErlingInterface"),RF_Public|RF_Standalone,nullptr,GWarn));
        FAssetRegistryModule::AssetCreated(Blueprint);
    }
    else if(Blueprint->WidgetTree) Blueprint->WidgetTree->Rename(nullptr,GetTransientPackage(),REN_DontCreateRedirectors|REN_NonTransactional);
    Blueprint->WidgetTree=NewObject<UWidgetTree>(Blueprint,TEXT("WidgetTree"),RF_Transactional);
    ErlingUIAuthoring::FLayout Layout(Blueprint->WidgetTree);
    for(auto Value:Manifest->GetArrayField(TEXT("assets"))) Layout.Assets.Add(Value->AsObject()->GetStringField(TEXT("name")),Value->AsObject());
    Layout.Build();if(Layout.Missing) return 1;
    TArray<UWidget*> DesignerWidgets;Blueprint->WidgetTree->GetAllWidgets(DesignerWidgets);
    for(UWidget* Widget:DesignerWidgets) if(!Blueprint->WidgetVariableNameToGuidMap.Contains(Widget->GetFName())) Blueprint->OnVariableAdded(Widget->GetFName());
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    if(Blueprint->Status==BS_Error) { UE_LOG(LogTemp,Error,TEXT("UI blueprint compile failed"));return 1; }
    Blueprint->MarkPackageDirty();FSavePackageArgs Save;Save.TopLevelFlags=RF_Public|RF_Standalone;Save.SaveFlags=SAVE_NoError;
    const FString Filename=FPackageName::LongPackageNameToFilename(PackageName,FPackageName::GetAssetPackageExtension());
    const bool Saved=UPackage::SavePackage(Blueprint->GetOutermost(),Blueprint,*Filename,Save);
    TArray<UWidget*> Widgets;Blueprint->WidgetTree->GetAllWidgets(Widgets);
    UE_LOG(LogTemp,Display,TEXT("ERLING_UI_BUILD: saved=%d widgets=%d package=%s"),Saved,Widgets.Num(),*PackageName);
    return Saved?0:1;
}
