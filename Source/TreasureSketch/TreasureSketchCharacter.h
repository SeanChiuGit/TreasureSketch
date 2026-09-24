#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TreasureSketchCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;

UCLASS()
class TREASURESKETCH_API ATreasureSketchCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ATreasureSketchCharacter();

protected:
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UCameraComponent> FollowCamera;

    void MoveForward(float Value);
    void MoveRight(float Value);
    void Turn(float Value);
    void LookUp(float Value);
};
