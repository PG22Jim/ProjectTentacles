// Copyright (C) The Tentacle Zone 2023. All Rights Reserved.


#include "Characters/Player/PlayerActionComponent.h"

#include "NiagaraFunctionLibrary.h"
#include "Characters/Enemies/EnemyBase.h"
#include "Characters/Player/PlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Encounter/SwampWater.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

// Sets default values for this component's properties
UPlayerActionComponent::UPlayerActionComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}

void UPlayerActionComponent::StopSpecificMovingTimeline(EPlayerAttackAnimations CurrentAttackAnim)
{
	switch (CurrentAttackAnim)
	{
		case EPlayerAttackAnimations::TwoHandWidePush:
			TwoHandWidePushTimeLine.Stop();
			break;
		case EPlayerAttackAnimations::TwoHandPush:
			TwoHandPushTimeLine.Stop();
			break;
		case EPlayerAttackAnimations::TwoHandClap:
			TwoHandClapTimeLine.Stop();
			break;
		case EPlayerAttackAnimations::CloseRange: break;
		default: ;
	}
}

void UPlayerActionComponent::PauseComboResetTimer()
{
	const UWorld* World = GetWorld();
	if(!World) return;

	World->GetTimerManager().PauseTimer(ComboResetTimerHandle);
}

void UPlayerActionComponent::ResumeComboResetTimer()
{
	const UWorld* World = GetWorld();
	if(!World) return;

	World->GetTimerManager().UnPauseTimer(ComboResetTimerHandle);
}

void UPlayerActionComponent::ClearSpecialAbilityCDTimer()
{
	const UWorld* World = GetWorld();
	if(!World) return;

	World->GetTimerManager().ClearTimer(SpecialAbilityCDTimerHandle);
}

void UPlayerActionComponent::StartSpecialAbilityCDTimer()
{
	const UWorld* World = GetWorld();
	if(!World) return;

	World->GetTimerManager().SetTimer(SpecialAbilityCDTimerHandle,this, &UPlayerActionComponent::RestoreAbilitiesInTick, 0.05, true, -1);
}


// =================================================== Begin Functions =======================================================
void UPlayerActionComponent::BeginPlay()
{
	Super::BeginPlay();

	InitializeOwnerRef();
	// ...
	InitializeTimelineComp();
}

void UPlayerActionComponent::InitializeOwnerRef()
{
	AActor* OwnerActor = GetOwner();
	if(OwnerActor == nullptr) return;
	
	APlayerCharacter* CastedOwnerActor = Cast<APlayerCharacter>(OwnerActor);
	if(CastedOwnerActor == nullptr) return;

	PlayerOwnerRef = CastedOwnerActor;

	PlayerOwnerRef->OnExecutePlayerAction.BindDynamic(this, &UPlayerActionComponent::ExecutePlayerAction);
	PlayerOwnerRef->OnReceivingIncomingDamage.BindDynamic(this, &UPlayerActionComponent::ReceivingDamage);
	PlayerOwnerRef->OnTriggeringCounter.BindDynamic(this, &UPlayerActionComponent::TriggerCounterAttack);
	PlayerOwnerRef->OnEnteringPreCounterState.BindDynamic(this, &UPlayerActionComponent::TriggerPreCounter);
	PlayerOwnerRef->OnEnableComboResetTimer.BindDynamic(this, &UPlayerActionComponent::OnStartingComboResetTimer);
}

void UPlayerActionComponent::InitializeTimelineComp()
{
	FOnTimelineFloat MovingAttackPosUpdate;
	MovingAttackPosUpdate.BindDynamic(this, &UPlayerActionComponent::MovingAttackMovement);
	TwoHandWidePushTimeLine.AddInterpFloat(TwoHandWidePushCurve, MovingAttackPosUpdate);
	TwoHandPushTimeLine.AddInterpFloat(TwoHandPushCurve, MovingAttackPosUpdate);
	TwoHandClapTimeLine.AddInterpFloat(TwoHandClapCurve, MovingAttackPosUpdate);
	CloseToPerformFinisherTimeLine.AddInterpFloat(CloseToPerformFinisherCurve, MovingAttackPosUpdate);
	
	FOnTimelineFloat DodgingPosUpdate;
	FOnTimelineEventStatic onDodgeFinishedCallback;
	DodgingPosUpdate.BindDynamic(this, &UPlayerActionComponent::DodgeMovement);
	onDodgeFinishedCallback.BindUFunction(this, FName{ TEXT("SetCollisionBlockToEnemy") });
	DodgeLerpingTimeLine.AddInterpFloat(DodgeLerpingCurve, DodgingPosUpdate);
	DodgeLerpingTimeLine.SetTimelineFinishedFunc(onDodgeFinishedCallback);
	
	FOnTimelineFloat TurnRotatorUpdate;
	TurnRotatorUpdate.BindDynamic(this, &UPlayerActionComponent::TurnRotationUpdate);
	
	FOnTimelineEvent TurnRotatorFinish;
	TurnRotatorFinish.BindDynamic(this, &UPlayerActionComponent::EnterCounterAttackState);
	
	TurnRotationTimeline.AddInterpFloat(CounterRotationCurve, TurnRotatorUpdate);
	TurnRotationTimeline.SetTimelineFinishedFunc(TurnRotatorFinish);
	
}

// ======================================================== Tick ======================================================

void UPlayerActionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const float PlayerStoredInputX = PlayerOwnerRef->GetPlayerInputDir().GetInputDirectionX();
	const float PlayerStoredInputY = PlayerOwnerRef->GetPlayerInputDir().GetInputDirectionY();
	if(PlayerStoredInputX != 0.0f || PlayerStoredInputY != 0.0f)
		TryToUpdateTarget();

	// delta time will change due to player's combo time
	const float DeltaWithComboBonus = DeltaTime * PlayerOwnerRef->GetUpdatedAttackingSpeedBonus();
	TwoHandWidePushTimeLine.TickTimeline(DeltaWithComboBonus);
	TwoHandPushTimeLine.TickTimeline(DeltaWithComboBonus);
	TwoHandClapTimeLine.TickTimeline(DeltaWithComboBonus);
	CloseToPerformFinisherTimeLine.TickTimeline(DeltaWithComboBonus);
	DodgeLerpingTimeLine.TickTimeline(DeltaTime);
	TurnRotationTimeline.TickTimeline(DeltaTime);

}

// ====================================================== Attack ==============================================
void UPlayerActionComponent::BeginMeleeAttack()
{
	// Reference validation check
	if(PlayerOwnerRef == nullptr) return;

	// if player is not having target, return;
	AEnemyBase* RegisteredTarget = PlayerOwnerRef->GetTargetActor();
	if(PlayerOwnerRef->GetTargetActor() == nullptr) return;


	// Get Current target and player location for later use
	const FVector TargetActorPos = RegisteredTarget->GetActorLocation();
	
	// set lerping start and end position to variable
	MovingStartPos = PlayerOwnerRef->GetActorLocation();
	SetAttackMovementPositions(TargetActorPos);
	
	MakePlayerEnemyFaceEachOther(RegisteredTarget);
	
	const bool bIsClose = TargetDistanceCheck(RegisteredTarget);

	TArray<UAnimMontage*> ListOfMeleeMontages;
	
	if(!bIsClose)
	{
		ListOfMeleeMontages = MeleeAttackMontages;
		PerformMelee(RegisteredTarget, ListOfMeleeMontages, true);
		return;
	}

	ListOfMeleeMontages = CloseMeleeAttackMontages;
	PerformMelee(RegisteredTarget, ListOfMeleeMontages, false);
}

void UPlayerActionComponent::PerformMelee(AEnemyBase* RegisteredTarget, TArray<UAnimMontage*> ListOfMeleeMontages, bool IsLongRange)
{
	const int32 DecidedIndex = GetDifferentCloseMeleeMontage(ListOfMeleeMontages);

	if(DecidedIndex < 0 || DecidedIndex >= ListOfMeleeMontages.Num()) return;
	
	UAnimMontage* DecidedMontage = ListOfMeleeMontages[DecidedIndex];
	
	EPlayerAttackType SelectedAttackType = EPlayerAttackType::LongMeleeAttack;
	if(!IsLongRange)
		SelectedAttackType = EPlayerAttackType::ShortMeleeAttack;

	

	PlayerOwnerRef->SetCurrentAttackType(SelectedAttackType);

	const EPlayerAttackAnimations SelectedAttackAnimations = GetAttackAnimationByRndNum(DecidedIndex, IsLongRange);
	PlayerOwnerRef->SetCurrentAttackAnim(SelectedAttackAnimations);
	
	// change current action state enum
	PlayerOwnerRef->SetCurrentActionState(EActionState::BeforeAttack);

	
	// // Check if Enemy is dying or now, if is, finish him
	// int32 EnemyCurrentHealth = RegisteredTarget->GetEnemyHealth();
	
	// Set damaging actor and stop damage actor's movement
	PlayerOwnerRef->SetDamagingActor(RegisteredTarget);
	RegisteredTarget->TryStopMoving();

	PlayerOwnerRef->SetCurrentDamage(CurrentComboCount);
	ClearComboResetTimer();
	
	// Play attack montage
	CurrentPlayingMontage = DecidedMontage;
	const float CurrentComboSpeed = CalculateCurrentComboSpeed();
	PlayerOwnerRef->PlayAnimMontage(CurrentPlayingMontage, CurrentComboSpeed, "Default");

	// if it is long range attack, play long range animation moving timeline
	// Start attack movement timeline depends on the result of playing montage
	if(IsLongRange) StartAttackMovementTimeline(SelectedAttackAnimations);
	
	// combo count increment
	ComboCountIncrement();

	PlayerOwnerRef->SetUpdatedAttackingSpeedBonus(1 + (CurrentComboCount * ComboSpeedMultiplier));
}

int32 UPlayerActionComponent::GetDifferentCloseMeleeMontage(TArray<UAnimMontage*> ListOfMeleeMontages)
{
	const int32 MaxNumCloseMeleeMontages = ListOfMeleeMontages.Num();
	
	int32 RndIndex;

	do
	{
		RndIndex = UKismetMathLibrary::RandomIntegerInRange(0, MaxNumCloseMeleeMontages - 1);
	}
	while (LastMeleeMontage == ListOfMeleeMontages[RndIndex]);
	
	return RndIndex;
}

bool UPlayerActionComponent::TargetDistanceCheck(AEnemyBase* Target)
{
	const FVector CurrentPlayerPos = PlayerOwnerRef->GetActorLocation();
	const FVector CurrentTargetPos = Target->GetActorLocation();
	
	const float DistanceToTarget = UKismetMathLibrary::Vector_Distance(CurrentPlayerPos, CurrentTargetPos);

	return DistanceToTarget < MaxDistanceToBeClose;
}

void UPlayerActionComponent::ComboCountIncrement()
{
	CurrentComboCount++;
}

void UPlayerActionComponent::FinishEnemy()
{
	AEnemyBase* CurrentTarget = PlayerOwnerRef->GetTargetActor();
	if(CurrentTarget == nullptr) return;
	
	// Player attack montage
	CurrentPlayingMontage = FinisherAnimMontages;
	
	PlayerOwnerRef->PlayAnimMontage(CurrentPlayingMontage, 1, "Default");

	CurrentTarget->PlayFinishedAnimation();
	CloseToPerformFinisherTimeLine.PlayFromStart();
}

