#pragma once
#include "Commandlets/Commandlet.h"
#include "ErlingBuildUICommandlet.generated.h"

/** One-time designer asset authoring tool. Never runs during gameplay. */
UCLASS()
class UErlingBuildUICommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UErlingBuildUICommandlet();
    virtual int32 Main(const FString& Params) override;
};
