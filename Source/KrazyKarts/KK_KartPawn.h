// Copyright 2023 DME Games.  Made for the GameDev.TV Multiplayer Course.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "KK_KartPawn.generated.h"

UCLASS()
class KRAZYKARTS_API AKK_KartPawn : public APawn
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	AKK_KartPawn();
	
	UPROPERTY(EditDefaultsOnly, Category = "Setup")
	class UBoxComponent* CollisionComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Setup")
	USkeletalMeshComponent* SkeletalMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Setup")
	class USpringArmComponent* SpringArmComponent;
	
	UPROPERTY(EditDefaultsOnly, Category = "Setup")
	class UCameraComponent* CameraComponent;

	// The force applied to the car when the Throttle is fully down, gets multiplied by the mass of the vehicle.
	UPROPERTY(EditDefaultsOnly, Category = "Setup")
	float DrivingForceMultiplier = 10.f;

	// The mass of the vehicle in KG.
	UPROPERTY(EditDefaultsOnly, Category = "Setup")
	float VehicleMass = 1000.f;

	// Minimum radius of the car turning at full lock in metres.
	UPROPERTY(EditDefaultsOnly, Category = "Setup")
	float MinimumTurningRadius = 10.f;

	// Air resistance.  Higher means more drag.
	UPROPERTY(EditDefaultsOnly, Category = "Setup")
	float DragCoefficient = 16.f;

	// Resistance of the tyres on the road.  Higher means more drag.
	UPROPERTY(EditDefaultsOnly, Category = "Setup")
	float RollingCoefficient = 0.0150f;
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	void CalculateNewVelocity(float DeltaTime);
	void UpdateLocationFromVelocity(float DeltaTime);
	void UpdateVehicleRotation(float DeltaTime);
	FVector GetAirResistance() const;
	FVector GetRollingResistance() const;
	
public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

private:
	void MoveForward(float Val);
	void MoveRight(float Val);

	FVector Velocity = FVector(0.f);

	// Throttle being applied by the player
	float Throttle;

	// Steering being applied by the player
	float Steering;

	// World Gravity, used to calculate normal force for rolling resistance
	float NormalForce;
};


