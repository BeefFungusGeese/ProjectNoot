
#include "PenguinCharacter.h"

#include "PenguinMovementComponent.h"

APenguinCharacter::APenguinCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UPenguinMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	bUseControllerRotationYaw = true;
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
}


