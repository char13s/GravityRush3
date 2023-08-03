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

	MeshStartingPosOffset = GetMesh()->GetRelativeLocation();
	MeshStartingRotOffset = GetMesh()->GetRelativeRotation();
}

//////////////////////////////////////////////////////////////////////////
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


void AProtoGravityShiftCharacter::ShiftAccelerating(FVector gravityDirection, float gravityForce)
{
	GetCharacterMovement()->Velocity = gravityDirection.GetSafeNormal() * gravityForce;
}

void AProtoGravityShiftCharacter::AdjustToWall(FHitResult hitInfo)
{
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Flying);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->bOrientRotationToMovement = false;


	FVector lookDir = FVector(hitInfo.Normal.X, hitInfo.Normal.Y, 0).GetSafeNormal();
	lookDir *= -1;
	FVector capsulePos = GetCapsuleComponent()->GetComponentLocation();
	lookDir += capsulePos;
	FRotator lookRotator = UKismetMathLibrary::FindLookAtRotation(capsulePos, lookDir);

	FLatentActionInfo CapsuleLatentInfo;
	CapsuleLatentInfo.CallbackTarget = this;

	UKismetSystemLibrary::MoveComponentTo(GetCapsuleComponent(), hitInfo.ImpactPoint,lookRotator,
		false, false, 0.2f, true,EMoveComponentAction::Type::Move, CapsuleLatentInfo);

	FVector capsuleRight = UKismetMathLibrary::GetRightVector(lookRotator);
	//FVector capsuleForward = UKismetMathLibrary::GetForwardVector(lookRotator);
	FTransform transform = UKismetMathLibrary::MakeTransform(hitInfo.ImpactPoint, lookRotator);
	MeshWallRotator = UKismetMathLibrary::MakeRotFromZX(hitInfo.Normal, capsuleRight * -1);

	FLatentActionInfo MeshLatentInfo;
	MeshLatentInfo.CallbackTarget = this;
	MeshLatentInfo.ExecutionFunction = FName(TEXT("OnMeshMoveEnded"));
	MeshLatentInfo.Linkage = 0;
	//FRotator meshRelativeRotator = UKismetMathLibrary::InverseTransformRotation(transform, MeshWallRotator);

	UE_LOG(LogTemp, Log, TEXT("MeshStartingPosOffset:%s "), *MeshStartingPosOffset.ToString());







	UKismetSystemLibrary::MoveComponentTo(GetMesh(), hitInfo.ImpactPoint, MeshWallRotator
		, false, false, 0.2f, true,EMoveComponentAction::Type::Move, MeshLatentInfo);

	WallNormal = hitInfo.Normal;
	WallRight = capsuleRight;
	WallForward = GetMesh()->GetRightVector();

	UE_LOG(LogTemp, Log, TEXT("Normal:%s "), *WallNormal.ToString());
	UE_LOG(LogTemp, Log, TEXT("Right:%s "), *WallRight.ToString());
	UE_LOG(LogTemp, Log, TEXT("Forward:%s "), *WallForward.ToString());

}

void AProtoGravityShiftCharacter::OnMeshMoveEnded()
{
	WallForward = GetMesh()->GetRightVector();
	UE_LOG(LogTemp, Warning, TEXT("ON MESH MOVE ENDED"));
}

void AProtoGravityShiftCharacter::MoveOnWall(FVector2D inputVector, FVector forward, FVector right, FVector normal, FRotator wallRotator)
{
	ConsumeMovementInputVector();
	AddMovementInput(right, inputVector.X);
	AddMovementInput(forward, inputVector.Y);
	OrientMeshToWall(inputVector, forward, right, normal, wallRotator);
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
	GetMesh()->SetWorldRotation(finalRotation);
}



