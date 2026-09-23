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
#include "Engine/FontFace.h"
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
        const FName WidgetName=Name.IsEmpty()?MakeUniqueObjectName(Tree,T::StaticClass(),*FString::Printf(TEXT("Element_%03d"),++Serial)):FName(*Name);
        auto* Widget=Tree->ConstructWidget<T>(T::StaticClass(), WidgetName);
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
        auto* T=Make<UErlingUIText>(Name); T->English=FText::FromString(En).ToUpper(); T->Polish=FText::FromString(Pl).ToUpper(); T->SetLanguage(true);
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
        Text(Body,Name+TEXT("Label"),En,Pl,Icon.IsEmpty()?26:98,(H-38)/2,W-(Icon.IsEmpty()?80:150),38,22,White,Primary);
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
        Text(Body,Value,TEXT(""),TEXT(""),326,15,245,38,20,Pink,false,true);
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
        if(!Mouse) Text(C,TEXT(""),Key,Key==TEXT("SPACE")?TEXT("SPACJA"):Key,X,4,KeyW,27,13,White,true,true);
        Text(C,TEXT(""),En,Pl,X+KeyW+12,3,W-KeyW-12,30,15,White);
    }
    bool UpdateStorybook()
    {
        // Update visual properties in place, preserving actions and Designer additions.
        const FLinearColor Cream=FLinearColor::FromSRGBColor(FColor(243,238,228));
        const FLinearColor Rose=FLinearColor::FromSRGBColor(FColor(218,91,145));
        const FString FontPackage=TEXT("/Game/Erling/UI/Fonts/F_Storybook");
        auto* Font=FPackageName::DoesPackageExist(FontPackage)?LoadObject<UFont>(nullptr,*(FontPackage+TEXT(".F_Storybook"))):nullptr;
        // Keep the established UMG font asset, but refresh its embedded face so
        // every existing widget picks up the selected menu typeface in place.
        {
            TArray<uint8> Bytes;
            const FString Filename=FPaths::ProjectContentDir()/TEXT("Erling/UI/SourceArt/Fonts/FF_CorsairPE-Rg.otf");
            if(!FFileHelper::LoadFileToArray(Bytes,*Filename)) return false;
            auto* Face=LoadObject<UFontFace>(nullptr,TEXT("/Game/Erling/UI/Fonts/FF_CorsairPE.FF_CorsairPE"));
            const bool bNewFace=!Face;
            if(!Face) Face=NewObject<UFontFace>(CreatePackage(TEXT("/Game/Erling/UI/Fonts/FF_CorsairPE")),TEXT("FF_CorsairPE"),RF_Public|RF_Standalone);
            if(bNewFace) Face->InitializeFromBulkData(Filename,EFontHinting::Default,Bytes.GetData(),Bytes.Num());
            Face->SourceFilename=Filename;
            Face->LoadingPolicy=EFontLoadingPolicy::Inline;
            TArray<uint8> FallbackBytes;
            const FString FallbackFilename=FPaths::ProjectContentDir()/TEXT("Erling/UI/SourceArt/Fonts/FF_Brother.otf");
            if(!FFileHelper::LoadFileToArray(FallbackBytes,*FallbackFilename)) return false;
            auto* FallbackFace=LoadObject<UFontFace>(nullptr,TEXT("/Game/Erling/UI/Fonts/FF_BrotherDemo.FF_BrotherDemo"));
            const bool bNewFallbackFace=!FallbackFace;
            if(!FallbackFace) FallbackFace=NewObject<UFontFace>(CreatePackage(TEXT("/Game/Erling/UI/Fonts/FF_BrotherDemo")),TEXT("FF_BrotherDemo"),RF_Public|RF_Standalone);
            if(bNewFallbackFace) FallbackFace->InitializeFromBulkData(FallbackFilename,EFontHinting::Default,FallbackBytes.GetData(),FallbackBytes.Num());
            FallbackFace->SourceFilename=FallbackFilename;
            FallbackFace->LoadingPolicy=EFontLoadingPolicy::Inline;
            TArray<uint8> QBytes;
            const FString QFilename=FPaths::ProjectContentDir()/TEXT("Erling/UI/SourceArt/Fonts/FF_Brother-QBaseline.otf");
            if(!FFileHelper::LoadFileToArray(QBytes,*QFilename)) return false;
            auto* QFace=LoadObject<UFontFace>(nullptr,TEXT("/Game/Erling/UI/Fonts/FF_BrotherQBaseline.FF_BrotherQBaseline"));
            const bool bNewQFace=!QFace;
            if(!QFace) QFace=NewObject<UFontFace>(CreatePackage(TEXT("/Game/Erling/UI/Fonts/FF_BrotherQBaseline")),TEXT("FF_BrotherQBaseline"),RF_Public|RF_Standalone);
            if(bNewQFace) QFace->InitializeFromBulkData(QFilename,EFontHinting::Default,QBytes.GetData(),QBytes.Num());
            QFace->SourceFilename=QFilename;
            QFace->LoadingPolicy=EFontLoadingPolicy::Inline;
            const bool bNewFont=!Font;
            if(!Font) Font=NewObject<UFont>(CreatePackage(*FontPackage),TEXT("F_Storybook"),RF_Public|RF_Standalone);
            Font->FontCacheType=EFontCacheType::Runtime;
            FTypefaceEntry Entry(TEXT("Regular")); Entry.Font=FFontData(Face);
            Font->GetMutableInternalCompositeFont().DefaultTypeface.Fonts.Reset();
            Font->GetMutableInternalCompositeFont().DefaultTypeface.Fonts.Add(Entry);
            // Brother DEMO is consulted only if Corsair PE cannot supply a glyph.
            FTypefaceEntry FallbackEntry(TEXT("Regular"));FallbackEntry.Font=FFontData(FallbackFace);
            auto& Fallback=Font->GetMutableInternalCompositeFont().FallbackTypeface.Typeface;
            Fallback.Fonts.Reset();Fallback.Fonts.Add(FallbackEntry);
            // Use a dedicated Brother face with its Q aligned to Corsair's cap line.
            auto& SubTypefaces=Font->GetMutableInternalCompositeFont().SubTypefaces;
            SubTypefaces.Reset();
            FCompositeSubFont CapitalQ;
            FTypefaceEntry QEntry(TEXT("Regular"));QEntry.Font=FFontData(QFace);
            CapitalQ.Typeface.Fonts.Add(QEntry);
            CapitalQ.CharacterRanges.Emplace(0x0051);
            // Keep its rendered height matched after repositioning the glyph outline.
            CapitalQ.ScalingFactor=0.97f;
#if WITH_EDITORONLY_DATA
            CapitalQ.EditorName=TEXT("Brother DEMO - capital Q");
#endif
            SubTypefaces.Add(MoveTemp(CapitalQ));
            for(UObject* Asset:TArray<UObject*>{Face,FallbackFace,QFace,Font})
            {
                if((Asset==Face&&bNewFace)||(Asset==FallbackFace&&bNewFallbackFace)||(Asset==QFace&&bNewQFace)||(Asset==Font&&bNewFont)) FAssetRegistryModule::AssetCreated(Asset); Asset->MarkPackageDirty();
                FSavePackageArgs Save;Save.TopLevelFlags=RF_Public|RF_Standalone;Save.SaveFlags=SAVE_NoError;
                if(!UPackage::SavePackage(Asset->GetOutermost(),Asset,*FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Save)) return false;
            }
        }
        auto Move=[&](UWidget* W,float X,float Y,float Width,float Height)
        {
            if(!W)return;
            // The panel owns the serialized slot; loaded Widget::Slot back-pointers
            // can refer to a stale template until the Slate tree is constructed.
            TArray<UWidget*> All;Tree->GetAllWidgets(All);
            for(UWidget* Candidate:All) if(auto* Panel=Cast<UPanelWidget>(Candidate))
                for(UPanelSlot* Owned:Panel->GetSlots()) if(Owned->Content==W)
                    if(auto* Slot=Cast<UCanvasPanelSlot>(Owned))
                    { Slot->SetAutoSize(false);Slot->SetAnchors(FAnchors(0,0));Slot->SetAlignment(FVector2D::ZeroVector);Slot->SetPosition(FVector2D(X,Y));Slot->SetSize(FVector2D(Width,Height)); }
        };
        auto SetType=[&](UTextBlock* T,int32 Size,int32 Weight=0)
        {
            FSlateFontInfo Info(Font,Size,FName(TEXT("Regular")));
            Info.OutlineSettings.OutlineSize=Weight;Info.OutlineSettings.OutlineColor=Cream;
            T->SetFont(Info);
        };
        TArray<UWidget*> Widgets;Tree->GetAllWidgets(Widgets);
        for(UWidget* W:Widgets)
        {
            if(auto* T=Cast<UTextBlock>(W))
            {
                if(auto* Localized=Cast<UErlingUIText>(T))
                    if(Localized->English.ToString().Equals(TEXT("Graphics quality"),ESearchCase::IgnoreCase))
                    {
                        Localized->English=FText::FromString(TEXT("GRAPHICS QUALITY"));
                        Localized->SetLanguage(false);
                    }
                SetType(T,T->GetFont().Size);
                const auto Old=T->GetColorAndOpacity().GetSpecifiedColor();
                T->SetColorAndOpacity(Old.R>Old.G*2.f?Rose:Cream);
            }
            if(auto* I=Cast<UImage>(W))
            {
                auto* Resource=I->GetBrush().GetResourceObject();if(!Resource) continue;
                FString Name=Resource->GetName();
                if(Name.Contains(TEXT("SheenOverlay"))||Name.Contains(TEXT("Panel_Frame"))||Name.StartsWith(TEXT("T_UI_Glow_")))
                    I->SetVisibility(ESlateVisibility::Collapsed);
                else if(Name.StartsWith(TEXT("T_UI_Icon_")))
                {
                    Name=Name.Replace(TEXT("_Silver"),TEXT("_White")).Replace(TEXT("_Accent"),TEXT("_White"));
                    I->SetBrush(Brush(Name));I->SetColorAndOpacity(Cream);I->SetRenderOpacity(1);
                }
                else if(Assets.Contains(Name)) { I->SetBrush(Brush(Name));I->SetRenderOpacity(.98f); }
            }
            if(auto* B=Cast<UErlingUIButton>(W))
            {
                const auto* ButtonCanvasSlot=Cast<UCanvasPanelSlot>(B->Slot);
                const float ButtonHeight=ButtonCanvasSlot?ButtonCanvasSlot->GetSize().Y:0.f;
                if(auto* Body=Cast<UCanvasPanel>(B->GetContent()))
                    for(UWidget* Child:Body->GetAllChildren())
                        if(Cast<UTextBlock>(Child))
                            if(auto* TextSlot=Cast<UCanvasPanelSlot>(Child->Slot);TextSlot&&ButtonHeight>0.f)
                            {
                                FVector2D Position=TextSlot->GetPosition();
                                Position.Y=(ButtonHeight-TextSlot->GetSize().Y)*.5f;
                                // Text geometry can be centered while Corsair's visible glyphs still sit low.
                                // Tune the few button families against rendered screenshots at design resolution.
                                const FName ButtonName=B->GetFName();
                                const FName TextName=Child->GetFName();
                                float VisualNudge=0.f;
                                if(ButtonName==TEXT("SaveSettingsButton")) VisualNudge=-5.f;
                                else if(ButtonName==TEXT("SaveAppearanceButton")) VisualNudge=-3.f;
                                else if(ButtonName==TEXT("LanguageButton")&&TextName==TEXT("LanguageButtonLabel")) VisualNudge=-5.f;
                                else if(ButtonName==TEXT("CameraButton")&&TextName==TEXT("CameraButtonLabel")) VisualNudge=-3.f;
                                else if(ButtonName==TEXT("QualityButton")) VisualNudge=TextName==TEXT("QualityButtonLabel")?-1.f:-1.5f;
                                else if(ButtonName==TEXT("PauseSettingButton")) VisualNudge=TextName==TEXT("PauseSettingButtonLabel")?-3.f:-1.5f;
                                else if(ButtonName==TEXT("PauseSettingsButton")) VisualNudge=-3.f;
                                else if(ButtonName==TEXT("ArtStationButton")) VisualNudge=-2.f;
                                else if(ButtonName==TEXT("LinkedInButton")||ButtonName==TEXT("CreditsBackButton")) VisualNudge=-2.f;
                                else if(ButtonName==TEXT("CharacterBackButton")) VisualNudge=-2.f;
                                else if(ButtonName==TEXT("GitHubButton")) VisualNudge=-3.f;
                                Position.Y+=VisualNudge;
                                TextSlot->SetPosition(Position);
                            }
                FButtonStyle Style=B->GetStyle();
                auto Replace=[&](FSlateBrush& Value) {if(auto* R=Value.GetResourceObject()) if(Assets.Contains(R->GetName())) Value=Brush(R->GetName());};
                Replace(Style.Normal);Replace(Style.Hovered);Replace(Style.Pressed);Replace(Style.Disabled);
                if(auto* Icon=Cast<UImage>(B->GetContent()))
                {
                    Style.NormalPadding=FMargin(0);Style.PressedPadding=FMargin(0,1,0,-1);
                    // Keep a visible arrow inside the compact wardrobe buttons.
                    for(UPanelSlot* Owned:B->GetSlots()) if(auto* ContentSlot=Cast<UButtonSlot>(Owned))
                    {ContentSlot->SetPadding(FMargin(12));ContentSlot->SetHorizontalAlignment(HAlign_Fill);ContentSlot->SetVerticalAlignment(VAlign_Fill);}
                }
                B->SetStyle(Style);
            }
            if(auto* Bar=Cast<UProgressBar>(W)) Bar->SetFillColorAndOpacity(Rose);
        }
        auto* Main=Cast<UCanvasPanel>(Tree->FindWidget(TEXT("MainPanel")));
        if(!Main) return false;
        Move(Main,76,98,478,894);
        for(UWidget* W:Main->GetAllChildren())
        {
            if(auto* I=Cast<UImage>(W))
            {
                const auto* R=I->GetBrush().GetResourceObject();
                if(R&&R->GetName()==TEXT("T_UI_Panel_Dark")) Move(I,0,0,478,894);
                else I->SetVisibility(ESlateVisibility::Collapsed);
            }
            if(auto* T=Cast<UErlingUIText>(W))
            {
                const FString En=T->English.ToString();
                if(En==TEXT("ERLING")) {Move(T,46,-4,394,150);SetType(T,108,2);}
                else if(En.Contains(TEXT("C L U B"))||En==TEXT("FOOTBALL CLUB"))
                {
                    T->English=FText::FromString(TEXT("FOOTBALL CLUB"));T->Polish=T->English;T->SetLanguage(false);
                    Move(T,58,164,366,64);SetType(T,38);auto Info=T->GetFont();Info.LetterSpacing=120;T->SetFont(Info);T->SetColorAndOpacity(Rose);
                }
            }
        }
        for(UWidget* W:Widgets) if(auto* T=Cast<UErlingUIText>(W))
            if(T->English.ToString()==TEXT("Your player. Your pitch."))
            {
                SetType(T,32);T->SetAutoWrapText(false);T->SetMinDesiredWidth(0);
                auto* Fit=Cast<UScaleBox>(Tree->FindWidget(TEXT("StorybookTaglineFit")));
                if(!Fit)
                {
                    Main->RemoveChild(T);Fit=Make<UScaleBox>(TEXT("StorybookTaglineFit"));
                    Fit->SetStretch(EStretch::ScaleToFit);Fit->SetStretchDirection(EStretchDirection::DownOnly);
                    Fit->AddChild(T);At(Main,Fit,46,270,396,64);
                    auto* FitSlot=CastChecked<UScaleBoxSlot>(T->Slot);
                    FitSlot->SetHorizontalAlignment(HAlign_Left);FitSlot->SetVerticalAlignment(VAlign_Center);
                }
                Move(Fit,46,270,396,64);
            }
        const TCHAR* Names[]={TEXT("PlayButton"),TEXT("CharacterButton"),TEXT("SettingsButton"),TEXT("CreditsButton"),TEXT("QuitButton")};
        for(int32 Index=0;Index<5;++Index)
        {
            auto* B=Cast<UErlingUIButton>(Tree->FindWidget(Names[Index]));if(!B)return false;
            Move(B,34,374+Index*102,410,86);
            auto* Body=Cast<UCanvasPanel>(B->GetContent());
            for(UWidget* W:Body->GetAllChildren())
            {
                if(auto* T=Cast<UTextBlock>(W))
                {
                    const float TextY[]={6.f,16.f,12.f,16.5f,16.5f};
                    Move(T,104,TextY[Index],250,58);SetType(T,Index==0?36:Index==1?26:30);
                }
                if(auto* I=Cast<UImage>(W))
                {
                    const bool Chevron=I->GetBrush().GetResourceObject()->GetName().Contains(TEXT("Chevron"));
                    Move(I,Chevron?363:20,Chevron?27:16,Chevron?24:54,Chevron?32:54);
                }
            }
        }
        if(auto* Character=Cast<UCanvasPanel>(Tree->FindWidget(TEXT("CharacterPanel"))))
            for(UWidget* Child:Character->GetAllChildren())
                if(Cast<UTextBlock>(Child))
                    if(auto* Slot=Cast<UCanvasPanelSlot>(Child->Slot))
                    {
                        FVector2D Position=Slot->GetPosition();
                        if(FMath::IsNearlyEqual(Position.X,138.f,1.f)&&Position.Y>=230.f&&Position.Y<730.f)
                        {
                            const int32 Row=FMath::Clamp(FMath::RoundToInt((Position.Y-247.f)/106.f),0,4);
                            const float LabelNudge[]={0.f,0.f,0.f,3.f,0.f};
                            Position.Y=215.f+Row*106.f+27.f+LabelNudge[Row];
                            Slot->SetPosition(Position);
                        }
                    }
        for(int32 Index=0;Index<5;++Index)
            if(auto* Value=Tree->FindWidget(FName(*FString::Printf(TEXT("AppearanceValue%d"),Index))))
                if(auto* Slot=Cast<UCanvasPanelSlot>(Value->Slot))
                {
                    FVector2D Position=Slot->GetPosition();
                    const float ValueNudge[]={-2.f,2.f,-2.f,-2.f,-2.f};
                    Position.Y=215.f+Index*106.f+27.f+ValueNudge[Index];
                    Slot->SetPosition(Position);
                }
        if(auto* Preview=Tree->FindWidget(TEXT("PreviewValue")))
            if(auto* Slot=Cast<UCanvasPanelSlot>(Preview->Slot))
            {
                FVector2D Position=Slot->GetPosition();
                Position.Y=768.f;
                Slot->SetPosition(Position);
            }
        for(UWidget* W:Widgets)
            if(auto* T=Cast<UErlingUIText>(W))
            {
                const FString English=T->English.ToString();
                if(English.Equals(TEXT("Animation preview"),ESearchCase::IgnoreCase))
                    if(auto* Slot=Cast<UCanvasPanelSlot>(T->Slot))
                    { FVector2D Position=Slot->GetPosition();Position.Y-=1.f;Slot->SetPosition(Position); }
                if(English.StartsWith(TEXT("Drag to rotate")))
                    Move(T,1082,1027,620,30);
            }
        Move(Tree->FindWidget(TEXT("ToastValue")),72,17,728,38);
        auto* HUD=Cast<UCanvasPanel>(Tree->FindWidget(TEXT("GameHUD")));
        auto* Hints=Cast<UCanvasPanel>(Tree->FindWidget(TEXT("ControlHints")));
        if(!HUD||!Hints)return false;
        for(UWidget* W:HUD->GetAllChildren())
            if(auto* I=Cast<UImage>(W))
                if(auto* R=I->GetBrush().GetResourceObject())
                    if(R->GetName()==TEXT("T_UI_Panel_Dark")) Move(I,42,978,1836,82);
        // Only the informational row is rebuilt; no input bindings live here.
        const auto OldHints=Hints->GetAllChildren();Hints->ClearChildren();
        for(UWidget* W:OldHints) W->Rename(nullptr,GetTransientPackage(),REN_DontCreateRedirectors|REN_NonTransactional);
        Move(Hints,76,994,1770,54);
        auto KeyHint=[&](float X,float KeyWidth,float TotalWidth,const FString& Key,const FString& En,const FString& Pl)
        {
            if(Key==TEXT("WASD"))
                for(int32 K=0;K<4;++K)
                {
                    Image(Hints,TEXT("T_UI_Keycap_Wide_Normal"),X+K*37,3,37,44);
                    auto* T=Text(Hints,TEXT(""),Key.Mid(K,1),Key.Mid(K,1),X+K*37,3,37,44,22,Cream,false,true);SetType(T,22);
                }
            else if(Key.IsEmpty()) Image(Hints,TEXT("T_UI_Mouse_Outline"),X,0,34,48);
            else
            {
                Image(Hints,TEXT("T_UI_Keycap_Wide_Normal"),X,3,KeyWidth,44);
                auto* T=Text(Hints,TEXT(""),Key,Key,X,3,KeyWidth,44,22,Cream,false,true);SetType(T,22);
            }
            auto* T=Text(Hints,TEXT(""),En,Pl,X+KeyWidth+10,3,TotalWidth-KeyWidth-10,44,20,Cream);SetType(T,20);
        };
        KeyHint(0,148,237,TEXT("WASD"),TEXT("move"),TEXT("ruch"));
        KeyHint(245,88,202,TEXT("CTRL"),TEXT("control"),TEXT("kontrola"));
        KeyHint(455,96,195,TEXT("SHIFT"),TEXT("sprint"),TEXT("sprint"));
        KeyHint(658,105,196,TEXT("SPACE"),TEXT("jump"),TEXT("skok"));
        KeyHint(862,34,272,TEXT(""),TEXT("shoot / pass"),TEXT("strzał / podanie"));
        KeyHint(1142,84,186,TEXT("RMB"),TEXT("slide"),TEXT("wślizg"));
        KeyHint(1336,40,203,TEXT("R"),TEXT("reset ball"),TEXT("reset piłki"));
        KeyHint(1547,78,196,TEXT("ESC"),TEXT("pause"),TEXT("pauza"));
        return Missing==0;
    }
    void UpdateHUD()
    {
        // Preserve all other Designer edits when updating an existing interface.
        if(auto* Goals=Cast<UCanvasPanel>(Tree->FindWidget(TEXT("GoalsPanel"))))
        {
            if(auto* Slot=Cast<UCanvasPanelSlot>(Goals->GetChildAt(0)->Slot))
            { Slot->SetPosition(FVector2D(-20,-10)); Slot->SetSize(FVector2D(396,110)); }
            for(UWidget* Child:Goals->GetAllChildren())
                if(auto* Label=Cast<UTextBlock>(Child))
                    if(auto* Slot=Cast<UCanvasPanelSlot>(Label->Slot))
                    {
                        const FString Value=Label->GetText().ToString();
                        FVector2D Position=Slot->GetPosition();
                        if(Value.Contains(TEXT("GOLE"))||Value.Contains(TEXT("GOALS"))) Position.Y=16;
                        else if(Label->GetFName()==TEXT("GoalsValue")) Position.Y=3;
                        Slot->SetPosition(Position);
                    }
        }
        if(!Tree->FindWidget(TEXT("PlayerShotPanel")))
            if(auto* HUD=Cast<UCanvasPanel>(Tree->FindWidget(TEXT("GameHUD"))))
            {
                auto* Shot=Panel(HUD,TEXT("PlayerShotPanel"),0,0,150,26);
                auto* Bar=Make<UProgressBar>(TEXT("PlayerShotFill"));
                FProgressBarStyle Style;
                Style.SetBackgroundImage(Brush(TEXT("T_UI_Slider_Track"))).SetFillImage(Brush(TEXT("T_UI_Slider_Fill_White")));
                Bar->SetWidgetStyle(Style); Bar->SetFillColorAndOpacity(Pink);
                At(Shot,Bar,10,9,130,8);
                Shot->SetVisibility(ESlateVisibility::Collapsed);
            }
    }
    bool UpdateCharacterHint()
    {
        TArray<UWidget*> Widgets;Tree->GetAllWidgets(Widgets);
        for(UWidget* Widget:Widgets)
            if(auto* Label=Cast<UErlingUIText>(Widget);Label&&Label->English.ToString().StartsWith(TEXT("Drag to rotate")))
                if(auto* Parent=Cast<UCanvasPanel>(Label->GetParent()))
                    for(UWidget* Child:Parent->GetAllChildren())
                        if(auto* Image=Cast<UImage>(Child);Image&&Image->GetBrush().GetResourceObject()&&Image->GetBrush().GetResourceObject()->GetName()==TEXT("T_UI_Panel_Dark"))
                            if(auto* Back=Cast<UCanvasPanelSlot>(Image->Slot);Back&&Back->GetPosition().Y>1000)
                            {
                                Back->SetPosition(FVector2D(1062,1017));Back->SetSize(FVector2D(660,53));
                                auto* TextSlot=CastChecked<UCanvasPanelSlot>(Label->Slot);
                                TextSlot->SetPosition(FVector2D(1082,1028));TextSlot->SetSize(FVector2D(620,30));
                                return true;
                            }
        UE_LOG(LogTemp,Error,TEXT("Character hint widgets not found; interface left intact"));return false;
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
            Text(Character,TEXT(""),En[I],Pl[I],138,Y+27,160,40,20,Muted);
            Arrow(Character,FString::Printf(TEXT("AppearancePrevious%d"),I),EErlingUIAction::AppearancePrevious,I,306,Y+19,false);
            Text(Character,FString::Printf(TEXT("AppearanceValue%d"),I),TEXT(""),TEXT(""),370,Y+27,282,40,21,White,false,true);
            Arrow(Character,FString::Printf(TEXT("AppearanceNext%d"),I),EErlingUIAction::AppearanceNext,I,660,Y+19,true);
        }
        Text(Character,TEXT(""),TEXT("Animation preview"),TEXT("Podgląd animacji"),50,768,250,34,17,Muted);
        Arrow(Character,TEXT("PreviewPrevious"),EErlingUIAction::PreviewPrevious,0,306,756,false);
        Text(Character,TEXT("PreviewValue"),TEXT("Idle"),TEXT("Bezczynność"),370,765,282,38,15,Muted,false,true);
        Arrow(Character,TEXT("PreviewNext"),EErlingUIAction::PreviewNext,0,660,756,true);
        Button(Character,TEXT("CharacterBackButton"),EErlingUIAction::Back,TEXT("Back"),TEXT("Wstecz"),TEXT("T_UI_Icon_ArrowLeft_White"),36,842,298,74);
        Button(Pages[1],TEXT("SaveAppearanceButton"),EErlingUIAction::SaveAppearance,TEXT("Save appearance"),TEXT("Zapisz wygląd"),TEXT(""),1502,928,350,82,true);
        Image(Pages[1],TEXT("T_UI_Panel_Dark"),1062,1017,660,53,.8f);
        Text(Pages[1],TEXT(""),TEXT("Drag to rotate  ·  Scroll to zoom"),TEXT("Przeciągnij, aby obrócić  ·  Kółko myszy: zoom"),1082,1028,620,30,16,Muted,false,true);

        auto* Goals=Canvas(Pages[2],TEXT("GoalsPanel"),782,32,356,90);
        Image(Goals,TEXT("T_UI_HUD_Goals_Backplate"),0,0,356,90);
        Text(Goals,TEXT(""),TEXT("GOALS"),TEXT("GOLE"),40,16,180,52,27,White,true);
        Text(Goals,TEXT("GoalsValue"),TEXT("00"),TEXT("00"),233,3,90,66,39,Pink,true,true);
        Image(Pages[2],TEXT("T_UI_Panel_Dark"),42,975,1810,90,.78f);
        auto* Hints=Canvas(Pages[2],TEXT("ControlHints"),70,1009,1780,42);
        Hint(Hints,0,206,TEXT("W A S D"),TEXT("move"),TEXT("ruch"));
        Hint(Hints,216,178,TEXT("CTRL"),TEXT("control"),TEXT("kontrola"));
        Hint(Hints,398,170,TEXT("SHIFT"),TEXT("sprint"),TEXT("sprint"));
        Hint(Hints,570,166,TEXT("SPACE"),TEXT("jump"),TEXT("skok"));
        Hint(Hints,744,338,TEXT(""),TEXT("shoot / pass"),TEXT("strzał / podanie"),true);
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
        SettingRow(Settings,TEXT("QualityButton"),EErlingUIAction::Quality,TEXT("Graphics Quality"),TEXT("Jakość grafiki"),TEXT("QualityValue"),687);
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
        UpdateHUD();
    }
};
}

