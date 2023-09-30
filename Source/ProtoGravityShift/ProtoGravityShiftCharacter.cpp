// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProtoGravityShiftCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Blueprint/UserWidget.h"

//////////////////////////////////////////////////////////////////////////
// AProtoGravityShiftCharacter

AProtoGravityShiftCharacter::AProtoGravityShiftCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;

	CameraOffsetTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("CameraOffsetTimeline"));

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AProtoGravityShiftCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	//Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
	UpdateFunctionSignature.BindDynamic(this, &AProtoGravityShiftCharacter::UpdateCameraOffsetTimeline);
	if (CameraOffsetTimelineFloatCurve != nullptr)
	{
		CameraOffsetTimeline->AddInterpFloat(CameraOffsetTimelineFloatCurve, UpdateFunctionSignature);
	}

	MeshStartingPosOffset = GetMesh()->GetRelativeLocation();
	MeshStartingRotOffset = GetMesh()->GetRelativeRotation();

	DefaultAirControl = GetCharacterMovement()->AirControl;
	DefaultGravityScale = GetCharacterMovement()->GravityScale;

	MarkerWidget = CreateWidget<UGravityMarkerWidget>(this->GetGameInstance(), MarkerWidgetClass);
	MarkerWidget->AddToViewport();
	MarkerWidget->SetVisibility(ESlateVisibility::Hidden);

}

void AProtoGravityShiftCharacter::Tick(float deltaTime)
{
	Super::Tick(deltaTime);
	//UE_LOG(LogTemp, Log, TEXT("ShiftState: %s"), *UEnum::GetDisplayValueAsText(ShiftState).ToString());
}

// Input

void AProtoGravityShiftCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent)) {

		//Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		//Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AProtoGravityShiftCharacter::Move);

		//Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AProtoGravityShiftCharacter::Look);

	}

}

void AProtoGravityShiftCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AProtoGravityShiftCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

// Gravity Controls

void AProtoGravityShiftCharacter::GoBackToGround()
{
	GetCharacterMovement()->GravityScale = DefaultGravityScale;
	GetCharacterMovement()->AirControl = DefaultAirControl;
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Falling);
	GetCharacterMovement()->bOrientRotationToMovement = true;
	CameraOffsetTimeline->Reverse();
	MarkerWidget->SetVisibility(ESlateVisibility::Hidden);

	ResetMeshRotation();

	ShiftState = EShiftState::E_NoShift;
}

void AProtoGravityShiftCharacter::ResetMeshRotation()
{
	FLatentActionInfo MeshLatentInfo;
	MeshLatentInfo.CallbackTarget = this;
	UKismetSystemLibrary::MoveComponentTo(GetMesh(), MeshStartingPosOffset, MeshStartingRotOffset
		, false, false, BackToGroundTransitionDuration, true, EMoveComponentAction::Move, MeshLatentInfo);
}

void AProtoGravityShiftCharacter::EnterLevitating()
{
	GetCharacterMovement()->Velocity = FVector::ZeroVector;
	GetCharacterMovement()->AirControl = 0;
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Falling);
	GetCharacterMovement()->GravityScale = 0;
	GetCharacterMovement()->bOrientRotationToMovement = false;

	CameraOffsetTimeline->Play();
	MarkerWidget->SetVisibility(ESlateVisibility::Visible);


	ResetMeshRotation();
}

void AProtoGravityShiftCharacter::EnterAcceleration()
{
	MarkerWidget->SetVisibility(ESlateVisibility::Hidden);
	CameraOffsetTimeline->Reverse();
	GetCharacterMovement()->AirControl = DefaultAirControl;
	GetCharacterMovement()->GravityScale = 0;
	GravityDirection = CalculateGravityDirection();
	CurrentShiftAcceleration = ShiftStartSpeed;
}

