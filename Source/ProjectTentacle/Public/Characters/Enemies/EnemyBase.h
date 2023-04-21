// Copyright (C) The Tentacle Zone 2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnemyBaseController.h"
#include "EnemyType.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Characters/Base/CharacterActionInterface.h"
#include "Characters/Base/DamageInterface.h"
#include "Characters/Base/EnemyWidgetInterface.h"
#include "Characters/Base/Widget_EnemyAttackIndicator.h"
#include "Characters/Base/Widget_EnemyTargetIconWidget.h"
#include "Components/TimelineComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Character.h"
#include "EnemyBase.generated.h"

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnUpdatingEnemyAttackType, EEnemyAttackType, NewAttackType);
DECLARE_DYNAMIC_DELEGATE_ThreeParams(FOnFinishAttackingTask, UBehaviorTreeComponent*, BehaviorTreeReference, bool, bIsSuccess, bool, DoesGetInterupted);

UCLASS()
class PROJECTTENTACLE_API AEnemyBase : public ACharacter, public ICharacterActionInterface, public IDamageInterface, public IEnemyWidgetInterface
{
	GENERATED_BODY()

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	void InitializeWidgetComponents();
	
	void InitializeEnemyControllerRef();

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Movement")
	float WalkSpeed = 600.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Movement")
	float StrafeSpeed = 300.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= DebugSetting)
	bool EnableAttackMovement = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= DebugSetting)
	bool EnableEnemyAttackTracking = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= DebugSetting, meta=(ClampMin=0, ClampMax=1))
	float AttackTrackingLimitInAlpha = 0.5f; 
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Combat)
	EEnemyType UnitType = EEnemyType::Melee;

	UPROPERTY(EditDefaultsOnly, Category=Combat)
	float AttackCompletionTime = 2.0f;
	
	UPROPERTY(EditDefaultsOnly, Category=Combat)
	float AttackCounterableTime = 1.4f;
	

	// Updating Enemy Attack Delegate Signature
	FOnUpdatingEnemyAttackType OnUpdatingEnemyAttackIndicator;
	
	// User Widget Components
	UPROPERTY(VisibleAnywhere)
	UWidgetComponent* EnemyAttackIndicatorWidgetComponent;
	
	UPROPERTY(VisibleAnywhere)
	UWidgetComponent* EnemyTargetedIconWidgetComponent;

	// Indicator Widget references
	UPROPERTY()
	UWidget_EnemyAttackIndicator* AttackIndicatorRef;

	UPROPERTY()
	UWidget_EnemyTargetIconWidget* EnemyTargetWidgetRef;


	// Enemy Controller, BT, and BB Component reference 
	UPROPERTY()
	AEnemyBaseController* CurrentEnemyBaseController;

	UPROPERTY()
	UBehaviorTreeComponent* BTComponent;
	
	UPROPERTY()
	UBlackboardComponent* BBComponent;


	FTimerHandle GettingUpTimerHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Setting_Lying)
	float TimeToGetUp = 3.0f;	

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Setting_Lying)
	UAnimMontage* GetUpMontage;
	
	// Enemy Property variable
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = EnemyProperty)
	int32 Health = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = EnemyProperty)
	int32 MaxHealth = 10;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = EnemyProperty)
	EEnemyCurrentState CurrentEnemyState = EEnemyCurrentState::WaitToAttack;

	bool IsDead = false;



	
	// Receiving Damage Animations
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ReceiveDamageAnimations)
	UAnimMontage* ReceiveShortFlipKick;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ReceiveDamageAnimations)
	UAnimMontage* ReceiveFlyingKick;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ReceiveDamageAnimations)
	UAnimMontage* ReceiveFlyingPunch;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ReceiveDamageAnimations)
	UAnimMontage* ReceiveSpinKick;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ReceiveDamageAnimations)
	UAnimMontage* ReceiveDashingDoubleKick;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ReceiveDamageAnimations)
	UAnimMontage* FinishedAnimation;


	// Attacking variable to set up or adjust in blueprint
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category= Attack_Setting)
	TArray<TEnumAsByte<EObjectTypeQuery>> FilterType;

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category= Attack_Setting)
	UClass* FilteringClass;

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category= Attack_Setting)
	TSubclassOf<UDamageType> DamageType;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category= Attack_Setting)
	EEnemyAttackType CurrentAttackType;

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category= Attack_Setting)
	int32 BaseDamageAmount = 2;

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category= Attack_Setting)
	float AttackMovingDistance = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting)
	float OffsetFromPlayer = 50.0f;

	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting)
	UCurveFloat* UncounterableAttackMovingCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting)
	UCurveFloat* CounterableAttackMovingCurve;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting)
	float CounterTriggerRadius = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting)
	float CounterTriggerHeight = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting)
	float DamageTriggerRadius = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting)
	float DamageTriggerHeight = 10.0f;
	
	
	bool IsAttacking = false;
	
	// Attacking Animation
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category= Attack_Animations)
	UAnimMontage* CounterableAttackMontage;

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category= Attack_Animations)
	UAnimMontage* CounterVictimMontage;

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category= Attack_Animations)
	UAnimMontage* NotCounterableAttackMontage;



	// ===================================================== Attack ====================================================
	EEnemyAttackAnimMontages SetAttackType();

	FVector CalculateDestinationForAttackMoving(FVector PlayerPos);

	void PlaySpecificAttackMovingTimeline(EEnemyAttackType AttackType);
	
	TArray<AActor*> GetActorsInFrontOfEnemy(bool IsDamaging);

	// ===================================================== Receive Damage =================================================
	void PlayReceiveDamageAnimation(EPlayerAttackType ReceivedAttackType);
	
	void PlayDeathAnimation(EPlayerAttackType ReceivedAttackType);

	void HealthReduction(float DamageAmount);

	
	// ===================================================== On Death =======================================================
	void OnDeath();

	void RagDollPhysicsOnDead();

	
	
	// ===================================================== Stunning ===========================================

	void BeginLyingCountDown();

	void PlayLyingMontage();

	void RecoverFromLying();
	
	
