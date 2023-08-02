// Copyright (C) The Tentacle Zone 2023. All Rights Reserved.


#include "Characters/Enemies/EnemyRanged.h"

#include "Characters/Player/PlayerDamageInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"


void AEnemyRanged::ExecuteAttack()
{
	Super::ExecuteAttack();

	// if(!CheckCanSeePlayer())
	// {
	// 	TryFinishAttackTask(EEnemyCurrentState::WaitToAttack);
	// 	return;
	// }

	PlayAnimMontage(KneelDownToAimAnim, 1, "Default");
}

void AEnemyRanged::BeginFire()
{
	// Hide the Attack indicator
	OnHideAttackIndicator();
	
	// Start fire animation
	if(CurrentEnemyState != EEnemyCurrentState::Attacking) return;

	StopAnimMontage();
	PlayAnimMontage(KneelFireAnim,1, "Default");
}

void AEnemyRanged::InSightConditionUpdate()
{
	const bool IsInCameraSight = WasRecentlyRendered(CheckInSightTick);

	ShowOrHidePlayerHUD(IsInCameraSight);
}

void AEnemyRanged::OnDeath()
{
	Super::OnDeath();

	if(CurrentEnemyState == EEnemyCurrentState::Attacking)
	{
		// Stop current montage
		StopAnimMontage();

		// Stop timers
		StopAimingTimer();
		StopCheckInSightTimer();
		
		// Stop Both attack indicators
		OnHideAttackIndicator();
		SpawnOrCollapsePlayerHUD(false);
		
		// Finish task
		TryFinishAttackTask(EEnemyCurrentState::Damaged);
	}
}

AActor* AEnemyRanged::GetDamageActorByLineTrace()
{
	// Hit result
	FHitResult Hit;
	// Empty array of ignoring actor, maybe add Enemies classes to be ignored
	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(this);
	
	// Capsule trace by channel
	const FVector CurrentPos = GetActorLocation();
	const FVector FacingDir = GetActorForwardVector();
	const FVector AimingDestination = CurrentPos + (FacingDir * AimingRange);
	
	const bool bHit = UKismetSystemLibrary::LineTraceSingle(this, CurrentPos, AimingDestination, UEngineTypes::ConvertToTraceType(ECC_Camera),false, IgnoreActors,  EDrawDebugTrace::None,Hit,true);

	if(bHit) return Hit.GetActor();

	return nullptr;
}

void AEnemyRanged::StopAimingTimer()
{
	const UWorld* World = GetWorld();
	if(!World) return;

	World->GetTimerManager().ClearTimer(AimingTimerHandle);
}

void AEnemyRanged::StopCheckInSightTimer()
{
	const UWorld* World = GetWorld();
	if(!World) return;

	World->GetTimerManager().ClearTimer(CheckInSightTimerHandle);
}

bool AEnemyRanged::TryCachePlayerRef()
{
	// early return Cache result to true if already has player character reference
	if(PlayerRef) return true;

	// if PlayerRef is nullptr, get player character from world
	const UWorld* World = GetWorld();
	if(!World) return false;
	ACharacter* PlayerCha = UGameplayStatics::GetPlayerCharacter(World, 0);
	if(!PlayerCha) return false;

	// if the result is not nullptr, store it and return Cache result to true
	PlayerRef = PlayerCha;
	return true;
}

void AEnemyRanged::SpawnOrCollapsePlayerHUD(bool Spawn)
{
	const bool PlayerRefCacheResult = TryCachePlayerRef();

	if(!PlayerRefCacheResult) return;
	
	if(PlayerRef->GetClass()->ImplementsInterface(UCharacterActionInterface::StaticClass()))
		ICharacterActionInterface::Execute_OnShowPlayerIndicatorHUD(PlayerRef, Spawn);
}

void AEnemyRanged::ShowOrHidePlayerHUD(bool Show)
{
	const bool PlayerRefCacheResult = TryCachePlayerRef();
	
	if(!PlayerRefCacheResult) return;

	if(PlayerRef->GetClass()->ImplementsInterface(UCharacterActionInterface::StaticClass()))
		ICharacterActionInterface::Execute_OnChangePlayerIndicatorHUD_Visibility(PlayerRef, Show);
}

