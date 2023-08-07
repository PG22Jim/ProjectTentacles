// Copyright (C) The Tentacle Zone 2023. All Rights Reserved.


#include "Characters/Enemies/EnemyBase.h"

#include "NiagaraFunctionLibrary.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Characters/Enemies/EnemyBaseController.h"
#include "Characters/Player/PlayerDamageInterface.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"

// Sets default values
AEnemyBase::AEnemyBase()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	GetCharacterMovement()->bUseRVOAvoidance = true;

	// Set up indicator and target icon widget component
	EnemyAttackIndicatorWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT( "EnemyAttackIndicatorWidget" ));
	EnemyTargetedIconWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT( "EnemyTargetedIconWidget" ));
	EnemyAttackIndicatorWidgetComponent->AttachToComponent( RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	EnemyTargetedIconWidgetComponent->AttachToComponent( RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
}

void AEnemyBase::RegisterOwningEncounter(AEncounterVolume* NewOwningEncounter)
{
	TryGetOwnController();
	OwnController->RegisterOwningEncounter(NewOwningEncounter);
}

void AEnemyBase::EngageTarget(AActor* Target)
{
	TryGetOwnController();
	OwnController->EngageTarget(Target);
}

void AEnemyBase::ReceiveDamageFromPlayer_Implementation(float DamageAmount, AActor* DamageCauser,
	EPlayerAttackType PlayerAttackType)
{
	IDamageInterface::ReceiveDamageFromPlayer_Implementation(DamageAmount, DamageCauser, PlayerAttackType);
	if(OnInterruptStrafe.IsBound()) OnInterruptStrafe.Execute();
	
	HealthReduction(DamageAmount);
	
	if(UnitType != EEnemyType::Brute)
	{
		if((CurrentEnemyState == EEnemyCurrentState::Attacking || CurrentEnemyState == EEnemyCurrentState::Countered))
		{
			OnCancelCounterableAttack();
			
			TryGetOwnController();
			OwnController->RegisterCompletedAttack();
		}
	}

	PlayReceiveDamageSound();
	PlayReceiveDamageVFX(DamageCauser->GetActorLocation());

	if(Health > 0)
	{
		PlayReceiveDamageAnimation(PlayerAttackType);
		if(PlayerAttackType != EPlayerAttackType::CounterAttack)
		{
			TryResumeMoving();
		}
		return;
	}
	
	OnDeath();
}

// Called when the game starts or when spawned
void AEnemyBase::BeginPlay()
{
	Super::BeginPlay();
	StartingTransform = GetMesh()->GetRelativeTransform();
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	InitializeWidgetComponents();

	InitializeEnemyControllerRef();
}

void AEnemyBase::CheckForShackExit()
{
	if (IsDead) return;

	// If controller or BB is missing (so the unit is not functional) or it hasn't left the shack yet, kill the unit
	TryGetOwnController();
	if (!OwnController || !BBComponent || BBComponent->GetValueAsBool(ShackExitKeyName)) {
		OnDeath();
	}
}

void AEnemyBase::Reset()
{
	Super::Reset();
	Health = MaxHealth;

	// Reset state, re-enable collision, and turn off ragdoll
	TrySwitchEnemyState(EEnemyCurrentState::WaitToAttack);

	IsDead = false;
	
	TurnCollisionOffOrOn(false);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void AEnemyBase::ResetMeshTransform()
{
	GetMesh()->SetRelativeTransform(StartingTransform, false, nullptr, GetMesh()->IsSimulatingPhysics() ? ETeleportType::TeleportPhysics : ETeleportType::None);
}

void AEnemyBase::InitializeWidgetComponents()
{
	// Get attack indicator widget reference
	UUserWidget* ReturnAttackIndicatorWidget = EnemyAttackIndicatorWidgetComponent->GetWidget();
	UWidget_EnemyAttackIndicator* CastedAttackIndicatorWidget = Cast<UWidget_EnemyAttackIndicator>(ReturnAttackIndicatorWidget);
	
	if(CastedAttackIndicatorWidget)
	{
		// Bind function and unshow the indicator
		AttackIndicatorRef = CastedAttackIndicatorWidget;
		OnUpdatingEnemyAttackIndicator.BindDynamic(AttackIndicatorRef, &UWidget_EnemyAttackIndicator::OnReceivingNewAttackType);
		AttackIndicatorRef->UnShowIndicator();
		AttackIndicatorRef->SetUnitType(UnitType);
	}

	// Get target icon widget reference
	UUserWidget* ReturnTargetedIconWidget = EnemyTargetedIconWidgetComponent->GetWidget();
	UWidget_EnemyTargetIconWidget* CastedTargetedIconWidget = Cast<UWidget_EnemyTargetIconWidget>(ReturnTargetedIconWidget);

	if(CastedTargetedIconWidget)
	{
		// set variable to casted widget and hide indicator
		EnemyTargetWidgetRef = CastedTargetedIconWidget;
		EnemyTargetWidgetRef->UnShowIndicator();
	}
}

void AEnemyBase::InitializeEnemyControllerRef()
{
	TryGetOwnController();
}

// Called every frame
void AEnemyBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

float AEnemyBase::GetAttackCompletionTime() const
{
	return AttackCompletionTime;
}

float AEnemyBase::GetAttackCounterableTime() const
{
	return AttackCounterableTime;
}


void AEnemyBase::OnHideAttackIndicator()
{
	if(AttackIndicatorRef)
		AttackIndicatorRef->UnShowIndicator();
}

// TODO: Based Class
void AEnemyBase::TryGetOwnController()
{
	if(OwnController)
	{
		if(!BBComponent)
		{
			BBComponent = OwnController->GetBlackboardComponent();
		}
		return;
	}
	OwnController = Cast<AEnemyBaseController>(GetController());
}


// ====================================================== Attack ======================================================

void AEnemyBase::ExecuteAttack()
{
	AttackTaskOn = true;

	if(UnitType != EEnemyType::Brute)
	{
		SetAttackType();
	}

	// Update attack type in indicator's reference
	if(AttackIndicatorRef)
			AttackIndicatorRef->OnReceivingNewAttackType(CurrentAttackType);
	
	// Set is attacking
	TrySwitchEnemyState(EEnemyCurrentState::Attacking);

	// Set Character unable to move to prevent task early finish and enemy is able to move while animation montage
	TryStopMoving();
	

}

void AEnemyBase::SetAttackType()
{
	// If Unit Type is Range, the attack is UnableToCounter
	if(UnitType == EEnemyType::Ranged)
	{
		CurrentAttackType = EEnemyAttackType::UnableToCounter;
		return;
	}
	
	int32 CounterableRndInt = UKismetMathLibrary::RandomInteger(2);

	if(CounterableRndInt == 0)
		CurrentAttackType = EEnemyAttackType::UnableToCounter;
	else
		CurrentAttackType = EEnemyAttackType::AbleToCounter;
}

void AEnemyBase::EnableStrafe(const bool bStrafe) const
{
	GetCharacterMovement()->MaxWalkSpeed = bStrafe ? StrafeSpeed : WalkSpeed;
}

void AEnemyBase::ExecuteRangedAttack(AActor* Target)
{
	if(Target->GetClass()->ImplementsInterface(UPlayerDamageInterface::StaticClass()))
	{
		IPlayerDamageInterface::Execute_ReceiveDamageFromEnemy(Target, BaseDamageAmount, this, EEnemyAttackType::UnableToCounter);
	}
}

EEnemyType AEnemyBase::GetType() const
{
	return UnitType;
}

void AEnemyBase::PlayFinishedAnimation()
{
	PlayAnimMontage(FinishedAnimation,1,NAME_None);
}

void AEnemyBase::TryStopMoving()
{
	UCharacterMovementComponent* MovementComp = GetCharacterMovement();
	if(MovementComp->MovementMode == EMovementMode::MOVE_Walking)
		MovementComp->DisableMovement();
}

void AEnemyBase::TryResumeMoving()
{
	UCharacterMovementComponent* MovementComp = GetCharacterMovement();
	if(MovementComp->MovementMode == EMovementMode::MOVE_None)
		MovementComp->SetMovementMode(EMovementMode::MOVE_Walking);
}

void AEnemyBase::TrySwitchEnemyState(EEnemyCurrentState NewState)
{
	if(CurrentEnemyState == NewState) return;

	if(NewState == EEnemyCurrentState::Countered) bUseControllerRotationYaw = false;
	CurrentEnemyState = NewState;
	
}

// Finish attack task and switch to requested task
void AEnemyBase::TryFinishAttackTask(EEnemyCurrentState SwitchingState)
{
	// if BT component is valid and if current enemy state is attacking
	if(BTComponent && (CurrentEnemyState == EEnemyCurrentState::Attacking || CurrentEnemyState == EEnemyCurrentState::Countered))
	{
		// switch state to standing
		TrySwitchEnemyState(SwitchingState);
		if(AttackTaskOn)
		{
			const bool bIsBound = OnFinishAttackingTask.ExecuteIfBound(BTComponent, true, false);
			AttackTaskOn = false;
		}
	}
}

void AEnemyBase::OnResumeMovement_Implementation()
{
	ICharacterActionInterface::OnResumeMovement_Implementation();

	TryResumeMoving();
}

void AEnemyBase::OnResetEnemyCurrentState_Implementation()
{
	ICharacterActionInterface::OnResetEnemyCurrentState_Implementation();

	CurrentEnemyState = EEnemyCurrentState::WaitToAttack;
}

// ================================================== Interface Functions ============================================
void AEnemyBase::TryToDamagePlayer_Implementation()
{
	ICharacterActionInterface::TryToDamagePlayer_Implementation();

	if(UnitType != EEnemyType::Brute) TryFinishAttackTask(EEnemyCurrentState::WaitToAttack);
}

void AEnemyBase::OnPullingEnemy_Implementation(FVector PullingDest, float PullingPower, float WorldDeltaSec)
{
	ICharacterActionInterface::OnPullingEnemy_Implementation(PullingDest, PullingPower, WorldDeltaSec);

	TryStopMoving();

	// move them little bit toward stun tentacle
	const FVector CurrentEnemyPos = GetActorLocation();
	
	FVector OffsetWithoutZ = PullingDest - CurrentEnemyPos;
	OffsetWithoutZ.Z = 0;
	
	const FVector DirToTentacle = UKismetMathLibrary::Normal(OffsetWithoutZ);
	const FVector DeltaLocation = DirToTentacle * WorldDeltaSec * PullingPower;

	SetActorLocation(CurrentEnemyPos + DeltaLocation, true);
}

// ===================================================== Damage Receive ========================================================

void AEnemyBase::HealthReduction(float DamageAmount)
{
	float ReducingDamage = DamageAmount;

	// if enemy is brute and he is not stunned, his receiving damage will be reduced
	if(UnitType == EEnemyType::Brute)
	{
		if(CurrentEnemyState != EEnemyCurrentState::Stunned)
		{
			ReducingDamage = DamageAmount - ((DamageAmount / 100 ) * DamageReductionPercentage);
		}
	}
	
	// clamp health that is deducted
	Health = UKismetMathLibrary::Clamp((Health - ReducingDamage),0,MaxHealth);
}

void AEnemyBase::PlayReceiveDamageAnimation(EPlayerAttackType ReceivedAttackType)
{
	if(ReceivedAttackType == EPlayerAttackType::LongMeleeAttack)
	{
		PlayAnimMontage(EnemyReceiveLargeDamageAnim,1,NAME_None);
	}
	else if (ReceivedAttackType == EPlayerAttackType::ShortMeleeAttack)
	{
		PlayAnimMontage(EnemyReceiveSmallDamageAnim,1,NAME_None);
	}
}

void AEnemyBase::PlayReceiveDamageSound()
{
	if(BoneBreakSound)
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), BoneBreakSound, GetActorLocation());
}

