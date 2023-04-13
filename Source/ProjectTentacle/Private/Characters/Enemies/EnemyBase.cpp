// Copyright (C) The Tentacle Zone 2023. All Rights Reserved.


#include "Characters/Enemies/EnemyBase.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Characters/Enemies/EnemyBaseController.h"
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

// Called when the game starts or when spawned
void AEnemyBase::BeginPlay()
{
	Super::BeginPlay();

	InitializeWidgetComponents();

	InitializeEnemyControllerRef();
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

	// timeline binding
	FOnTimelineFloat MovingAttackPosUpdate;
	MovingAttackPosUpdate.BindDynamic(this, &AEnemyBase::UpdateAttackingPosition);
	AttackMovingTimeline.AddInterpFloat(AttackMovingCurve, MovingAttackPosUpdate);
}

void AEnemyBase::InitializeEnemyControllerRef()
{
	
	// Get enemy base controller
	AController* EnemyController = GetController();
	if(Controller == nullptr) return;
		
	AEnemyBaseController* CastedEnemyBaseController =  Cast<AEnemyBaseController>(EnemyController);
	if(CastedEnemyBaseController == nullptr) return;

	// Assign Enemy base controller
	if(CurrentEnemyBaseController == nullptr)
		CurrentEnemyBaseController = CastedEnemyBaseController;

	UBlackboardComponent* CastedBBComponent = CastedEnemyBaseController->GetBlackboardComponent();
	if(CastedBBComponent)
		BBComponent = CastedBBComponent;
}

// Called every frame
void AEnemyBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	AttackMovingTimeline.TickTimeline(DeltaTime);
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

void AEnemyBase::TryGetOwnController()
{
	if(OwnController) return;
	OwnController = Cast<AEnemyBaseController>(GetController());
}


// ====================================================== Attack ======================================================

void AEnemyBase::ExecuteAttack()
{
	// SetAttackType and get the result enum of it
	EEnemyAttackAnimMontages ResultEnemyAnimMontage = SetAttackType();

	
	if(AttackIndicatorRef)
	{
		// execute delegate function to update variables in Indicator widget class
		OnUpdatingEnemyAttackIndicator.Execute(CurrentAttackType);
	}

	// Save lerp start and end position for later timeline function usage
	SelfAttackStartPos = GetActorLocation();

	// Get Player Position
	const ACharacter* PlayerCharacter = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	FVector PlayerCurrentPos = PlayerCharacter->GetActorLocation();
	
	const FVector DestinationPos = CalculateDestinationForAttackMoving(PlayerCurrentPos);
	AttackMovingDestination = DestinationPos;

	IsAttacking = true;
	
	// switch case on current attack type to fire different animation 
	switch (CurrentAttackType)
	{
		case EEnemyAttackType::AbleToCounter:
			if(CounterableAttackMontage != nullptr)
				PlayAnimMontage(CounterableAttackMontage, 1, "Default");
			//AttackMovingTimeline.PlayFromStart();
			break;
		case EEnemyAttackType::UnableToCounter:
			if(NotCounterableAttackMontage != nullptr)
				PlayAnimMontage(NotCounterableAttackMontage, 1, "Default");
			//AttackMovingTimeline.PlayFromStart();
			break;
		default: break;
	}

}

EEnemyAttackAnimMontages AEnemyBase::SetAttackType()
{
	int32 CounterableRndInt = UKismetMathLibrary::RandomInteger(2);

	if(CounterableRndInt == 0)
	{
		CurrentAttackType = EEnemyAttackType::UnableToCounter;
		return EEnemyAttackAnimMontages::MMAKick;
	}
	
	CurrentAttackType = EEnemyAttackType::AbleToCounter;
	return EEnemyAttackAnimMontages::HeadButt;
}


