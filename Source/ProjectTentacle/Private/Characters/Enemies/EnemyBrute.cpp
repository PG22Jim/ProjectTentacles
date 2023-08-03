// Copyright (C) The Tentacle Zone 2023. All Rights Reserved.


#include "Characters/Enemies/EnemyBrute.h"

#include "NiagaraFunctionLibrary.h"
#include "Characters/Player/PlayerDamageInterface.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"


AEnemyBrute::AEnemyBrute()
{

}

void AEnemyBrute::BeginPlay()
{
	Super::BeginPlay();

	// 
	InitializeTimelineComp();

	// Applying capsule component's begin overlap function
	UCapsuleComponent* CapsuleComp = GetCapsuleComponent();
	if(CapsuleComp) CapsuleComp->OnComponentBeginOverlap.AddDynamic(this, &AEnemyBrute::OnDealChargeDamage);
}
	
void AEnemyBrute::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ChargeMovingTimeline.TickTimeline(DeltaSeconds);
	CounterableMovingTimeline.TickTimeline(DeltaSeconds);
	JumpSlamMovingTimeline.TickTimeline(DeltaSeconds);
}

void AEnemyBrute::TryToDamagePlayer_Implementation()
{
	Super::TryToDamagePlayer_Implementation();
	
	if(IsSecondAttack || BruteAttack == EBruteAttackType::JumpSlam) TryFinishAttackTask(EEnemyCurrentState::WaitToAttack);

	
	TArray<AActor*> FoundActorList = GetActorsInFrontOfEnemy(true);

	if(FoundActorList.Num() != 0)
	{
		for (AActor* EachFoundActor : FoundActorList)
		{
			IPlayerDamageInterface::Execute_ReceiveDamageFromEnemy(EachFoundActor, BaseDamageAmount, this, CurrentAttackType);
		}
	}

	
}

void AEnemyBrute::TryTriggerPlayerCounter_Implementation()
{
	Super::TryTriggerPlayerCounter_Implementation();

	TArray<AActor*> FoundActorList = GetActorsInFrontOfEnemy(false);

	if(FoundActorList.Num() != 0)
	{
		for (AActor* EachFoundActor : FoundActorList)
		{
			ICharacterActionInterface::Execute_TryStoreCounterTarget(EachFoundActor, this);
		}
	}

}

void AEnemyBrute::OnCounterTimeEnd_Implementation()
{
	Super::OnCounterTimeEnd_Implementation();

	OnHideAttackIndicator();

	if(!IsCountered)
	{
		if(const UWorld* World = GetWorld())
		{
			ACharacter* PlayerCharacterClass = UGameplayStatics::GetPlayerCharacter(GetWorld(),0);
			if(PlayerCharacterClass->GetClass()->ImplementsInterface(UCharacterActionInterface::StaticClass()))
				ICharacterActionInterface::Execute_TryRemoveCounterTarget(PlayerCharacterClass, this);
		}
	}

}

void AEnemyBrute::OnSetIsCountered_Implementation(bool Countered)
{
	Super::OnSetIsCountered_Implementation(Countered);

	SetIsCountered(Countered);
}

void AEnemyBrute::OnStartCounteredAnimation_Implementation()
{
	Super::OnStartCounteredAnimation_Implementation();

	//TrySwitchEnemyState(EEnemyCurrentState::Countered);

	PlayReceiveCounterAnimation();
}

void AEnemyBrute::ReceiveDamageFromPlayer_Implementation(float DamageAmount, AActor* DamageCauser,
	EPlayerAttackType PlayerAttackType)
{
	Super::ReceiveDamageFromPlayer_Implementation(DamageAmount, DamageCauser, PlayerAttackType);
	
	// if player is doing counter attack damage
	if(PlayerAttackType == EPlayerAttackType::CounterAttack)
	{

		if(CounteredTime == 0)
		{
			CounteredTime += 1;
			PlayAnimMontage(DamageReceiveAnimation, 1, "Default");
			return;
		}

		OnStunned();
		
		SetIsCountered(false);


		ICharacterActionInterface::Execute_TryClearCounterVictim(DamageCauser, this);
		

		// TryFinishAttackTask(EEnemyCurrentState::Stunned);
		//
		// TrySwitchEnemyState(EEnemyCurrentState::Stunned);
		//
		// StopAnimMontage();
		// PlayAnimMontage(StunAnimation, 1, "Default");
		//
		// GetWorld()->GetTimerManager().SetTimer(StunningTimerHandle, this, &AEnemyBrute::RecoverFromStunState, TotalStunDuration, false, -1);
		//
		return;
	}

	PlayAnimMontage(DamageReceiveAnimation, 1, "Default");
}

