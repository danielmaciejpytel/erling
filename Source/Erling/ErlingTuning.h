#pragma once
#include "CoreMinimal.h"

// Single source of truth for pitch dimensions and ball physics.
// All values are Unreal units (cm) unless stated otherwise. The pitch is
// symmetric around the origin: goals sit at +X and -X.
namespace ErlingPitch
{
	/** Goal line / goal posts. Shots aim at this X plane. */
	inline constexpr float GoalLineX = 2740.f;
	/** A ball whose centre crosses this X plane inside the mouth is a goal. */
	inline constexpr float GoalPlaneX = 2762.f;
	/** Past this X a ball that is outside the mouth counts as a miss. */
	inline constexpr float MissCheckX = 2720.f;
	/** Centre of the goal net side/roof collision (depth-wise). */
	inline constexpr float NetCenterX = 2870.f;
	/** Back of the goal net. */
	inline constexpr float NetBackX = 2990.f;
	/** Goal post Y position (half the visual goal width). */
	inline constexpr float GoalPostY = 350.f;
	/** Crossbar height. */
	inline constexpr float CrossbarZ = 260.f;
	/** Half width of the scoring area inside the posts. */
	inline constexpr float ScoringHalfWidth = 320.f;
	/** Maximum ball-centre height that still counts as under the bar. */
	inline constexpr float ScoringMaxZ = 238.f;
	/** Half width used when deciding whether a shot is aimed on target. */
	inline constexpr float AimedHalfWidth = 319.f;
	/** Touchline Y. */
	inline constexpr float TouchlineY = 1750.f;

	/** Signed goal line for the goal a ball travelling along +X/-X is heading to. */
	inline float GoalLineFor(float DirectionX)
	{
		return DirectionX >= 0.f ? GoalLineX : -GoalLineX;
	}
	inline float GoalPlaneFor(float DirectionX)
	{
		return DirectionX >= 0.f ? GoalPlaneX : -GoalPlaneX;
	}
}

namespace ErlingBall
{
	/** Scale applied to /Engine/BasicShapes/Sphere (100 cm) -> 44 cm ball. */
	inline constexpr float MeshScale = .44f;
	inline constexpr float MassKg = .43f;
	inline constexpr float LinearDamping = .3f;
	inline constexpr float AngularDamping = .2f;
	inline constexpr float Restitution = .22f;
	inline constexpr float Friction = .45f;
	/** Ball centre height when placed at the kick-off spot. */
	inline constexpr float RestHeight = 28.f;
	/** Completed shots kept on the pitch as independent physics balls. */
	inline constexpr int32 MaxFinishedBalls = 5;
}

namespace ErlingPossession
{
	/** Planar distance within which the player can shoot the ball. */
	inline constexpr float HasBallRadius = 205.f;
	/** Normal dribble-control envelope. */
	inline constexpr float ControlRadius = 230.f;
	/** Control envelope while sprinting. */
	inline constexpr float SprintControlRadius = 245.f;
	/** Envelope while a sprint touch is released / a sprint stop is recovering. */
	inline constexpr float ReleaseControlRadius = 270.f;
	/** A pending sprint-stop recovery is abandoned beyond this gap. */
	inline constexpr float RecoveryAbandonDistance = 420.f;
	/** Seconds after a sprint release during which the touch is still recoverable. */
	inline constexpr float RecoveryWindow = 1.35f;
	/** Minimum planar clearance between the ball and either foot socket. */
	inline constexpr float FootClearance = 38.f;
	/** Dribbling is suspended for this long after a shot. */
	inline constexpr float PostShotLockout = .85f;
}
