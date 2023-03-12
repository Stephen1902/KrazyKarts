// Copyright 2023 DME Games.  Made for the GameDev.TV Multiplayer Course.


#include "KK_KartPawn.h"

#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/SpringArmComponent.h"

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

}

// Called when the game starts or when spawned
void AKK_KartPawn::BeginPlay()
{
	Super::BeginPlay();
	
}

void AKK_KartPawn::CalculateNewVelocity(float DeltaTime)
{
	FVector Force = GetActorForwardVector() * (DrivingForceMultiplier * VehicleMass) * Throttle;
	Force += GetResistance();
	
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
	const float RotationAngle = MaxSteeringDegreesPerSecond * Steering * DeltaTime;
	const FQuat RotationDelta(GetActorUpVector(), FMath::DegreesToRadians(RotationAngle));
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
}

// Called to bind functionality to input
void AKK_KartPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// set up gameplay key bindings
	check(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &AKK_KartPawn::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AKK_KartPawn::MoveRight);
}

void AKK_KartPawn::MoveForward(float Val)
{
	Throttle = Val;
}

void AKK_KartPawn::MoveRight(float Val)
{
	Steering = Val;
}

FVector AKK_KartPawn::GetResistance() const
{
	return - Velocity.GetSafeNormal() * Velocity.SizeSquared() * DragCoefficient ;
}