void UPlayerActionComponent::SetAttackMovementPositions(FVector TargetPos)
{

	// Get direction from target to player
	FVector OffsetWithoutZ = MovingStartPos - TargetPos;
	OffsetWithoutZ.Z = 0;
	const FVector DirFromTargetToPlayer = UKismetMathLibrary::Normal(OffsetWithoutZ);

	// Get lerp end position
	MovingEndPos = TargetPos + (DirFromTargetToPlayer * 150);
}


EPlayerAttackAnimations UPlayerActionComponent::GetAttackAnimationByRndNum(int32 RndNum, bool IsLongRange)
{
	if(!IsLongRange) return EPlayerAttackAnimations::CloseRange;
	
	return static_cast<EPlayerAttackAnimations>(RndNum);
}

void UPlayerActionComponent::StartAttackMovementTimeline(EPlayerAttackAnimations CurrentAttackAnim)
{
	switch (CurrentAttackAnim)
	{
		case EPlayerAttackAnimations::TwoHandWidePush:
			TwoHandWidePushTimeLine.PlayFromStart();
			break;
		case EPlayerAttackAnimations::TwoHandPush:
			TwoHandPushTimeLine.PlayFromStart();
			break;
		case EPlayerAttackAnimations::TwoHandClap:
			TwoHandClapTimeLine.PlayFromStart();
			break;
		case EPlayerAttackAnimations::CloseRange: break;
		default: ;
	}
}

float UPlayerActionComponent::CalculateCurrentComboSpeed()
{
	
	const float ExpectedComboSpeedBonus = static_cast<float>(CurrentComboCount) * ComboSpeedMultiplier;
	const float AdjustedSpeedBonus = UKismetMathLibrary::Clamp(ExpectedComboSpeedBonus, 0, MaxComboSpeedBonus);
	
	return 1 + AdjustedSpeedBonus;
}

void UPlayerActionComponent::OnStartingComboResetTimer()
{
	WaitToResetComboCount();
}

void UPlayerActionComponent::WaitToResetComboCount()
{
	const UWorld* World = GetWorld();
	if(!World) return;

	FTimerManager& WorldTimerManager = World->GetTimerManager();

	WorldTimerManager.SetTimer(ComboResetTimerHandle,this, &UPlayerActionComponent::ResetComboCount, ComboCountExistTime, false, -1);
}

// ====================================================== Evade ===============================================

void UPlayerActionComponent::ClearComboResetTimer()
{
	// Stop combo count reset timer handle
	const UWorld* World = GetWorld();
	if(!World) return;
	World->GetTimerManager().ClearTimer(ComboResetTimerHandle);
}

void UPlayerActionComponent::BeginEvade()
{
	// Set target countered to prevent clearing target
	AEnemyBase* CurrentStoredCounterTarget = PlayerOwnerRef->GetCounteringTarget();
	ICharacterActionInterface::Execute_OnSetIsCountered(CurrentStoredCounterTarget ,true);

	// Make previous targeted enemy able to move if player is attacking
	if(PlayerOwnerRef->GetCurrentActionState() == EActionState::Attack)
	{
		const EPlayerAttackAnimations CurrentRegisteredAttackAnim = PlayerOwnerRef->GetCurrentAttackAnim();
		StopSpecificMovingTimeline(CurrentRegisteredAttackAnim);

		// Make damaging actor resume moving
		AEnemyBase* ResumeMovementDamagingActor = PlayerOwnerRef->GetDamagingActor();
		if(ResumeMovementDamagingActor)
			ResumeMovementDamagingActor->TryResumeMoving();
	}	
	
	
	// Enter Pre Counter State
	EnterPreCounterState();

	


	
	// // if evade montage is null pointer, just return
	// if(EvadeAnimMontage == nullptr) return;
	//
	// PlayerOwnerRef->SetCurrentActionState(EActionState::Evade);
	//
	// CurrentPlayingMontage = EvadeAnimMontage;
	//
	// const int32 RndPerformIndex = UKismetMathLibrary::RandomIntegerInRange(0,1);
	//
	// if(RndPerformIndex == 0)
	// {
	// 	PlayerOwnerRef->PlayAnimMontage(CurrentPlayingMontage, 1, "DodgeRight");
	// 	return;
	// }
	//
	// PlayerOwnerRef->PlayAnimMontage(CurrentPlayingMontage, 1, "DodgeLeft");
}

// ================================================== Pre Counter State =================================================
void UPlayerActionComponent::EnterPreCounterState()
{
	PlayerOwnerRef->SetCurrentActionState(EActionState::SpecialAttack);

	// Get Start and End lerping Rotation Yaw
	StartTurnRotationZ = PlayerOwnerRef->GetActorRotation().Yaw;

	AEnemyBase* CurrentCounterTarget = PlayerOwnerRef->GetCounteringTarget();

	const FVector PlayerCurrentPos = PlayerOwnerRef->GetActorLocation();
	FVector CounterTargetPos = CurrentCounterTarget->GetActorLocation();
	CounterTargetPos.Z = PlayerCurrentPos.Z;

	const FRotator RotationToTarget = UKismetMathLibrary::FindLookAtRotation(PlayerCurrentPos, CounterTargetPos);
	EndTurnRotationZ = RotationToTarget.Yaw;

	// Get Start and End lerping positions
	CounterMoveStartPos = PlayerCurrentPos;
	CounterMoveEndPos = GetCounterPos(CurrentCounterTarget);
	
	// Decide which side should player turn
	const float AngleChange = EndTurnRotationZ - StartTurnRotationZ;

	const bool IsLeftTurn = AngleChange < 0;

	if(IsLeftTurn)
		PlayerOwnerRef->PlayAnimMontage(RotateAnimationLeft, 6.0f, "Default");
	else
		PlayerOwnerRef->PlayAnimMontage(RotateAnimationRight, 6.0f, "Default");

	TurnRotationTimeline.PlayFromStart();
}