void AEnemyBase::PlayReceiveDamageVFX(FVector DamageInstigatorPos)
{
	const FVector SelfPos = GetActorLocation();
	const FVector DirToInstigator = UKismetMathLibrary::Normal(DamageInstigatorPos - SelfPos);
	const FVector VFXSpawnPos = SelfPos + (DirToInstigator * 75);
	if(UseNiagara_HitEffect && NS_HitEffect)
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), NS_HitEffect, VFXSpawnPos, FRotator::ZeroRotator, ParticleEffectScale);
	else if(C_HitEffect)
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), C_HitEffect, VFXSpawnPos, FRotator::ZeroRotator, ParticleEffectScale);
}


void AEnemyBase::TryClearFromPlayerTarget()
{
	ACharacter* PlayerCharacterClass = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	if(PlayerCharacterClass->GetClass()->ImplementsInterface(UCharacterActionInterface::StaticClass()))
	{
		ICharacterActionInterface::Execute_DetachEnemyTarget(PlayerCharacterClass);
		ICharacterActionInterface::Execute_TryClearCounterVictim(PlayerCharacterClass, this);
	}
}

// ===================================================== On Death ========================================================
void AEnemyBase::OnDeath()
{
	// remove self from Encounter volume list
	TryGetOwnController();
	OwnController->OnDeath();
	
	// Clear from player's target reference
	TryClearFromPlayerTarget();

	if(CurrentEnemyState == EEnemyCurrentState::Attacking)
	{
		TryFinishAttackTask(EEnemyCurrentState::WaitToAttack);
		OnHideAttackIndicator();
		StopAnimMontage();
	}
	
	
	// flip death bool to prevent getting selected
	IsDead = true;

	// Clear Collision
	TurnCollisionOffOrOn(true);
	
	// Ragdoll Physics
	RagDollPhysicsOnDead();

	// TODO: Dissolve after few seconds
}

