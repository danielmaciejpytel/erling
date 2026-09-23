#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ErlingStylizedGoal.generated.h"

class UBoxComponent;
class UPhysicalMaterial;
class UPrimitiveComponent;
class UProceduralMeshComponent;
class USceneComponent;
class UStaticMeshComponent;

/** A goal whose local +X points behind the goal line. Place its origin at the front centre on the turf. */
UCLASS()
class ERLING_API AErlingStylizedGoal : public AActor
{
	GENERATED_BODY()

public:
	AErlingStylizedGoal();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Called by the existing ball hit handler before it absorbs the ball velocity. */
	void ReactToBall(UPrimitiveComponent* NetComponent, UPrimitiveComponent* BallComponent,
	                 const FVector& WorldImpact, float IncomingSpeed);
	UFUNCTION(BlueprintPure, Category = "Goal|Net")
	float GetMaxNetDisplacement() const;
	UFUNCTION(BlueprintPure, Category = "Goal|Net")
	int32 GetNetImpactCount() const { return NetImpactCount; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Goal")
	TObjectPtr<UStaticMeshComponent> FrameVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Goal")
	TObjectPtr<UProceduralMeshComponent> NetVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Goal|Collision")
	TObjectPtr<UBoxComponent> LeftPostCollision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Goal|Collision")
	TObjectPtr<UBoxComponent> RightPostCollision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Goal|Collision")
	TObjectPtr<UBoxComponent> CrossbarCollision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Goal|Collision")
	TObjectPtr<UBoxComponent> LeftSupportCollision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Goal|Collision")
	TObjectPtr<UBoxComponent> RightSupportCollision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Goal|Collision")
	TObjectPtr<UBoxComponent> LeftRearSupportCollision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Goal|Collision")
	TObjectPtr<UBoxComponent> RightRearSupportCollision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Goal|Collision")
	TObjectPtr<UBoxComponent> FrontNetCollision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Goal|Collision")
	TObjectPtr<UBoxComponent> SlopedNetCollision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Goal|Collision")
	TObjectPtr<UBoxComponent> RearNetCollision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Goal|Collision")
	TObjectPtr<UProceduralMeshComponent> LeftNetCollision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Goal|Collision")
	TObjectPtr<UProceduralMeshComponent> RightNetCollision;

private:
	struct FNetImpact
	{
		FVector LocalPoint = FVector::ZeroVector;
		TWeakObjectPtr<UPrimitiveComponent> LastBall;
		float Displacement = 0.f;
		float Velocity = 0.f;
		float LastHitAt = -100.f;
	};

	void BuildNet();
	void ConfigureCollision();
	void UpdateNetDeformation();
	void AppendRope(const TArray<FVector>& Points, uint8 Panel, float Radius);
	void AppendKnot(const FVector& Centre, uint8 Panel, float Radius);
	float AnchorWeight(const FVector& Point, uint8 Panel) const;
	FVector SlopedSurface(float Along, float Across) const;

	UPROPERTY(VisibleAnywhere, Category = "Goal")
	TObjectPtr<USceneComponent> GoalRoot;
	UPROPERTY(Transient)
	TObjectPtr<UPhysicalMaterial> NetPhysicalMaterial;
	UPROPERTY(VisibleAnywhere, Category = "Goal|Net")
	int32 NetImpactCount = 0;

	TArray<FVector> RestVertices;
	TArray<FVector> CurrentVertices;
	TArray<FVector> MeshNormals;
	TArray<FVector2D> MeshUVs;
	TArray<int32> MeshTriangles;
	TArray<uint8> VertexPanels;
	TArray<float> VertexWeights;
	FNetImpact Impacts[3];
};
