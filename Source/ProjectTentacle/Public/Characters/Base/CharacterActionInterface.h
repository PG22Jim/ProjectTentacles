// Copyright (C) The Tentacle Zone 2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CharacterActionInterface.generated.h"



class AEnemyBase;


// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UCharacterActionInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */



class PROJECTTENTACLE_API ICharacterActionInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void ActionEnd(bool BufferingCheck);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void StartWaitForCombo();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void EnterUnableCancelAttack();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void TryTriggerPlayerCounter();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void DetachEnemyTarget();


	// Player Delegate
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void TryStoreCounterTarget(AEnemyBase* CounterTarget);
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void TryRemoveCounterTarget(AEnemyBase* CounterTarget);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void OnActivateComboResetTimer();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void OnShowPlayerIndicatorHUD(bool Show);
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void OnChangePlayerIndicatorHUD_Visibility(bool IsVisible);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void OnApplyChargeKnockForce(FVector ApplyingForce, FVector ForceDirection);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void TryClearCounterVictim(AEnemyBase* ClearingVictim);
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void OnMakingTentacleVisible(bool bShowTentacle);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void OnEnterOrExitCombat(bool bEnterCombat);
	
	
	// Enemy Delegate
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void OnCounterTimeEnd();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void OnPullingEnemy(FVector PullingDest, float PullingPower, float WorldDeltaSec);
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void OnResumeMovement();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void OnResetEnemyCurrentState();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void OnSetIsCountered(bool Countered);
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void OnStartCounteredAnimation();
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void ReceiveAttackInCounterState(AActor* CounteringTarget);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void StartLyingOnTheGround();
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void RepeatLyingOnTheGround();
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void TryToDamagePlayer();
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void DamagingTarget();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void OnBeginStun();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void OnResumeFromStunTimerCountDown();
	
	
	
	// Brute
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void OnContinueSecondAttackMontage();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void OnDealAoeDamage();
	
};
