// Copyright (C) The Tentacle Zone 2023. All Rights Reserved.


#include "AI/Tasks/BTTask_StrafeTarget.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "Characters/Enemies/EnemyBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"


UBTTask_StrafeTarget::UBTTask_StrafeTarget()
{
	NodeName = "Strafe Target";
	
	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_StrafeTarget, TargetKey), AActor::StaticClass());
	TargetKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_StrafeTarget, TargetKey));
	CircleDirectionKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_StrafeTarget, CircleDirectionKey));
}

void UBTTask_StrafeTarget::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);

	const UBlackboardData* BBAsset = GetBlackboardAsset();
	if (BBAsset)
	{
		TargetKey.ResolveSelectedKey(*BBAsset);
	}
	else
	{
		UE_LOG(LogBehaviorTree, Warning, TEXT("Can't initialize task: %s, make sure that behavior tree specifies blackboard asset!"), *GetName());
	}
}

EBTNodeResult::Type UBTTask_StrafeTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// Resolve Target
	bIsObject = TargetKey.SelectedKeyType == UBlackboardKeyType_Object::StaticClass();
	const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	bShouldMoveRight = Blackboard->GetValueAsBool(CircleDirectionKey.SelectedKeyName);
	OwnController = OwnerComp.GetAIOwner();
	OwnPawn = Cast<AEnemyBase>(OwnController->GetCharacter());
	if(bIsObject)
	{
		TargetActor = Cast<AActor>(Blackboard->GetValueAsObject(TargetKey.SelectedKeyName));
		if(!TargetActor) return EBTNodeResult::Failed;
	} else
	{
		TargetLocation = Blackboard->GetValueAsVector(TargetKey.SelectedKeyName);
		if(TargetLocation == FVector::ZeroVector) return EBTNodeResult::Failed;
	}
	StrafeAroundLocation(bIsObject ? TargetActor->GetActorLocation() : TargetLocation);

	// TODO Change direction if it hits something
	return EBTNodeResult::Succeeded;
}

void UBTTask_StrafeTarget::StrafeAroundLocation(const FVector Location) const
{
	const FVector CharLocation = OwnPawn->GetActorLocation();
	const FVector ForwardToTarget = Location - CharLocation;
	const FVector RightDirection = FVector::CrossProduct(ForwardToTarget, FVector::UpVector);
	const FVector Direction = StrafeDistance * (RightDirection * (bShouldMoveRight ? 1 : -1)).GetSafeNormal();
	OwnPawn->EnableStrafe();
	OwnController->MoveToLocation(CharLocation + Direction);
}
