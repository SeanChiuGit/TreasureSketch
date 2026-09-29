#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TreasureSketchCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;
class UAnimSequence;
class UMaterialInstanceDynamic;

UCLASS()
class TREASURESKETCH_API ATreasureSketchCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ATreasureSketchCharacter();
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    void SetSpectatorHidden(bool bShouldHide);
    void SetMovementSpeedMultiplier(float Multiplier);
    void SetCanyonTestMode(bool bEnabled);
    bool IsCanyonTestMode() const { return bCanyonTestMode; }
    void SetShoveWindingUp(bool bWindingUp);
    bool IsShoveWindingUp() const { return bShoveWindingUp; }

    UFUNCTION(BlueprintCallable, Category="Party Explorer")
    void SetSuitColor(FLinearColor Color);

    UFUNCTION(BlueprintCallable, Category="Party Explorer")
    void PlayDigAnimation();

    UFUNCTION(BlueprintCallable, Category="Party Explorer")
    void PlayReadBookAnimation();

    UFUNCTION(BlueprintCallable, Category="Party Explorer")
    void StopReadBookAnimation();

protected:
    virtual void BeginPlay() override;
    virtual void CheckJumpInput(float DeltaTime) override;

    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
    friend class FExplorerRaceFlowTest;
    static constexpr float NormalJumpVelocity = 620.f;
    static constexpr float LowWaterJumpVelocity = 900.f;
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UCameraComponent> FollowCamera;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> BodyMesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> HeadMesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> LeftArmMesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> RightArmMesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> LeftLegMesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> RightLegMesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> FaceMesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> BackBookMesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> BackStaffMesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> OpenBookMesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> HandStaffMesh;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> SuitMaterial;

    UPROPERTY()
    TObjectPtr<UAnimSequence> IdleAnimation;

    UPROPERTY()
    TObjectPtr<UAnimSequence> RunAnimation;

    UPROPERTY()
    TObjectPtr<UAnimSequence> JumpAnimation;

    UPROPERTY()
    TObjectPtr<UAnimSequence> DigAnimation;

    UPROPERTY()
    TObjectPtr<UAnimSequence> ReadBookAnimation;

    UPROPERTY()
    TObjectPtr<UAnimSequence> ShoveAnimation;

    UPROPERTY(Transient)
    TObjectPtr<UAnimSequence> CurrentPartyAnimation;

    float PartyActionUntil = 0.f;
    bool bActionGearVisible = false;
    bool bReadingBook = false;

    UPROPERTY(ReplicatedUsing=OnRep_SpectatorHidden)
    bool bSpectatorHidden = false;

    UFUNCTION()
    void OnRep_SpectatorHidden();

    UPROPERTY(ReplicatedUsing=OnRep_ShoveWindingUp)
    bool bShoveWindingUp = false;
    bool bCanyonTestMode = false;

    UFUNCTION()
    void OnRep_ShoveWindingUp();

    void MoveForward(float Value);
    void MoveRight(float Value);
    void Turn(float Value);
    void LookUp(float Value);
    void FlyVertical(float Value);
    void ToggleCanyonFlight();
    void PlayPartyAnimation(UAnimSequence* Animation, bool bLooping, float PlayRate = 1.f);
    void ApplyReadBookAnimation();

    UFUNCTION(Server, Reliable)
    void ServerPlayReadBookAnimation();

    UFUNCTION(NetMulticast, Reliable)
    void MulticastPlayReadBookAnimation();

    UFUNCTION(Server, Reliable)
    void ServerStopReadBookAnimation();

    UFUNCTION(NetMulticast, Reliable)
    void MulticastStopReadBookAnimation();

    void SetDefaultGearVisibility();
};
