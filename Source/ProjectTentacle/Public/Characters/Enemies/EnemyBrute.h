// Copyright (C) The Tentacle Zone 2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/Enemies/EnemyBase.h"
#include "Characters/Player/PlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/TimelineComponent.h"
#include "EnemyBrute.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, BlueprintType)
class PROJECTTENTACLE_API AEnemyBrute : public AEnemyBase
{
	GENERATED_BODY()


private:

	// Class variables for timeline function usage
	FVector AttackMovingDestination;
	FVector SelfAttackStartPos;

	void InitializeTimelineComp();
	void TryStopAttackMovement();


	void PlaySpecificAttackMovingTimeline(EEnemyAttackType EnemyAttack);
	void UpdateAttackingVariables();
	
	void ChargeKnock(AActor* KnockingActor);
	
	void SetCapsuleCompCollision(ECollisionChannel ResponseChannel, ECollisionResponse RequestResponse);
	bool ShouldKeepCharging(FVector DirToPlayer);
	
	void PlayReceiveCounterAnimation();

	FVector GetChargeDirection(FVector DirToPlayer, FVector ActorCurrentPos);
	FVector GetJumpSlamPosition(FVector DirFromPlayerToSelf, FVector PlayerPos);
	bool CheckIfPlayerDodge();
	void OnCancelCharge(const bool bHitWall);

	EBruteAttackType BruteAttack = EBruteAttackType::Swipe;
	

	//
	int32 CounteredTime = 0; 
	bool IsSecondAttack = false;

	// Timeline for enemy attack movement
	FTimeline ChargeMovingTimeline;
	FTimeline JumpSlamMovingTimeline;
	FTimeline CounterableMovingTimeline;

	float MaxTravelDistance = 0;
	float RemainAttackDistance = 0;
	float TravelDistancePerTick = 0;

	bool DoesPlayerDodge = false;
	FVector StartJumpingLocation = FVector(0,0,0);
	FVector EndJumpingLocation = FVector(0,0,0);
	
	FVector AttackMovingDir = FVector(0,0,0);

	APlayerCharacter* PlayerRef;
	
protected:
	
	// bool to check if enemy is in counter state
	UPROPERTY(BlueprintReadOnly)
	bool IsCountered = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= VFX)
	UNiagaraSystem* NS_JumpSlam;
	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting_General)
	EBruteAttackType DebugAttackType;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting_General)
	float CounterableAttackMovingDistance = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting_General)
	float OffsetFromPlayer = 50.0f;
	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting_General)
	float DistanceToDecideFarOrClose = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting_General)
	float ChanceToExecuteCharging = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting_Swipe)
	float ToTalTravelTime_Swipe = 0.9;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting_Swipe)
	float CounterTriggerRadius = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting_Swipe)
	float CounterTriggerHeight = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting_Swipe)
	float DamageTriggerRadius = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting_Swipe)
	float DamageTriggerHeight = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting_Swipe)
	float AttackTravelDistance_Swipe = 200.0f;

	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting_JumpSlam)
	float TotalTravelTime_JumpSlam = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting_JumpSlam)
	float AttackTravelDistance_JumpSlam = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting_JumpSlam)
	float JumpSlamHeight = 500.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting_JumpSlam)
	float AoeDamageRadius = 500.0f;

	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting_Charge)
	float ChargingDistance = 800.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting_Charge)
	float TotalTravelTime_Charging = 4.0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting_Charge)
	float ChargeKnockForce = 500.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting_Charge)
	float ChargeKnockAngle = 35.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting_Charge)
	float AttackTravelDistance_Charge = 2000.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting_Charge)
	float ChargeTrackingAngle = 5.0f;
	
	

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = DamageReceiveSetting)
	UAnimMontage* DamageReceiveAnimation;


	UPROPERTY(BlueprintReadOnly)
	bool IsStunned = false;


	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting_Movement)
	UCurveFloat* UncounterableAttackMovingCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting_Movement)
	UCurveFloat* JumpSlamHeightCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting_Movement)
	UCurveFloat* JumpSlamDistanceCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting_Movement)
	UCurveFloat* CounterableAttackMovingCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting_Animation)
	UAnimMontage* CouterableAttackSecond;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting_Animation)
	UAnimMontage* JumpSlamAttack;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AttackSetting_Animation)
	UAnimMontage* UnableCounterAttackSecond;


	void TryGetPlayerRef();
	virtual void ExecuteAttack() override;

	FVector CalculateDestinationForAttackMoving(FVector PlayerCurrentPos, float CurrentTimelineAlpha);
	
	UFUNCTION()
	void UpdateAttackingPosition(float Alpha);

	UFUNCTION()
	void OnDealChargeDamage(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	
	void OnContinueSecondAttackMontage_Implementation() override;

	// UFUNCTION()
	// void RecoverFromStunState();
	
	TArray<AActor*> GetActorsInFrontOfEnemy(bool IsDamaging);
	TArray<AActor*> GetActorsAroundEnemy();

	virtual void OnDeath() override;
	
public:


	// ================================== Get And Set =================================
	void SetIsCountered(bool NewIsCountered) {IsCountered = NewIsCountered;}

	UFUNCTION(BlueprintCallable, BlueprintPure)
	float GetRemainAttackDistance() const {return RemainAttackDistance;}
	
	UFUNCTION(BlueprintCallable, BlueprintPure)
	float GetTravelDistancePerTick() const {return TravelDistancePerTick;}
	
	EBruteAttackType FindExecutingAttackType();
	
	void SetExecutingAttackTypes(EBruteAttackType ExecutingBruteAction);
	
	UFUNCTION(BlueprintCallable)
	void TestFunction();

	UFUNCTION(BlueprintImplementableEvent)
	void DebugLine(FVector PlaceToDraw, FVector PlaceToEnd);
	
	UFUNCTION(BlueprintImplementableEvent)
	void DebugSphere(FVector PlaceToDraw, float radius, float Height);


	AEnemyBrute();
	
	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;


	virtual void TryToDamagePlayer_Implementation() override;

	virtual void TryTriggerPlayerCounter_Implementation() override;

	virtual void OnCounterTimeEnd_Implementation() override;

	virtual void OnSetIsCountered_Implementation(bool Countered) override;

	virtual void OnStartCounteredAnimation_Implementation() override;

	virtual void ReceiveDamageFromPlayer_Implementation(float DamageAmount, AActor* DamageCauser, EPlayerAttackType PlayerAttackType) override;

	virtual void OnDealAoeDamage_Implementation() override;


};