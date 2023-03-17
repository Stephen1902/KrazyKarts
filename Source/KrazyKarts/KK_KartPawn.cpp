// Copyright 2023 DME Games.  Made for the GameDev.TV Multiplayer Course.


#include "KK_KartPawn.h"

#include "DrawDebugHelpers.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/SpringArmComponent.h"
#include "Net/UnrealNetwork.h"

const static FName MOVE_FORWARD_NAME = TEXT("MoveForward");
const static FName MOVE_RIGHT_NAME = TEXT("MoveRight");

// Sets default values
AKK_KartPawn::AKK_KartPawn()
{
	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	CollisionComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision Comp"));
	SetRootComponent(CollisionComponent);
	CollisionComponent->SetBoxExtent(FVector(256.f, 96.f, 64.f));
	CollisionComponent->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	SkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh Comp"));
	SkeletalMesh->SetupAttachment(RootComponent);
	SkeletalMesh->SetRelativeLocation(FVector(0.f, 0.f, -70.f));

	// See if there is a valid skeletal mesh, set it if there is
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> VehicleMesh(TEXT("/Game/Vehicle/Sedan/Sedan_SkelMesh"));
	if (VehicleMesh.Succeeded())	{ SkeletalMesh->SetSkeletalMesh(VehicleMesh.Object);  }

	// See if there is a valid animation class, set it if there is
	static ConstructorHelpers::FClassFinder<UObject> AnimBPClass(TEXT("/Game/Vehicle/Sedan/Sedan_AnimBP"));
	if (AnimBPClass.Succeeded()) 	{  SkeletalMesh->SetAnimInstanceClass(AnimBPClass.Class); }
	
	// Create a spring arm component
	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>(TEXT("Spring Arm Component"));
	SpringArmComponent->TargetOffset = FVector(0.f, 0.f, 200.f);
	SpringArmComponent->SetRelativeRotation(FRotator(-15.f, 0.f, 0.f));
	SpringArmComponent->SetupAttachment(SkeletalMesh);
	SpringArmComponent->TargetArmLength = 600.0f;
	SpringArmComponent->bEnableCameraRotationLag = true;
	SpringArmComponent->CameraRotationLagSpeed = 7.f;
	SpringArmComponent->bInheritPitch = false;
	SpringArmComponent->bInheritRoll = false;

	// Create camera component 
	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera Component"));
	CameraComponent->SetupAttachment(SpringArmComponent, USpringArmComponent::SocketName);
	CameraComponent->bUsePawnControlRotation = false;
	CameraComponent->FieldOfView = 90.f;

	// Sets this actor to always replicate
	bReplicates = true;
	NetUpdateFrequency = 66.0f;
	MinNetUpdateFrequency = 33.0f;

}

// Called when the game starts or when spawned
void AKK_KartPawn::BeginPlay()
{
	Super::BeginPlay();

	// Get the axis scales as set in the editor to help prevent cheating
	GetAxisScales();
	
	// Unreal Units are cm, we need metres so divide by 100.
	NormalForce = -GetWorld()->GetGravityZ() / 100.f;

}

void AKK_KartPawn::CalculateNewVelocity(float DeltaTime)
{
	FVector Force = GetActorForwardVector() * (DrivingForceMultiplier * VehicleMass) * Throttle;
	Force += GetAirResistance();
	Force += GetRollingResistance();
	
	const FVector Acceleration = Force / VehicleMass;
	
	Velocity += Acceleration * DeltaTime;
}

void AKK_KartPawn::UpdateLocationFromVelocity(float DeltaTime)
{
	// The 100 converts the cm of the Unreal default  to m/s 
	const FVector TranslationThisTick = Velocity * 100 * DeltaTime;

	FHitResult HitResult;
	AddActorWorldOffset(TranslationThisTick, true, &HitResult);
	
	// Check if there was a valid hit result
	if (HitResult.IsValidBlockingHit())
	{
		Velocity = FVector::ZeroVector;
	}
}

void AKK_KartPawn::UpdateVehicleRotation(float DeltaTime)
{
	const float DeltaRotation = FVector::DotProduct(GetActorForwardVector(), Velocity) * DeltaTime;
	const float RotationAngle = (DeltaRotation / MinimumTurningRadius) * Steering;
	const FQuat RotationDelta(GetActorUpVector(), RotationAngle);
	Velocity = RotationDelta.RotateVector(Velocity);
	
	AddActorWorldRotation(RotationDelta);
}