void AEnemyBrute::OnDealAoeDamage_Implementation()
{
	Super::OnDealAoeDamage_Implementation();

	if(NS_JumpSlam)
	{
		// Hit result
		FHitResult Hit;
		// Empty array of ignoring actor, maybe add Enemies classes to be ignored
		TArray<AActor*> IgnoreActors;
		IgnoreActors.Add(this);

		const FVector TraceStart = GetActorLocation();
		FVector DownEnd = TraceStart;
		DownEnd.Z -= 200.0f;
		const bool bHit = UKismetSystemLibrary::LineTraceSingle(this, TraceStart, DownEnd, UEngineTypes::ConvertToTraceType(ECC_Visibility),false, IgnoreActors,  EDrawDebugTrace::None,Hit,true);
		const FVector VFXSpawnPos = bHit ? Hit.Location : GetActorLocation();
		
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), NS_JumpSlam, VFXSpawnPos);
	}
	
	TryFinishAttackTask(EEnemyCurrentState::WaitToAttack);
	
	TArray<AActor*> FoundActorList = GetActorsAroundEnemy();

	if(FoundActorList.Num() != 0)
	{
		for (AActor* EachFoundActor : FoundActorList)
		{
			IPlayerDamageInterface::Execute_ReceiveDamageFromEnemy(EachFoundActor, BaseDamageAmount, this, CurrentAttackType);
		}
	}
}

void AEnemyBrute::InitializeTimelineComp()
{
	// timeline funcitons binding
	FOnTimelineFloat MovingAttackPosUpdate;
	MovingAttackPosUpdate.BindDynamic(this, &AEnemyBrute::UpdateAttackingPosition);
	CounterableMovingTimeline.AddInterpFloat(CounterableAttackMovingCurve, MovingAttackPosUpdate);
	JumpSlamMovingTimeline.AddInterpFloat(JumpSlamHeightCurve, MovingAttackPosUpdate);
	ChargeMovingTimeline.AddInterpFloat(UncounterableAttackMovingCurve, MovingAttackPosUpdate);
}


void AEnemyBrute::PlaySpecificAttackMovingTimeline(EEnemyAttackType EnemyAttack)
{
	// switch case to play specific timeline
	switch (EnemyAttack)
	{
	case EEnemyAttackType::AbleToCounter:
		CounterableMovingTimeline.PlayFromStart();
		break;
	case EEnemyAttackType::UnableToCounter:
		if(BruteAttack == EBruteAttackType::Charging) ChargeMovingTimeline.PlayFromStart();
		else JumpSlamMovingTimeline.PlayFromStart();
		break;
	default: break;
	}
}

EBruteAttackType AEnemyBrute::FindExecutingAttackType()
{

	// TODO: Debug Testing
	// SetExecutingAttackTypes(DebugAttackType);
	// return DebugAttackType;

	const UWorld* World = GetWorld();
	if(!World)
	{
		SetExecutingAttackTypes(EBruteAttackType::Charging);
		return EBruteAttackType::Charging;
	}
	
	// Get player position and brute's position
	const ACharacter* PlayerCha = UGameplayStatics::GetPlayerCharacter(World,0);
	// if somehow player is invalid, return charging
	if(!PlayerCha)
	{
		SetExecutingAttackTypes(EBruteAttackType::Charging);
		return EBruteAttackType::Charging;
	}
	
	const FVector PlayerPos = PlayerCha->GetActorLocation();
	FVector CurrentLocationWithSameZ = GetActorLocation();
	CurrentLocationWithSameZ.Z = PlayerPos.Z;
	
	// if distance is close enough, get random float to see execute swipe or charge 
	const float Distance = UKismetMathLibrary::Vector_Distance(CurrentLocationWithSameZ, PlayerPos);
	if(Distance < DistanceToDecideFarOrClose)
	{
		const float RndChance = UKismetMathLibrary::RandomFloatInRange(0,100);
		if(RndChance < ChanceToExecuteCharging)
		{
			SetExecutingAttackTypes(EBruteAttackType::Charging);
			return EBruteAttackType::Charging;
		}
		
		SetExecutingAttackTypes(EBruteAttackType::Swipe);
		return EBruteAttackType::Swipe;
	}

	// if distance is far enough, execute jump slam
	SetExecutingAttackTypes(EBruteAttackType::JumpSlam);
	return EBruteAttackType::JumpSlam;
}

