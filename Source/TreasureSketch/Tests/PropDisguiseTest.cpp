#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "../PropDisguise.h"
#include "../TreasureSketchCharacter.h"
#include "../TreasureSketchPlayerController.h"
#include "../TreasureSketchPlayerState.h"
#include "../TreasureSketchGameState.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPropDisguiseTest, "TreasureSketch.RoomSettings.PropDisguise",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPropDisguiseTest::RunTest(const FString& Parameters)
{
    auto* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto* GS = World->SpawnActor<ATreasureSketchGameState>();
    World->SetGameState(GS);
    GS->RoomMode = ETreasureRoomMode::HideAndSeek;
    GS->bGameStarted = true;
    GS->Phase = ETreasureRoundPhase::HunterSearching;
    GS->RoundSerial = 1;
    auto* Character = World->SpawnActor<ATreasureSketchCharacter>();
    auto* PC = World->SpawnActor<ATreasureSketchPlayerController>();
    auto* PS = World->SpawnActor<ATreasureSketchPlayerState>();
    PC->SetPlayerState(PS);
    PS->SetOwner(PC);
    PS->PlayerRole = ETreasurePlayerRole::Hunter;
    PC->Possess(Character);
    auto* Target = World->SpawnActor<AActor>();
    auto* Instances = NewObject<UInstancedStaticMeshComponent>(Target);
    Target->SetRootComponent(Instances);
    Instances->RegisterComponent();
    auto* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    Instances->SetStaticMesh(Cube);
    Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    const FVector SmallScale(0.25f, 0.5f, 0.75f);
    const FRotator SourceRotation(10.f, 30.f, 15.f);
    Instances->AddInstance(FTransform(SourceRotation, FVector(1000.f, 0.f, 0.f), SmallScale), true);
    auto* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    auto* Material = UMaterialInstanceDynamic::Create(Base, Target);
    const FLinearColor PropColor(0.2f, 0.7f, 0.1f);
    Material->SetVectorParameterValue(TEXT("Color"), PropColor);
    Instances->SetMaterial(0, Material);
    FPropDisguise Form;
    TestTrue(TEXT("Noncolliding decorative instances can be selected"),
        PropDisguise::FindTarget(World, FVector::ZeroVector, FVector::ForwardVector, Form));
    TestTrue(TEXT("Exact target instance scale is copied"), Form.Scale.Equals(SmallScale));
    TestTrue(TEXT("Target color is copied"), Form.Colors.Num() == 1 && Form.Colors[0].Equals(PropColor));
    TestTrue(TEXT("Dynamic material uses a network-addressable parent"), Form.Materials.Num() == 1 && Form.Materials[0] == Base);
    Character->SetActorRotation(FRotator(0.f, 70.f, 0.f));
    PC->ServerTransformIntoProp_Implementation(1, FVector::ZeroVector, FVector::ForwardVector, false);
    TestTrue(TEXT("Hider can transform"), Character->IsPropDisguised());
    TestTrue(TEXT("Disguise preserves source rotation even when player faces another direction"),
        Character->DisguiseMesh->GetComponentQuat().Equals(SourceRotation.Quaternion(), 0.001f));
    TestFalse(TEXT("Human body is hidden"), Character->GetMesh()->IsVisible());
    TestTrue(TEXT("Prop mesh is visible"), Character->DisguiseMesh->IsVisible());
    Character->PlayDigAnimation();
    TestFalse(TEXT("Digging does not expose the human staff"), Character->HandStaffMesh->IsVisible());
    TestTrue(TEXT("Digging preserves disguise"), Character->IsPropDisguised());
    TestEqual(TEXT("Disguise does not slow walking"), Character->GetCharacterMovement()->MaxWalkSpeed, 520.f);
    TestEqual(TEXT("Disguise preserves jumping"), Character->GetCharacterMovement()->JumpZVelocity, 620.f);

    Instances->ClearInstances();
    const FVector LargeScale(30.f, 15.f, 20.f);
    Instances->AddInstance(FTransform(FRotator::ZeroRotator, FVector(50000.f, 0.f, 0.f), LargeScale), true);
    PC->ServerTransformIntoProp_Implementation(1, FVector::ZeroVector, FVector::ForwardVector, false);
    TestTrue(TEXT("No distance limit or cooldown when changing props"), Character->Disguise.Scale.Equals(LargeScale));
    TestTrue(TEXT("Large props keep original size"), Character->DisguiseMesh->GetRelativeScale3D().Equals(LargeScale));
    TestTrue(TEXT("Camera accommodates large props"), Character->GetDisguiseViewDistance() > 500.f);
    for (int32 Index = 0; Index < 5; ++Index)
    {
        PC->ServerTransformIntoProp_Implementation(1, FVector::ZeroVector, FVector::ForwardVector, true);
        TestFalse(TEXT("Restore is immediate"), Character->IsPropDisguised());
        PC->ServerTransformIntoProp_Implementation(1, FVector::ZeroVector, FVector::ForwardVector, false);
        TestTrue(TEXT("Repeated transformation has no usage limit"), Character->IsPropDisguised());
    }
    Character->SetPropDisguise(FPropDisguise());
    PS->PlayerRole = ETreasurePlayerRole::Scout;
    PC->ServerTransformIntoProp_Implementation(1, FVector::ZeroVector, FVector::ForwardVector, false);
    TestFalse(TEXT("Catcher does not receive the hider skill"), Character->IsPropDisguised());
    PS->PlayerRole = ETreasurePlayerRole::Hunter;
    PC->ServerTransformIntoProp_Implementation(0, FVector::ZeroVector, FVector::ForwardVector, false);
    TestFalse(TEXT("Old-round requests do not affect a new round"), Character->IsPropDisguised());
    Instances->SetHiddenInGame(true);
    TestFalse(TEXT("Invisible collision helper meshes are not props"),
        PropDisguise::FindTarget(World, FVector::ZeroVector, FVector::ForwardVector, Form));
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}
#endif