void UPlayerActionComponent::EnterCounterAttackState()
{
	PlayerOwnerRef->StopAnimMontage();

	PlayerOwnerRef->SetCounteringTarget(PlayerOwnerRef->GetCounteringTarget());
	
	BeginCounterAttack();
}


// ================================================== Counter ======================================================
void UPlayerActionComponent::BeginCounterAttack()
{
	// ref validation check
	if(PlayerOwnerRef == nullptr) return;

	AEnemyBase* StoredCounterTarget = PlayerOwnerRef->GetCounteringTarget();
	if(!StoredCounterTarget) return;
	
	// if Dodge montage is null pointer, just return
	if(CounterAttackMontages == nullptr) return;
	
	PlayerOwnerRef->SetDamagingActor(StoredCounterTarget);
	
	PlayerOwnerRef->SetCurrentAttackType(EPlayerAttackType::CounterAttack);

	const EEnemyType TargetEnemyType = StoredCounterTarget->GetUnitType();

	if(TargetEnemyType != EEnemyType::Brute)
		CurrentPlayingMontage = CounterAttackMontages;
	else
		CurrentPlayingMontage = CounterBruteMontage;

	MakePlayerEnemyFaceEachOther(StoredCounterTarget);
	
	
	if(StoredCounterTarget->GetClass()->ImplementsInterface(UCharacterActionInterface::StaticClass()))
	{
		ICharacterActionInterface::Execute_OnStartCounteredAnimation(StoredCounterTarget);
	}


		
	PlayerOwnerRef->PlayAnimMontage(CurrentPlayingMontage, 1, "Start");
	PlayerOwnerRef->OnCounterStart.ExecuteIfBound();
		
	ComboCountIncrement();

	// Stop Combo Count timer
	ClearComboResetTimer();

	// Clear counter target reference and set counterable bool to false
	AEnemyBase* DeletingCounterTargetRef = PlayerOwnerRef->GetCounteringTarget();
	PlayerOwnerRef->ClearCounteringTarget(DeletingCounterTargetRef);
	PlayerOwnerRef->TryTurnCounterCapable(false);
}

FVector UPlayerActionComponent::GetCounterPos(AEnemyBase* CounterVictim)
{
	// // Hit result
	// FHitResult Hit;
	// // Empty array of ignoring actor, add CounterVictim and PlayerRef to be ignored
	// TArray<AActor*> IgnoreActors;
	// IgnoreActors.Add(CounterVictim);
	// IgnoreActors.Add(PlayerOwnerRef);
	//
	const FVector StartPos = CounterVictim->GetActorLocation();
	const FVector EndPos = StartPos + (CounterVictim->GetActorForwardVector() * 90);
	//
	// // Line trace by channel from Kismet System Library 
	// const bool bHit = UKismetSystemLibrary::LineTraceSingle(this, StartPos, EndPos, UEngineTypes::ConvertToTraceType(ECC_Visibility), false, IgnoreActors, EDrawDebugTrace::None,Hit,true);
	//
	//

	return EndPos;
}

void UPlayerActionComponent::TurnRotationUpdate(float Alpha)
{
	
	const FRotator CurrentPlayerRotation = PlayerOwnerRef->GetActorRotation();
	const float CurrentRotatorYaw = UKismetMathLibrary::Lerp(StartTurnRotationZ, EndTurnRotationZ, Alpha);
	const FRotator NewRotation = FRotator(CurrentPlayerRotation.Pitch,CurrentRotatorYaw,CurrentPlayerRotation.Roll);
	
	PlayerOwnerRef->SetActorRotation(NewRotation);

	const FVector NewPos = UKismetMathLibrary::VLerp(CounterMoveStartPos, CounterMoveEndPos, Alpha);
	PlayerOwnerRef->SetActorLocation(NewPos);
}



// ==================================================== Dodge ===============================================

void UPlayerActionComponent::BeginDodge()
{
	// ref validation check
	if(PlayerOwnerRef == nullptr) return;

	// if player's current action state is before attacking, cancel movement lerping, clear target, and reset target's movement mode
	if(PlayerOwnerRef->GetCurrentActionState() == EActionState::BeforeAttack)
	{
		// Stop timeline movement
		StopSpecificMovingTimeline(PlayerOwnerRef->GetCurrentAttackAnim());

		// Reset target's movement mode to walking
		AEnemyBase* AllocatedDamageActor = PlayerOwnerRef->GetDamagingActor();
		AllocatedDamageActor->TryResumeMoving();
	}
	
	
	// Get Player facing direction
	const FVector PlayerFaceDir = PlayerOwnerRef->GetActorForwardVector();

	// Get dodging direction
	const FVector PlayerDodgingDir = DecideDodgingDirection(PlayerFaceDir);

	// Get dodging montage depends on dodging direction
	UAnimMontage* PlayerDodgingMontage = FrontRollingMontage;
	InstantRotation(PlayerDodgingDir);
	//UAnimMontage* PlayerDodgingMontage = DecideDodgingMontage(PlayerDodgingDir);

	// Set action state and playing montage to dodging
	PlayerOwnerRef->SetCurrentActionState(EActionState::Dodge);
	CurrentPlayingMontage = PlayerDodgingMontage;

	// Set moving locations
	const FVector DodgeStart = PlayerOwnerRef->GetActorLocation();
	const FVector DodgingDest = DodgeStart + (PlayerDodgingDir * DodgeDistance);
	MovingStartPos = DodgeStart;
	MovingEndPos = DodgingDest;

	

	// Play both lerping timeline and montage
	PlayerOwnerRef->PlayAnimMontage(CurrentPlayingMontage, 1 , NAME_None);
	SetCollisionIgnoreToEnemy();
	DodgeLerpingTimeLine.PlayFromStart();
}