void AEnemyBase::OnSpawn()
{
	Super::Reset();
	Health = MaxHealth;

	// Reset state, re-enable collision, and turn off ragdoll
	TrySwitchEnemyState(EEnemyCurrentState::WaitToAttack);

	IsDead = false;
	
	TurnCollisionOffOrOn(false);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	FTimerHandle ShackExitTimer;
	GetWorldTimerManager().SetTimer(ShackExitTimer, this, &AEnemyBase::CheckForShackExit, ShackExitTimeout, false);
	if(!OwnController) return;

	OwnController->OnSpawn();
}


void AEnemyBase::TurnCollisionOffOrOn(bool TurnCollisionOff)
{
	
	UCapsuleComponent* CapsuleComp = GetCapsuleComponent();
	if(TurnCollisionOff)
		CapsuleComp->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Ignore);
	else
		CapsuleComp->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Block);

	
	USkeletalMeshComponent* SkeleMeshComp = GetMesh();
	
	if(TurnCollisionOff)
		SkeleMeshComp->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Ignore);
	else
		SkeleMeshComp->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Block);
}

void AEnemyBase::RagDollPhysicsOnDead()
{
	USkeletalMeshComponent* SelfCharacterMesh = GetMesh();

	SelfCharacterMesh->SetSimulatePhysics(true);
	
	
	const ACharacter* PlayerCharacterClass = UGameplayStatics::GetPlayerCharacter(GetWorld(),0);
	const FVector PlayerPos = PlayerCharacterClass->GetActorLocation();
	const FVector SelfPos = GetActorLocation();
	const FVector DirFromPlayerToSelf = UKismetMathLibrary::Normal((SelfPos - PlayerPos));
	
	SelfCharacterMesh->AddImpulse(DirFromPlayerToSelf * 5000,"Spine1",true);
	
	UCharacterMovementComponent* SelfCharacterMovement = GetCharacterMovement();
	SelfCharacterMovement->DisableMovement();
}

