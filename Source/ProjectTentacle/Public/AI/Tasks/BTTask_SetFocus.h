// Copyright (C) The Tentacle Zone 2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_SetFocus.generated.h"

/**
 * TickTask function that never completes, need to be aborted by decorator
 * TODO: Needs to timeout at some point, otherwise there's a danger of an endless wait
 */
UCLASS()
class PROJECTTENTACLE_API UBTTask_SetFocus : public UBTTaskNode
{
	GENERATED_BODY()
public:
	UBTTask_SetFocus();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Precision;
	
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	
	// static float CalculateFlatAngle(const FVector& VectorA, const FVector& VectorB);
};