UErlingBuildUICommandlet::UErlingBuildUICommandlet() { IsClient=false;IsServer=false;IsEditor=true;LogToConsole=true; }

int32 UErlingBuildUICommandlet::Main(const FString& Params)
{
    const FString PackageName=TEXT("/Game/Erling/UI/WBP_ErlingInterface");
    UWidgetBlueprint* Blueprint=FPackageName::DoesPackageExist(PackageName)?LoadObject<UWidgetBlueprint>(nullptr,*(PackageName+TEXT(".WBP_ErlingInterface"))):nullptr;
    const bool UpdateHUD=FParse::Param(*Params,TEXT("UpdateHUD"));
    const bool UpdateHint=FParse::Param(*Params,TEXT("UpdateCharacterHint"));
    const bool Storybook=FParse::Param(*Params,TEXT("Storybook"));
    const bool PartialUpdate=UpdateHUD||UpdateHint||Storybook;
    if(PartialUpdate && (!Blueprint || !Blueprint->WidgetTree)) { UE_LOG(LogTemp,Error,TEXT("Partial update requires an existing interface."));return 1; }
    if (Blueprint && !PartialUpdate && !FParse::Param(*Params,TEXT("Replace"))) { UE_LOG(LogTemp,Error,TEXT("Designer asset exists. Refusing to overwrite; archive it and use -Replace explicitly."));return 1; }
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
    else if(!PartialUpdate && Blueprint->WidgetTree) Blueprint->WidgetTree->Rename(nullptr,GetTransientPackage(),REN_DontCreateRedirectors|REN_NonTransactional);
    if(!PartialUpdate || !Blueprint->WidgetTree) Blueprint->WidgetTree=NewObject<UWidgetTree>(Blueprint,TEXT("WidgetTree"),RF_Transactional);
    ErlingUIAuthoring::FLayout Layout(Blueprint->WidgetTree);
    for(auto Value:Manifest->GetArrayField(TEXT("assets"))) Layout.Assets.Add(Value->AsObject()->GetStringField(TEXT("name")),Value->AsObject());
    if(Storybook) { if(!Layout.UpdateStorybook()) return 1; }
    else if(UpdateHint) { if(!Layout.UpdateCharacterHint()) return 1; }
    else if(UpdateHUD) Layout.UpdateHUD(); else Layout.Build();
    if(Layout.Missing) return 1;
    TArray<UWidget*> DesignerWidgets;Blueprint->WidgetTree->GetAllWidgets(DesignerWidgets);
    if(Storybook)
    {
        TSet<FName> LiveNames;for(UWidget* Widget:DesignerWidgets) LiveNames.Add(Widget->GetFName());
        TArray<FName> OldNames;Blueprint->WidgetVariableNameToGuidMap.GetKeys(OldNames);
        for(FName Name:OldNames) if(!LiveNames.Contains(Name)) Blueprint->OnVariableRemoved(Name);
    }
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
