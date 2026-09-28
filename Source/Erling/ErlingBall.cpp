#include "ErlingBall.h"
#include "ErlingTuning.h"
#include "ErlingStylizedGoal.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

AErlingBall::AErlingBall()
{
	bReplicates = true;
	bStaticMeshReplicateMovement = true;
}
void AErlingBall::ConfigureLiveBall()
{
	UStaticMeshComponent* Ball = GetStaticMeshComponent();
	Ball->SetMobility(EComponentMobility::Movable);
	Ball->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	Ball->SetWorldScale3D(FVector(ErlingBall::MeshScale));
	Ball->SetCollisionProfileName(TEXT("PhysicsActor"));
	Ball->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Ball->SetSimulatePhysics(true);
	Ball->SetMassOverrideInKg(NAME_None, ErlingBall::MassKg);
	Ball->SetLinearDamping(ErlingBall::LinearDamping);
	Ball->SetAngularDamping(ErlingBall::AngularDamping);
	Ball->BodyInstance.bUseCCD = true;
	Ball->SetNotifyRigidBodyCollision(true);
	Ball->OnComponentHit.AddDynamic(this, &AErlingBall::NetHit);
	if (auto M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Erling/Materials/M_Ball.M_Ball")))
		Ball->SetMaterial(0, M);
	Ball->SetRenderCustomDepth(true);
	Ball->SetCustomDepthStencilValue(1);
	auto PM = NewObject<UPhysicalMaterial>(this);
	PM->Restitution = ErlingBall::Restitution;
	PM->Friction = ErlingBall::Friction;
	Ball->SetPhysMaterialOverride(PM);
}
void AErlingBall::NetHit(
    UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (HitComponent && OtherComp && OtherComp->ComponentHasTag(TEXT("GoalNet")))
	{
		auto V = HitComponent->GetPhysicsLinearVelocity();
		if (auto* Goal = Cast<AErlingStylizedGoal>(OtherComp->GetOwner()))
			Goal->ReactToBall(OtherComp, HitComponent, Hit.ImpactPoint, V.Size());
		HitComponent->SetPhysicsLinearVelocity(FVector(0, 0, FMath::Min(V.Z, 0.f)));
		HitComponent->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
	}
}
UStaticMeshComponent* AErlingBall::SpawnFinishedCopy()
{
	// Preserve the completed shot as an independent physical ball. Keep the live
	// component stable so input/contact code never observes a newly initialized body.
	auto* Old = GetStaticMeshComponent();
	auto* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(Old->GetComponentLocation(), Old->GetComponentRotation());
	if (!Actor)
		return nullptr;
	auto* Fresh = Actor->GetStaticMeshComponent();
	Fresh->SetMobility(EComponentMobility::Movable);
	Fresh->SetStaticMesh(Old->GetStaticMesh());
	Fresh->SetWorldScale3D(Old->GetComponentScale());
	for (int32 I = 0; I < Old->GetNumMaterials(); ++I)
		Fresh->SetMaterial(I, Old->GetMaterial(I));
	Fresh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Fresh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Fresh->SetSimulatePhysics(true);
	Fresh->SetMassOverrideInKg(NAME_None, ErlingBall::MassKg);
	Fresh->SetAngularDamping(ErlingBall::AngularDamping);
	Fresh->BodyInstance.bUseCCD = true;
	Fresh->SetPhysMaterialOverride(Old->BodyInstance.GetSimplePhysicalMaterial());
	Fresh->SetRenderCustomDepth(Old->bRenderCustomDepth);
	Fresh->SetCustomDepthStencilValue(Old->CustomDepthStencilValue);
	Fresh->SetNotifyRigidBodyCollision(true);
	Fresh->OnComponentHit.AddDynamic(this, &AErlingBall::NetHit);
	Fresh->SetVisibility(true);
	Fresh->SetLinearDamping(1.2f);
	Fresh->SetPhysicsLinearVelocity(Old->GetPhysicsLinearVelocity());
	Fresh->SetPhysicsAngularVelocityInRadians(Old->GetPhysicsAngularVelocityInRadians());
	// Finished attempts keep ground/net physics without deflecting the live ball.
	Fresh->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
	return Fresh;
}
