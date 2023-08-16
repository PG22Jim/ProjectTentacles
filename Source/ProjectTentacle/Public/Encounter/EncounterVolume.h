// Copyright (C) The Tentacle Zone 2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EncounterVolumeInterface.h"
#include "GameFramework/Actor.h"
#include "WaveParams.h"
#include "Components/BoxComponent.h"
#include "EncounterVolume.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEncounterComplete);
class UNavigationInvokerComponent;
class AEnemyBase;

UCLASS()
class PROJECTTENTACLE_API AEncounterVolume : public AActor, public IEncounterVolumeInterface
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AEncounterVolume();

	// Try to start the encounter, if it hasn't already
	UFUNCTION()
	void TryTriggerEncounter(AActor* Target);

	UFUNCTION(BlueprintImplementableEvent)
	void UpdateTutorial();
	
	// Return list of contained units excluding passed pawn
	TArray<AEnemyBase*> GetAlliesForPawn(APawn* Pawn);

	// Register unit on attack queue
	void RegisterOnAttackQueue(class AEnemyBaseController* RegisteringController);
	
	// Register when a unit has completed it's attack
	void RegisterCompletedAttack(AEnemyBaseController* RegisteringController);
	
	// Fire when a unit is destroyed tp check if we should trigger spawn and update our queues
	void RegisterUnitDestroyed(AEnemyBaseController* Unit, bool bForceDespawn);

	void AddSpawnedUnitToEncounter(AEnemyBase* Unit);

	void RemoveDeadUnitFromEncounter(AEnemyBaseController* DeadUnit);

	// Delegate function to be execute to send all enemy to reposition
	UFUNCTION()
	void SendAllEnemyToReposition(bool DoesIncludeHeavy);

	virtual void AssignQueueEnemyToReposition_Implementation(bool DoesIncludeHeavy) override;

	// Marks the encounter as completed (Used after reloading from a save)
	void MarkComplete();
	bool IsComplete() const;
	bool IsActive() const;

	// Debug to kill active units
	void KillUnits();

	float GetEncounterStartDelay() const;

protected:
	// Blueprint exposed delegate, fires when the encounter is complete (All spawns complete + all units defeated)
	UPROPERTY(EditDefaultsOnly, BlueprintAssignable)
	FEncounterComplete EncounterComplete;
	
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnActorHitTriggerCollision(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
											UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION(BlueprintImplementableEvent)
	void OnEncounterBegin_BP();

	
	// Area around the encounter to generate navigation
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	UNavigationInvokerComponent* NavInvoker;

	// Root component
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	USceneComponent* Root;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	UBoxComponent* CollisionMesh;

	
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Combat)
	bool IsPlayerInsideEncounter = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Combat)
	bool IsLastEncounter = false;
	
	// Set of all contained units at any time
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Combat)
	TSet<AEnemyBase*> ContainedUnits;

	// Delay between being spotted and the encounter starting
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Combat)
	float EncounterStartDelay = 1.f;
	
	// Delay between tickets being issued to attackers on basic (melee, healer, and ranged) and heavy (brute) queues
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Combat)
	float AttackStartDelayBasic = 3.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Combat)
	float AttackStartDelayHeavy = 3.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Spawn)
	class AUnitPool* UnitPool;

	// Wave system setup
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Spawn)
	float DespawnTimer = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Spawn)
	TArray<FWaveParams> WaveParameters;

	// Attack queue for melee and ranged units
	TArray<AEnemyBaseController*> AttackQueueBasic;

	// Attack queue for brutes
	TArray<AEnemyBaseController*> AttackQueueHeavy;

	// Pop random enemy from the queue and command them to attack
	UFUNCTION()
	void BeginAttack(bool bIsBasic = true);

	// Wake all spawn points associated with the current wave
	UFUNCTION()
	void StartSpawn();

	// Return unit to pool or destroy if pool is unavailable
	UFUNCTION()
	void DespawnUnit(AEnemyBaseController* Unit);

	// Tracks when the PC is in their counter state
	UFUNCTION()
	void PCCounterStart();
	UFUNCTION()
	void PCCounterStop();

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent)
	void OnPlayerWin();
	
private:
	// Called on begin play and reset
	void Setup();
	// Register the encounter object with each contained units
	void RegisterEncounterForUnits();
	// Register the encounter object with each contained spawn point
	void RegisterEncounterForSpawnPoints();
	// Engage contained units when encounter starts
	void EngageContainedUnits(AActor* Target);
	// Track whether the encounter has started yet
	unsigned int bIsEncounterActive:1;
	// Track if contained spawn points have begun spawning
	unsigned int bWaveStartedSpawning:1;
	// Track whether the encounter is complete yet
	unsigned int bIsEncounterComplete:1;

	bool AllSpawnsComplete() const;
	
	// Timer handle for basic attack queue
	FTimerHandle BasicQueueTimer, HeavyQueueTimer;
	// Timer handle to begin span after elapsed time
	FTimerHandle SpawnStartTimer;

	// Cache reference to world timer manager
	static FTimerManager* WorldTimerManager;
	void TryCacheTimerManager() const;
	// Start timer for queues
	void StartQueueTimer(bool bIsBasic = true);
	FTimerDelegate BasicQueueDelegate, HeavyQueueDelegate;
	TArray<AEnemyBaseController*>* GetAttackQueue(bool bIsBasic = true);
	void TriggerNextWave();
	void ResetSpawnPoints() const;

	int8 TotalUnits, DefeatedUnits, CurrentWave;
	unsigned int bIsPCCountering:1;
	AEnemyBaseController* LastAttackerBasic, *LastAttackerHeavy;
	class APlayerCharacter* EncounterTarget;
	FWaveParams* CurrentWaveParams;
};


