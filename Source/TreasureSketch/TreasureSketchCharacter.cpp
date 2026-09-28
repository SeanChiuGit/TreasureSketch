#include "TreasureSketchCharacter.h"
#include "TreasureSketchGameState.h"
#include "Engine/World.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ATreasureSketchCharacter::ATreasureSketchCharacter()
{
    GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);
    GetCharacterMovement()->MaxWalkSpeed = 520.f;
    GetCharacterMovement()->JumpZVelocity = 620.f;
    GetCharacterMovement()->AirControl = 0.25f;

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 500.f;
    CameraBoom->bUsePawnControlRotation = true;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BaseMaterial(
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

    BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
    HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
    LeftArmMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftArmMesh"));
    RightArmMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightArmMesh"));
    LeftLegMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftLegMesh"));
    RightLegMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightLegMesh"));
    FaceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FaceMesh"));

    const auto SetupPart = [this](UStaticMeshComponent* Part,
        bool bSphere, const FVector& Location, const FVector& Scale)
    {
        Part->SetupAttachment(GetCapsuleComponent());
        Part->SetStaticMesh(bSphere ? Sphere.Object : Cube.Object);
        Part->SetRelativeLocation(Location);
        Part->SetRelativeScale3D(Scale);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetMaterial(0, BaseMaterial.Object);
    };
    SetupPart(BodyMesh.Get(), false, FVector(0.f, 0.f, -8.f), FVector(0.45f, 0.34f, 0.70f));
    SetupPart(HeadMesh.Get(), true, FVector(0.f, 0.f, 47.f), FVector(0.36f, 0.36f, 0.36f));
    SetupPart(LeftArmMesh.Get(), false, FVector(0.f, -28.f, -14.f), FVector(0.16f, 0.18f, 0.58f));
    SetupPart(RightArmMesh.Get(), false, FVector(0.f, 28.f, -14.f), FVector(0.16f, 0.18f, 0.58f));
    SetupPart(LeftLegMesh.Get(), false, FVector(0.f, -13.f, -63.f), FVector(0.20f, 0.20f, 0.58f));
    SetupPart(RightLegMesh.Get(), false, FVector(0.f, 13.f, -63.f), FVector(0.20f, 0.20f, 0.58f));
    SetupPart(FaceMesh.Get(), false, FVector(20.f, 0.f, 49.f), FVector(0.06f, 0.22f, 0.10f));

    bUseControllerRotationYaw = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;
}

void ATreasureSketchCharacter::BeginPlay()
{
    Super::BeginPlay();
    if (const ATreasureSketchGameState* GS = GetWorld()->GetGameState<ATreasureSketchGameState>())
        SetMovementSpeedMultiplier(GS->MovementSpeedMultiplier);
    const auto ColorPart = [](UStaticMeshComponent* Part, const FLinearColor& Color)
    {
        if (UMaterialInstanceDynamic* Material = Part->CreateAndSetMaterialInstanceDynamic(0))
            Material->SetVectorParameterValue(TEXT("Color"), Color);
    };
    ColorPart(BodyMesh.Get(), FLinearColor(0.08f, 0.55f, 0.68f));
    ColorPart(HeadMesh.Get(), FLinearColor(0.94f, 0.72f, 0.49f));
    ColorPart(LeftArmMesh.Get(), FLinearColor(0.08f, 0.55f, 0.68f));
    ColorPart(RightArmMesh.Get(), FLinearColor(0.08f, 0.55f, 0.68f));
    ColorPart(LeftLegMesh.Get(), FLinearColor(0.08f, 0.15f, 0.28f));
    ColorPart(RightLegMesh.Get(), FLinearColor(0.08f, 0.15f, 0.28f));
    ColorPart(FaceMesh.Get(), FLinearColor(0.02f, 0.06f, 0.09f));
}

void ATreasureSketchCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ATreasureSketchCharacter, bSpectatorHidden);
    DOREPLIFETIME(ATreasureSketchCharacter, bShoveWindingUp);
}

void ATreasureSketchCharacter::SetShoveWindingUp(bool bWindingUp)
{
    if (!HasAuthority() || bShoveWindingUp == bWindingUp) return;
    bShoveWindingUp = bWindingUp;
    OnRep_ShoveWindingUp();
    ForceNetUpdate();
}

void ATreasureSketchCharacter::OnRep_ShoveWindingUp()
{
    // The raised, orange arm is visible to nearby players during the windup.
    RightArmMesh->SetRelativeLocation(bShoveWindingUp ? FVector(30.f, 28.f, 3.f) : FVector(0.f, 28.f, -14.f));
    RightArmMesh->SetRelativeRotation(bShoveWindingUp ? FRotator(-65.f, 0.f, 0.f) : FRotator::ZeroRotator);
    if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(RightArmMesh->GetMaterial(0)))
        Material->SetVectorParameterValue(TEXT("Color"), bShoveWindingUp
            ? FLinearColor(1.f, 0.33f, 0.04f) : FLinearColor(0.08f, 0.55f, 0.68f));
}

void ATreasureSketchCharacter::SetMovementSpeedMultiplier(float Multiplier)
{
    const float Scale = FMath::IsFinite(Multiplier) ? FMath::Clamp(Multiplier, 0.5f, 5.f) : 1.f;
    GetCharacterMovement()->MaxWalkSpeed = 520.f * Scale;
    GetCharacterMovement()->MaxAcceleration = 2048.f * Scale;
    GetCharacterMovement()->BrakingDecelerationWalking = 2048.f * Scale;
}

void ATreasureSketchCharacter::SetSpectatorHidden(bool bShouldHide)
{
    if (!HasAuthority()) return;
    bSpectatorHidden = bShouldHide;
    OnRep_SpectatorHidden();
    ForceNetUpdate();
}

void ATreasureSketchCharacter::OnRep_SpectatorHidden()
{
    SetActorHiddenInGame(bSpectatorHidden);
    GetCapsuleComponent()->SetCollisionEnabled(
        bSpectatorHidden ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
    GetCharacterMovement()->SetMovementMode(bSpectatorHidden ? MOVE_None : MOVE_Walking);
}

void ATreasureSketchCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    PlayerInputComponent->BindAxis("MoveForward", this, &ATreasureSketchCharacter::MoveForward);
    PlayerInputComponent->BindAxis("MoveRight", this, &ATreasureSketchCharacter::MoveRight);
    PlayerInputComponent->BindAxis("Turn", this, &ATreasureSketchCharacter::Turn);
    PlayerInputComponent->BindAxis("LookUp", this, &ATreasureSketchCharacter::LookUp);
    PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
    PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
}

void ATreasureSketchCharacter::MoveForward(float Value)
{
    if (Controller && !FMath::IsNearlyZero(Value))
    {
        const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
        AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Value);
    }
}

void ATreasureSketchCharacter::MoveRight(float Value)
{
    if (Controller && !FMath::IsNearlyZero(Value))
    {
        const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
        AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Value);
    }
}

void ATreasureSketchCharacter::Turn(float Value) { AddControllerYawInput(Value); }
void ATreasureSketchCharacter::LookUp(float Value) { AddControllerPitchInput(Value); }
