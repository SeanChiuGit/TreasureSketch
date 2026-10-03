#include "TreasureSketchCharacter.h"
#include "TreasureSketchGameState.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Animation/AnimSequence.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
void CaptureMagePreview(const TCHAR* Name)
{
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots/MagePreview"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    FScreenshotRequest::RequestScreenshot(
        FPaths::Combine(Directory, FString(Name) + TEXT(".png")), true, false);
}
}

ATreasureSketchCharacter::ATreasureSketchCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);
    GetCharacterMovement()->MaxWalkSpeed = 520.f;
    GetCharacterMovement()->JumpZVelocity = NormalJumpVelocity;
    GetCharacterMovement()->AirControl = 0.25f;

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 500.f;
    CameraBoom->bUsePawnControlRotation = true;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;
    DisguiseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DisguiseMesh"));
    DisguiseMesh->SetupAttachment(GetCapsuleComponent());
    DisguiseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DisguiseMesh->SetVisibility(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BaseMaterial(
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> MageExplorerMesh(
        TEXT("/Game/Characters/KayKitMageExplorer/SK_KayKitMageExplorer.SK_KayKitMageExplorer"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> MageIdle(
        TEXT("/Game/Characters/KayKitMageExplorer/SK_KayKitMageExplorer_Anim_Rig_Medium_A_MageExplorer_Idle_A.SK_KayKitMageExplorer_Anim_Rig_Medium_A_MageExplorer_Idle_A"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> MageRun(
        TEXT("/Game/Characters/KayKitMageExplorer/SK_KayKitMageExplorer_Anim_Rig_Medium_A_MageExplorer_Running_A.SK_KayKitMageExplorer_Anim_Rig_Medium_A_MageExplorer_Running_A"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> MageJump(
        TEXT("/Game/Characters/KayKitMageExplorer/SK_KayKitMageExplorer_Anim_Rig_Medium_A_MageExplorer_Jump_Full_Short.SK_KayKitMageExplorer_Anim_Rig_Medium_A_MageExplorer_Jump_Full_Short"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> MageDig(
        TEXT("/Game/Characters/KayKitMageExplorer/SK_KayKitMageExplorer_Anim_Rig_Medium_A_MageExplorer_WandDig.SK_KayKitMageExplorer_Anim_Rig_Medium_A_MageExplorer_WandDig"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> MageRead(
        TEXT("/Game/Characters/KayKitMageExplorer/SK_KayKitMageExplorer_Anim_Rig_Medium_A_MageExplorer_ReadBook.SK_KayKitMageExplorer_Anim_Rig_Medium_A_MageExplorer_ReadBook"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> MageShove(
        TEXT("/Game/Characters/KayKitMageExplorer/SK_KayKitMageExplorer_Anim_Rig_Medium_A_MageExplorer_Throw.SK_KayKitMageExplorer_Anim_Rig_Medium_A_MageExplorer_Throw"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ClosedBook(
        TEXT("/Game/Characters/KayKitMageExplorer/Props/SM_Spellbook_Closed.SM_Spellbook_Closed"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> OpenBook(
        TEXT("/Game/Characters/KayKitMageExplorer/Props/SM_Spellbook_Open.SM_Spellbook_Open"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MagicStaff(
        TEXT("/Game/Characters/KayKitMageExplorer/Props/SM_MagicStaff.SM_MagicStaff"));

    if (MageExplorerMesh.Succeeded())
    {
        GetMesh()->SetSkeletalMeshAsset(MageExplorerMesh.Object);
        GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -96.f));
        GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
        GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    }
    IdleAnimation = MageIdle.Object;
    RunAnimation = MageRun.Object;
    JumpAnimation = MageJump.Object;
    DigAnimation = MageDig.Object;
    ReadBookAnimation = MageRead.Object;
    ShoveAnimation = MageShove.Object;

    BackBookMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackBookMesh"));
    // Back gear is placed in character space so it stays stable while the
    // single-node animations switch.  The imported KayKit character faces +X.
    BackBookMesh->SetupAttachment(GetCapsuleComponent());
    BackBookMesh->SetStaticMesh(ClosedBook.Object);
    BackBookMesh->SetRelativeLocation(FVector(-60.f, 26.f, 8.f));
    BackBookMesh->SetRelativeRotation(FRotator(5.f, -10.f, 18.f));
    BackBookMesh->SetRelativeScale3D(FVector(0.84f));
    BackBookMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    BackStaffMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackStaffMesh"));
    BackStaffMesh->SetupAttachment(GetCapsuleComponent());
    BackStaffMesh->SetStaticMesh(MagicStaff.Object);
    BackStaffMesh->SetRelativeLocation(FVector(-62.f, -24.f, 22.f));
    BackStaffMesh->SetRelativeRotation(FRotator(0.f, 7.f, 28.f));
    BackStaffMesh->SetRelativeScale3D(FVector(0.82f));
    BackStaffMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    OpenBookMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OpenBookMesh"));
    OpenBookMesh->SetupAttachment(GetCapsuleComponent());
    OpenBookMesh->SetStaticMesh(OpenBook.Object);
    OpenBookMesh->SetRelativeLocation(FVector(49.f, 0.f, 0.f));
    OpenBookMesh->SetRelativeRotation(FRotator(-8.f, 90.f, 0.f));
    OpenBookMesh->SetRelativeScale3D(FVector(0.58f));
    OpenBookMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    OpenBookMesh->SetVisibility(false);

    HandStaffMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HandStaffMesh"));
    // Match the prop position authored against the WandDig pose.  The imported
    // KayKit helper bones use a different basis in UE, so their zero transform
    // leaves the staff visibly detached from the palm.
    HandStaffMesh->SetupAttachment(GetCapsuleComponent());
    HandStaffMesh->SetStaticMesh(MagicStaff.Object);
    HandStaffMesh->SetRelativeLocation(FVector(38.f, -60.f, -35.f));
    HandStaffMesh->SetRelativeRotation(FRotator(0.f, 4.f, -8.f));
    HandStaffMesh->SetRelativeScale3D(FVector(0.76f));
    HandStaffMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HandStaffMesh->SetVisibility(false);

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

    if (GetMesh()->GetSkeletalMeshAsset())
    {
        BodyMesh->SetVisibility(false);
        HeadMesh->SetVisibility(false);
        LeftArmMesh->SetVisibility(false);
        RightArmMesh->SetVisibility(false);
        LeftLegMesh->SetVisibility(false);
        RightLegMesh->SetVisibility(false);
        FaceMesh->SetVisibility(false);
        SetSuitColor(FLinearColor(0.035f, 0.58f, 0.62f));
        PlayPartyAnimation(IdleAnimation, true);
    }

    if (FParse::Param(FCommandLine::Get(), TEXT("MagePreview")) && GetWorld())
    {
        if (AController* OwningController = GetController())
            OwningController->SetControlRotation(FRotator(-8.f, 155.f, 0.f));

        FTimerHandle IdleShot;
        GetWorldTimerManager().SetTimer(IdleShot, []
        {
            CaptureMagePreview(TEXT("01_Idle"));
        }, 3.f, false);

        FTimerHandle StartRead;
        GetWorldTimerManager().SetTimer(StartRead, [this]
        {
            PlayReadBookAnimation();
        }, 5.f, false);

        FTimerHandle ReadShot;
        GetWorldTimerManager().SetTimer(ReadShot, []
        {
            CaptureMagePreview(TEXT("02_ReadBook"));
        }, 6.f, false);

        FTimerHandle StartDig;
        GetWorldTimerManager().SetTimer(StartDig, [this]
        {
            PlayDigAnimation();
        }, 8.f, false);

        FTimerHandle DigShot;
        GetWorldTimerManager().SetTimer(DigShot, []
        {
            CaptureMagePreview(TEXT("03_WandDig"));
        }, 8.65f, false);

        FTimerHandle StartRun;
        GetWorldTimerManager().SetTimer(StartRun, [this]
        {
            SetDefaultGearVisibility();
            bActionGearVisible = false;
            PlayPartyAnimation(RunAnimation, true, 1.15f);
            PartyActionUntil = GetWorld()->GetTimeSeconds() + 3.f;
        }, 11.f, false);

        FTimerHandle RunShot;
        GetWorldTimerManager().SetTimer(RunShot, []
        {
            CaptureMagePreview(TEXT("04_Run"));
        }, 11.6f, false);
    }
}

void ATreasureSketchCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (IsPropDisguised()) HideNormalDisguiseParts();
    if (!GetMesh()->GetSkeletalMeshAsset() || !GetWorld()) return;

    // Keep the staff grip locked to the animated right palm while preserving
    // the mostly upright orientation from the Blender WandDig preview.
    if (HandStaffMesh->IsVisible())
    {
        // Unreal sanitizes KayKit's Blender bone name `hand.r` to `hand_r`
        // during FBX import. Using the dotted name silently returned the mesh
        // root at the character's feet.
        const FVector HandLocation = GetMesh()->GetSocketLocation(TEXT("hand_r"));
        HandStaffMesh->SetWorldLocation(HandLocation - GetActorUpVector() * 18.f);
        HandStaffMesh->SetWorldRotation(FRotator(8.f, GetActorRotation().Yaw + 4.f, 0.f));
    }

    const float Now = GetWorld()->GetTimeSeconds();
    if (bReadingBook) return;
    if (Now < PartyActionUntil) return;
    if (bActionGearVisible)
    {
        SetDefaultGearVisibility();
        bActionGearVisible = false;
    }

    UAnimSequence* Desired = nullptr;
    if (GetCharacterMovement()->IsFalling()) Desired = JumpAnimation;
    else if (GetVelocity().SizeSquared2D() > 100.f) Desired = RunAnimation;
    else Desired = IdleAnimation;

    if (Desired != CurrentPartyAnimation)
        PlayPartyAnimation(Desired, true, Desired == RunAnimation ? 1.15f : 1.f);
}

void ATreasureSketchCharacter::SetSuitColor(FLinearColor Color)
{
    if (!GetMesh()->GetSkeletalMeshAsset()) return;
    const int32 SuitIndex = GetMesh()->GetMaterialIndex(TEXT("MI_PartyExplorer_SuitColor"));
    if (SuitIndex == INDEX_NONE) return;
    UMaterialInterface* ColorMaterial = LoadObject<UMaterialInterface>(
        nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (!ColorMaterial) return;
    SuitMaterial = UMaterialInstanceDynamic::Create(ColorMaterial, this);
    SuitMaterial->SetVectorParameterValue(TEXT("Color"), Color);
    GetMesh()->SetMaterial(SuitIndex, SuitMaterial);
}

void ATreasureSketchCharacter::PlayPartyAnimation(UAnimSequence* Animation, bool bLooping, float PlayRate)
{
    if (!Animation || !GetMesh()->GetSkeletalMeshAsset()) return;
    GetMesh()->PlayAnimation(Animation, bLooping);
    GetMesh()->SetPlayRate(PlayRate);
    CurrentPartyAnimation = Animation;
}

void ATreasureSketchCharacter::PlayDigAnimation()
{
    BackBookMesh->SetVisibility(true);
    BackStaffMesh->SetVisibility(false);
    OpenBookMesh->SetVisibility(false);
    HandStaffMesh->SetVisibility(true);
    bActionGearVisible = true;
    PlayPartyAnimation(DigAnimation, false, 1.25f);
    if (GetWorld()) PartyActionUntil = GetWorld()->GetTimeSeconds() + 1.2f;
    if (IsPropDisguised()) HideNormalDisguiseParts();
}

void ATreasureSketchCharacter::PlayReadBookAnimation()
{
    if (HasAuthority())
    {
        MulticastPlayReadBookAnimation();
        return;
    }

    // Play immediately for the owning player, then ask the server to mirror it
    // to spectators and the other clients.
    ApplyReadBookAnimation();
    ServerPlayReadBookAnimation();
}

void ATreasureSketchCharacter::ServerPlayReadBookAnimation_Implementation()
{
    MulticastPlayReadBookAnimation();
}

void ATreasureSketchCharacter::MulticastPlayReadBookAnimation_Implementation()
{
    // The owner already played it before the RPC round trip.
    if (!IsLocallyControlled() || HasAuthority())
        ApplyReadBookAnimation();
}

void ATreasureSketchCharacter::ApplyReadBookAnimation()
{
    BackBookMesh->SetVisibility(false);
    BackStaffMesh->SetVisibility(true);
    OpenBookMesh->SetVisibility(true);
    HandStaffMesh->SetVisibility(false);
    bActionGearVisible = true;
    bReadingBook = true;
    PlayPartyAnimation(ReadBookAnimation, true, 1.f);
    PartyActionUntil = TNumericLimits<float>::Max();
    if (IsPropDisguised()) HideNormalDisguiseParts();
}

void ATreasureSketchCharacter::StopReadBookAnimation()
{
    if (HasAuthority())
    {
        MulticastStopReadBookAnimation();
        return;
    }

    SetDefaultGearVisibility();
    bReadingBook = false;
    bActionGearVisible = false;
    PartyActionUntil = 0.f;
    PlayPartyAnimation(IdleAnimation, true);
    ServerStopReadBookAnimation();
}

void ATreasureSketchCharacter::ServerStopReadBookAnimation_Implementation()
{
    MulticastStopReadBookAnimation();
}

void ATreasureSketchCharacter::MulticastStopReadBookAnimation_Implementation()
{
    if (IsLocallyControlled() && !HasAuthority()) return;
    SetDefaultGearVisibility();
    bReadingBook = false;
    bActionGearVisible = false;
    PartyActionUntil = 0.f;
    PlayPartyAnimation(IdleAnimation, true);
}

void ATreasureSketchCharacter::SetDefaultGearVisibility()
{
    if (IsPropDisguised()) { HideNormalDisguiseParts(); return; }
    BackBookMesh->SetVisibility(true);
    BackStaffMesh->SetVisibility(true);
    OpenBookMesh->SetVisibility(false);
    HandStaffMesh->SetVisibility(false);
}

void ATreasureSketchCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ATreasureSketchCharacter, bSpectatorHidden);
    DOREPLIFETIME(ATreasureSketchCharacter, bShoveWindingUp);
    DOREPLIFETIME(ATreasureSketchCharacter, bDiggingPose);
    DOREPLIFETIME(ATreasureSketchCharacter, bWaterSlowed);
    DOREPLIFETIME(ATreasureSketchCharacter, Disguise);
}

float ATreasureSketchCharacter::GetDisguiseViewDistance() const
{
    return CameraBoom->TargetArmLength + CameraBoom->TargetOffset.Size();
}

void ATreasureSketchCharacter::HideNormalDisguiseParts()
{
    GetMesh()->SetVisibility(false);
    for (UStaticMeshComponent* Part : {BodyMesh.Get(), HeadMesh.Get(), LeftArmMesh.Get(), RightArmMesh.Get(),
        LeftLegMesh.Get(), RightLegMesh.Get(), FaceMesh.Get(), BackBookMesh.Get(), BackStaffMesh.Get(),
        OpenBookMesh.Get(), HandStaffMesh.Get()}) Part->SetVisibility(false);
}

void ATreasureSketchCharacter::SetPropDisguise(const FPropDisguise& Form)
{
    if (!HasAuthority()) return;
    Disguise = Form;
    if (Form.Mesh) Disguise.Rotation = (GetActorQuat().Inverse() * Form.Rotation.Quaternion()).Rotator();
    OnRep_PropDisguise();
    ForceNetUpdate();
}

void ATreasureSketchCharacter::OnRep_PropDisguise()
{
    DisguiseMesh->SetStaticMesh(Disguise.Mesh);
    DisguiseMesh->SetVisibility(IsPropDisguised());
    if (!IsPropDisguised())
    {
        const bool bSkeletal = GetMesh()->GetSkeletalMeshAsset() != nullptr;
        GetMesh()->SetVisibility(bSkeletal);
        for (UStaticMeshComponent* Part : {BodyMesh.Get(), HeadMesh.Get(), LeftArmMesh.Get(), RightArmMesh.Get(),
            LeftLegMesh.Get(), RightLegMesh.Get(), FaceMesh.Get()}) Part->SetVisibility(!bSkeletal);
        SetDefaultGearVisibility();
        CameraBoom->TargetArmLength = 500.f;
        CameraBoom->TargetOffset = FVector::ZeroVector;
        return;
    }
    const FTransform Shape(Disguise.Rotation, FVector::ZeroVector, Disguise.Scale);
    const FBox Bounds = Disguise.Mesh->GetBoundingBox().TransformBy(Shape);
    DisguiseMesh->SetRelativeScale3D(Disguise.Scale);
    DisguiseMesh->SetRelativeRotation(Disguise.Rotation);
    DisguiseMesh->SetRelativeLocation(FVector(-Bounds.GetCenter().X, -Bounds.GetCenter().Y,
        -GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - Bounds.Min.Z));
    for (int32 Index = 0; Index < Disguise.Materials.Num(); ++Index)
    {
        DisguiseMesh->SetMaterial(Index, Disguise.Materials[Index]);
        if (Disguise.Colors.IsValidIndex(Index))
            if (auto* Material = DisguiseMesh->CreateAndSetMaterialInstanceDynamic(Index))
                Material->SetVectorParameterValue(TEXT("Color"), Disguise.Colors[Index]);
    }
    CameraBoom->TargetArmLength = FMath::Max(500.f, Bounds.GetExtent().Size() * 2.f);
    CameraBoom->TargetOffset = FVector(0.f, 0.f, Bounds.GetExtent().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    HideNormalDisguiseParts();
}

void ATreasureSketchCharacter::SetShoveWindingUp(bool bWindingUp)
{
    if (!HasAuthority() || bShoveWindingUp == bWindingUp) return;
    bShoveWindingUp = bWindingUp;
    OnRep_ShoveWindingUp();
    ForceNetUpdate();
}

void ATreasureSketchCharacter::SetDiggingPose(bool bDigging)
{
    if (!HasAuthority() || bDiggingPose == bDigging) return;
    bDiggingPose = bDigging;
    OnRep_DiggingPose();
    ForceNetUpdate();
}

void ATreasureSketchCharacter::OnRep_DiggingPose()
{
    LeftArmMesh->SetRelativeRotation(bDiggingPose ? FRotator(-42.f, 0.f, 0.f) : FRotator::ZeroRotator);
    OnRep_ShoveWindingUp();
}

void ATreasureSketchCharacter::SetWaterSlowed(bool bSlowed)
{
    if (!HasAuthority() || bWaterSlowed == bSlowed) return;
    bWaterSlowed = bSlowed;
    OnRep_WaterSlowed();
    ForceNetUpdate();
}

void ATreasureSketchCharacter::OnRep_WaterSlowed()
{
    const ATreasureSketchGameState* GS = GetWorld() ? GetWorld()->GetGameState<ATreasureSketchGameState>() : nullptr;
    SetMovementSpeedMultiplier(GS ? GS->MovementSpeedMultiplier : 1.f);
}

void ATreasureSketchCharacter::OnRep_ShoveWindingUp()
{
    // The raised, orange arm is visible to nearby players during the windup.
    RightArmMesh->SetRelativeLocation(bShoveWindingUp ? FVector(30.f, 28.f, 3.f) : FVector(0.f, 28.f, -14.f));
    RightArmMesh->SetRelativeRotation(bShoveWindingUp ? FRotator(-65.f, 0.f, 0.f)
        : bDiggingPose ? FRotator(-42.f, 0.f, 0.f) : FRotator::ZeroRotator);
    if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(RightArmMesh->GetMaterial(0)))
        Material->SetVectorParameterValue(TEXT("Color"), bShoveWindingUp
            ? FLinearColor(1.f, 0.33f, 0.04f) : FLinearColor(0.08f, 0.55f, 0.68f));
    if (bShoveWindingUp)
    {
        PlayPartyAnimation(ShoveAnimation, false, 1.7f);
        if (GetWorld()) PartyActionUntil = GetWorld()->GetTimeSeconds() + 0.65f;
    }
}

void ATreasureSketchCharacter::SetMovementSpeedMultiplier(float Multiplier)
{
    const float BaseScale = FMath::IsFinite(Multiplier) ? FMath::Clamp(Multiplier, 0.5f, 5.f) : 1.f;
    const float Scale = BaseScale * (bWaterSlowed ? 0.75f : 1.f);
    GetCharacterMovement()->MaxWalkSpeed = 520.f * Scale;
    GetCharacterMovement()->MaxAcceleration = 2048.f * Scale;
    GetCharacterMovement()->BrakingDecelerationWalking = 2048.f * Scale;
}

void ATreasureSketchCharacter::SetCanyonTestMode(bool bEnabled)
{
    bCanyonTestMode = bEnabled;
    const ATreasureSketchGameState* GS = GetWorld() ? GetWorld()->GetGameState<ATreasureSketchGameState>() : nullptr;
    const float Scale = bEnabled ? 5.f : GS ? GS->MovementSpeedMultiplier : 1.f;
    SetMovementSpeedMultiplier(Scale);
    GetCharacterMovement()->MaxFlySpeed = 520.f * Scale;
    GetCharacterMovement()->BrakingDecelerationFlying = 2048.f * Scale;
    if (!bEnabled || GetCharacterMovement()->MovementMode == MOVE_Flying)
        GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void ATreasureSketchCharacter::CheckJumpInput(float DeltaTime)
{
    if (bPressedJump)
    {
        const float FeetZ = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        GetCharacterMovement()->JumpZVelocity = FeetZ < 5.f ? LowWaterJumpVelocity : NormalJumpVelocity;
    }
    Super::CheckJumpInput(DeltaTime);
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
    PlayerInputComponent->BindAxis("CanyonFlyVertical", this, &ATreasureSketchCharacter::FlyVertical);
    PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
    PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
    PlayerInputComponent->BindAction("CanyonToggleFlight", IE_Pressed, this,
        &ATreasureSketchCharacter::ToggleCanyonFlight);
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

void ATreasureSketchCharacter::FlyVertical(float Value)
{
    if (bCanyonTestMode && GetCharacterMovement()->MovementMode == MOVE_Flying
        && !FMath::IsNearlyZero(Value))
        AddMovementInput(FVector::UpVector, Value);
}

void ATreasureSketchCharacter::ToggleCanyonFlight()
{
    if (!bCanyonTestMode) return;
    GetCharacterMovement()->SetMovementMode(
        GetCharacterMovement()->MovementMode == MOVE_Flying ? MOVE_Falling : MOVE_Flying);
}
