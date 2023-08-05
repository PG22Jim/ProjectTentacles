// Copyright (C) The Tentacle Zone 2023. All Rights Reserved.


#include "Encounter/EncounterVolume.h"

#include "NavigationInvokerComponent.h"
#include "NavigationSystem.h"
#include "ProjectTentacleGameInstance.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Characters/Enemies/EnemyBase.h"
#include "Characters/Enemies/EnemyBaseController.h"
#include "Characters/Enemies/UnitPool.h"
#include "Characters/Player/PlayerCharacter.h"
#include "Encounter/SpawnPoint.h"
#include "Kismet/GameplayStatics.h"

FTimerManager* AEncounterVolume::WorldTimerManager = nullptr;
// Sets default values
AEncounterVolume::AEncounterVolume()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	NavInvoker = CreateDefaultSubobject<UNavigationInvokerComponent>(TEXT("Nav Invoker"));
	NavInvoker->SetGenerationRadii(1000.f, 1000.f);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Encounter Root"));
	Root->SetMobility(EComponentMobility::Static);
	SetRootComponent(Root);

	CollisionMesh = CreateDefaultSubobject<UBoxComponent>(TEXT("Box Collision"));
	CollisionMesh->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);

	CollisionMesh->OnComponentBeginOverlap.AddDynamic(this, &AEncounterVolume::OnActorHitTriggerCollision);
}


void AEncounterVolume::TryTriggerEncounter(AActor* Target)
{
	// Early return if encounter has already begun or player hasn't enter encounter
	if(bIsEncounterActive || bIsEncounterComplete || !IsPlayerInsideEncounter) return;
	UpdateTutorial();
	
	bIsEncounterActive = true;
	EngageContainedUnits(Target);
	TriggerNextWave();
}

// Gets a list of units in the encounter apart from the specified pawn
TArray<AEnemyBase*> AEncounterVolume::GetAlliesForPawn(APawn* Pawn)
{
	TArray<AEnemyBase*> AlliesForPawn;
	for(AEnemyBase* ContainedUnit : ContainedUnits)
	{
		if(ContainedUnit == Pawn) continue;
		AlliesForPawn.Add(ContainedUnit);
	}

	return AlliesForPawn;
}

// Adds the next unit to the attack queue
void AEncounterVolume::RegisterOnAttackQueue(AEnemyBaseController* RegisteringController)
{
	TArray<AEnemyBaseController*>* AttackQueue = GetAttackQueue(RegisteringController->IsBasic());
	
	if(AttackQueue->Num() == 0)
	{
		// Start timer to handle the queue
		StartQueueTimer(RegisteringController->IsBasic());
	}

	AttackQueue->Add(RegisteringController);
}

// Tracks when the last unit finishes it's attack, and starts a queue timer for the next attacker
void AEncounterVolume::RegisterCompletedAttack(AEnemyBaseController* RegisteringController)
{
	(RegisteringController->IsBasic() ? LastAttackerBasic : LastAttackerHeavy) = RegisteringController;
	StartQueueTimer(RegisteringController->IsBasic());
}

