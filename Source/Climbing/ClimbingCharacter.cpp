// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClimbingCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Components/ArrowComponent.h"
#include "Kismet/KismetMathLibrary.h"

#include "Kismet/KismetSystemLibrary.h"


//////////////////////////////////////////////////////////////////////////
// AClimbingCharacter

AClimbingCharacter::AClimbingCharacter() : bDetectLedge(false)
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

	// Arrow for line trace
	LedgeFinder = CreateDefaultSubobject<UArrowComponent>(TEXT("LedgeFinder"));
	LedgeFinder->SetupAttachment(GetMesh());
}

void AClimbingCharacter::BeginPlay()
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
}

void AClimbingCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	FHitResult CapsuleHit;
	FHitResult LineHit;

	if(TraceForLedge(CapsuleHit))
	{
		GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Blue, FString::Printf(TEXT("Capsule hit %s!"), *CapsuleHit.GetActor()->GetActorNameOrLabel()));

		if (VerticalTrace(LineHit))
		{
			bDetectLedge = true;
			LedgeHeightLocation = CheckLedgeImpact.Z;
			GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Blue, FString::Printf(TEXT("Line hit %s!"), *LineHit.GetActor()->GetActorNameOrLabel()));
		}
		else
		{
			bDetectLedge = false;
		}
	}

	GEngine->AddOnScreenDebugMessage(-1, .001f, FColor::Cyan, FString::Printf(TEXT("LedgeHeightLocation: %f"), LedgeHeightLocation));
	
}

//////////////////////////////////////////////////////////////////////////
// Input

void AClimbingCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		//Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		//Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AClimbingCharacter::Move);

		//Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AClimbingCharacter::Look);

	}

}

void AClimbingCharacter::Move(const FInputActionValue& Value)
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

void AClimbingCharacter::Look(const FInputActionValue& Value)
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

bool AClimbingCharacter::TraceForLedge(FHitResult &Hit)
{
	// Get the actors location and move it 50.f up
	FVector TraceStart = GetActorLocation() + FVector(0.f, 0.f, 50.f);
	// Scale the actors forward vector and add the actor's location and move it 50.f up
	FVector TraceEnd = GetActorForwardVector() * 33.f + GetActorLocation() + FVector(0.f, 0.f, 50.f);
	// Make a capsule to trace with
	FCollisionShape Capsule = FCollisionShape::MakeCapsule(22.f, 100.f);
	// Capsule trace
	TArray<AActor*> ActorsToIgnore = TArray<AActor*>();
	bool ValidHit = UKismetSystemLibrary::CapsuleTraceSingleByProfile(this, TraceStart, TraceEnd, Capsule.GetCapsuleRadius(), Capsule.GetCapsuleHalfHeight(), FName(TEXT("Ledge")), false, ActorsToIgnore, EDrawDebugTrace::ForOneFrame, Hit, true, FLinearColor::Yellow, FLinearColor::Yellow);
	//bool ValidHit = UKismetSystemLibrary::CapsuleTraceSingle(this, TraceStart, TraceEnd, Capsule.GetCapsuleRadius(), Capsule.GetCapsuleHalfHeight(), TraceTypeQuery1, false, ActorsToIgnore, EDrawDebugTrace::ForOneFrame, Hit, true, FLinearColor::Yellow, FLinearColor::Yellow);
	FVector CapsuleImpactPoint = Hit.ImpactPoint;
	FVector CapsuleImpactNormal = Hit.ImpactNormal;
	float ImpactRot = UKismetMathLibrary::MakeRotFromX(CapsuleImpactNormal).Yaw;

	return ValidHit;
}

bool AClimbingCharacter::VerticalTrace(FHitResult& Hit)
{
	// Get the arrow component location and move it up 80.f
	FVector TraceStart = LedgeFinder->GetComponentLocation() + FVector(0.f, 0.f, 80.f);
	// Getthe arrow component location
	FVector TraceEnd = LedgeFinder->GetComponentLocation();
	// Line trace for ledge collision
	bool ValidHit = UKismetSystemLibrary::LineTraceSingleByProfile(this, TraceStart, TraceEnd, FName(TEXT("Ledge")), false, TArray<AActor*>(), EDrawDebugTrace::ForOneFrame, Hit, true, FLinearColor::White, FLinearColor::White);

	CheckLedgeImpact = Hit.ImpactPoint;
	return ValidHit;
}