void AEnemyBase::DisableRagDoll()
{
	GetMesh()->SetSimulatePhysics(false);
	GetMesh()->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	ResetMeshTransform();
}

void AEnemyBase::TimeoutAttack()
{
	TryFinishAttackTask(EEnemyCurrentState::WaitToAttack);
}

void AEnemyBase::OnStunned()
{
	if(CurrentEnemyState == EEnemyCurrentState::Attacking)
	{
		OnHideAttackIndicator();

		OnCancelCounterableAttack();
		
	}
		
	TryFinishAttackTask(EEnemyCurrentState::Stunned);

	TrySwitchEnemyState(EEnemyCurrentState::Stunned);

	

	TryStopMoving();
	
	StopAnimMontage();
	PlayAnimMontage(OnGettingStunnedAnimation, 1, "Default");
}

void AEnemyBase::RecoverFromStunState()
{
	TrySwitchEnemyState(EEnemyCurrentState::WaitToAttack);	
	
	TryResumeMoving();
}

void AEnemyBase::OnCancelCounterableAttack()
{
	ACharacter* PlayerCha = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	
	// Try to clear player's counter target when this performing counterable attack enemy get damaged and interrupted
	if(CurrentAttackType == EEnemyAttackType::AbleToCounter && PlayerCha->GetClass()->ImplementsInterface(UCharacterActionInterface::StaticClass()))
	{
		ICharacterActionInterface::Execute_TryRemoveCounterTarget(PlayerCha, this);
	}
}