// Removes references to a defeated unit and sets a timer to despawn the body
void AEncounterVolume::RegisterUnitDestroyed(AEnemyBaseController* Unit, bool bForceDespawn)
{
	// Remove units from queues and set
	TArray<AEnemyBaseController*>* AttackQueue = GetAttackQueue(Unit->IsBasic());
	if(AttackQueue->Contains(Unit)) AttackQueue->Remove(Unit);
	if(ContainedUnits.Contains(Unit->GetOwnPawn())) ContainedUnits.Remove(Unit->GetOwnPawn());

	// Check if the encounter is complete
	if(ContainedUnits.Num() <= 0 && AllSpawnsComplete())
	{
		bIsEncounterComplete = true;
		if(EncounterComplete.IsBound())
		{
			EncounterComplete.Broadcast();
			if(UWorld* World = GetWorld())
			{
				ACharacter* PlayerCha = UGameplayStatics::GetPlayerCharacter(World, 0);
				if(!PlayerCha) return;

				// check if owner class has character action interface
				if(PlayerCha->GetClass()->ImplementsInterface(UCharacterActionInterface::StaticClass()))
				{
					// if it has character action interface, it means its base character, execute its SwitchToIdleState function
					ICharacterActionInterface::Execute_OnEnterOrExitCombat(PlayerCha, false);
				}
			}

		}
	}

	if(bForceDespawn)
	{
		DespawnUnit(Unit);	
	} else
	{
		FTimerHandle DespawnHandle;
		FTimerDelegate DespawnDelegate;
		DespawnDelegate.BindUFunction(this, FName("DespawnUnit"), Unit);
		WorldTimerManager->SetTimer(DespawnHandle, DespawnDelegate, DespawnTimer, false);
	}
	
	// Check if spawn has started yet
	if(bWaveStartedSpawning) return;
	// If not
	++DefeatedUnits;
	
	// Check if should start
	if(!CurrentWaveParams) return;
	
	const float CompletionPercentage = TotalUnits == 0.f ? TotalUnits : static_cast<float>(DefeatedUnits) / static_cast<float>(TotalUnits);
	if(CompletionPercentage >= CurrentWaveParams->SpawnStartEncounterCompletionPercent / 100.f)
	{
		TryCacheTimerManager();
		WorldTimerManager->ClearTimer(SpawnStartTimer);
		StartSpawn();
	}
}

// Register newly spawned units on the encounter and update their target
void AEncounterVolume::AddSpawnedUnitToEncounter(AEnemyBase* Unit)
{
	ContainedUnits.Add(Unit);
	Unit->RegisterOwningEncounter(this);
	if(bIsEncounterActive)
	{
		Unit->EngageTarget(EncounterTarget);
	}
}

void AEncounterVolume::RemoveDeadUnitFromEncounter(AEnemyBaseController* DeadUnit)
{
	// Remove units from queues and set
	if(AttackQueueBasic.Contains(DeadUnit)) AttackQueueBasic.Remove(DeadUnit);
	if(AttackQueueHeavy.Contains(DeadUnit)) AttackQueueHeavy.Remove(DeadUnit);
	if(ContainedUnits.Contains(DeadUnit->GetOwnPawn())) ContainedUnits.Remove(DeadUnit->GetOwnPawn());
}

// Called when the game starts or when spawned
void AEncounterVolume::BeginPlay()
{
	Super::BeginPlay();
	Setup();
}

void AEncounterVolume::OnActorHitTriggerCollision(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// make collision ignore if player is already in the encounter
	if(IsPlayerInsideEncounter)
	{
		CollisionMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
		return;
	}
	
	APlayerCharacter* CastPlayer = Cast<APlayerCharacter>(OtherActor);
	
	if(!CastPlayer) return;

	CastPlayer->OnEnterOrExitCombat_Implementation(true);
	IsPlayerInsideEncounter = true;
	CollisionMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
}

void AEncounterVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if(EncounterTarget)
	{
		EncounterTarget->OnCounterStart.Unbind();
		EncounterTarget->OnCounterStart.Unbind();
	}
	Super::EndPlay(EndPlayReason);
}

// Determine which unit gets an attack ticket and triggers the next attack
void AEncounterVolume::BeginAttack(bool bIsBasic)
{
	// Skip if the PC is countering
	if(bIsPCCountering) {
		StartQueueTimer(bIsBasic);
		return;
	}
	// Try to make sure the same unit doesn't keep attacking if there is more than one unit
	TArray<AEnemyBaseController*>* AttackQueue = GetAttackQueue(bIsBasic);
	const int8 QueueSize = AttackQueue->Num();
	if (QueueSize == 0) return;

	// Check if unit is stunned
	if (QueueSize == 1 && Cast<AEnemyBase>((*AttackQueue)[0]->GetPawn())->GetCurrentEnemyState() == EEnemyCurrentState::Stunned) StartQueueTimer(bIsBasic);
	
	int8 RandomIndex = FMath::RandRange(0, QueueSize - 1);

	while(QueueSize > 1 && ((bIsBasic ? LastAttackerBasic : LastAttackerHeavy) == (*AttackQueue)[RandomIndex] || Cast<AEnemyBase>((*AttackQueue)[RandomIndex]->GetPawn())->GetCurrentEnemyState() == EEnemyCurrentState::Stunned))
	{
		RandomIndex = FMath::RandRange(0, QueueSize - 1);
	}
	
	if((*AttackQueue)[RandomIndex]) (*AttackQueue)[RandomIndex]->BeginAttack();
	AttackQueue->RemoveAt(RandomIndex);
}