void AEnemyBrute::SetExecutingAttackTypes(EBruteAttackType ExecutingBruteAction)
{
	BruteAttack = ExecutingBruteAction;

	switch (BruteAttack)
	{
	case EBruteAttackType::Swipe:
		CurrentAttackType = EEnemyAttackType::AbleToCounter;
		break;
	case EBruteAttackType::Charging:
		CurrentAttackType = EEnemyAttackType::UnableToCounter;
		break;
	case EBruteAttackType::JumpSlam:
		CurrentAttackType = EEnemyAttackType::UnableToCounter;
		break;
	default: break;
	}
}

void AEnemyBrute::TestFunction()
{
	PlayAnimMontage(DamageReceiveAnimation, 1, "Default");
}

void AEnemyBrute::TryGetPlayerRef()
{
	// if player reference is nullptr, get player character as reference
	if(!PlayerRef)
	{
		const UWorld* World = GetWorld();
		if(!World) return;
		
		ACharacter* PlayerCha = UGameplayStatics::GetPlayerCharacter(World, 0);
		APlayerCharacter* CastedPlayer = Cast<APlayerCharacter>(PlayerCha);
		if(CastedPlayer)
			PlayerRef = CastedPlayer;
	}
}

void AEnemyBrute::ExecuteAttack()
{
	Super::ExecuteAttack();

	// TODO: Debug Testing
	// {
	// 	FVector PlayerPos = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0)->GetActorLocation();
	//
	// 	FVector CurrentPos = GetActorLocation();
	//
	// 	FVector DirFromPlayer = UKismetMathLibrary::Normal((CurrentPos - PlayerPos));
	//
	// 	float TestDistance = 0;
	//
	// 	EBruteAttackType DecidedAttackType = FindExecutingAttackType();
	// 	SetExecutingAttackTypes(DecidedAttackType);
	//
	// 	if(DecidedAttackType == EBruteAttackType::Swipe)
	// 		TestDistance = 200;
	// 	else if(DecidedAttackType == EBruteAttackType::Charging)
	// 		TestDistance = 1200;
	// 	else
	// 		TestDistance = 700;
	// 	
	//
	// 	SetActorLocation(PlayerPos + (DirFromPlayer * TestDistance));
	// 	
	// }

	// Allocate player reference if player reference variable is not valid
	TryGetPlayerRef();
	
	
	// Update attack indicator's attack type
	if(AttackIndicatorRef)
		AttackIndicatorRef->OnReceivingNewAttackType(CurrentAttackType);

	// reset counter time and update moving variable
	CounteredTime = 0;
	UpdateAttackingVariables();
	IsSecondAttack = false;
	
	
	// switch case on current attack type to fire different animation 
	switch (BruteAttack)
	{
		case EBruteAttackType::Swipe:
			PlayAnimMontage(CounterableAttackMontage, 1, "Default");
			PlaySpecificAttackMovingTimeline(CurrentAttackType);
			break;
		case EBruteAttackType::Charging:
			if(NotCounterableAttackMontage)
				PlayAnimMontage(NotCounterableAttackMontage, 1, "Default");
			DoesPlayerDodge = false;
		
			break;
		case EBruteAttackType::JumpSlam:
			if(JumpSlamAttack)
				PlayAnimMontage(JumpSlamAttack, 1, "Default");
			PlaySpecificAttackMovingTimeline(CurrentAttackType);
			DoesPlayerDodge = false;

			break;
		default: break;
	}
	


}

