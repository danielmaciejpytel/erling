#pragma once
#include "CoreMinimal.h"
#include "ErlingTuning.h"

enum class EErlingShotKind : uint8 { Pass, Power, UnderBar, TooHigh };

struct FErlingShotEvaluation
{
	FVector Velocity = FVector::ZeroVector;
	FVector AimTarget = FVector::ZeroVector;
	bool bHasAim = false;
	EErlingShotKind Kind = EErlingShotKind::Pass;
};

namespace ErlingShot
{
	inline constexpr float MaxHoldSeconds = .75f;
	// Preserve the existing ballistic curve's input range; only the button hold is shorter.
	inline constexpr float MaxPowerSeconds = 1.5f;

	inline float PowerFromHoldSeconds(float HeldSeconds)
	{
		return FMath::Clamp(HeldSeconds / MaxHoldSeconds, 0.f, 1.f) * MaxPowerSeconds;
	}
	inline float HoldSecondsFromPower(float PowerSeconds)
	{
		return FMath::Clamp(PowerSeconds / MaxPowerSeconds, 0.f, 1.f) * MaxHoldSeconds;
	}
	inline float ChargeFraction(float PowerSeconds)
	{
		return FMath::Clamp(PowerSeconds / MaxPowerSeconds, 0.f, 1.f);
	}
	inline EErlingShotKind Classify(float PowerSeconds, float PlanarSpeed, bool HasCrossing, float CrossingHeight)
	{
		// Both goal frames preserve the underside at CrossbarZ - 9 cm.
		// The ball's centre must also leave room for its radius.
		const float ClearCentreHeight = FMath::Min(ErlingPitch::ScoringMaxZ,
		    ErlingPitch::CrossbarZ - 9.f - 50.f * ErlingBall::MeshScale);
		if (HasCrossing && CrossingHeight > ClearCentreHeight)
			return EErlingShotKind::TooHigh;
		if (HasCrossing && PlanarSpeed >= 2600.f && CrossingHeight >= 180.f && CrossingHeight <= 228.f)
			return EErlingShotKind::UnderBar;
		return PowerSeconds < .35f ? EErlingShotKind::Pass : EErlingShotKind::Power;
	}
}