FVector UPlayerActionComponent::DecideDodgingDirection(FVector PlayerFaceDir)
{
	const FInputDirection OwnerInputDirection = PlayerOwnerRef->GetPlayerInputDir();
	
	// if input direction is both 0, it means player didn't 
	if(OwnerInputDirection.GetInputDirectionX() == 0 && OwnerInputDirection.GetInputDirectionY() == 0)
	{
		const FVector PlayerBackDir = -1 * PlayerFaceDir;
		return PlayerBackDir;
	}
	
	// Get input direction
	const FVector SelfPos = PlayerOwnerRef->GetActorLocation();
	const FRotator Rotation = PlayerOwnerRef->Controller->GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);
	const FVector RightX = UKismetMathLibrary::GetRightVector(YawRotation);
	const FVector ForwardY = UKismetMathLibrary::GetForwardVector(YawRotation);
	const FVector RightInputDir = RightX * OwnerInputDirection.GetInputDirectionX();
	const FVector ForwardInputDir = ForwardY * OwnerInputDirection.GetInputDirectionY();
	const FVector InputDest = SelfPos + ((RightInputDir * 50) + (ForwardInputDir * 50));
	const FVector SelfToInputDestDir = UKismetMathLibrary::GetDirectionUnitVector(SelfPos, InputDest);

	return SelfToInputDestDir;
}

UAnimMontage* UPlayerActionComponent::DecideDodgingMontage(FVector PlayerDodgingDirection)
{
	// Get Player facing direction
	const FVector PlayerFaceDir = PlayerOwnerRef->GetActorForwardVector();
	
	const float DotProduct = UKismetMathLibrary::Dot_VectorVector(PlayerFaceDir, PlayerDodgingDirection);

	// Dot product is negative = player is dodging backward
	if(DotProduct < 0)
	{
		InstantRotation((-1 * PlayerDodgingDirection));
		return BackFlipMontage;
	}

	InstantRotation(PlayerDodgingDirection);
	return FrontRollingMontage;
}

// =============================================== Special Ability ===================================================

void UPlayerActionComponent::ExecuteSpecialAbility(int32 AbilityIndex)
{
	switch (AbilityIndex)
	{
		case 1:
			if(CurrentSpecialMeter1 < MaxSpecialMeter) break;
			ResetAbilityMeters();
			ClearSpecialAbilityCDTimer();
			SpawnStunTentacle();
			StartSpecialAbilityCDTimer();
			break;
		case 2:
			if(CurrentSpecialMeter2 < MaxSpecialMeter) break;
			ResetAbilityMeters();
			ClearSpecialAbilityCDTimer();
			SpawnAttackingTentacle();
			StartSpecialAbilityCDTimer();
			break;
		// case 3:
		// 	if(CurrentSpecialMeter3 < MaxSpecialMeter) break;
		// 	ResetAbilityMeters();
		// 	ClearSpecialAbilityCDTimer();
		// 	StartSpecialAbilityCDTimer();
		// 	break;
		default: break;
	}
}

void UPlayerActionComponent::ResetAbilityMeters()
{
	CurrentSpecialMeter1 = 0;
	CurrentSpecialMeter2 = 0;
	// CurrentSpecialMeter3 = 0;
}

void UPlayerActionComponent::RestoreAbilitiesInTick()
{
	const float Special1RegenPerTick = (MaxSpecialMeter / SpecialAbilityCooldown1) * 0.05f; 
	const float Special2RegenPerTick = (MaxSpecialMeter / SpecialAbilityCooldown2) * 0.05f; 
	const float Special3RegenPerTick = (MaxSpecialMeter / SpecialAbilityCooldown3) * 0.05f;

	CurrentSpecialMeter1 = UKismetMathLibrary::FClamp( CurrentSpecialMeter1 + Special1RegenPerTick, 0, MaxSpecialMeter);
	CurrentSpecialMeter2 = UKismetMathLibrary::FClamp( CurrentSpecialMeter2 + Special2RegenPerTick, 0, MaxSpecialMeter);
	// CurrentSpecialMeter3 = UKismetMathLibrary::FClamp( CurrentSpecialMeter3 + Special3RegenPerTick, 0, MaxSpecialMeter);

	if(CurrentSpecialMeter1 >= MaxSpecialMeter && CurrentSpecialMeter2 >= MaxSpecialMeter)
		ClearSpecialAbilityCDTimer();
	
}

void UPlayerActionComponent::SpawnStunTentacle()
{
	if(UWorld* World = GetWorld())
	{
		// set spawn parameter
		FActorSpawnParameters StunTentacleParams;
		StunTentacleParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		// Get ground location by using line trace
		FVector SpawnLocation = PlayerOwnerRef->GetActorLocation();
		const FVector TraceEndLocation = SpawnLocation + ((-1 * PlayerOwnerRef->GetActorUpVector()) * 500.0f);
		
		FHitResult Hit;
		TArray<AActor*> IgnoreActors;
		IgnoreActors.Add(PlayerOwnerRef);
		const bool IsHit = UKismetSystemLibrary::LineTraceSingle(World, SpawnLocation, TraceEndLocation, UEngineTypes::ConvertToTraceType(ECC_Camera), false, IgnoreActors, EDrawDebugTrace::None,Hit,true);

		if(IsHit)
			SpawnLocation = Hit.ImpactPoint;
		else
			SpawnLocation = Hit.TraceEnd;
			
		
		
		// spawn stun tentacle
		AStunTentacle* SpawnTentacle = World->SpawnActor<AStunTentacle>(StunTentacleClass, SpawnLocation, {0,0,0},StunTentacleParams);
	}
}