// TODO: NO NEED 
FVector AEnemyBrute::CalculateDestinationForAttackMoving(FVector PlayerCurrentPos, float CurrentTimelineAlpha)
{
	// Get direction from self to player
	FVector OffsetWithoutZ = PlayerCurrentPos - SelfAttackStartPos;
	OffsetWithoutZ.Z = 0;
	const FVector DirFromSelfToPlayer = UKismetMathLibrary::Normal(OffsetWithoutZ);

	// if enemy's attack is swiping
	if(CurrentAttackType == EEnemyAttackType::AbleToCounter)
	{
		const FVector SupposedAttackMovingDestination = SelfAttackStartPos + (DirFromSelfToPlayer * 50);
		return SupposedAttackMovingDestination;
	
		// // Hit result
		// FHitResult Hit;
		// // Empty array of ignoring actor, maybe add Enemies classes to be ignored
		// TArray<AActor*> IgnoreActors;
		// IgnoreActors.Add(this);
		//
		// // Capsule trace by channel
		// const bool bHit = UKismetSystemLibrary::LineTraceSingle(this, SelfAttackStartPos, SupposedAttackMovingDestination, UEngineTypes::ConvertToTraceType(ECC_Visibility),false, IgnoreActors,  EDrawDebugTrace::None,Hit,true);
		//
		// if(!bHit) return SupposedAttackMovingDestination;
		//
		// const FVector DirFromPlayerToSelf = DirFromSelfToPlayer * -1;
		//
		//
		//
		// return Hit.ImpactPoint + (DirFromPlayerToSelf * OffsetFromPlayer);
	}
	else
	{
		// if enemy's attack is charging
		const FVector CurrentPos = GetActorLocation();
		const float RemainingMovingDistance = ChargingDistance * (1 - CurrentTimelineAlpha);
		const FVector SupposedAttackMovingDestination = CurrentPos + (DirFromSelfToPlayer * RemainingMovingDistance);
		
		return SupposedAttackMovingDestination;
	}
	
}

void AEnemyBrute::UpdateAttackingPosition(float Alpha)
{
	const UWorld* World = GetWorld();
	if(!World) return;
	
	ACharacter* PlayerCha = UGameplayStatics::GetPlayerCharacter(World,0);
	if(!PlayerCha) return;
	
	const FVector PlayerPos = PlayerCha->GetActorLocation();
	const FVector CurrentLocation = GetActorLocation();
	const float CapHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight(); 

	// Hit result
	FHitResult Hit;
	// Empty array of ignoring actor, maybe add Enemies classes to be ignored
	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(this);
	IgnoreActors.Add(PlayerCha);

	

	// Get direction from self to player
	FVector OffsetWithoutZ = PlayerPos - CurrentLocation;
	OffsetWithoutZ.Z = 0;
	const FVector DirFromSelfToPlayer = UKismetMathLibrary::Normal(OffsetWithoutZ);
	
	// if brute's attack is swiping
	if(BruteAttack == EBruteAttackType::Swipe)
	{
		// check if there is enough time and is still far enough
		const float Displacement = UKismetMathLibrary::Vector_Distance(PlayerPos, CurrentLocation);
		
		if(RemainAttackDistance > TravelDistancePerTick && Displacement > 120.0f)
		{
			// time decrement
			RemainAttackDistance -= TravelDistancePerTick;

			
			const FVector SupposedMovingPos = CurrentLocation + (DirFromSelfToPlayer * TravelDistancePerTick);

			// line trace to adjust position on slope
			const FVector UpdatedPos = GetVerticalUpdatedMovePos(SupposedMovingPos, false, 1.0f, CapHalfHeight, IgnoreActors);

			// set location and rotation
			SetActorLocation(UpdatedPos);
			SetActorRotation(UKismetMathLibrary::FindLookAtRotation(CurrentLocation, SupposedMovingPos));
		}
		
		return;
	}

	// check did player dodge, if player did dodge, set it to true
	if(!DoesPlayerDodge)
	{
		if(CheckIfPlayerDodge()) DoesPlayerDodge = true;
	}
	
	// if brute's attack is charging
	if(BruteAttack == EBruteAttackType::Charging)
	{
		// check if player should keep charging
		const bool ShouldCharge = ShouldKeepCharging(DirFromSelfToPlayer);
		if(!ShouldCharge)
		{
			OnCancelCharge(false);
			return;
		}
		
		// distance decrement
		RemainAttackDistance -= TravelDistancePerTick;


		// update charging direction if player has not dodge yet
		FVector ChargingDirection = GetActorForwardVector();
		if(!DoesPlayerDodge) ChargingDirection = DirFromSelfToPlayer;

		
		const FVector SupposedMovingPos = CurrentLocation + (ChargingDirection * TravelDistancePerTick);
		
		// line trace to adjust position on slope
		const FVector UpdatedPos = GetVerticalUpdatedMovePos(SupposedMovingPos, false, 1.0f, CapHalfHeight, IgnoreActors);

		
		SetActorLocation(UpdatedPos);
		
		// record moving direction
		FVector AttackMovingOffsetWithoutZ = UpdatedPos - CurrentLocation;
		AttackMovingOffsetWithoutZ.Z = 0;
		AttackMovingDir = UKismetMathLibrary::Normal(AttackMovingOffsetWithoutZ);
		SetActorRotation(UKismetMathLibrary::FindLookAtRotation(CurrentLocation, SupposedMovingPos));
		
		
		// Check if brute is charging to wall
		const FVector TraceCheckingPos = UpdatedPos + (ChargingDirection * (TravelDistancePerTick * 5));

		// Capsule trace by channel
		const bool bHitWall = UKismetSystemLibrary::CapsuleTraceSingle(this, UpdatedPos, TraceCheckingPos, 15.0f, CapHalfHeight / 2,
			UEngineTypes::ConvertToTraceType(ECC_Camera),false, IgnoreActors,  EDrawDebugTrace::None,Hit,true);

		// if charge to wall, cancel charge
		if(bHitWall) OnCancelCharge(true);
		
		return;
	}


	// if Brute is performing Jump Slam

	// distance decrement
	RemainAttackDistance -= TravelDistancePerTick;

	// get ground curve alpha
	const float GroundPlayBackPos = JumpSlamMovingTimeline.GetPlaybackPosition();
	const float GroundAlpha = JumpSlamDistanceCurve->GetFloatValue(GroundPlayBackPos);
	
	// Check if player does dodge, if not, update JumpSlamPosition
	if(!DoesPlayerDodge && GroundPlayBackPos < 1.9) EndJumpingLocation = GetJumpSlamPosition((DirFromSelfToPlayer * -1), PlayerPos);

	// get supposed moving position with
	FVector SupposedMovingPos = UKismetMathLibrary::VLerp(StartJumpingLocation, EndJumpingLocation, GroundAlpha);
	float SupposedHeight = UKismetMathLibrary::Lerp(StartJumpingLocation.Z, StartJumpingLocation.Z + JumpSlamHeight, Alpha);
	SupposedMovingPos.Z = SupposedHeight;

	// line trace to adjust position on slope
	const FVector UpdatedPos = GetVerticalUpdatedMovePos(SupposedMovingPos, true, GroundAlpha, CapHalfHeight, IgnoreActors);

	
	SetActorLocation(UpdatedPos);
}

