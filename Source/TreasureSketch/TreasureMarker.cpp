#include "TreasureMarker.h"
#include "TreasureRules.h"
#include "TreasureSketchGameState.h"

#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ATreasureMarker::ATreasureMarker()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = false;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(SceneRoot);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BaseMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

    BarA = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarA"));
    BarA->SetupAttachment(SceneRoot);
    BarA->SetStaticMesh(Cube.Object);
    BarA->SetRelativeScale3D(FVector(2.2f, 0.28f, 0.08f));
    BarA->SetRelativeRotation(FRotator(0.f, 45.f, 0.f));
    BarA->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BarA->SetMaterial(0, BaseMaterial.Object);

    BarB = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarB"));
    BarB->SetupAttachment(SceneRoot);
    BarB->SetStaticMesh(Cube.Object);
    BarB->SetRelativeScale3D(FVector(2.2f, 0.28f, 0.08f));
    BarB->SetRelativeRotation(FRotator(0.f, -45.f, 0.f));
    BarB->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BarB->SetMaterial(0, BaseMaterial.Object);
}

void ATreasureMarker::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!GetWorld()) return;
    const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>();
    if (GS && !GS->bTreasureRangeVisible) return;

    const FVector Center = GetActorLocation();
    const FVector Bottom = Center - FVector(0.f, 0.f, TreasureRules::DigVerticalHalfHeight);
    const FVector Top = Center + FVector(0.f, 0.f, TreasureRules::DigVerticalHalfHeight);
    DrawDebugCylinder(GetWorld(), Bottom, Top, TreasureRules::DigHorizontalRadius, 48,
        FColor(255, 45, 25), false, 0.f, 0, 7.f);
    DrawDebugCircle(GetWorld(), Center, TreasureRules::DigHorizontalRadius, 64,
        FColor(255, 220, 40), false, 0.f, 0, 10.f, FVector(1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), false);
}

void ATreasureMarker::BeginPlay()
{
    Super::BeginPlay();
    for (UStaticMeshComponent* Bar : { BarA.Get(), BarB.Get() })
    {
        if (UMaterialInstanceDynamic* Material = Bar->CreateAndSetMaterialInstanceDynamic(0))
        {
            Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.9f, 0.01f, 0.01f, 1.f));
        }
    }
}