public:
	void EnableStrafe(bool bStrafe = true) const;
	void ExecuteRangedAttack(AActor* Target);

	EEnemyType GetType() const;

	// public delegate signature for finishing attacking task
	FOnFinishAttackingTask OnFinishAttackingTask;
	
	// Sets default values for this character's properties
	AEnemyBase();

	// Register encounters and targets with owning controller
	void RegisterOwningEncounter(class AEncounterVolume* NewOwningEncounter);
	void EngageTarget(AActor* Target);

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	float GetAttackCompletionTime() const;
	float GetAttackCounterableTime() const;


	void OnHideAttackIndicator();
	

	UFUNCTION(BlueprintCallable)
	void ExecuteAttack();
	void StartCounterAttackAnimation();
	
	// Instantly rotate to desired direction
	void InstantRotation(FVector RotatingVector);
	
	void PlayFinishedAnimation();


	void TryStopMoving();
	void TryResumeMoving();

	void TryStopAttackMovement();

	// ============================================= Timeline function ====================================================
	
	UFUNCTION()
	void UpdateAttackingPosition(float Alpha);


	// ============================================= Utility Functions ====================================================

	void TrySwitchEnemyState(EEnemyCurrentState NewState) { if(CurrentEnemyState != NewState) CurrentEnemyState = NewState;}

	

	// ============================================= Get and Set functions ================================================
	bool GetIsDead() const { return IsDead;}
	
	int32 GetEnemyHealth() const { return Health;}
	void SetEnemyHealth(int32 NewHealth) {Health = NewHealth;}

	UFUNCTION(BlueprintCallable, BlueprintPure)
	UBlackboardComponent* GetBBComponent() const {return BBComponent;}
	
	UFUNCTION(BlueprintCallable, BlueprintPure)
	EEnemyAttackType GetEnemyStoredAttackType() const {return CurrentAttackType;}

	EEnemyCurrentState GetCurrentEnemyState() const {return CurrentEnemyState;}
	
	UFUNCTION(BlueprintCallable)
	UBehaviorTreeComponent* GetBehaviourTreeComponent() const {return BTComponent;}
	void SetBehaviourTreeComponent(UBehaviorTreeComponent* NewBehaviourTreeComponent) {BTComponent = NewBehaviourTreeComponent;}

	UFUNCTION(BlueprintCallable, BlueprintPure)
	bool GetIsCountered() const {return IsCountered;}
	void SetIsCountered(bool NewIsCountered) {IsCountered = NewIsCountered;}

	
	// ================================================== Interface Functions ============================================
	
	virtual void OnResumeMovement_Implementation() override;

	virtual void OnResetEnemyCurrentState_Implementation() override;
	
	virtual void TryToDamagePlayer_Implementation() override;

	virtual void TryTriggerPlayerCounter_Implementation() override;

	virtual void OnCounterTimeEnd_Implementation() override;

	virtual void ReceiveDamageFromPlayer_Implementation(int32 DamageAmount, AActor* DamageCauser, EPlayerAttackType PlayerAttackType) override;

	virtual void StartLyingOnTheGround_Implementation() override;

	virtual void RepeatLyingOnTheGround_Implementation() override;
	
	virtual void ShowEnemyAttackIndicator_Implementation() override;

	virtual void UnShowEnemyAttackIndicator_Implementation() override;

	virtual void ShowPlayerTargetIndicator_Implementation() override;
	
	virtual void UnShowPlayerTargetIndicator_Implementation() override;
	

private:

	// bool to check if enemy is in counter state
	bool IsCountered = false;

	// bool to check if enemy is in attack task
	bool AttackTaskOn = false;
	
	// Timeline for enemy attack movement
	FTimeline UnCounterMovingTimeline;
	FTimeline CounterableMovingTimeline;

	// Class variables for timeline function usage
	FVector AttackMovingDestination;
	FVector SelfAttackStartPos;
	
	class AEnemyBaseController* OwnController;
	void TryGetOwnController();

	void TryClearFromPlayerTarget();

	void TurnCollisionOffOrOn(bool TurnCollisionOff);
	
	void TryFinishAttackTask(EEnemyCurrentState SwitchingState);

	
};