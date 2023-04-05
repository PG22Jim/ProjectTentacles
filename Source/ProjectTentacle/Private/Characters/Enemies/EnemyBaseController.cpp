// Copyright (C) The Tentacle Zone 2023. All Rights Reserved.


#include "Characters/Enemies/EnemyBaseController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Characters/Enemies/EncounterVolume.h"
#include "Characters/Enemies/EnemyBase.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"


AEnemyBaseController::AEnemyBaseController()
{
	bAttachToPawn = true;
	Blackboard = CreateDefaultSubobject<UBlackboardComponent>(TEXT("Blackboard"));

	// Set up perception system and sight sense
	Perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));
	Sight = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("Sight Config"));
	
	Sight->DetectionByAffiliation.bDetectEnemies = true;
	Sight->DetectionByAffiliation.bDetectFriendlies = false;
	Sight->DetectionByAffiliation.bDetectNeutrals = false;
	
	Perception->SetDominantSense(UAISense_Sight::StaticClass());
	Perception->ConfigureSense(*Sight);

	// Configure to team 2
	SetGenericTeamId(FGenericTeamId(2));
}

void AEnemyBaseController::BeginPlay()
{
	Super::BeginPlay();
	EncounterTarget = nullptr;
	OwnPawn = Cast<AEnemyBase>(GetPawn());
	if(!BehaviorTree) return;

	// Init blackboard and run master behaviour
	Blackboard->InitializeBlackboard(*BehaviorTree->BlackboardAsset);
	RunBehaviorTree(BehaviorTree);

	// Bind UFunction to perception updated delegate
	Perception->OnTargetPerceptionUpdated.AddDynamic(this, &AEnemyBaseController::UpdatePerception);
}

void AEnemyBaseController::RegisterOwningEncounter(AEncounterVolume* NewOwningEncounter)
{
	OwningEncounter = NewOwningEncounter;
}

// Engage target and disable further perception
void AEnemyBaseController::EngageTarget(AActor* Target)
{
	Blackboard->SetValueAsBool("bIsEncounterActive", true);
	Blackboard->SetValueAsObject("Target", Target);
	EncounterTarget = Target;
	Perception->SetSenseEnabled(UAISense_Sight::StaticClass(), false);
}

TArray<AEnemyBase*> AEnemyBaseController::GetAllies() const
{
	return OwningEncounter->GetAlliesForPawn(OwnPawn);
}

// Add unit to attack queue
void AEnemyBaseController::RegisterOnAttackQueue()
{
	OwningEncounter->RegisterOnBasicAttackQueue(this);
}

// Trigger attack state
void AEnemyBaseController::BeginAttack()
{
	// Begin attack montage here
	GEngine->AddOnScreenDebugMessage(INDEX_NONE, 1.0f, FColor::Purple, FString::Printf(TEXT("%s began attack"), *OwnPawn->GetHumanReadableName()));
	MoveToActor(EncounterTarget);
	GetBlackboardComponent()->SetValueAsBool("bIsAttacking", true);
}

// Register after attack has completed
void AEnemyBaseController::RegisterCompletedAttack()
{
	OwningEncounter->RegisterCompletedBasicAttack(this);
}

AEnemyBase* AEnemyBaseController::GetOwnPawn()
{
	return OwnPawn;
}

void AEnemyBaseController::UpdatePerception(AActor* Actor, FAIStimulus Stimulus)
{
	GEngine->AddOnScreenDebugMessage(INDEX_NONE, 1.0f, FColor::Purple, Actor->GetHumanReadableName());
	if(!OwningEncounter) return;
	OwningEncounter->TryTriggerEncounter(Actor);
}