FVector AEnemyBase::CalculateDestinationForAttackMoving(FVector PlayerPos)
{
	// Get direction from self to player
	FVector OffsetWithoutZ = PlayerPos - SelfAttackStartPos;
	OffsetWithoutZ.Z = 0;
	const FVector DirFromSelfToPlayer = UKismetMathLibrary::Normal(OffsetWithoutZ);

	const FVector SupposedAttackMovingDestination = SelfAttackStartPos + (DirFromSelfToPlayer * AttackMovingDistance);

	
	// Hit result
	FHitResult Hit;
	// Empty array of ignoring actor, maybe add Enemies classes to be ignored
	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(this);
	
	// Capsule trace by channel
	const bool bHit = UKismetSystemLibrary::LineTraceSingle(this, SelfAttackStartPos, SupposedAttackMovingDestination, UEngineTypes::ConvertToTraceType(ECC_Visibility),false, IgnoreActors,  EDrawDebugTrace::None,Hit,true);

	if(!bHit) return SupposedAttackMovingDestination;

	const FVector DirFromPlayerToSelf = DirFromSelfToPlayer * -1;
	
	return Hit.ImpactPoint + (DirFromPlayerToSelf * OffsetFromPlayer);
}

TArray<AActor*> AEnemyBase::GetActorsInFrontOfEnemy()
{
	const UWorld* World = GetWorld();
	
	FName AttackSocketName;
	if(CurrentAttackType == EEnemyAttackType::AbleToCounter)
		AttackSocketName = "RightHand";
	else
		AttackSocketName = "RightFoot";
	
	const FVector HeadSocketLocation = GetMesh()->GetSocketLocation(AttackSocketName);

	TArray<AActor*> FoundActorList;

	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(this);
	
	UKismetSystemLibrary::SphereOverlapActors(World, HeadSocketLocation, 120, FilterType, FilteringClass, IgnoreActors,FoundActorList);

	return FoundActorList;
}

void AEnemyBase::StartCounterAttackAnimation()
{
	if(!CounterVictimMontage) return;
	
	StopAnimMontage();
	OnHideAttackIndicator();
	PlayAnimMontage(CounterVictimMontage,1,"Start");
}

// ===================================================== Stunning ===========================================
void AEnemyBase::BeginLyingCountDown()
{
	const UWorld* World = GetWorld();
	if(World == nullptr) return;

	PlayLyingMontage();
	
	World->GetTimerManager().SetTimer(GettingUpTimerHandle,this, &AEnemyBase::RecoverFromLying, TimeToGetUp, false, -1);
}

void AEnemyBase::PlayLyingMontage()
{
	if(!CounterVictimMontage) return;
	
	StopAnimMontage();
	PlayAnimMontage(CounterVictimMontage,1,"LyingLoop");
}


void AEnemyBase::RecoverFromLying()
{
	if(!GetUpMontage) return;

	CurrentEnemyState = EEnemyCurrentState::Standing;
	
	StopAnimMontage();
	PlayAnimMontage(GetUpMontage,1,"Default");
}

EEnemyType AEnemyBase::GetType() const
{
	return UnitType;
}


void AEnemyBase::PlayDamageReceiveAnimation(int32 AttackTypIndex)
{
	if(AttackTypIndex > 4) return;
	
	switch (AttackTypIndex)
	{
	case 0:
		PlayAnimMontage(ReceiveShortFlipKick,1,NAME_None);
		break;
	case 1:
		PlayAnimMontage(ReceiveFlyingKick,1,NAME_None);
		break;
	case 2:
		PlayAnimMontage(ReceiveFlyingPunch,1,NAME_None);
		break;
	case 3:
		PlayAnimMontage(ReceiveSpinKick,1,NAME_None);
		break;
	case 4:
		PlayAnimMontage(ReceiveDashingDoubleKick,1,NAME_None);
		break;
	default: break;
	}
}

void AEnemyBase::PlayFinishedAnimation()
{
	PlayAnimMontage(FinishedAnimation,1,NAME_None);
}

// ============================================= Timeline function ====================================================

void AEnemyBase::UpdateAttackingPosition(float Alpha)
{
	const FVector CharacterCurrentPos = GetActorLocation();
	
	const FVector LerpPos = UKismetMathLibrary::VLerp(SelfAttackStartPos,AttackMovingDestination, Alpha);

	const FVector MovingPos = FVector(LerpPos.X, LerpPos.Y, CharacterCurrentPos.Z);

	SetActorLocation(MovingPos);
}


