// Copyright (C) The Tentacle Zone 2023. All Rights Reserved.


#include "Characters/Enemies/AN_PlayRifleReloadSound.h"

#include "Characters/Base/EnemyRangeInterface.h"

void UAN_PlayRifleReloadSound::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	Super::Notify(MeshComp, Animation);

	// valid check
	if(!MeshComp || !MeshComp->GetOwner()) return;

	// get owner actor reference
	AActor* OwnerRef = MeshComp->GetOwner();

	// check if owner class has character action interface
	if(OwnerRef->GetClass()->ImplementsInterface(UEnemyRangeInterface::StaticClass()))
	{
		// if it has UEnemyRangeInterface, it means its Ranged character, execute its OnRifleBeginAiming function
		IEnemyRangeInterface::Execute_OnReloading(OwnerRef);
	}



}
