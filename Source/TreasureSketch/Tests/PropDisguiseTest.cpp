#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "InputKeyEventArgs.h"
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
    // This isolated world does not initialize actors for play; allow the UI's
    // actual RPC wrapper to execute rather than bypassing it in the test.
    TGuardValue<bool> AllowScriptExecution(GAllowActorScriptExecutionInEditor, true);
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
    GS->ApplyMovementSpeed();
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
    const FVector SecondLocation(1000.f, 300.f, 0.f);
    Instances->AddInstance(FTransform(SourceRotation, SecondLocation, SmallScale), true);
    PC->TransformIntoProp();
    PC->UpdatePropSelection(0.2f);
    PC->ReleasePropSelection();
    TestFalse(TEXT("A short click does not enter prop selection"), PC->IsPropSelectionMode());
    TestFalse(TEXT("A short click does not transform"), Character->IsPropDisguised());
    PC->TransformIntoProp();
    PC->UpdatePropSelection(0.4f);
    TestTrue(TEXT("Holding left click opens the selection viewfinder"), PC->IsPropSelectionMode());
    PC->UpdatePropSelection(0.8f);
    TestFalse(TEXT("Continuing the opening hold never confirms a disguise"), Character->IsPropDisguised());
    PC->UpdatePropSelectionTarget(FVector::ZeroVector, SecondLocation.GetSafeNormal());
    TestTrue(TEXT("Aimed instance is highlighted"), PC->HasPropSelectionTarget());
    TestTrue(TEXT("Highlight matches only the aimed instance location"),
        PC->PropSelectionHighlight->GetComponentLocation().Equals(SecondLocation, 0.001f));
    PropDisguise::FTarget Picked;
    TestTrue(TEXT("Visible selection overlay does not become the target"),
        PropDisguise::FindTarget(World, FVector::ZeroVector, SecondLocation.GetSafeNormal(), Form, &Picked)
        && Picked.Component.Get() == Instances && Picked.InstanceIndex == 1);
    TestTrue(TEXT("Highlight preserves the original prop material color"), Form.Colors[0].Equals(PropColor));
    TestEqual(TEXT("Highlight does not remove or replace map instances"), Instances->GetInstanceCount(), 2);
    PC->TransformIntoProp();
    TestFalse(TEXT("Opening press must be released before confirmation"), Character->IsPropDisguised());
    PC->ReleasePropSelection();
    PC->UpdatePropSelectionTarget(FVector::ZeroVector, -FVector::ForwardVector);
    TestFalse(TEXT("Looking at empty space clears the highlight"), PC->HasPropSelectionTarget());
    PC->TransformIntoProp();
    TestTrue(TEXT("An empty confirmation keeps the selector open"), PC->IsPropSelectionMode());
    TestFalse(TEXT("An empty confirmation does not transform"), Character->IsPropDisguised());
    PC->ReleasePropSelection();
    PC->UpdatePropSelectionTarget(FVector::ZeroVector, SecondLocation.GetSafeNormal());
    PC->TransformIntoProp();
    TestTrue(TEXT("The next click confirms the highlighted prop"), Character->IsPropDisguised());
    TestFalse(TEXT("Confirming closes the viewfinder"), PC->IsPropSelectionMode());
    TestFalse(TEXT("Confirming removes the local highlight"), PC->PropSelectionHighlight->IsVisible());
    Character->SetPropDisguise(FPropDisguise());
    PC->TransformIntoProp();
    PC->UpdatePropSelection(0.4f);
    PC->UpdatePropSelectionTarget(FVector::ZeroVector, FVector::ForwardVector);
    FInputKeyEventArgs Escape;
    Escape.Key = EKeys::Escape;
    Escape.Event = IE_Pressed;
    Escape.AmountDepressed = 1.f;
    TestTrue(TEXT("Escape is handled by prop selection"), PC->InputKey(Escape));
    TestFalse(TEXT("Escape closes the selector without opening pause"), PC->IsPropSelectionMode() || PC->bPauseMenuOpen);
    TestFalse(TEXT("Escape removes the highlight without transforming"), PC->PropSelectionHighlight->IsVisible() || Character->IsPropDisguised());
    PC->TransformIntoProp();
    PC->UpdatePropSelection(0.4f);
    PC->UpdatePropSelectionTarget(FVector::ZeroVector, FVector::ForwardVector);
    ++GS->RoundSerial;
    PC->UpdatePropSelection(0.016f);
    TestFalse(TEXT("New round invalidates selection and clears its highlight"), PC->IsPropSelectionMode() || PC->PropSelectionHighlight->IsVisible());
    GS->RoundSerial = 1;
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
    TestEqual(TEXT("Disguise does not slow walking"), Character->GetCharacterMovement()->MaxWalkSpeed, 1040.f);
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
