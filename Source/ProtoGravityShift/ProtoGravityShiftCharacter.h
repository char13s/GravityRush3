// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "GravityMarkerWidget.h"
#include <Components/TimelineComponent.h>
#include "ProtoGravityShiftCharacter.generated.h"

UENUM(BlueprintType)
enum EShiftState 
{
	E_NoShift			UMETA(DisplayName = "NoShift"),
	E_Levitating		UMETA(DisplayName = "Levitating"),
	E_Accelerating		UMETA(DisplayName = "Accelerating"),
	E_WallGrounded		UMETA(DisplayName = "WallGrounded"),
};


UCLASS(config=Game)
class AProtoGravityShiftCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category = GravityShift)
	TEnumAsByte<EShiftState> ShiftState = EShiftState::E_NoShift;
	
	UPROPERTY(BlueprintReadWrite, Category = GravityShift)
	FString ShiftStateString;

private:
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
	float WallRaycastLength = 200;
	/******************************************************************************************/
	UPROPERTY(BlueprintReadWrite, Category = GravityShift, meta = (AllowPrivateAccess = "true"))
	FRotator MeshWallRotator;
	UPROPERTY(BlueprintReadWrite, Category = GravityShift, meta = (AllowPrivateAccess = "true"))
	FVector WallNormal;
	UPROPERTY(BlueprintReadWrite, Category = GravityShift, meta = (AllowPrivateAccess = "true"))
	FVector WallRight;
	UPROPERTY(BlueprintReadWrite, Category = GravityShift, meta = (AllowPrivateAccess = "true"))
	FVector WallForward;
	UPROPERTY(BlueprintReadWrite, Category = GravityShift, meta = (AllowPrivateAccess = "true"))
	FVector MeshStartingPosOffset;
	UPROPERTY(BlueprintReadWrite, Category = GravityShift, meta = (AllowPrivateAccess = "true"))
	FRotator MeshStartingRotOffset;

	UPROPERTY(BlueprintReadWrite, Category = GravityShift, meta = (AllowPrivateAccess = "true"))
	float DefaultGravityScale;
	UPROPERTY(BlueprintReadWrite, Category = GravityShift, meta = (AllowPrivateAccess = "true"))
	float DefaultAirControl;

	UPROPERTY(BlueprintReadWrite, Category = GravityShift, meta = (AllowPrivateAccess = "true"))
	FVector GravityDirection;
	/******************************************************************************************/

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = GravityShift, meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGravityMarkerWidget> MarkerWidgetClass;

	UUserWidget* MarkerWidget;

	UPROPERTY(EditAnywhere, Category = GravityShift)
	UCurveFloat* CameraOffsetTimelineFloatCurve;
	UPROPERTY(EditAnywhere, Category = GravityShift)
	FVector CameraOffsetDefault;
	UPROPERTY(EditAnywhere, Category = GravityShift)
	FVector CameraOffsetLevitating;

	UPROPERTY(EditAnywhere, Category = GravityShift)
	float ShiftAcceleration = 20;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = GravityShift, meta = (AllowPrivateAccess = "true"))
	float ShiftStartSpeed = 980;
	UPROPERTY(EditAnywhere, Category = GravityShift)
	float MaxShiftSpeed = 10000;


protected:

	//TimelineComponent to animate Door meshes
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	UTimelineComponent* CameraOffsetTimeline;

	FOnTimelineFloat UpdateFunctionSignature;

private:
	float CurrentShiftAcceleration;

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

	// Called every frame
	virtual void Tick(float deltaTime) override;

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

public:
	
	UFUNCTION(BlueprintCallable, Category = GravityShift)
	void SetShiftState(EShiftState newState);

	UFUNCTION(BlueprintPure, Category = GravityShift)
	FString GetShiftStateString(EShiftState newState);

private:

	UFUNCTION(BlueprintCallable, Category = GravityShift)
	void GoBackToGround();

	UFUNCTION(BlueprintCallable, Category = GravityShift)
	void EnterLevitating();

	UFUNCTION(BlueprintCallable, Category = GravityShift)
	void EnterAcceleration();

	FVector CalculateGravityDirection();


	UFUNCTION(BlueprintCallable, Category = GravityShift)
	void ShiftAccelerating(float deltaTime);

	UFUNCTION(BlueprintCallable, Category = GravityShift)
	void ApplyWallGravity(float deltaTime);


	UFUNCTION(BlueprintCallable, Category = GravityShift)
	void AdjustToWall(FHitResult hitInfo);

	UFUNCTION(BlueprintCallable, Category = GravityShift)
	void MoveOnWall(FVector2D inputVector, FVector forward, FVector right, FVector normal, FRotator wallRotator);

	void OrientMeshToWall(FVector2D inputVector, FVector forward, FVector right, FVector normal, FRotator wallRotator);

	UFUNCTION()
	void UpdateCameraOffsetTimeline(float output);
};

