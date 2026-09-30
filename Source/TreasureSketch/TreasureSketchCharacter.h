#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TreasureSketchCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;

UCLASS()
class TREASURESKETCH_API ATreasureSketchCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ATreasureSketchCharacter();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    void SetSpectatorHidden(bool bShouldHide);
    void SetMovementSpeedMultiplier(float Multiplier);
    void SetShoveWindingUp(bool bWindingUp);
    void SetDiggingPose(bool bDigging);
    void SetWaterSlowed(bool bSlowed);
    bool IsWaterSlowed() const { return bWaterSlowed; }
    bool IsShoveWindingUp() const { return bShoveWindingUp; }

protected:
    virtual void BeginPlay() override;
    virtual void CheckJumpInput(float DeltaTime) override;

    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
    friend class ATreasureSketchGameMode;
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

    UPROPERTY(ReplicatedUsing=OnRep_SpectatorHidden)
    bool bSpectatorHidden = false;

    UFUNCTION()
    void OnRep_SpectatorHidden();

    UPROPERTY(ReplicatedUsing=OnRep_ShoveWindingUp)
    bool bShoveWindingUp = false;

    UFUNCTION()
    void OnRep_ShoveWindingUp();

    UPROPERTY(ReplicatedUsing=OnRep_DiggingPose)
    bool bDiggingPose = false;

    UFUNCTION()
    void OnRep_DiggingPose();

    UPROPERTY(ReplicatedUsing=OnRep_WaterSlowed)
    bool bWaterSlowed = false;

    UFUNCTION()
    void OnRep_WaterSlowed();

    bool bWasInShallowWater = false;
    float WaterSlowUntilServerTime = 0.f;

    void MoveForward(float Value);
    void MoveRight(float Value);
    void Turn(float Value);
    void LookUp(float Value);
};
