// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "GravityMarkerWidget.h"
#include <Components/TimelineComponent.h>
#include "ProtoGravityShiftCharacter.generated.h"


UCLASS(config=Game)
class AProtoGravityShiftCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* FollowCamera;
	
	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputMappingContext* DefaultMappingContext;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = GravityShift, meta = (AllowPrivateAccess = "true"))
	float WallCapsuleTransitionDuration = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = GravityShift, meta = (AllowPrivateAccess = "true"))
	float WallMeshTransitionDuration = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = GravityShift, meta = (AllowPrivateAccess = "true"))
	float BackToGroundTransitionDuration = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = GravityShift, meta = (AllowPrivateAccess = "true"))
	float GravityForce = 980;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = GravityShift, meta = (AllowPrivateAccess = "true"))
	float WallRaycastLength = 200;
	/******************************************************************************************/
	UPROPERTY(BlueprintReadWrite, Category = Character, meta = (AllowPrivateAccess = "true"))
	FRotator MeshWallRotator;
	UPROPERTY(BlueprintReadWrite, Category = Character, meta = (AllowPrivateAccess = "true"))
	FVector WallNormal;
	UPROPERTY(BlueprintReadWrite, Category = Character, meta = (AllowPrivateAccess = "true"))
	FVector WallRight;
	UPROPERTY(BlueprintReadWrite, Category = Character, meta = (AllowPrivateAccess = "true"))
	FVector WallForward;
	UPROPERTY(BlueprintReadWrite, Category = Character, meta = (AllowPrivateAccess = "true"))
	FVector MeshStartingPosOffset;
	UPROPERTY(BlueprintReadWrite, Category = Character, meta = (AllowPrivateAccess = "true"))
	FRotator MeshStartingRotOffset;

	UPROPERTY(BlueprintReadWrite, Category = Character, meta = (AllowPrivateAccess = "true"))
	float DefaultGravityScale;
	UPROPERTY(BlueprintReadWrite, Category = Character, meta = (AllowPrivateAccess = "true"))
	float DefaultAirControl;

	UPROPERTY(BlueprintReadWrite, Category = Character, meta = (AllowPrivateAccess = "true"))
	FVector GravityDirection;
	/******************************************************************************************/

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = GravityShift, meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGravityMarkerWidget> MarkerWidgetClass;

	UUserWidget* MarkerWidget;

	UPROPERTY(EditAnywhere, Category = GravityShift)
	UCurveFloat* CameraOffsetTimelineFloatCurve;
	UPROPERTY(EditAnywhere, Category = GravityShift)
	float CameraOffset = 50;
protected:

	//TimelineComponent to animate Door meshes
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	UTimelineComponent* CameraOffsetTimeline;

	FOnTimelineFloat UpdateFunctionSignature;

public:
	AProtoGravityShiftCharacter();

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }


protected:

	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// To add mapping context
	virtual void BeginPlay();

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

private:

	UFUNCTION(BlueprintCallable, Category = Character)
	void GoBackToGround();

	UFUNCTION(BlueprintCallable, Category = Character)
	void EnterLevitating();

	UFUNCTION(BlueprintCallable, Category = Character)
	void EnterAcceleration();

	FVector CalculateGravityDirection();


	UFUNCTION(BlueprintCallable, Category = Character)
	void ShiftAccelerating(FVector direction, float force);

	UFUNCTION(BlueprintCallable, Category = Character)
	void ApplyWallGravity();


	UFUNCTION(BlueprintCallable, Category = Character)
	void AdjustToWall(FHitResult hitInfo);

	UFUNCTION(BlueprintCallable, Category = Character)
	void MoveOnWall(FVector2D inputVector, FVector forward, FVector right, FVector normal, FRotator wallRotator);

	void OrientMeshToWall(FVector2D inputVector, FVector forward, FVector right, FVector normal, FRotator wallRotator);

	UFUNCTION()
	void UpdateCameraOffsetTimeline(float output);
};