void UPlayerActionComponent::SpawnAttackingTentacle()
{
	if(UWorld* World = GetWorld())
	{
		// set spawn parameter
		FActorSpawnParameters AttackTentacleParams;
		AttackTentacleParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		// Get ground location by using line trace
		FVector SpawnLocation = PlayerOwnerRef->GetActorLocation();
		const FVector TraceEndLocation = SpawnLocation + ((-1 * PlayerOwnerRef->GetActorUpVector()) * 500.0f);
		
		FHitResult Hit;
		TArray<AActor*> IgnoreActors;
		IgnoreActors.Add(PlayerOwnerRef);
		const bool IsHit = UKismetSystemLibrary::LineTraceSingle(World, SpawnLocation, TraceEndLocation, UEngineTypes::ConvertToTraceType(ECC_Camera), false, IgnoreActors, EDrawDebugTrace::None,Hit,true);

		if(IsHit)
			SpawnLocation = Hit.ImpactPoint;
		else
			SpawnLocation = Hit.TraceEnd;
			
		
		
		// spawn stun tentacle
		AAttackingTentacle* SpawnTentacle = World->SpawnActor<AAttackingTentacle>(AttackTentacleClass, SpawnLocation, {0,0,0},AttackTentacleParams);
	}
}


// ==================================================== Utility ===============================================

void UPlayerActionComponent::TryToUpdateTarget()
{
	// Get All enemy around player
	TArray<AEnemyBase*> OpponentAroundSelf = GetAllOpponentAroundSelf();
	
	// if there is no opponent around, simply return
	if(OpponentAroundSelf.Num() == 0) return;
	
	// Get target direction to face to
	AEnemyBase* ResultFacingEnemy = GetTargetEnemy(OpponentAroundSelf);
	
	// if there is no direction, return
	if(ResultFacingEnemy == nullptr) return;
	
	// if result is not saved target enemy, update it for later usage
	if(ResultFacingEnemy && PlayerOwnerRef->GetTargetActor() != ResultFacingEnemy)
		PlayerOwnerRef->SetTargetActor(ResultFacingEnemy);
}

TArray<AEnemyBase*> UPlayerActionComponent::GetAllOpponentAroundSelf()
{
	TArray<AActor*> FoundActorList;
	TArray<AEnemyBase*> ReturnActors;
	
	const UWorld* World = GetWorld();
	if(World == nullptr) return ReturnActors;

	const FVector SelfPos = PlayerOwnerRef->GetActorLocation();

	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(PlayerOwnerRef);
	
	UKismetSystemLibrary::SphereOverlapActors(World,SelfPos, DetectionRange, FilterType, FilteringClass, IgnoreActors,FoundActorList);

	// if found enemies is not zero, there are enemy in range
	if(FoundActorList.Num() != 0)
	{
		for (AActor* EachFoundActor : FoundActorList)
		{
			AEnemyBase* FoundCharacter = Cast<AEnemyBase>(EachFoundActor);
			if(FoundCharacter != nullptr)
			{
				const bool IsBlocked = IsObjectBlocking(FoundCharacter, SelfPos);
				if(IsEnemyTargetable(FoundCharacter) && !IsBlocked)
					ReturnActors.Add(FoundCharacter);
			}
		}
	}
	
	return ReturnActors;
}

bool UPlayerActionComponent::IsEnemyTargetable(AEnemyBase* TargetEnemy)
{
	// first, check if enemy is dead, if dead, return false, player cannot target dead enemy
	const bool IsEnemyDead = TargetEnemy->GetIsDead();
	
	if(IsEnemyDead) return false;
	
	// check if enemy is brute, if enemy is brute, return true, no need to check if brute is stunned or not
	const bool IsEnemyBrute = TargetEnemy->GetUnitType() == EEnemyType::Brute;
	if(IsEnemyBrute) return true;
	
	// if enemy is not brute, then, enemy only will be melee and ranged, check if enemy is countered / stunned, if true, player cannot attack stunned enemy
	const bool IsEnemyStun = TargetEnemy->GetCurrentEnemyState() == EEnemyCurrentState::Countered;

	// // TODO: Delete
	// {
	// 	GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Yellow, FString::Printf(TEXT("Is %s Stun: %s"), *TargetEnemy->GetName(), IsEnemyStun? TEXT("true") : TEXT("false")));
	// }
	
	return !IsEnemyStun;
}

bool UPlayerActionComponent::IsObjectBlocking(AEnemyBase* TargetEnemy, FVector SelfPosition) const
{
	// Use linetrace to check if something is blocking enemy
	FVector TargetPos = TargetEnemy->GetActorLocation();

	FHitResult Hit;
	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(PlayerOwnerRef);
	IgnoreActors.Add(TargetEnemy);
	const bool bHit = UKismetSystemLibrary::LineTraceSingle(GetWorld(), SelfPosition, TargetPos, UEngineTypes::ConvertToTraceType(ECC_Camera), false, IgnoreActors, EDrawDebugTrace::None,Hit,true);
	if(bHit) return true;

	return false;
}

void UPlayerActionComponent::InstantRotation(FVector RotatingVector)
{
	const FRotator InputRotation = UKismetMathLibrary::MakeRotFromX(RotatingVector);

	PlayerOwnerRef->SetActorRotation(InputRotation);
}

