
#include "PenguinMovementComponent.h"
#include "GameFramework/Character.h"

UPenguinMovementComponent::UPenguinMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPenguinMovementComponent::IsWalkable(const FHitResult& Hit) const
{
	if (!Hit.IsValidBlockingHit())
	{
		// No hit, or starting in penetration
		return false;
	}

	// Never walk up vertical surfaces.
	if (Hit.ImpactNormal.Z < UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	if (IsCrouching())
	{
		return true;
	}

	float TestWalkableZ = GetWalkableFloorZ();

	// See if this component overrides the walkable floor z.
	const UPrimitiveComponent* HitComponent = Hit.Component.Get();
	if (HitComponent)
	{
		const FWalkableSlopeOverride& SlopeOverride = HitComponent->GetWalkableSlopeOverride();
		TestWalkableZ = SlopeOverride.ModifyWalkableFloorZ(TestWalkableZ);
	}

	// Can't walk on this surface if it is too steep.
	if (Hit.ImpactNormal.Z < TestWalkableZ)
	{
		return false;
	}

	return true;
}

void UPenguinMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	// Do not update velocity when using root motion or when SimulatedProxy and not simulating root motion - SimulatedProxy are repped their Velocity
	if (!HasValidData() || HasAnimRootMotion() || DeltaTime < MIN_TICK_TIME || (CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy && !bWasSimulatingRootMotion))
	{
		return;
	}

	Friction = FMath::Max(0.f, Friction);
	const float MaxAccel = GetMaxAcceleration();
	const float MaxMovementSpeed = GetMaxSpeed();

	// Check if path following requested movement
	bool bZeroRequestedAcceleration = true;
	FVector RequestedAcceleration = FVector::ZeroVector;
	float RequestedSpeed = 0.0f;
	if (ApplyRequestedMove(DeltaTime, MaxAccel, MaxMovementSpeed, Friction, BrakingDeceleration, RequestedAcceleration, RequestedSpeed))
	{
		bZeroRequestedAcceleration = false;
	}

	if (bForceMaxAccel)
	{
		// Force acceleration at full speed.
		// In consideration order for direction: Acceleration, then Velocity, then Pawn's rotation.
		if (Acceleration.SizeSquared() > UE_SMALL_NUMBER)
		{
			Acceleration = Acceleration.GetSafeNormal() * MaxAccel;
		}
		else
		{
			Acceleration = MaxAccel * (Velocity.SizeSquared() < UE_SMALL_NUMBER ? UpdatedComponent->GetForwardVector() : Velocity.GetSafeNormal());
		}

		AnalogInputModifier = 1.f;
	}

	// Apply braking or deceleration
	const bool bZeroAcceleration = Acceleration.IsZero();

	// Apply fluid friction
	if (bFluid)
	{
		Velocity = Velocity * (1.f - FMath::Min(Friction * DeltaTime, 1.f));
	}


	// Compute final desired acceleration to be sent to the movement mode
	FVector DesiredAcceleration = FVector::ZeroVector;
	if (!bZeroAcceleration)
	{
		DesiredAcceleration += Acceleration;
	}

	if (!bZeroRequestedAcceleration)
	{
		DesiredAcceleration += RequestedAcceleration;
	}

	// Remember if we were going above limit already so we retain speed
	const float CurrentSpeed = Velocity.Size();

	// Apply move according to current mode
	float DesiredMaxSpeed = MaxMovementSpeed;
	const FVector MoveAcceleration = ComputeAcceleration(DeltaTime, DesiredAcceleration, Friction, DesiredMaxSpeed);

	// Path following above didn't care about the analog modifier, but we do for everything else below, so get the fully modified value.
	// Use max of requested speed and max speed if we modified the speed in ApplyRequestedMove above.
	const float MaxSpeed = FMath::Max3(RequestedSpeed, DesiredMaxSpeed, GetMinAnalogSpeed());

	const bool bVelocityOverMax = IsExceedingMaxSpeed(MaxSpeed);

	Velocity += MoveAcceleration * DeltaTime;

	// Only apply braking if there is no acceleration, or we are over our max speed and need to slow down to it.
	if ((bZeroAcceleration && bZeroRequestedAcceleration) || bVelocityOverMax)
	{
		const FVector OldVelocity = Velocity;

		const float ActualBrakingFriction = (bUseSeparateBrakingFriction ? BrakingFriction : Friction);
		ApplyVelocityBraking(DeltaTime, ActualBrakingFriction, BrakingDeceleration);

		// Don't allow braking to lower us below max speed if we started above it.
		if (bVelocityOverMax && Velocity.SizeSquared() < FMath::Square(MaxSpeed) && FVector::DotProduct(Acceleration, OldVelocity) > 0.0f)
		{
			Velocity = OldVelocity.GetSafeNormal() * MaxSpeed;
		}
	}

	const float DesiredMaxInputSpeed = IsExceedingMaxSpeed(MaxSpeed) ? CurrentSpeed : MaxSpeed;
	Velocity = Velocity.GetClampedToMaxSize(DesiredMaxInputSpeed);

	if (bUseRVOAvoidance)
	{
		CalcAvoidanceVelocity(DeltaTime);
	}
}

float GetAxisDeltaRotation(float InAxisRotationRate, float DeltaTime)
{
	// Values over 360 don't do anything, see FMath::FixedTurn. However we are trying to avoid giant floats from overflowing other calculations.
	return (InAxisRotationRate >= 0.f) ? FMath::Min(InAxisRotationRate * DeltaTime, 360.f) : 360.f;
}

