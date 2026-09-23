#include "ErlingStylizedGoal.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "ProceduralMeshComponent.h"

namespace
{
	constexpr float GoalHalfWidth = 350.f;
	constexpr float NetDepth = 250.f;
	constexpr float NetFrontHeight = 260.f;
	constexpr float NetRearHeight = 10.f;
	constexpr float NetFlatFrontDepth = 35.f;
	constexpr float NetShoulderDepth = 140.f;
	// The side profile follows the reference: a shallow top from the front
	// frame to a high shoulder, then a steep rear face down to the pink feet.
	constexpr float NetShoulderHeight = 235.f;
	constexpr float RopeRadius = 3.0f;
	constexpr float KnotRadius = 5.3f;
	constexpr float NetImpactResponseScale = 1.5f;
	constexpr int32 TubeSides = 8;
	constexpr int32 NetColumns = 18;
	// Space rear-face rows by height so the front view reads as a full grid.
	constexpr float NetRows[] = {0.f, 70.f, NetShoulderDepth, 155.f, 171.f, 186.f, 201.f, 217.f, 232.f, NetDepth};
	constexpr float SideColumns[] = {0.f, 50.f, 100.f, 150.f, 200.f, NetDepth};
	constexpr int32 SlopedPanel = 0;
	constexpr int32 LeftPanel = 1;
	constexpr int32 RightPanel = 2;

	float SmoothAnchor(float Distance, float Width)
	{
		const float T = FMath::Clamp(Distance / Width, 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	float NetTopAt(float X)
	{
		if (X <= NetFlatFrontDepth)
			return NetFrontHeight;
		if (X <= NetShoulderDepth)
			return FMath::Lerp(NetFrontHeight, NetShoulderHeight,
				(X - NetFlatFrontDepth) / (NetShoulderDepth - NetFlatFrontDepth));
		return FMath::Lerp(NetShoulderHeight, NetRearHeight,
			(X - NetShoulderDepth) / (NetDepth - NetShoulderDepth));
	}

	float NetEndXAtHeight(float Z)
	{
		if (Z >= NetFrontHeight)
			return NetFlatFrontDepth;
		if (Z >= NetShoulderHeight)
			return NetFlatFrontDepth + (NetFrontHeight - Z) *
				(NetShoulderDepth - NetFlatFrontDepth) / (NetFrontHeight - NetShoulderHeight);
		return FMath::Min(NetDepth, NetShoulderDepth + (NetShoulderHeight - Z) *
			(NetDepth - NetShoulderDepth) / (NetShoulderHeight - NetRearHeight));
	}
}

AErlingStylizedGoal::AErlingStylizedGoal()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	Tags.Add(TEXT("StylizedGoal"));

	GoalRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GoalRoot"));
	SetRootComponent(GoalRoot);

	FrameVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrameVisual"));
	FrameVisual->SetupAttachment(GoalRoot);
	FrameVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FrameVisual->SetCastShadow(true);

	NetVisual = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("NetVisual"));
	NetVisual->SetupAttachment(GoalRoot);
	NetVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NetVisual->SetCastShadow(false);
	NetVisual->bUseComplexAsSimpleCollision = false;

	auto MakeFrameBox = [this](const TCHAR* Name, const FVector& Centre, const FVector& Extent,
	                           const FRotator& Rotation = FRotator::ZeroRotator)
	{
		UBoxComponent* Box = CreateDefaultSubobject<UBoxComponent>(Name);
		Box->SetupAttachment(GoalRoot);
		Box->SetBoxExtent(Extent);
		Box->SetRelativeLocation(Centre);
		Box->SetRelativeRotation(Rotation);
		Box->SetCollisionProfileName(TEXT("BlockAll"));
		return Box;
	};

