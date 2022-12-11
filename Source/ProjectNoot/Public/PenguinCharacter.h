#pragma once

#include "GameFramework/Character.h"
#include "PenguinCharacter.generated.h"


/**
*/
UCLASS(Blueprintable)
class PROJECTNOOT_API APenguinCharacter : public ACharacter
{
	GENERATED_BODY()

	public:
		APenguinCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};