// Called every frame
void AKK_KartPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateVehicleRotation(DeltaTime);
	
	CalculateNewVelocity(DeltaTime);
	UpdateLocationFromVelocity(DeltaTime);

	
	if (HasAuthority())
	{
		ReplicatedTransform = GetActorTransform();
		//OnRep_Transform();
	}

	const FString RoleAsString = GetActorLocation().ToString();
	
	//DrawDebugString(GetWorld(), FVector(0, 0, 100), GetRoleAsText(RoleIn), this, FColor::White, DeltaTime);		
}

// Called to bind functionality to input
void AKK_KartPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// set up gameplay key bindings
	check(PlayerInputComponent);

	PlayerInputComponent->BindAxis(MOVE_FORWARD_NAME, this, &AKK_KartPawn::MoveForward);
	PlayerInputComponent->BindAxis(MOVE_RIGHT_NAME, this, &AKK_KartPawn::MoveRight);
}

void AKK_KartPawn::MoveForward(float Val)
{
	Throttle = Val;
	Server_MoveForward(Throttle);
}

void AKK_KartPawn::MoveRight(float Val)
{
	Steering = Val;
	Server_MoveRight(Steering);
}

void AKK_KartPawn::OnRep_Transform()
{
	SetActorTransform(ReplicatedTransform);
}


void AKK_KartPawn::GetAxisScales()
{
	// Get the instance of the InputSettings
	const UInputSettings* InputSettings = UInputSettings::GetInputSettings();

	// AxisMappings with all the information will be stored here
	TArray<FInputAxisKeyMapping> VerticalKeys;
	TArray<FInputAxisKeyMapping> HorizontalKeys;

	// Load the AxisMappings
	InputSettings->GetAxisMappingByName(FName(MOVE_FORWARD_NAME), VerticalKeys);
	InputSettings->GetAxisMappingByName(FName(MOVE_RIGHT_NAME), HorizontalKeys);

	// Assign each key to the correct direction
	for (const FInputAxisKeyMapping VerticalKey : VerticalKeys)
	{
		if (VerticalKey.Scale > 0.f)
			MaxForwardAxisScale = VerticalKey.Scale;
		else if (VerticalKey.Scale < 0.f)
			MaxBackwardAxisScale = VerticalKey.Scale;
	}

	for (const FInputAxisKeyMapping HorizontalKey : HorizontalKeys)
	{
		if (HorizontalKey.Scale > 0.f)
			MaxRightAxisScale = HorizontalKey.Scale;
		else if (HorizontalKey.Scale < 0.f)
			MaxLeftAxisScale = HorizontalKey.Scale;
	}
}

FString AKK_KartPawn::GetRoleAsText(ENetRole RoleIn)
{
	switch (RoleIn)
	{
	case ROLE_None:
		return "None";
	case ROLE_SimulatedProxy:
		return "SimulatedProxy";
	case ROLE_AutonomousProxy:
		return "AutonomousProxy";
	case ROLE_Authority:
		return "Authority";
	default:
		return "ERROR";
	}
}

void AKK_KartPawn::Server_MoveForward_Implementation(float Val)
{
	Throttle = FMath::Clamp(Val, MaxBackwardAxisScale, MaxForwardAxisScale);
	if (!FMath::IsNearlyZero(Val))
	{
		GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::Green, TEXT("Forward"));
	}
}

bool AKK_KartPawn::Server_MoveForward_Validate(float Val)
{
	bool ReturnValidated = true;
	if (Val < MaxBackwardAxisScale || Val > MaxForwardAxisScale)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cheating detected in MoveForward function"));
		ReturnValidated = false;
	}
	return ReturnValidated;
}

void AKK_KartPawn::Server_MoveRight_Implementation(float Val)
{
	Steering = FMath::Clamp(Val, MaxLeftAxisScale, MaxRightAxisScale);
	if (!FMath::IsNearlyZero(Val))
	{
		GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::Green, TEXT("Right"));
	}
}

bool AKK_KartPawn::Server_MoveRight_Validate(float Val)
{
	bool ReturnValidated = true;
	if (Val < MaxLeftAxisScale || Val > MaxRightAxisScale)
	{
		UE_LOG(LogTemp, Warning, TEXT("Cheating detected in MoveRight function"));
		ReturnValidated = false;
	}
	return ReturnValidated;
}

FVector AKK_KartPawn::GetAirResistance() const
{
	return - Velocity.GetSafeNormal() * Velocity.SizeSquared() * DragCoefficient ;
}

FVector AKK_KartPawn::GetRollingResistance() const
{
	return - Velocity.GetSafeNormal() * RollingCoefficient * NormalForce; 
}

void AKK_KartPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AKK_KartPawn, ReplicatedTransform);
}
