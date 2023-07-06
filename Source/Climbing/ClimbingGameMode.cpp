// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClimbingGameMode.h"
#include "ClimbingCharacter.h"
#include "UObject/ConstructorHelpers.h"

AClimbingGameMode::AClimbingGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