FRotator UPenguinMovementComponent::ComputeOrientToMovementRotation(const FRotator& CurrentRotation, float DeltaTime, FRotator& DeltaRotation) const
{
	DeltaRotation = FRotator(
		GetAxisDeltaRotation(DesiredTurnRate.Pitch, DeltaTime), 
		GetAxisDeltaRotation(DesiredTurnRate.Yaw, DeltaTime), 
		GetAxisDeltaRotation(DesiredTurnRate.Roll, DeltaTime));

	if (DesiredDirection.SizeSquared() < UE_KINDA_SMALL_NUMBER)
	{
		// AI path following request can orient us in that direction (it's effectively an acceleration)
		if (bHasRequestedVelocity && RequestedVelocity.SizeSquared() > UE_KINDA_SMALL_NUMBER)
		{
			return RequestedVelocity.GetSafeNormal().Rotation();
		}

		// Don't change rotation if there is no acceleration.
		return CurrentRotation;
	}

	// Rotate toward direction of acceleration.
	return DesiredDirection.GetSafeNormal().Rotation();
}

FVector UPenguinMovementComponent::ComputeAcceleration(float DeltaTime, const FVector& InputAcceleration, float& Friction, float& MaxSpeed)
{
	DesiredDirection = Acceleration;
	DesiredTurnRate = RotationRate;

	switch (MovementMode)
	{
	case MOVE_Walking:
	case MOVE_NavWalking:
		if (IsCrouching())
		{
			return SlidingMovement(DeltaTime, InputAcceleration, Friction, MaxSpeed, DesiredDirection, DesiredTurnRate);
		}
		else
		{
			return WalkingMovement(DeltaTime, InputAcceleration, Friction, MaxSpeed, DesiredDirection, DesiredTurnRate);
		}
	default:
		return InputAcceleration;
	}
}

FVector UPenguinMovementComponent::SlidingMovement(float DeltaTime, const FVector& InputAcceleration, float& Friction, float& MaxSpeed, FVector& OutDirection, FRotator& OutTurnRate) const
{
	if (FMath::IsNearlyZero(MaxSpeed))
	{
		return InputAcceleration;
	}

	const FVector ForwardDirection = CharacterOwner->GetActorForwardVector();
	const FVector InputDirection = InputAcceleration.GetSafeNormal();

	// Disable turning in place while going fast
	const float Speed = Velocity | ForwardDirection;
	const float ForwardRatio = LockDirectionAtMaxSpeed * FMath::Clamp(Speed / MaxSpeed, 0.0f, 1.0f);

	const FVector SlopeProject = FVector::VectorPlaneProject(CharacterOwner->GetActorUpVector(), CurrentFloor.HitResult.Normal);
	OutDirection = (ForwardDirection - SlopeProject * 0.05f + InputDirection * 0.1f * (1.0f - ForwardRatio)).GetSafeNormal();

	// Compute acceleration down the slope
	const FVector SlopeAcceleration = SlopeProject * GetGravityZ();

	// Brake against sideways sliding
	const FVector BreakAcceleration = (Velocity - ForwardDirection * (Velocity | ForwardDirection)) * AntiSlideFactor * Friction;

	// Accelerate forwards
	const FVector BaseAcceleration = ForwardDirection * (InputAcceleration | ForwardDirection);

	// Moving slower uphill
	MaxSpeed *= 1.0f + (CurrentFloor.HitResult.Normal | ForwardDirection) * SlopeFactor;

	Friction = 0.3f;
	return BaseAcceleration + SlopeAcceleration - BreakAcceleration;
}

FVector UPenguinMovementComponent::WalkingMovement(float DeltaTime, const FVector& InputAcceleration, float& Friction, float& MaxSpeed, FVector& OutDirection, FRotator& OutTurnRate) const
{
	if (FMath::IsNearlyZero(MaxSpeed))
	{
		return InputAcceleration;
	}

	const FVector ForwardDirection = CharacterOwner->GetActorForwardVector();
	const FVector InputDirection = InputAcceleration.GetSafeNormal();

	// Disable turning in place while going fast
	const float Speed = Velocity | ForwardDirection;
	const float ForwardRatio = LockDirectionAtMaxSpeed * FMath::Clamp(Speed / MaxSpeed, 0.0f, 1.0f);

	OutTurnRate *= 1.0f - ForwardRatio;

	// Rotate in full force if input goes behind character
	FVector MoveDirection = InputDirection;
	const float ForwardInputDot = ForwardDirection | InputDirection;
	if (FMath::IsNearlyEqual(ForwardInputDot, -1.0f))
	{
		MoveDirection = CharacterOwner->GetActorRightVector();
	}
	else if (ForwardInputDot < 0.0f)
	{
		MoveDirection = FVector::VectorPlaneProject(InputDirection, ForwardDirection).GetSafeNormal();
	}

	OutDirection = FMath::Lerp(MoveDirection, ForwardDirection, ForwardRatio).GetSafeNormal();

	// Moving slower uphill
	const float Slope = (CurrentFloor.HitResult.Normal | OutDirection);
	MaxSpeed *= 1.0f + FMath::Min(Slope, 0.0f) * SlopeFactor;
	
	// Slide more when going down a hill
	Friction *= 1.0f - FMath::Max(Slope, 0.0f);

	MaxSpeed = FMath::Max(MaxSpeed, MaxSpeed * AnalogInputModifier);

	// Accelerate forwards
	const FVector BaseAcceleration = FMath::Lerp(InputAcceleration, OutDirection * InputAcceleration.Size(), ForwardRatio);

	// Brake against sideways sliding
	const FVector BreakAcceleration = (Velocity - OutDirection * (Velocity | OutDirection)) * AntiSlideFactor * Friction;

	return BaseAcceleration - BreakAcceleration;
}