	// Keep the old inner edges of the playable goal mouth: Y = +/-341, Z = 251.
	LeftPostCollision = MakeFrameBox(TEXT("LeftPostCollision"), FVector(6, -356, 130), FVector(15, 15, 130));
	RightPostCollision = MakeFrameBox(TEXT("RightPostCollision"), FVector(6, 356, 130), FVector(15, 15, 130));
	CrossbarCollision = MakeFrameBox(TEXT("CrossbarCollision"), FVector(6, 0, 266), FVector(15, 371, 15));
	// The rear support is bent at the shoulder. Each straight span gets a
	// simple box so the ball sees the same two-piece silhouette as the art.
	const float UpperRun = 124.f;
	const float UpperDrop = 32.f;
	const float RearSupportRun = 110.f;
	const float RearSupportDrop = 227.f;
	LeftSupportCollision = MakeFrameBox(TEXT("LeftSupportCollision"), FVector(78, -356, 251),
	                                    FVector(FMath::Sqrt(FMath::Square(UpperRun) + FMath::Square(UpperDrop)) * .5f, 9, 9),
	                                    FRotator(-FMath::RadiansToDegrees(FMath::Atan2(UpperDrop, UpperRun)), 0, 0));
	RightSupportCollision = MakeFrameBox(TEXT("RightSupportCollision"), FVector(78, 356, 251),
	                                     FVector(FMath::Sqrt(FMath::Square(UpperRun) + FMath::Square(UpperDrop)) * .5f, 9, 9),
	                                     FRotator(-FMath::RadiansToDegrees(FMath::Atan2(UpperDrop, UpperRun)), 0, 0));
	LeftRearSupportCollision = MakeFrameBox(TEXT("LeftRearSupportCollision"), FVector(195, -356, 121.5f),
	                                        FVector(FMath::Sqrt(FMath::Square(RearSupportRun) + FMath::Square(RearSupportDrop)) * .5f, 9, 9),
	                                        FRotator(-FMath::RadiansToDegrees(FMath::Atan2(RearSupportDrop, RearSupportRun)), 0, 0));
	RightRearSupportCollision = MakeFrameBox(TEXT("RightRearSupportCollision"), FVector(195, 356, 121.5f),
	                                         FVector(FMath::Sqrt(FMath::Square(RearSupportRun) + FMath::Square(RearSupportDrop)) * .5f, 9, 9),
	                                         FRotator(-FMath::RadiansToDegrees(FMath::Atan2(RearSupportDrop, RearSupportRun)), 0, 0));

	// The short flat lead preserves the old under-crossbar clearance at the scoring plane.
	FrontNetCollision = MakeFrameBox(TEXT("FrontNetCollision"), FVector(NetFlatFrontDepth * .5f, 0, NetFrontHeight),
	                                 FVector(NetFlatFrontDepth * .5f, GoalHalfWidth, 4));
	FrontNetCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	FrontNetCollision->ComponentTags.Add(TEXT("GoalNet"));
	const float SlopeRun = NetShoulderDepth - NetFlatFrontDepth;
	const float SlopeDrop = NetFrontHeight - NetShoulderHeight;
	SlopedNetCollision = MakeFrameBox(TEXT("SlopedNetCollision"),
	                                  FVector((NetFlatFrontDepth + NetShoulderDepth) * .5f, 0,
	                                          (NetFrontHeight + NetShoulderHeight) * .5f),
	                                  FVector(FMath::Sqrt(FMath::Square(SlopeRun) + FMath::Square(SlopeDrop)) * .5f + 2.f,
	                                          GoalHalfWidth, 5),
	                                  FRotator(-FMath::RadiansToDegrees(FMath::Atan2(SlopeDrop, SlopeRun)), 0, 0));
	SlopedNetCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	SlopedNetCollision->ComponentTags.Add(TEXT("GoalNet"));
	const float RearRun = NetDepth - NetShoulderDepth;
	const float RearDrop = NetShoulderHeight - NetRearHeight;
	RearNetCollision = MakeFrameBox(TEXT("RearNetCollision"),
	                               FVector((NetShoulderDepth + NetDepth) * .5f, 0,
	                                       (NetShoulderHeight + NetRearHeight) * .5f),
	                               FVector(FMath::Sqrt(FMath::Square(RearRun) + FMath::Square(RearDrop)) * .5f + 2.f,
	                                       GoalHalfWidth, 5),
	                               FRotator(-FMath::RadiansToDegrees(FMath::Atan2(RearDrop, RearRun)), 0, 0));
	RearNetCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	RearNetCollision->ComponentTags.Add(TEXT("GoalNet"));

