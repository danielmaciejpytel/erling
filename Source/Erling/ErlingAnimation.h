#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ErlingAnimation.generated.h"

// Every wardrobe piece evaluates this same clock, including its morph curves.
UCLASS(Transient)
class ERLING_API UErlingAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};

UCLASS()
class ERLING_API UErlingMovement : public UCharacterMovementComponent
{
    GENERATED_BODY()
public:
    virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
};