bool AEnemyBrute::CheckIfPlayerDodge()
{
	if(!PlayerRef) return false;
	
	return PlayerRef->GetCurrentActionState() == EActionState::Dodge;
}

void AEnemyBrute::OnCancelCharge(const bool bHitWall)
{
	// stop montage and timeline
	StopAnimMontage();
	ChargeMovingTimeline.Stop();
	// reset capsule component collision , focus and MovementMode setting
	SetCapsuleCompCollision(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Block);
	OnSetFocus();
	TryResumeMoving();

	// if charage cancel result is wall hitting, stun function
	if(bHitWall) OnStunned();
	else TryFinishAttackTask(EEnemyCurrentState::WaitToAttack);
}

// TODO: NO NEED
FVector AEnemyBrute::GetChargeDirection(FVector DirToPlayer, FVector ActorCurrentPos)
{
	const FVector ForwardDir = GetActorForwardVector();
	const FVector RightDir = GetActorRightVector();
	const FVector LeftDir = RightDir * -1 ;


	const float DotResult = UKismetMathLibrary::Dot_VectorVector(RightDir,DirToPlayer);
	const bool IsRight = DotResult > 0;

	const float DotAngleOffset = 90 * (1 - (UKismetMathLibrary::Abs(DotResult)));

	const UWorld* World = GetWorld();
	if(!World) return FVector(0,0,0);
	
	const float DeltaSecond = World->GetDeltaSeconds();
	const float NumOfTickPerSecond = 1 / DeltaSecond;
	const float TrackingAnglePerTick = ChargeTrackingAngle / NumOfTickPerSecond;
	
	if(DotAngleOffset < TrackingAnglePerTick) return DirToPlayer;


	
	const float ReturnAlpha = TrackingAnglePerTick / 90;

	if(IsRight)
		return UKismetMathLibrary::VLerp(ForwardDir, RightDir, ReturnAlpha);
	

	return UKismetMathLibrary::VLerp(ForwardDir, LeftDir, ReturnAlpha);
}

