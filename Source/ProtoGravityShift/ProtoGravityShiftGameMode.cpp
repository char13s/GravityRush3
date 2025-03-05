// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProtoGravityShiftGameMode.h"
#include "ProtoGravityShiftCharacter.h"
#include "UObject/ConstructorHelpers.h"


AProtoGravityShiftGameMode::AProtoGravityShiftGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
