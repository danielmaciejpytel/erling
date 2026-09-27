#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ErlingHairSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Włosy"))
class ERLING_API UErlingHairSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    virtual FName GetCategoryName() const override { return TEXT("Erling"); }
#if WITH_EDITOR
    virtual FText GetSectionText() const override { return NSLOCTEXT("Erling", "HairSettings", "Włosy"); }
#endif

    UPROPERTY(Config, EditAnywhere, Category="Ruch", meta=(DisplayName="Siła ruchu", ClampMin="0", ClampMax="5", ToolTip="Siła kołysania. Wartość 0 wyłącza ruch włosów."))
    float HairStrength=1.f;

    UPROPERTY(Config, EditAnywhere, Category="Ruch", meta=(DisplayName="Siła reakcji na obrót", ClampMin="0", ClampMax="5", ToolTip="Siła opóźnienia kosmyków przy obracaniu postaci, również w podglądzie. Wartość 0 wyłącza reakcję na obrót."))
    float HairTurnStrength=1.f;

    UPROPERTY(Config, EditAnywhere, Category="Ruch", meta=(DisplayName="Maksymalny kąt przegubu", ClampMin="0", ClampMax="30", Units="deg", ToolTip="Limit dla jednego przegubu. Ruch końcówki sumuje się z trzech przegubów; większe wartości mogą powodować przenikanie."))
    float HairMaxAngle=7.f;

    UPROPERTY(Config, EditAnywhere, Category="Ruch", meta=(DisplayName="Sprężystość", ClampMin="1", ClampMax="1000", ToolTip="Wyższa wartość oznacza szybszą reakcję na ruch postaci."))
    float HairStiffness=225.f;

    UPROPERTY(Config, EditAnywhere, Category="Ruch", meta=(DisplayName="Tłumienie", ClampMin="0", ClampMax="100", ToolTip="Wyższa wartość mocniej wygasza kołysanie."))
    float HairDamping=18.f;

};
