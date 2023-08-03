// Copyright (C) The Tentacle Zone 2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "VFXAndSoundInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UVFXAndSoundInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class PROJECTTENTACLE_API IVFXAndSoundInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	UFUNCTION(BlueprintNativeEvent)
	void PlayReceiveDamageVFXAndSound(FVector Location);
};