FVector AEnemyBrute::GetJumpSlamPosition(FVector DirFromPlayerToSelf, FVector PlayerPos)
{
	return PlayerPos + (DirFromPlayerToSelf * 100);
}


void AEnemyBrute::OnContinueSecondAttackMontage_Implementation()
{
	Super::OnContinueSecondAttackMontage_Implementation();

	if(!PlayerRef) TryGetPlayerRef();
	
	// make brute rotate to player
	const FVector PlayerPos = PlayerRef->GetActorLocation();
	const FVector CurrentPos = GetActorLocation();
	SetActorRotation(UKismetMathLibrary::FindLookAtRotation(CurrentPos, PlayerPos));
	
	PlaySpecificAttackMovingTimeline(CurrentAttackType);

	// update moving variable again
	UpdateAttackingVariables();

	// if second attack is swiping
	if(CurrentAttackType == EEnemyAttackType::AbleToCounter)
	{
		IsSecondAttack = true;
		
		StopAnimMontage();
		PlayAnimMontage(CouterableAttackSecond, 1, "Default");
		PlaySpecificAttackMovingTimeline(CurrentAttackType);
		return;
	}

	// if second attack action is charging
	OnStopFocusing();
	StopAnimMontage();
	PlayAnimMontage(UnableCounterAttackSecond, 1, "Default");

	// set capsule component collision to overlaping
	SetCapsuleCompCollision(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);

	

}

TArray<AActor*> AEnemyBrute::GetActorsInFrontOfEnemy(bool IsDamaging)
{
	TArray<AActor*> FoundActorList;
	
	const UWorld* World = GetWorld();
	if(!World) return FoundActorList; 

	// get specific socket position
	FName AttackSocketName;
	if(!IsSecondAttack)
		AttackSocketName = "LeftHand";
	else
		AttackSocketName = "RightHand";
	const FVector HeadSocketLocation = GetMesh()->GetSocketLocation(AttackSocketName);

	// get ignoring actors for later overlap function
	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(this);
	
	TArray<AEnemyBase*> Allies = OwnController->GetAllies();
	if(Allies.Num() > 0)
		for (AEnemyBase* EachAlly : Allies)
		{
			IgnoreActors.Add(EachAlly);
		}

	// radius and height depend on damaging player or update counter target
	float Radius;
	float Height;
	
	if(IsDamaging)
	{
		Radius = DamageTriggerRadius;
		Height = DamageTriggerHeight;
	}
	else
	{
		Radius = CounterTriggerRadius;
		Height = CounterTriggerHeight;
	}
	
	UKismetSystemLibrary::CapsuleOverlapActors(World, HeadSocketLocation, Radius, Height, FilterType, FilteringClass, IgnoreActors, FoundActorList);
	return FoundActorList;
}

TArray<AActor*> AEnemyBrute::GetActorsAroundEnemy()
{
	TArray<AActor*> FoundActorList;
	
	const UWorld* World = GetWorld();
	if(!World) return FoundActorList; 

	FVector StartLocation = GetActorLocation();
	const FVector DownwardEnd = StartLocation + ((GetActorUpVector() * -1) * 100.0f);

	// Hit result
	FHitResult Hit;
	// Empty array of ignoring actor, maybe add Enemies classes to be ignored
	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(this);
	
	const bool bHit = UKismetSystemLibrary::LineTraceSingle(this, StartLocation, DownwardEnd, UEngineTypes::ConvertToTraceType(ECC_Visibility),false, IgnoreActors,  EDrawDebugTrace::None,Hit,true);
	if(bHit) StartLocation.Z = Hit.Location.Z;
	
	TArray<AEnemyBase*> Allies = OwnController->GetAllies();
	if(Allies.Num() > 0)
		for (AEnemyBase* EachAlly : Allies)
		{
			IgnoreActors.Add(EachAlly);
		}
	
	UKismetSystemLibrary::SphereOverlapActors(World, StartLocation, AoeDamageRadius, FilterType, FilteringClass, IgnoreActors, FoundActorList);
	
	return FoundActorList;
}