AEnemyBase* UPlayerActionComponent::GetTargetEnemy(TArray<AEnemyBase*> OpponentsAroundSelf)
{
	const FInputDirection OwnerInputDirection = PlayerOwnerRef->GetPlayerInputDir();

	
	const FVector SelfPos = PlayerOwnerRef->GetActorLocation();

	FVector SelfToInputDestDir;

	const float InputXValue = OwnerInputDirection.GetInputDirectionX(); 
	const float InputYValue = OwnerInputDirection.GetInputDirectionY(); 
	
	// find out which way is right
	if(InputXValue != 0 || InputYValue != 0)
	{
		const FRotator Rotation = PlayerOwnerRef->Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector RightX = UKismetMathLibrary::GetRightVector(YawRotation);
		const FVector ForwardY = UKismetMathLibrary::GetForwardVector(YawRotation);

		const FVector RightInputDir = RightX * InputXValue;
		const FVector ForwardInputDir = ForwardY * InputYValue;
		
		const FVector InputDest = SelfPos + ((RightInputDir * 50) + (ForwardInputDir * 50));

		SelfToInputDestDir = UKismetMathLibrary::GetDirectionUnitVector(SelfPos, InputDest);
	}
	// else
	// {
	// 	// if there is no input direction, it means player didn't press movement key, it means attack will happen in front of player
	// 	SelfToInputDestDir = PlayerOwnerRef->GetActorForwardVector();
	// }
	
	// set first one as closest target and iterating from opponents list
	AEnemyBase* ReturnTarget = OpponentsAroundSelf[0];
	
	// Set a fake dot product
	float TargetDotProduct = -1.0f;

	// Use dot product to check if this iterate enemy is in front of player 
	for (int32 i = 0; i < OpponentsAroundSelf.Num(); i++)
	{
		FVector EachCharacterPos = OpponentsAroundSelf[i]->GetActorLocation();

		EachCharacterPos.Z = SelfPos.Z;

		const FVector SelfToCharacterDir = UKismetMathLibrary::GetDirectionUnitVector(SelfPos, EachCharacterPos);


		const float DotProduct = UKismetMathLibrary::Dot_VectorVector(SelfToInputDestDir, SelfToCharacterDir);
		
		// if iterating dot product is not correct
		if(DotProduct < 0.70f) continue;

		// if iterating dotproduct is bigger than current target dot product, it means iterating actor will more likely be our target
		if(DotProduct > TargetDotProduct)
		{
			ReturnTarget = OpponentsAroundSelf[i];
			TargetDotProduct = DotProduct;
		}
	}

	if(TargetDotProduct < 0.70f)
	{
		return nullptr;
	}

	
	return ReturnTarget;
}


// ========================================== Timeline Function =========================================
void UPlayerActionComponent::MovingAttackMovement(float Alpha)
{
	UpdateTargetPosition();
	
	const FVector CharacterCurrentPos = PlayerOwnerRef->GetActorLocation();
	
	const FVector MovingPos = UKismetMathLibrary::VLerp(MovingStartPos, MovingEndPos, Alpha);

	const FVector LaunchingPos = FVector(MovingPos.X, MovingPos.Y, CharacterCurrentPos.Z);

	PlayerOwnerRef->SetActorLocation(LaunchingPos);
}

void UPlayerActionComponent::DodgeMovement(float Alpha)
{
	const UWorld* World = GetWorld();
	if(!World) return;

	const FVector UpVector = PlayerOwnerRef->GetActorUpVector();
	const FVector DownVector = UpVector * -1;
	const FVector CharacterCurrentPos = PlayerOwnerRef->GetActorLocation();
	const FVector MovingPos = UKismetMathLibrary::VLerp(MovingStartPos, MovingEndPos, Alpha);
	const FVector LaunchingPos = FVector(MovingPos.X, MovingPos.Y, CharacterCurrentPos.Z);

	const FVector LinetraceWallEnd = CharacterCurrentPos + (LaunchingPos - CharacterCurrentPos) * 10;
	
	// Try To Get if building is blocking or not
	FHitResult Hit;
	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(PlayerOwnerRef);

	const float CapHalfHeight = PlayerOwnerRef->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	
	const bool IsWallHit = UKismetSystemLibrary::LineTraceSingle(World, CharacterCurrentPos, LinetraceWallEnd, UEngineTypes::ConvertToTraceType(ECC_Camera), false, IgnoreActors, EDrawDebugTrace::None,Hit,true);
	const bool IsFloorHit = UKismetSystemLibrary::LineTraceSingle(World, LaunchingPos, LaunchingPos + (DownVector * CapHalfHeight * 1.5f), UEngineTypes::ConvertToTraceType(ECC_Camera), false, IgnoreActors, EDrawDebugTrace::None,Hit,true);
	
	if(!IsWallHit)
	{
		if(!IsFloorHit) return;

		const FVector FixedFloorPos = Hit.Location + (UpVector * CapHalfHeight);
		PlayerOwnerRef->SetActorLocation(FixedFloorPos, false);
	}
}

void UPlayerActionComponent::SetCollisionIgnoreToEnemy()
{
	PlayerOwnerRef->GetMesh()->SetCollisionResponseToChannel(ECC_Pawn, ECollisionResponse::ECR_Overlap);
	PlayerOwnerRef->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECollisionResponse::ECR_Overlap);
}

void UPlayerActionComponent::SetCollisionBlockToEnemy()
{
	PlayerOwnerRef->GetMesh()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	PlayerOwnerRef->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
}



void UPlayerActionComponent::UpdateTargetPosition()
{
	// Constantly update damaging target position to FVector MovingEndPos
	const AEnemyBase* CurrentDamagingTarget = PlayerOwnerRef->GetDamagingActor();
	const FVector CurrentDamageTargetPos = CurrentDamagingTarget->GetActorLocation();
	SetAttackMovementPositions(CurrentDamageTargetPos);
}



// ========================================================= Delegate Function =================================================

void UPlayerActionComponent::ExecutePlayerAction(EActionState ExecutingAction)
{
	switch (ExecutingAction)
	{
		case EActionState::Idle:
			break;
		case EActionState::Evade:
			BeginEvade();
			break;
		case EActionState::Attack:
			BeginMeleeAttack();
			break;
		case EActionState::Dodge:
			BeginDodge();
			break;
		case EActionState::SpecialAbility1:
			ExecuteSpecialAbility(1);
			break;
		case EActionState::SpecialAbility2:
			ExecuteSpecialAbility(2);
			break;
		default: break;
	}
}