	auto MakeSideNet = [this](const TCHAR* Name, float Side)
	{
		UProceduralMeshComponent* SideCollision = CreateDefaultSubobject<UProceduralMeshComponent>(Name);
		SideCollision->SetupAttachment(GoalRoot);
		SideCollision->SetRelativeLocation(FVector(0, Side * GoalHalfWidth, 0));
		SideCollision->SetCollisionProfileName(TEXT("BlockAll"));
		SideCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		SideCollision->SetCastShadow(false);
		SideCollision->SetVisibility(false);
		SideCollision->bUseComplexAsSimpleCollision = false;
		SideCollision->bUseAsyncCooking = false;
		SideCollision->ComponentTags.Add(TEXT("GoalNet"));
		return SideCollision;
	};
	LeftNetCollision = MakeSideNet(TEXT("LeftNetCollision"), -1.f);
	RightNetCollision = MakeSideNet(TEXT("RightNetCollision"), 1.f);
}

void AErlingStylizedGoal::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	FrameVisual->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,
		TEXT("/Game/Erling/Goals/SM_Goal_Stylized.SM_Goal_Stylized")));
	ConfigureCollision();
	BuildNet();
}

void AErlingStylizedGoal::BeginPlay()
{
	Super::BeginPlay();
	// Construction state is not serialized for an actor loaded from a level.
	ConfigureCollision();
	BuildNet();
	SetActorTickEnabled(false);
}

void AErlingStylizedGoal::ConfigureCollision()
{
	if (!NetPhysicalMaterial)
	{
		NetPhysicalMaterial = NewObject<UPhysicalMaterial>(this);
		NetPhysicalMaterial->Restitution = 0.f;
		NetPhysicalMaterial->bOverrideRestitutionCombineMode = true;
		NetPhysicalMaterial->RestitutionCombineMode = EFrictionCombineMode::Min;
		NetPhysicalMaterial->Friction = .9f;
	}
	SlopedNetCollision->SetPhysMaterialOverride(NetPhysicalMaterial);
	FrontNetCollision->SetPhysMaterialOverride(NetPhysicalMaterial);
	RearNetCollision->SetPhysMaterialOverride(NetPhysicalMaterial);

	TArray<FVector> Convex;
	for (float Y : {-4.f, 4.f})
	{
		Convex.Add(FVector(0, Y, 0));
		Convex.Add(FVector(0, Y, NetFrontHeight));
		Convex.Add(FVector(NetFlatFrontDepth, Y, NetFrontHeight));
		Convex.Add(FVector(NetShoulderDepth, Y, NetShoulderHeight));
		Convex.Add(FVector(NetDepth, Y, NetRearHeight));
		Convex.Add(FVector(NetDepth, Y, 0));
	}
	TArray<TArray<FVector>> ConvexMeshes;
	ConvexMeshes.Add(Convex);
	const TArray<int32> HullTriangles = {
		0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 5,
		6, 8, 7, 6, 9, 8, 6, 10, 9, 6, 11, 10
	};
	for (UProceduralMeshComponent* Side : {LeftNetCollision.Get(), RightNetCollision.Get()})
	{
		// A hidden section supplies accurate component bounds; the convex alone blocks the ball.
		Side->CreateMeshSection(0, Convex, HullTriangles,
			TArray<FVector>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), false);
		Side->SetMeshSectionVisible(0, false);
		Side->SetCollisionConvexMeshes(ConvexMeshes);
		Side->SetPhysMaterialOverride(NetPhysicalMaterial);
	}
}

FVector AErlingStylizedGoal::SlopedSurface(float Along, float Across) const
{
	const float X = NetDepth * Along;
	return FVector(X, GoalHalfWidth * Across, NetTopAt(X));
}

float AErlingStylizedGoal::AnchorWeight(const FVector& Point, uint8 Panel) const
{
	if (Panel == SlopedPanel)
	{
		const float Front = SmoothAnchor(Point.X, 30.f);
		const float Sides = SmoothAnchor(GoalHalfWidth - FMath::Abs(Point.Y), 32.f);
		return Front * Sides;
	}
	const float Top = NetTopAt(Point.X);
	return SmoothAnchor(Point.X, 30.f) * SmoothAnchor(Top - Point.Z, 28.f);
}