void AEnemyBase::OnStopFocusing()
{
	AAIController* CurrentAIOwner = BTComponent->GetAIOwner();
	if(!CurrentAIOwner) return;

	const APawn* Pawn = CurrentAIOwner->GetPawn();
	if(!Pawn) return;

	UBlackboardComponent* Blackboard = BTComponent->GetBlackboardComponent();

	AActor* AsActor = Cast<AActor>(Blackboard->GetValueAsObject("Target"));
	if(!AsActor) return;

	CurrentAIOwner->ClearFocus(EAIFocusPriority::Gameplay);
}

void AEnemyBase::OnSetFocus()
{
	AAIController* CurrentAIOwner = BTComponent->GetAIOwner();
	if(!CurrentAIOwner) return;

	const APawn* Pawn = CurrentAIOwner->GetPawn();
	if(!Pawn) return;

	UBlackboardComponent* Blackboard = BTComponent->GetBlackboardComponent();

	AActor* AsActor = Cast<AActor>(Blackboard->GetValueAsObject("Target"));
	if(!AsActor) return;

	CurrentAIOwner->SetFocus(AsActor);
}

FVector AEnemyBase::GetVerticalUpdatedMovePos(const FVector SupposeMovingPos, const bool bIsMovementVerticalInclude, const float GroundAlpha, const float CapHalfHeight, TArray<AActor*> IgnoringActors)
{
	const float VerticalLineTraceMultiply = bIsMovementVerticalInclude ? 2.0f : 1.5f;
	FHitResult Hit;
	
	// line trace to adjust position on slope
	const FVector FloorCheckPosStart = SupposeMovingPos + (GetActorUpVector() * CapHalfHeight);
	const FVector FloorCheckPosEnd = SupposeMovingPos + ((GetActorUpVector() * -1) * (CapHalfHeight * VerticalLineTraceMultiply));
	const bool bHitFloor = UKismetSystemLibrary::LineTraceSingle(this, FloorCheckPosStart, FloorCheckPosEnd,UEngineTypes::ConvertToTraceType(ECC_Camera),false, IgnoringActors,  EDrawDebugTrace::None,Hit,true);

	if(!bIsMovementVerticalInclude) return bHitFloor ? (Hit.Location + (GetActorUpVector() * CapHalfHeight)) : SupposeMovingPos;

	return bHitFloor && GroundAlpha >= 0.7f ? (Hit.Location + (GetActorUpVector() * CapHalfHeight)) : SupposeMovingPos;
}

void AEnemyBase::StartAttackTimeout()
{
	GetWorldTimerManager().SetTimer(AttackTimeoutHandle, this, &AEnemyBase::TimeoutAttack, AttackTimeoutDuration);
}


void AEnemyBase::ShowEnemyAttackIndicator_Implementation()
{
	IEnemyWidgetInterface::ShowEnemyAttackIndicator_Implementation();
	if(AttackIndicatorRef)
		AttackIndicatorRef->ShowIndicator();
}

void AEnemyBase::UnShowEnemyAttackIndicator_Implementation()
{
	IEnemyWidgetInterface::UnShowEnemyAttackIndicator_Implementation();

	OnHideAttackIndicator();
}

void AEnemyBase::ShowPlayerTargetIndicator_Implementation()
{
	IEnemyWidgetInterface::ShowPlayerTargetIndicator_Implementation();

	if(EnemyTargetWidgetRef)
		EnemyTargetWidgetRef->ShowIndicator();
}

void AEnemyBase::UnShowPlayerTargetIndicator_Implementation()
{
	IEnemyWidgetInterface::UnShowPlayerTargetIndicator_Implementation();

	if(EnemyTargetWidgetRef)
		EnemyTargetWidgetRef->UnShowIndicator();
}

void AEnemyBase::OnBeginStun_Implementation()
{
	ICharacterActionInterface::OnBeginStun_Implementation();

	OnStunned();
}

void AEnemyBase::OnResumeFromStunTimerCountDown_Implementation()
{
	ICharacterActionInterface::OnResumeFromStunTimerCountDown_Implementation();

	GetWorld()->GetTimerManager().SetTimer(StunningTimerHandle, this, &AEnemyBase::RecoverFromStunState, TotalStunDuration, false, -1);
}


void AEnemyBase::InstantRotation(FVector RotatingVector)
{
	const FRotator InputRotation = UKismetMathLibrary::MakeRotFromX(RotatingVector);

	SetActorRotation(InputRotation);
}
