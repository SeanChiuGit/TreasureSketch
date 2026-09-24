#include "TreasureMarker.h"

#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ATreasureMarker::ATreasureMarker()
{
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