void AErlingStylizedGoal::AppendRope(const TArray<FVector>& Points, uint8 Panel, float Radius)
{
	if (Points.Num() < 2)
		return;
	const int32 FirstVertex = RestVertices.Num();
	for (int32 I = 0; I < Points.Num(); ++I)
	{
		const FVector Tangent = (Points[FMath::Min(I + 1, Points.Num() - 1)] -
		                         Points[FMath::Max(I - 1, 0)]).GetSafeNormal();
		const FVector Reference = FMath::Abs(Tangent.Z) < .85f ? FVector::UpVector : FVector::RightVector;
		const FVector AxisA = FVector::CrossProduct(Tangent, Reference).GetSafeNormal();
		const FVector AxisB = FVector::CrossProduct(Tangent, AxisA).GetSafeNormal();
		const float Weight = AnchorWeight(Points[I], Panel);
		for (int32 Side = 0; Side < TubeSides; ++Side)
		{
			const float Angle = 2.f * PI * Side / TubeSides;
			const FVector Radial = FMath::Cos(Angle) * AxisA + FMath::Sin(Angle) * AxisB;
			RestVertices.Add(Points[I] + Radius * Radial);
			MeshNormals.Add(Radial);
			MeshUVs.Add(FVector2D(float(I) / float(Points.Num() - 1), float(Side) / TubeSides));
			VertexPanels.Add(Panel);
			VertexWeights.Add(Weight);
		}
	}
	for (int32 I = 0; I + 1 < Points.Num(); ++I)
		for (int32 Side = 0; Side < TubeSides; ++Side)
		{
			const int32 A = FirstVertex + I * TubeSides + Side;
			const int32 B = FirstVertex + (I + 1) * TubeSides + Side;
			const int32 C = FirstVertex + I * TubeSides + (Side + 1) % TubeSides;
			const int32 D = FirstVertex + (I + 1) * TubeSides + (Side + 1) % TubeSides;
			MeshTriangles.Append({A, C, B, C, D, B});
		}
}

void AErlingStylizedGoal::AppendKnot(const FVector& Centre, uint8 Panel, float Radius)
{
	// Rounded junctions are part of the same deforming mesh as the ropes.
	// Four latitude rings give a soft silhouette without a separate component
	// or a high triangle count for each junction.
	constexpr int32 Sides = 8;
	constexpr int32 Rings = 4;
	const int32 First = RestVertices.Num();
	const float Weight = AnchorWeight(Centre, Panel);
	auto AddVertex = [this, &Centre, Panel, Radius, Weight](const FVector& Normal, const FVector2D& UV)
	{
		RestVertices.Add(Centre + Radius * Normal);
		MeshNormals.Add(Normal);
		MeshUVs.Add(UV);
		VertexPanels.Add(Panel);
		VertexWeights.Add(Weight);
	};
	AddVertex(FVector::UpVector, FVector2D(.5f, 0.f));
	for (int32 Ring = 0; Ring < Rings; ++Ring)
	{
		const float Latitude = PI * (Ring + 1) / (Rings + 1);
		for (int32 Side = 0; Side < Sides; ++Side)
		{
			const float Longitude = 2.f * PI * Side / Sides;
			AddVertex(FVector(FMath::Sin(Latitude) * FMath::Cos(Longitude),
			                  FMath::Sin(Latitude) * FMath::Sin(Longitude), FMath::Cos(Latitude)),
			          FVector2D(float(Side) / Sides, float(Ring + 1) / (Rings + 1)));
		}
	}
	const int32 Bottom = RestVertices.Num();
	AddVertex(-FVector::UpVector, FVector2D(.5f, 1.f));
	for (int32 Side = 0; Side < Sides; ++Side)
	{
		const int32 Next = (Side + 1) % Sides;
		MeshTriangles.Append({First, First + 1 + Side, First + 1 + Next});
		for (int32 Ring = 0; Ring + 1 < Rings; ++Ring)
		{
			const int32 Upper = First + 1 + Ring * Sides;
			const int32 Lower = Upper + Sides;
			MeshTriangles.Append({Upper + Side, Lower + Side, Upper + Next,
			                      Upper + Next, Lower + Side, Lower + Next});
		}
		const int32 LastRing = First + 1 + (Rings - 1) * Sides;
		MeshTriangles.Append({LastRing + Side, Bottom, LastRing + Next});
	}
}