// Start spawning from spawn points associated with the current wave, then update the wave count
void AEncounterVolume::StartSpawn()
{
	if(bWaveStartedSpawning) return;
	bWaveStartedSpawning = true;
	if (CurrentWaveParams)
	{
		// Reset Spawn Points
		ResetSpawnPoints();
	}

	for(ASpawnPoint* SpawnPoint : CurrentWaveParams->ContainedSpawnPoints)
	{
		if(!SpawnPoint) continue;
		SpawnPoint->SetUnitPool(UnitPool);
		SpawnPoint->StartSpawningUnits();
		TotalUnits += SpawnPoint->GetNumToSpawn();
	}
	TriggerNextWave();
}

void AEncounterVolume::DespawnUnit(AEnemyBaseController* Unit)
{
	if(!UnitPool)
	{
		Unit->GetOwnPawn()->Destroy();
		return;
	}
	UnitPool->AddUnitToPool(Unit->GetOwnPawn());
}

void AEncounterVolume::PCCounterStart()
{
	bIsPCCountering = true;
}

void AEncounterVolume::PCCounterStop()
{
	bIsPCCountering = false;
}

void AEncounterVolume::Setup()
{
	bIsPCCountering = false;
	WorldTimerManager = &GetWorldTimerManager();
	bIsEncounterActive = false;
	bIsEncounterComplete = false;
	LastAttackerBasic = LastAttackerHeavy = nullptr;
	BasicQueueDelegate.BindUFunction(this, FName("BeginAttack"));
	HeavyQueueDelegate.BindUFunction(this, FName("BeginAttack"), false);
	EncounterTarget = nullptr;
	CurrentWaveParams = nullptr;
	TotalUnits = ContainedUnits.Num();
	CurrentWave = -1;
	RegisterEncounterForUnits();
	Cast<UProjectTentacleGameInstance>(GetGameInstance())->RegisterEncounterVolume(this);
}

// for loop to send all enemy to reposition
void AEncounterVolume::SendAllEnemyToReposition(bool DoesIncludeHeavy)
{
	for(const AEnemyBaseController* EachEnemyBaseController: AttackQueueBasic)
	{
		const AEnemyBase* EachOwnPawnRef = EachEnemyBaseController->GetOwnPawn();

		if(!EachOwnPawnRef) continue; 
		
		UBlackboardComponent* EachOwnBBComp = EachOwnPawnRef->GetBBComponent();
		if(!EachOwnBBComp) continue; 

		FName BBRepositionBoolName = "bNeedToReposition";
		
		EachOwnBBComp->SetValueAsBool(BBRepositionBoolName, true);

		// Clear the circling state if they need to reposition
		EachOwnBBComp->ClearValue("bShouldCircle");
	}
}

void AEncounterVolume::AssignQueueEnemyToReposition_Implementation(bool DoesIncludeHeavy)
{
	IEncounterVolumeInterface::AssignQueueEnemyToReposition_Implementation(DoesIncludeHeavy);

	SendAllEnemyToReposition(DoesIncludeHeavy);
}

// Marks the encounter as completed (Used after reloading from a save)
void AEncounterVolume::MarkComplete()
{
	bIsEncounterComplete = true;
	for(AEnemyBase* Unit: ContainedUnits)
	{
		ContainedUnits.Remove(Unit);
		Unit->OnDeath();
	}
}

bool AEncounterVolume::IsComplete() const
{
	return bIsEncounterComplete;
}

bool AEncounterVolume::IsActive() const
{
	return bIsEncounterActive;
}