FVector AProtoGravityShiftCharacter::CalculateGravityDirection()
{
	FVector endPoint = FollowCamera->GetComponentLocation() + (FollowCamera->GetForwardVector() * 9000);
	FVector startPoint = FollowCamera->GetComponentLocation();

	// Ignore any specific actors
	TArray<AActor*> ignoreActors;
	// Ignore self or remove this line to not ignore any
	ignoreActors.Init(this, 1);

	FHitResult hitResult;
	bool didHit = UKismetSystemLibrary::LineTraceSingle(GetWorld(), startPoint, endPoint, ETraceTypeQuery::TraceTypeQuery1, false, ignoreActors, EDrawDebugTrace::None, hitResult, true);
	if (didHit)
	{
		endPoint = hitResult.Location;
	}

	FVector result = endPoint - GetActorLocation();
	return result.GetSafeNormal();
}

// Tick functions

void AProtoGravityShiftCharacter::ShiftAccelerating(float deltaTime)
{
	CurrentShiftAcceleration += deltaTime * ShiftAcceleration;
	CurrentShiftAcceleration = UKismetMathLibrary::Min(CurrentShiftAcceleration, MaxShiftSpeed);
	GetCharacterMovement()->Velocity = GravityDirection.GetSafeNormal() * CurrentShiftAcceleration;

	ShiftState = EShiftState::E_Accelerating;
}

void AProtoGravityShiftCharacter::ApplyWallGravity(float deltaTime)
{
	// Ignore any specific actors
	TArray<AActor*> ignoreActors;
	// Ignore self or remove this line to not ignore any
	ignoreActors.Init(this, 1);

	float capsuleHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	FVector startTopPoint = GetActorLocation() + (FVector::UpVector * capsuleHeight);
	FVector endTopPoint = startTopPoint + (GravityDirection * WallRaycastLength);

	FHitResult hitResult;
	bool didTopHit = UKismetSystemLibrary::LineTraceSingle(GetWorld(), startTopPoint, endTopPoint, ETraceTypeQuery::TraceTypeQuery1
																	, false, ignoreActors, EDrawDebugTrace::None, hitResult, true,
																	FLinearColor::Red, FLinearColor::Green,5.0f);

	FVector startBottomPoint = GetActorLocation() - (FVector::UpVector * capsuleHeight);
	FVector endBottomPoint = startBottomPoint + (GravityDirection * WallRaycastLength);
	bool didBottomHit = UKismetSystemLibrary::LineTraceSingle(GetWorld(), startBottomPoint, endBottomPoint, ETraceTypeQuery::TraceTypeQuery1
																	, false, ignoreActors, EDrawDebugTrace::None, hitResult, true,
																	FLinearColor::Red, FLinearColor::Green, 5.0f);

	if (didTopHit || didBottomHit)
	{
		CurrentShiftAcceleration = ShiftStartSpeed;
	}
	else {
		ShiftAccelerating(deltaTime);
	}
}