void UPlayerActionComponent::ReceivingDamage(int32 DamageAmount, AActor* DamageCauser,
	EEnemyAttackType ReceivingAttackType)
{
	// early return if player is dodging, since player is not getting damaged when dodging
	EActionState PlayerCurrentAction = PlayerOwnerRef->GetCurrentActionState();

	if(PlayerCurrentAction == EActionState::Dodge) return;

	
	if(IsPlayerCanBeDamaged(PlayerCurrentAction, ReceivingAttackType))
	{
		// if player is attacking
		if(PlayerCurrentAction == EActionState::Attack)
		{
			// Stop player anim montage
			PlayerOwnerRef->StopAnimMontage();

			// stop player's attack movement
			const EPlayerAttackAnimations CurrentRegisteredAttackAnim = PlayerOwnerRef->GetCurrentAttackAnim();
			StopSpecificMovingTimeline(CurrentRegisteredAttackAnim);

			// Stop player's attaching tentacle material update
			PlayerOwnerRef->OnCancelTentacleMaterialChange();
			
			
			// make damaging enemy resume moving
			AEnemyBase* ResumeMovementDamagingActor = PlayerOwnerRef->GetDamagingActor();
			if(ResumeMovementDamagingActor)
				ResumeMovementDamagingActor->TryResumeMoving();
		}

		// Play Sound
		USoundBase* BoneBreakSound = PlayerOwnerRef->GetBoneBreakSound();
		if(BoneBreakSound)
			UGameplayStatics::PlaySoundAtLocation(GetWorld(), BoneBreakSound, PlayerOwnerRef->GetActorLocation());

		// Play hit effect
		UNiagaraSystem* NiagaraHitEffect = PlayerOwnerRef->GetNSHitEffect();
		UParticleSystem* CascadeHitEffect = PlayerOwnerRef->GetPHitEffect();
		const bool IsNiagara = PlayerOwnerRef->IsUsingNiagara();
		const FVector ParticleScale = PlayerOwnerRef->GetParticleEffectScale();
		
		const FVector SelfPos = PlayerOwnerRef->GetActorLocation();
		const FVector DirToInstigator = UKismetMathLibrary::Normal(DamageCauser->GetActorLocation() - SelfPos);
		const FVector VFXSpawnPos = SelfPos + (DirToInstigator * 75);
		if(IsNiagara && NiagaraHitEffect)
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), NiagaraHitEffect, VFXSpawnPos, FRotator::ZeroRotator, ParticleScale);
		else if(CascadeHitEffect)
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), CascadeHitEffect, VFXSpawnPos, FRotator::ZeroRotator, ParticleScale);
		

		// Damage player
		PlayerOwnerRef->HealthReduction(DamageAmount);

		// Set combo count to zero
		ResetComboCount();
		ClearComboResetTimer();

		// Player Play receiving damage montage
		if(ReceiveDamageMontage == nullptr) return;

		PlayerOwnerRef->SetCurrentActionState(EActionState::Recovering);

		// 
		// Instant Rotate to enemy
		const FVector DamageCauserLocation = DamageCauser->GetActorLocation();
		FVector PlayerLocation = PlayerOwnerRef->GetActorLocation();
		PlayerLocation.Z = DamageCauserLocation.Z;
		const FVector FacingEnemyDir = UKismetMathLibrary::Normal( DamageCauserLocation - PlayerLocation);
		InstantRotation(FacingEnemyDir);
	
		CurrentPlayingMontage = ReceiveDamageMontage;
		
		PlayerOwnerRef->PlayAnimMontage(CurrentPlayingMontage, 1, NAME_None);
	}
}


void UPlayerActionComponent::TriggerPreCounter(AActor* CounteringTarget)
{
	if(!CounteringTarget) return;

	AEnemyBase* CastedTarget = Cast<AEnemyBase>(CounteringTarget);
	if(CastedTarget == nullptr) return;

	PlayerOwnerRef->StopAnimMontage();

	PlayerOwnerRef->SetCounteringTarget(CastedTarget);

	EnterPreCounterState();
}


void UPlayerActionComponent::TriggerCounterAttack(AActor* CounteringTarget)
{
	AEnemyBase* CastedTarget = Cast<AEnemyBase>(CounteringTarget);
	if(CastedTarget == nullptr) return;
	
	PlayerOwnerRef->StopAnimMontage();

	PlayerOwnerRef->SetCounteringTarget(CastedTarget);
	
	BeginCounterAttack();

}


bool UPlayerActionComponent::IsPlayerCountering(EActionState PlayerCurrentAction, EEnemyAttackType ReceivingAttackType)
{
	// return true if player is in counter state and enemy's incoming attack is counterable
	return PlayerCurrentAction == EActionState::Evade && ReceivingAttackType == EEnemyAttackType::AbleToCounter;
}

bool UPlayerActionComponent::IsPlayerCanBeDamaged(EActionState PlayerCurrentAction,
	EEnemyAttackType ReceivingAttackType)
{
	// return true if player is standing or attacking or player is countering but incoming attack is not counterable
	return PlayerCurrentAction == EActionState::Idle || PlayerCurrentAction == EActionState::Attack || PlayerCurrentAction == EActionState::BeforeAttack || PlayerCurrentAction == EActionState::PreAction || (PlayerCurrentAction == EActionState::Evade && ReceivingAttackType == EEnemyAttackType::UnableToCounter);
}

void UPlayerActionComponent::MakePlayerEnemyFaceEachOther(AEnemyBase* TargetEnemyRef)
{
	// simply make player and targeted enemy rotate to each other
	const FVector TargetActorPos = TargetEnemyRef->GetActorLocation();
	const FVector PlayerPos = PlayerOwnerRef->GetActorLocation();
	
	const FVector FacingEnemyDir = UKismetMathLibrary::Normal(TargetActorPos - PlayerPos);
	const FVector FacingPlayerDir = UKismetMathLibrary::Normal(PlayerPos - TargetActorPos);
	InstantRotation(FacingEnemyDir);
	TargetEnemyRef->InstantRotation(FacingPlayerDir);
}