void AEnemyBase::ActionEnd_Implementation(bool BufferingCheck)
{
	ICharacterActionInterface::ActionEnd_Implementation(BufferingCheck);

	IsAttacking = false;
	
	if(BTComponent)
		const bool bIsBound = OnFinishAttackingTask.ExecuteIfBound(BTComponent, true, false);

}

// ================================================== Interface Functions ============================================
void AEnemyBase::TryToDamagePlayer_Implementation()
{
	ICharacterActionInterface::TryToDamagePlayer_Implementation();
	
	TArray<AActor*> FoundActorList = GetActorsInFrontOfEnemy();

	if(FoundActorList.Num() != 0)
	{
		for (AActor* EachFoundActor : FoundActorList)
		{
			IDamageInterface::Execute_ReceiveDamageFromEnemy(EachFoundActor, BaseDamageAmount, this, CurrentAttackType);
			//UGameplayStatics::ApplyDamage(EachFoundActor, 30, GetController(), this, DamageType);
		}
	}
}

void AEnemyBase::TryTriggerPlayerCounter_Implementation()
{
	ICharacterActionInterface::TryTriggerPlayerCounter_Implementation();

	TArray<AActor*> FoundActorList = GetActorsInFrontOfEnemy();

	if(FoundActorList.Num() != 0)
	{
		for (AActor* EachFoundActor : FoundActorList)
		{
			ICharacterActionInterface::Execute_ReceiveAttackInCounterState(EachFoundActor, this);
		}
	}

}

void AEnemyBase::ReceiveDamageFromPlayer_Implementation(int32 DamageAmount, AActor* DamageCauser,
                                                        EPlayerAttackType PlayerAttackType)
{
	IDamageInterface::ReceiveDamageFromPlayer_Implementation(DamageAmount, DamageCauser, PlayerAttackType);
	
	// if enemy is attack, stop montage, flip bool to false, unshow attack indicator, and execute onfinish attack delegate 
	if(IsAttacking && PlayerAttackType != EPlayerAttackType::CounterAttack)
	{
		IsAttacking = false;
		
		StopAnimMontage();
		
		OnHideAttackIndicator();
		
		if(BTComponent)
			const bool bIsBound = OnFinishAttackingTask.ExecuteIfBound(BTComponent, true, false);
	}
	

	// clamp health that is deducted 
	Health = UKismetMathLibrary::Clamp((Health - DamageAmount),0,MaxHealth);

	// Switch case on player's attack type to play different damage receive animation
	switch (PlayerAttackType)
	{
		case EPlayerAttackType::ShortFlipKick:
			PlayDamageReceiveAnimation(0);
			break;
		case EPlayerAttackType::FlyingKick:
			PlayDamageReceiveAnimation(1);
			break;
		case EPlayerAttackType::FlyingPunch:
			PlayDamageReceiveAnimation(2);
			break;
		case EPlayerAttackType::SpinKick:
			PlayDamageReceiveAnimation(3);
			break;
		case EPlayerAttackType::DashingDoubleKick:
			PlayDamageReceiveAnimation(4);
			break;
		case EPlayerAttackType::CounterAttack:

			break;
		case EPlayerAttackType::FastKick:
			PlayDamageReceiveAnimation(2);
			break;
		case EPlayerAttackType::FastPunch:
			PlayDamageReceiveAnimation(2);
			break;
		default: break;
	}

}

void AEnemyBase::StartLyingOnTheGround_Implementation()
{
	ICharacterActionInterface::StartLyingOnTheGround_Implementation();

	CurrentEnemyState = EEnemyCurrentState::Lying;
	
	BeginLyingCountDown();
}

void AEnemyBase::RepeatLyingOnTheGround_Implementation()
{
	ICharacterActionInterface::RepeatLyingOnTheGround_Implementation();

	PlayLyingMontage();
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



void AEnemyBase::InstantRotation(FVector RotatingVector)
{
	const FRotator InputRotation = UKismetMathLibrary::MakeRotFromX(RotatingVector);

	SetActorRotation(InputRotation);
}