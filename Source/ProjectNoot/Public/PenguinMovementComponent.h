#pragma once

#include "Kismet/KismetSystemLibrary.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "PenguinMovementComponent.generated.h"

/**
 * 
 */
UCLASS()
class PROJECTNOOT_API UPenguinMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

	public:
		UPenguinMovementComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
		virtual bool IsWalkable(const FHitResult& Hit) const override;
		virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
		FRotator ComputeOrientToMovementRotation(const FRotator& CurrentRotation, float DeltaTime, FRotator& DeltaRotation) const override;

		/** How much the character tries to keep their current direction when going at max speed. Scales with speed. */
		UPROPERTY(EditAnywhere, Category = "Movement", Meta = (Keywords = "C++"))
			float LockDirectionAtMaxSpeed = 0.75f;

		/** Breaking force factor against sideways sliding. */
		UPROPERTY(EditAnywhere, Category = "Movement", Meta = (Keywords = "C++"))
			float AntiSlideFactor = 10.0f;

		/** How much max speed changes with slopes. */
		UPROPERTY(EditAnywhere, Category = "Movement", Meta = (Keywords = "C++"))
			float SlopeFactor = 1.5f;

private:
	FVector ComputeAcceleration(float DeltaTime, const FVector& InputAcceleration, float& Friction, float& MaxSpeed);
	FVector SlidingMovement(float DeltaTime, const FVector& InputAcceleration, float& Friction, float& MaxSpeed, FVector& OutDirection, FRotator& OutTurnRate) const;
	FVector WalkingMovement(float DeltaTime, const FVector& InputAcceleration, float& Friction, float& MaxSpeed, FVector& OutDirection, FRotator& OutTurnRate) const;

	// We want to also control rotation from movement modes so we temporarily store them here
	FVector DesiredDirection = FVector::ZeroVector;
	FRotator DesiredTurnRate = FRotator::ZeroRotator;
};