void AEnemyBrute::OnDeath()
{
	Super::OnDeath();
}

void AEnemyBrute::TryStopAttackMovement()
{
}

void AEnemyBrute::UpdateAttackingVariables()
{
	float AttackTravelDistance = 0;
	float TotalTravelTime = 0;
	if(BruteAttack == EBruteAttackType::Swipe)
	{
		AttackTravelDistance = AttackTravelDistance_Swipe;
		TotalTravelTime = ToTalTravelTime_Swipe;
	}
	else if(BruteAttack == EBruteAttackType::Charging)
	{
		AttackTravelDistance = AttackTravelDistance_Charge;
		TotalTravelTime = TotalTravelTime_Charging;
	}
	else
	{
		AttackTravelDistance = AttackTravelDistance_JumpSlam;
		TotalTravelTime = TotalTravelTime_JumpSlam;
		StartJumpingLocation = GetActorLocation();
	}
	
	RemainAttackDistance = AttackTravelDistance;
	MaxTravelDistance = AttackTravelDistance;

	const UWorld* World = GetWorld();
	if(!World) return;
	const float DeltaSecond = World->GetDeltaSeconds();
	const float NumOfTickPerSecond = 1 / DeltaSecond;
	TravelDistancePerTick = AttackTravelDistance / (TotalTravelTime * NumOfTickPerSecond);
}

bool AEnemyBrute::ShouldKeepCharging(FVector DirToPlayer)
{
	// if brute charge through player, or finish traveling, end task and timeline
	if(RemainAttackDistance < TravelDistancePerTick * 3)
	{
		return false;
	}

	// if(AttackMovingDir != FVector(0,0,0))
	// {
	// 	const float DotResult = UKismetMathLibrary::Dot_VectorVector(AttackMovingDir, DirToPlayer);
	// 	if(DotResult < 0.2)
	// 	{
	// 		return false;
	// 	}
	// }

	return true;
}

void AEnemyBrute::PlayReceiveCounterAnimation()
{
}

void AEnemyBrute::ChargeKnock(AActor* KnockingActor)
{
	// calculate knocking force with direction and angle
	const FVector CharForwardDir = GetActorForwardVector();
	const FVector CharUpDir = GetActorUpVector();
	
	const float UpForce = UKismetMathLibrary::DegSin(ChargeKnockAngle) * ChargeKnockForce;
	const float ForwardForce = UKismetMathLibrary::DegCos(ChargeKnockAngle) * ChargeKnockForce;

	const FVector ForwardForceVector = CharForwardDir * ForwardForce;
	const FVector UpForceVector = CharUpDir * UpForce;

	const FVector KnockForce = ForwardForceVector + UpForceVector;
	const FVector KnockDirection = CharForwardDir + CharUpDir;
	
	if(KnockingActor->GetClass()->ImplementsInterface(UPlayerDamageInterface::StaticClass())) ICharacterActionInterface::Execute_OnApplyChargeKnockForce(KnockingActor, KnockForce, KnockDirection);
}

void AEnemyBrute::SetCapsuleCompCollision(ECollisionChannel ResponseChannel, ECollisionResponse RequestResponse)
{
	UCapsuleComponent* CapsuleComp = GetCapsuleComponent();
	if(CapsuleComp)
		CapsuleComp->SetCollisionResponseToChannel(ResponseChannel, RequestResponse);
}

void AEnemyBrute::OnDealChargeDamage(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
											UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	TryGetPlayerRef();
	if (!PlayerRef) return;

	if(CheckIfPlayerDodge() || PlayerRef->GetCurrentActionState() == EActionState::SpecialAttack) return;

	
	// check if owner class has player damage interface
	if(OtherActor->GetClass()->ImplementsInterface(UPlayerDamageInterface::StaticClass()))
	{
		
		StopAnimMontage();
		SetCapsuleCompCollision(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Block);
		TryResumeMoving();
		ChargeMovingTimeline.Stop();
		ChargeKnock(OtherActor);
		// if it has IPlayer DamageInterface, it means its player character, execute its Execute_ReceiveDamageFromEnemy function
		IPlayerDamageInterface::Execute_ReceiveDamageFromEnemy(OtherActor, BaseDamageAmount, this, CurrentAttackType);
		TryFinishAttackTask(EEnemyCurrentState::WaitToAttack);
	}
}