void AEnemyRanged::OnRifleBeginAiming_Implementation()
{
	IEnemyRangeInterface::OnRifleBeginAiming_Implementation();

	// Set timer to shoot
	const UWorld* World = GetWorld();
	if(!World) return;

	World->GetTimerManager().SetTimer(AimingTimerHandle, this, &AEnemyRanged::BeginFire, AimTimeToShoot, false, -1);

	// Start aiming loop animation
	StopAnimMontage();
	PlayAnimMontage(KneelAimingLoopAnim,1, "Default");

	// Show attack indicator
	if(AttackIndicatorRef)
		AttackIndicatorRef->ShowIndicator();

	// Show Player attack indicator HUD
	SpawnOrCollapsePlayerHUD(true);

	
	World->GetTimerManager().SetTimer(CheckInSightTimerHandle, this, &AEnemyRanged::InSightConditionUpdate, CheckInSightTick, true, -1);
}

void AEnemyRanged::OnRifleFinishFiring_Implementation()
{
	IEnemyRangeInterface::OnRifleFinishFiring_Implementation();

	// Start aiming loop animation
	StopAnimMontage();
	PlayAnimMontage(StandUpAnim,1, "Default");
}

void AEnemyRanged::TryToDamagePlayer_Implementation()
{
	Super::TryToDamagePlayer_Implementation();

	SpawnOrCollapsePlayerHUD(false);
	StopCheckInSightTimer();

	// Play fire sound and VFX

	
	const FVector MuzzlePos = RifleMeshRef ? RifleMeshRef->GetSocketLocation("Muzzle") : GetActorLocation();
	UGameplayStatics::PlaySoundAtLocation(GetWorld(), RifleFireSound, MuzzlePos);
	
	AActor* SupposeDamageActor = GetDamageActorByLineTrace();
	if(!SupposeDamageActor) return;

	
	if(SupposeDamageActor->GetClass()->ImplementsInterface(UPlayerDamageInterface::StaticClass()))
		IPlayerDamageInterface::Execute_ReceiveDamageFromEnemy(SupposeDamageActor, BaseDamageAmount, this, EEnemyAttackType::UnableToCounter);
}

void AEnemyRanged::ReceiveDamageFromPlayer_Implementation(float DamageAmount, AActor* DamageCauser,
	EPlayerAttackType PlayerAttackType)
{
	Super::ReceiveDamageFromPlayer_Implementation(DamageAmount, DamageCauser, PlayerAttackType);

	bool StateChanged = false;
	const EEnemyCurrentState InitialState = CurrentEnemyState;
	
	// if enemy is attack, stop montage, cancel fire timer, unshow attack indicator, and execute onfinish attack delegate
	if(CurrentEnemyState == EEnemyCurrentState::Attacking)
	{
		// Stop current montage
		StopAnimMontage();

		// Stop timers
		StopAimingTimer();
		StopCheckInSightTimer();
		
		// Stop Both attack indicators
		OnHideAttackIndicator();
		SpawnOrCollapsePlayerHUD(false);
		
		// Finish task
		TryFinishAttackTask(EEnemyCurrentState::Damaged);

		// change StateChanged bool to true to prevent changing again
		StateChanged = true;

	}
	
	// if bool StateChanged is false, it means enemy is not taking damage when it get countered or get damaged while doing attack
	if(!StateChanged) TrySwitchEnemyState(EEnemyCurrentState::Damaged);
}

void AEnemyRanged::OnReloading_Implementation()
{
	IEnemyRangeInterface::OnReloading_Implementation();

	UGameplayStatics::PlaySoundAtLocation(GetWorld(), RifleReloadSound, GetActorLocation());
}


bool AEnemyRanged::CheckCanSeePlayer()
{
	const UWorld* World = GetWorld();
	if(!World) return false;
	
	const ACharacter* PlayerCha = UGameplayStatics::GetPlayerCharacter(World, 0);
	if(PlayerCha) return false;
	
	const FVector EnemyLocation = GetActorLocation();
	const FVector PlayerLocation = PlayerCha->GetActorLocation();
	
	FHitResult Hit;
	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(this);
	const bool IsHit = UKismetSystemLibrary::CapsuleTraceSingle(GetWorld(), EnemyLocation, PlayerLocation, 10.0f, 20.0f , UEngineTypes::ConvertToTraceType(ECC_Camera), false, IgnoreActors, EDrawDebugTrace::None,Hit,true);
	
	
	if(IsHit) return false;

	return true;
}
