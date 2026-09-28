#pragma once
#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "ErlingBall.generated.h"

/**
 * The physical match ball: mesh, physics setup, net contact and the frozen copies
 * left on the pitch after finished attempts. Game modes spawn one and keep its
 * mesh component as their live ball. Replicates its movement for future
 * networked modes.
 */
UCLASS()
class ERLING_API AErlingBall : public AStaticMeshActor
{
	GENERATED_BODY()
public:
	AErlingBall();

	/** Mesh, material, physics and net contact for the playable ball. */
	void ConfigureLiveBall();
	/** Independent physical copy of the ball in its current state. */
	UStaticMeshComponent* SpawnFinishedCopy();

	UFUNCTION() void NetHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse,
	    const FHitResult& Hit);
};