void AErlingStylizedGoal::BuildNet()
{
	RestVertices.Reset();
	CurrentVertices.Reset();
	MeshNormals.Reset();
	MeshUVs.Reset();
	MeshTriangles.Reset();
	VertexPanels.Reset();
	VertexWeights.Reset();

	// The reference has about eighteen openings across the mouth and eight
	// visible rear rows. The knots soften an otherwise even, readable grid.
	for (int32 Column = 0; Column <= NetColumns; ++Column)
	{
		TArray<FVector> Points;
		const float Across = -1.f + 2.f * Column / NetColumns;
		for (int32 Step = 0; Step <= 48; ++Step)
			Points.Add(SlopedSurface(float(Step) / 48.f, Across));
		AppendRope(Points, SlopedPanel, RopeRadius);
	}
	for (float RowX : NetRows)
	{
		TArray<FVector> Points;
		const float Along = RowX / NetDepth;
		for (int32 Step = 0; Step <= 72; ++Step)
			Points.Add(SlopedSurface(Along, -1.f + 2.f * Step / 72.f));
		AppendRope(Points, SlopedPanel, RopeRadius);
		for (int32 Column = 0; Column <= NetColumns; ++Column)
			AppendKnot(SlopedSurface(Along, -1.f + 2.f * Column / NetColumns), SlopedPanel, KnotRadius);
	}

	for (int32 Side : {-1, 1})
	{
		const uint8 Panel = Side < 0 ? LeftPanel : RightPanel;
		// A straight local plane keeps the side ties aligned at every crossing.
		auto SidePoint = [Side](float X, float Z)
		{
			return FVector(X, Side * GoalHalfWidth, Z);
		};
		for (float X : SideColumns)
		{
			TArray<FVector> Points;
			const float Top = NetTopAt(X);
			const int32 Steps = FMath::Max(1, FMath::CeilToInt((Top - 7.f) / 19.f));
			for (int32 Step = 0; Step <= Steps; ++Step)
				Points.Add(SidePoint(X, FMath::Lerp(7.f, Top, float(Step) / Steps)));
			AppendRope(Points, Panel, RopeRadius * .9f);
		}
		for (float Z : {7.f, 57.f, 107.f, 157.f, 207.f, 257.f})
		{
			TArray<FVector> Points;
			const float EndX = NetEndXAtHeight(Z);
			const int32 Steps = FMath::Max(1, FMath::CeilToInt(EndX / 19.f));
			for (int32 Step = 0; Step <= Steps; ++Step)
				Points.Add(SidePoint(EndX * Step / Steps, Z));
			AppendRope(Points, Panel, RopeRadius * .9f);
			for (float X : SideColumns)
				if (X <= EndX + .1f)
					AppendKnot(SidePoint(X, Z), Panel, KnotRadius * .9f);
		}
	}

	CurrentVertices = RestVertices;
	NetVisual->CreateMeshSection(0, RestVertices, MeshTriangles, MeshNormals, MeshUVs,
	                             TArray<FColor>(), TArray<FProcMeshTangent>(), false);
	UMaterialInterface* NetMaterial = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Erling/Goals/M_GoalNet_Stylized.M_GoalNet_Stylized"));
	if (!NetMaterial)
		NetMaterial = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Game/Erling/Materials/MI_Net.MI_Net"));
	if (NetMaterial)
		NetVisual->SetMaterial(0, NetMaterial);
}