void AProtoGravityShiftCharacter::AdjustToWall(FHitResult hitInfo)
{
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Flying);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->bOrientRotationToMovement = false;

	FVector lookDir = FVector(hitInfo.Normal.X, hitInfo.Normal.Y, 0).GetSafeNormal();
	lookDir *= -500;

	FVector capsulePos = GetCapsuleComponent()->GetComponentLocation();
	FVector endPoint = capsulePos + lookDir;
	FRotator lookRotator = UKismetMathLibrary::FindLookAtRotation(capsulePos, endPoint);

	FLatentActionInfo CapsuleLatentInfo;
	CapsuleLatentInfo.CallbackTarget = this;

	/*************************************************************************************/

	float capsuleHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	float capsuleRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();

	FVector location = hitInfo.ImpactPoint;
	location += (hitInfo.ImpactNormal * capsuleRadius);

	if (hitInfo.ImpactNormal.Z < 0)
	{
		location -= (FVector::UpVector * capsuleHeight);
	}

	if (WallCapsuleTransitionDuration > 0)
	{
		UKismetSystemLibrary::MoveComponentTo(GetCapsuleComponent(), location, lookRotator,
			false, false, WallCapsuleTransitionDuration, true, EMoveComponentAction::Type::Move, CapsuleLatentInfo);
	}
	else {
		GetCapsuleComponent()->SetWorldLocation(location);
		GetCapsuleComponent()->SetWorldRotation(lookRotator);
	}
	/*************************************************************************************/

	FVector capsuleRight = UKismetMathLibrary::GetRightVector(lookRotator);


	FTransform transform = UKismetMathLibrary::MakeTransform(hitInfo.ImpactPoint, lookRotator);
	MeshWallRotator = UKismetMathLibrary::MakeRotFromZX(hitInfo.Normal, capsuleRight * -1);

	FLatentActionInfo MeshLatentInfo;
	MeshLatentInfo.CallbackTarget = this;

	/*************************************************************************************/
	FVector meshPosOffset = UKismetMathLibrary::InverseTransformLocation(GetRootComponent()->GetRelativeTransform(), hitInfo.ImpactPoint);
	FRotator meshRot = UKismetMathLibrary::InverseTransformRotation(transform, MeshWallRotator);
	if (WallMeshTransitionDuration > 0)
	{
		UKismetSystemLibrary::MoveComponentTo(GetMesh(), meshPosOffset, meshRot
			, false, false, WallMeshTransitionDuration, true, EMoveComponentAction::Type::Move, MeshLatentInfo);
	}
	else {
		GetMesh()->SetRelativeLocation(meshPosOffset);
		GetMesh()->SetRelativeRotation(meshRot);
	}
	/*************************************************************************************/

	WallNormal = hitInfo.Normal;
	WallRight = capsuleRight;
	WallForward = UKismetMathLibrary::GetRightVector(MeshWallRotator);

	GravityDirection = -hitInfo.Normal;

	ShiftState = EShiftState::E_WallGrounded;
}

// Move on wall functions

void AProtoGravityShiftCharacter::MoveOnWall(FVector2D inputVector, FVector forward, FVector right, FVector normal, FRotator wallRotator)
{
	ConsumeMovementInputVector();
	AddMovementInput(right, inputVector.X);
	AddMovementInput(forward, inputVector.Y);
	OrientMeshToWall(inputVector, forward, right, normal, MeshWallRotator);
}

void AProtoGravityShiftCharacter::OrientMeshToWall(FVector2D inputVector, FVector forward, FVector right, FVector normal, FRotator wallRotator)
{
	FVector inputDirection = (right * inputVector.X) + (forward * inputVector.Y);
	inputDirection.Normalize();

	double rad = FMath::Acos(forward.GetSafeNormal().Dot(inputDirection.GetSafeNormal()));
	double angle = FMath::RadiansToDegrees(rad);

	if (FMath::Abs(inputVector.X) > 0)
	{
		angle *= FMath::Sign(inputVector.X);
	}

	FVector forwardVector = UKismetMathLibrary::GetForwardVector(wallRotator);
	FVector adjustedWallRotation = UKismetMathLibrary::RotateAngleAxis(forwardVector, angle, normal);

	FRotator finalRotation = UKismetMathLibrary::MakeRotFromZX(normal, adjustedWallRotation);

	FLatentActionInfo MeshLatentInfo;
	MeshLatentInfo.CallbackTarget = this;

	FRotator relativeRotation = UKismetMathLibrary::InverseTransformRotation(GetRootComponent()->GetRelativeTransform(), finalRotation);
	UKismetSystemLibrary::MoveComponentTo(GetMesh(), GetMesh()->GetRelativeLocation(), relativeRotation,
		false, false, 0.1f, true, EMoveComponentAction::Type::Move, MeshLatentInfo);
	/*************************************************************************************/

}

void AProtoGravityShiftCharacter::UpdateCameraOffsetTimeline(float output)
{
	if (CameraBoom != nullptr)
	{
		CameraBoom->SocketOffset = UKismetMathLibrary::VLerp(CameraOffsetDefault, CameraOffsetLevitating, output);
	}
}
