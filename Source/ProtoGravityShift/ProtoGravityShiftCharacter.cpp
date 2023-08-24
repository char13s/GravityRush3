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

	DefaultAirControl = GetCharacterMovement()->AirControl;
	DefaultGravityScale = GetCharacterMovement()->GravityScale;
	
	MarkerWidget = CreateWidget<UGravityMarkerWidget>(this->GetGameInstance(), MarkerWidgetClass);
	MarkerWidget->AddToViewport();
	MarkerWidget->SetVisibility(ESlateVisibility::Hidden);
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

//////////////////////////////////////////////////////////////////////////
// Gravity Controls

void AProtoGravityShiftCharacter::GoBackToGround()
{
	GetCharacterMovement()->GravityScale = DefaultGravityScale;
	GetCharacterMovement()->AirControl = DefaultAirControl;
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Falling);
	GetCharacterMovement()->bOrientRotationToMovement = true;
	CameraBoom->SocketOffset = FVector::ZeroVector;

	MarkerWidget->SetVisibility(ESlateVisibility::Hidden);

	FLatentActionInfo MeshLatentInfo;
	MeshLatentInfo.CallbackTarget = this;
	UKismetSystemLibrary::MoveComponentTo(GetMesh(), MeshStartingPosOffset, MeshStartingRotOffset
		, false, false, backToGroundTransitionDuration, true, EMoveComponentAction::Move, MeshLatentInfo);
}

void AProtoGravityShiftCharacter::EnterLevitating()
{
	GetCharacterMovement()->Velocity = FVector::ZeroVector;
	GetCharacterMovement()->AirControl = 0;
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Falling);
	GetCharacterMovement()->GravityScale = 0;
	
	MarkerWidget->SetVisibility(ESlateVisibility::Visible);

	CameraBoom->SocketOffset = FVector(0,50,0);
}

void AProtoGravityShiftCharacter::Accelerate()
{
	MarkerWidget->SetVisibility(ESlateVisibility::Hidden);
	CameraBoom->SocketOffset = FVector::ZeroVector;
	GetCharacterMovement()->AirControl = DefaultAirControl;
	GetCharacterMovement()->GravityScale = 0;
	GravityDirection = CalculateGravityDirection();
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

//////////////////////////////////////////////////////////////////////////
// Tick functions

void AProtoGravityShiftCharacter::ShiftAccelerating(FVector direction, float gravityForce)
{
	GetCharacterMovement()->Velocity = direction.GetSafeNormal() * gravityForce;
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
	if (wallCapsuleTransitionDuration > 0)
	{
		UKismetSystemLibrary::MoveComponentTo(GetCapsuleComponent(), hitInfo.ImpactPoint, lookRotator,
			false, false, wallCapsuleTransitionDuration, true, EMoveComponentAction::Type::Move, CapsuleLatentInfo);
	}
	else {
		GetCapsuleComponent()->SetWorldLocation(hitInfo.ImpactPoint);
		GetCapsuleComponent()->SetWorldRotation(lookRotator);
	}
	/*************************************************************************************/

	FVector capsuleRight = UKismetMathLibrary::GetRightVector(lookRotator);

	FTransform transform = UKismetMathLibrary::MakeTransform(hitInfo.ImpactPoint, lookRotator);
	MeshWallRotator = UKismetMathLibrary::MakeRotFromZX(hitInfo.Normal, capsuleRight * -1);

	FLatentActionInfo MeshLatentInfo;
	MeshLatentInfo.CallbackTarget = this;

	FRotator meshRot = UKismetMathLibrary::InverseTransformRotation(transform, MeshWallRotator);

	/*************************************************************************************/
	if (wallMeshTransitionDuration > 0)
	{
		FVector meshPosOffset = UKismetMathLibrary::TransformDirection(transform, MeshStartingPosOffset);

		UKismetSystemLibrary::MoveComponentTo(GetMesh(), hitInfo.ImpactPoint + meshPosOffset, MeshWallRotator
			, false, false, wallMeshTransitionDuration, true, EMoveComponentAction::Type::Move, MeshLatentInfo);

	}
	else {
		GetMesh()->SetRelativeLocation(MeshStartingPosOffset);
		GetMesh()->SetRelativeRotation(meshRot);
	}
	/*************************************************************************************/

	WallNormal = hitInfo.Normal;
	WallRight = capsuleRight;
	WallForward = GetMesh()->GetRightVector();
	UE_LOG(LogTemp, Log, TEXT("Wall Right:%s | Forward:%s"), *WallRight.ToString(), *WallForward.ToString());

}

//////////////////////////////////////////////////////////////////////////
// Move on wall functions

void AProtoGravityShiftCharacter::MoveOnWall(FVector2D inputVector, FVector forward, FVector right, FVector normal, FRotator wallRotator)
{
	ConsumeMovementInputVector();
	AddMovementInput(WallRight, inputVector.X);
	AddMovementInput(WallForward, inputVector.Y);
	OrientMeshToWall(inputVector, WallForward, WallRight, WallNormal, MeshWallRotator);
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

	UE_LOG(LogTemp, Log, TEXT("forward:%s "), *forward.ToString());
	UE_LOG(LogTemp, Log, TEXT("right:%s "), *right.ToString());
	UE_LOG(LogTemp, Log, TEXT("normal:%s "), *normal.ToString());
	UE_LOG(LogTemp, Log, TEXT("---------------------"));
	UE_LOG(LogTemp, Log, TEXT("wallRotator:%s "), *wallRotator.ToString());
	UE_LOG(LogTemp, Log, TEXT("forwardVector:%s "), *forwardVector.ToString());
	UE_LOG(LogTemp, Log, TEXT("adjustedWallRotation:%s "), *adjustedWallRotation.ToString());
	UE_LOG(LogTemp, Log, TEXT("angle:%f "), angle);
	UE_LOG(LogTemp, Log, TEXT("---------------------"));
	UE_LOG(LogTemp, Log, TEXT("rot forward:%s "), *UKismetMathLibrary::GetForwardVector(finalRotation).ToString());
	UE_LOG(LogTemp, Log, TEXT("rot right:%s "), *UKismetMathLibrary::GetRightVector(finalRotation).ToString());
	UE_LOG(LogTemp, Log, TEXT("rot normal:%s "), *UKismetMathLibrary::GetUpVector(finalRotation).ToString());
	UE_LOG(LogTemp, Log, TEXT("---------------------------------------"));

	UE_LOG(LogTemp, Log, TEXT("PREV:%s"), *GetMesh()->GetComponentRotation().ToString());
	GetMesh()->SetWorldRotation(finalRotation);
	UE_LOG(LogTemp, Log, TEXT("AFTER:%s"), *GetMesh()->GetComponentRotation().ToString());

}