// Debug to kill active units
void AEncounterVolume::KillUnits()
{
	ResetSpawnPoints();
	for(AEnemyBase* ContainedUnit : ContainedUnits)
	{
		if(!ContainedUnit) continue;
		ContainedUnit->OnDeath();
	}
}

float AEncounterVolume::GetEncounterStartDelay() const
{
	return EncounterStartDelay;
}

// Register this encounter with contained units
void AEncounterVolume::RegisterEncounterForUnits()
{
	for(AEnemyBase* ContainedUnit : ContainedUnits)
	{
		if(!ContainedUnit) continue;
		ContainedUnit->RegisterOwningEncounter(this);
	}
}

// Register the encounter object with each contained spawn point
void AEncounterVolume::RegisterEncounterForSpawnPoints()
{
	for(ASpawnPoint* SpawnPoint : CurrentWaveParams->ContainedSpawnPoints)
	{
		if(!SpawnPoint) continue;
		SpawnPoint->RegisterOwningEncounter(this);
	}
}

// Trigger for contained units to engage the target
void AEncounterVolume::EngageContainedUnits(AActor* Target)
{
	EncounterTarget = Cast<APlayerCharacter>(Target);
	if(EncounterTarget)
	{
		EncounterTarget->OnCounterStart.BindDynamic(this, &AEncounterVolume::PCCounterStart);
		EncounterTarget->OnCounterStop.BindDynamic(this, &AEncounterVolume::PCCounterStop);
	}
	
	for(AEnemyBase* ContainedUnit : ContainedUnits)
	{
		ContainedUnit->EngageTarget(Target);
	}
}

// Check if all spawn points have completed
bool AEncounterVolume::AllSpawnsComplete() const
{
	bool bAllComplete = true;

	for(int8 i = 0; i < WaveParameters.Num(); ++i)
	{
		const FWaveParams* WaveParams = &WaveParameters[i];
		for(const ASpawnPoint* SpawnPoint : WaveParams->ContainedSpawnPoints)
		{
			if(!SpawnPoint) continue;
			bAllComplete = bAllComplete && SpawnPoint->IsSpawningComplete();
		}
	}

	return bAllComplete;
}

void AEncounterVolume::TryCacheTimerManager() const
{
	if(WorldTimerManager) return;
	
	WorldTimerManager = &GetWorldTimerManager();
}

void AEncounterVolume::StartQueueTimer(bool bIsBasic)
{
	TryCacheTimerManager();
	bIsBasic ? WorldTimerManager->SetTimer(BasicQueueTimer, BasicQueueDelegate, AttackStartDelayBasic, false, AttackStartDelayBasic)
	: WorldTimerManager->SetTimer(HeavyQueueTimer, HeavyQueueDelegate, AttackStartDelayHeavy, false, AttackStartDelayHeavy);
}

TArray<AEnemyBaseController*>* AEncounterVolume::GetAttackQueue(bool bIsBasic)
{
	return bIsBasic ? &AttackQueueBasic : &AttackQueueHeavy;
}

void AEncounterVolume::TriggerNextWave()
{
	// Increment wave number
	++CurrentWave;
	if(CurrentWave >= WaveParameters.Num())
	{
		CurrentWaveParams = nullptr;
		return;
	}
	CurrentWaveParams = &WaveParameters[CurrentWave];
	if(!CurrentWaveParams) return;
	bWaveStartedSpawning = false;
	// Register Spawn Points
	RegisterEncounterForSpawnPoints();
	// Start timer
	TryCacheTimerManager();
	WorldTimerManager->SetTimer(SpawnStartTimer, this, &AEncounterVolume::StartSpawn, CurrentWaveParams->SpawnStartTime, false, CurrentWaveParams->SpawnStartTime);
	// Check percentage based start
}

void AEncounterVolume::ResetSpawnPoints() const
{
	if (!CurrentWaveParams) return;
	for(ASpawnPoint* SpawnPoint : CurrentWaveParams->ContainedSpawnPoints)
	{
		if(!SpawnPoint) continue;
		SpawnPoint->StopSpawningUnits();
	}
}