void AErlingStylizedGoal::ReactToBall(UPrimitiveComponent* NetComponent, UPrimitiveComponent* BallComponent,
	                                 const FVector& WorldImpact, float IncomingSpeed)
{
	int32 Panel = INDEX_NONE;
	if (NetComponent == SlopedNetCollision || NetComponent == RearNetCollision ||
		NetComponent == FrontNetCollision)
		Panel = SlopedPanel;
	else if (NetComponent == LeftNetCollision)
		Panel = LeftPanel;
	else if (NetComponent == RightNetCollision)
		Panel = RightPanel;
	if (Panel == INDEX_NONE || !BallComponent || RestVertices.IsEmpty())
		return;

	FNetImpact& Impact = Impacts[Panel];
	const float Now = GetWorld()->GetTimeSeconds();
	if (Impact.LastBall.Get() == BallComponent && Now - Impact.LastHitAt < .08f)
		return;
	Impact.LastBall = BallComponent;
	Impact.LastHitAt = Now;
	++NetImpactCount;
	Impact.LocalPoint = GetActorTransform().InverseTransformPosition(
		WorldImpact.IsNearlyZero() ? BallComponent->GetComponentLocation() : WorldImpact);
	Impact.Displacement = FMath::Max(Impact.Displacement,
		FMath::Clamp((5.f + IncomingSpeed * .0025f) * NetImpactResponseScale,
			6.f * NetImpactResponseScale, 12.f * NetImpactResponseScale));
	Impact.Velocity = 0.f;
	SetActorTickEnabled(true);
	UpdateNetDeformation();
}

float AErlingStylizedGoal::GetMaxNetDisplacement() const
{
	float MaxDisplacement = 0.f;
	for (const FNetImpact& Impact : Impacts)
		MaxDisplacement = FMath::Max(MaxDisplacement, FMath::Abs(Impact.Displacement));
	return MaxDisplacement;
}

void AErlingStylizedGoal::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	float Remaining = FMath::Min(DeltaSeconds, .25f);
	bool Active = false;
	while (Remaining > 0.f)
	{
		const float Step = FMath::Min(Remaining, 1.f / 120.f);
		Remaining -= Step;
		for (FNetImpact& Impact : Impacts)
		{
			Impact.Velocity += (-190.f * Impact.Displacement - 18.f * Impact.Velocity) * Step;
			Impact.Displacement = FMath::Clamp(Impact.Displacement + Impact.Velocity * Step,
				-2.f * NetImpactResponseScale, 12.f * NetImpactResponseScale);
		}
	}
	for (FNetImpact& Impact : Impacts)
	{
		if (FMath::Abs(Impact.Displacement) < .035f && FMath::Abs(Impact.Velocity) < .15f)
		{
			Impact.Displacement = 0.f;
			Impact.Velocity = 0.f;
		}
		Active |= Impact.Displacement != 0.f;
	}
	UpdateNetDeformation();
	if (!Active)
		SetActorTickEnabled(false);
}

void AErlingStylizedGoal::UpdateNetDeformation()
{
	if (RestVertices.IsEmpty())
		return;
	const FVector SideDirections[2] = {
		FVector(.2f, -1, .08f).GetSafeNormal(),
		FVector(.2f, 1, .08f).GetSafeNormal()
	};
	for (int32 I = 0; I < RestVertices.Num(); ++I)
	{
		const int32 Panel = VertexPanels[I];
		const FNetImpact& Impact = Impacts[Panel];
		const float DistanceSquared = FVector::DistSquared(RestVertices[I], Impact.LocalPoint);
		const float Falloff = FMath::Exp(-DistanceSquared / (2.f * FMath::Square(115.f)));
		const FVector Direction = Panel == SlopedPanel
			? FMath::Lerp(FVector(.25f, 0, .97f), FVector(1.f, 0, .08f),
				FMath::Clamp((RestVertices[I].X - NetFlatFrontDepth) / (NetDepth - NetFlatFrontDepth), 0.f, 1.f)).GetSafeNormal()
			: SideDirections[Panel - LeftPanel];
		CurrentVertices[I] = RestVertices[I] + Direction *
			(Impact.Displacement * VertexWeights[I] * Falloff);
	}
	NetVisual->UpdateMeshSection(0, CurrentVertices, TArray<FVector>(), TArray<FVector2D>(),
	                            TArray<FColor>(), TArray<FProcMeshTangent>());
}
