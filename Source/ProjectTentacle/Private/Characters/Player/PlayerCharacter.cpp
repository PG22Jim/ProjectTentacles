// Copyright (C) The Tentacle Zone 2023. All Rights Reserved.


#include "Characters/Player/PlayerCharacter.h"

#include "NavigationSystem.h"
#include "ProjectTentacleGameInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"
#include "ProjectTentacle/ProjectTentacleGameModeBase.h"
#include "UI/UserWidget_HitIndicator.h"

FGenericTeamId APlayerCharacter::TeamId = FGenericTeamId(1);
// ==================================================== Constructor =========================================

void APlayerCharacter::StartSwampDamageTick(float Damage, float TickInterval)
{
	// If taking damage already, ignore
	if(bTakingSwampDamage) return;

	bTakingSwampDamage = true;
	// Start timer on damage method
	SwampDamageDelegate.BindUFunction(this, FName("DealSwampDamage"), Damage, TickInterval);
	FTimerHandle SwampDamageTimer;
	GetWorldTimerManager().SetTimer(SwampDamageTimer, SwampDamageDelegate, TickInterval, false);
}

void APlayerCharacter::DealSwampDamage(float Damage, float TickTime)
{
	HealthReduction(Damage);
	
	if(!bTakingSwampDamage) return;
	FTimerHandle SwampDamageTimer;
	GetWorldTimerManager().SetTimer(SwampDamageTimer, SwampDamageDelegate, TickTime, false);
}

void APlayerCharacter::ToggleOHKO()
{
	bIsOHKOEnabled = ~bIsOHKOEnabled;
}

void APlayerCharacter::StopSwampDamageTick()
{
	// If not taking damage ignore
	if(!bTakingSwampDamage) return;
	
	bTakingSwampDamage = false;
}

void APlayerCharacter::ShowHitIndicator(const float CounterTime, const FVector HitLocation) const
{
	HUDRef->PopIndicator(CounterTime, HitLocation);
}

void APlayerCharacter::CollapseHitIndicator() const
{
	HUDRef->CollapseIndicator();
}

APlayerCharacter::APlayerCharacter()
{
	CreatCameraComponents();
	
	StimuliSource = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("Stimuli Source"));
	StimuliSource->bAutoRegister = true;
	StimuliSource->RegisterForSense(UAISense_Sight::StaticClass());
}

FGenericTeamId APlayerCharacter::GetGenericTeamId() const
{
	return TeamId;
}

void APlayerCharacter::CreatCameraComponents()
{
	USkeletalMeshComponent* PlayerSkeletonMeshComp = GetMesh();
	if(!PlayerSkeletonMeshComp) return;

	UCapsuleComponent* PlayerCapsule = GetCapsuleComponent();
	
	// // Create a camera boom (pulls in towards the player if there is a collision)
	// ShoulderViewSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("ShoulderViewSpringArm"));
	// ShoulderViewSpringArm->SetupAttachment(PlayerCapsule);
	// //CameraBoom->SetupAttachment(PlayerSkeletonMeshComp, "Spine");
	// ShoulderViewSpringArm->TargetArmLength = 300.0f; // The camera follows at this distance behind the character	
	// ShoulderViewSpringArm->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a camera boom (pulls in towards the player if there is a collision)
	CombatSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CombatSpringArm"));
	CombatSpringArm->SetupAttachment(PlayerCapsule);
	//CameraBoom->SetupAttachment(PlayerSkeletonMeshComp, "Spine");
	CombatSpringArm->TargetArmLength = 300.0f; // The camera follows at this distance behind the character	
	CombatSpringArm->bUsePawnControlRotation = true; // Rotate the arm based on the controller
	
	ExecutionSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("ExecutionSpringArm"));
	ExecutionSpringArm->SetupAttachment(PlayerCapsule);
	//ActionSpringArmComp->SetupAttachment(PlayerSkeletonMeshComp, "Spine2");
	ExecutionSpringArm->TargetArmLength = 300.0f; // The camera follows at this distance behind the character	
	ExecutionSpringArm->bUsePawnControlRotation = true; // Rotate the arm based on the controller


	// // Create a shoulder view camera
	// NormalCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("NormalCamera"));
	// NormalCamera->SetupAttachment(ShoulderViewSpringArm, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	// NormalCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm
	//
	// NormalCameraChild = CreateDefaultSubobject<UChildActorComponent>(TEXT("NormalCameraChild"));
	// NormalCameraChild->SetupAttachment(NormalCamera, USpringArmComponent::SocketName);

	
	// Create a combat camera
	CombatCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("CombatCamera"));
	CombatCamera->SetupAttachment(CombatSpringArm, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	CombatCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	CombatCameraChild = CreateDefaultSubobject<UChildActorComponent>(TEXT("CombatCameraChild"));
	CombatCameraChild->SetupAttachment(CombatCamera, USpringArmComponent::SocketName);
	
	// Create a execution camera
	ExecutionCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ExecutionCamera"));
	ExecutionCamera->SetupAttachment(ExecutionSpringArm, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	ExecutionCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	ExecutionCameraChild = CreateDefaultSubobject<UChildActorComponent>(TEXT("ExecutionCameraChild"));
	ExecutionCameraChild->SetupAttachment(ExecutionCamera, USpringArmComponent::SocketName);
}

void APlayerCharacter::TryCachePlayerController()
{
	if(PlayerCurrentController) return;

	const UWorld* World = GetWorld();
	if(!World) return;

	PlayerCurrentController = UGameplayStatics::GetPlayerController(World,0);
}


void APlayerCharacter::TimelineInitialization()
{
	FOnTimelineFloat CameraRotationUpdate;
	CameraRotationUpdate.BindDynamic(this, &APlayerCharacter::OnUpdatingCameraMovement);
	
	FOnTimelineEvent CameraRotationFinish;
	CameraRotationFinish.BindDynamic(this, &APlayerCharacter::OnFinishCameraMovement);
	
	CameraSwitchingTimeline.AddInterpFloat(CameraRotationCurve, CameraRotationUpdate);
	CameraSwitchingTimeline.SetTimelineFinishedFunc(CameraRotationFinish);


	FOnTimelineFloat CombatCameraSwitchUpdate;
	CombatCameraSwitchUpdate.BindDynamic(this, &APlayerCharacter::OnEnterCombatCameraUpdate);
	CombatCameraSwitchTimeline.AddInterpFloat(CameraSwitchingCurve, CombatCameraSwitchUpdate);
	

	FOnTimelineFloat TentacleMaterialUpdate;
	TentacleMaterialUpdate.BindDynamic(this, &APlayerCharacter::OnUpdateTentacleMaterial);
	TentacleMaterialChangingTimeline.AddInterpFloat(MaterialChangingCurve, TentacleMaterialUpdate);
}

void APlayerCharacter::TentacleAttachment()
{
	if(UWorld* World = GetWorld())
	{
		// set spawn parameter
		FActorSpawnParameters StunTentacleParams;
		StunTentacleParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		// Get ground location by using line trace
		USkeletalMeshComponent* PlayerMesh = GetMesh();
		FTransform TentacleSpawnTrans = GetMesh()->GetSocketTransform("TentacleSocket");
		
		// spawn stun tentacle
		TentacleOnRightHand = World->SpawnActor<AAttachingTentacle>(StunTentacleClass, TentacleSpawnTrans, StunTentacleParams);

		if(!TentacleOnRightHand) return;
		
		const FAttachmentTransformRules AttachmentRule = FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true);
		TentacleOnRightHand->AttachToComponent(PlayerMesh, AttachmentRule, "TentacleSocket");
	}
}

// =================================== Begin Play, Set up InputComponent, Tick ==============================
void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Variable set up
	CharacterCurrentHealth = CharacterMaxHealth;
	GameModeRef = nullptr;
	bIsDead = false;
	bIsOHKOEnabled = false;

	// initialize timeline and tentacle on player's hand
	TimelineInitialization();

	TentacleAttachment();
	
	TryCacheInstanceRef();
	if(InstanceRef && InstanceRef->ShouldSaveAtPCSpawn()) InstanceRef->SaveGame();

	InCombatCameraSocketOffset = CombatSpringArm->SocketOffset;
}

void APlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// time line tick
	CameraSwitchingTimeline.TickTimeline(DeltaSeconds);
	CombatCameraSwitchTimeline.TickTimeline(DeltaSeconds);
	
	const float DeltaWithSpeedBonus = DeltaSeconds * UpdatedAttackingSpeedBonus;
	TentacleMaterialChangingTimeline.TickTimeline(DeltaWithSpeedBonus);

	TryCachePlayerController();
	
	// reset input vector
	if(PlayerCurrentController->WasInputKeyJustReleased(MovingForwardKey) || PlayerCurrentController->WasInputKeyJustReleased(MovingBackKey))
	{
		InputDirection.SetPreviousInputDirectionY(InputDirection.GetInputDirectionY());
		InputDirection.SetInputDirectionY(0.0f);
	}
	
	if(PlayerCurrentController->WasInputKeyJustReleased(MovingLeftKey) || PlayerCurrentController->WasInputKeyJustReleased(MovingRightKey))
	{
		InputDirection.SetPreviousInputDirectionX(InputDirection.GetInputDirectionX());
		InputDirection.SetInputDirectionX(0.0f);
	}
}


// ==================================================== Movement ==============================================
void APlayerCharacter::LookUpAtRate(float Value)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Value * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void APlayerCharacter::TurnAtRate(float Value)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Value * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void APlayerCharacter::MoveForward(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// Set input direction Y value
		InputDirection.SetPreviousInputDirectionY(InputDirection.GetInputDirectionY());
		InputDirection.SetInputDirectionY(Value);
		
		if(CurrentActionState == EActionState::Idle) AddMovementInput(Direction, Value);
	}
}

void APlayerCharacter::MoveRight(float Value)
{
	if ( (Controller != nullptr) && (Value != 0.0f))
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
	
		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		InputDirection.SetPreviousInputDirectionX(InputDirection.GetInputDirectionX());
		InputDirection.SetInputDirectionX(Value);
		
		// add movement in that direction
		if(CurrentActionState == EActionState::Idle) AddMovementInput(Direction, Value);
	}
}


// // ====================================================== Attack ==============================================
void APlayerCharacter::TryMeleeAttack()
{
	// if player is recovering from action or is dodging, return
	if(CanPerformAttack())
	{
		bool bExecuted = OnExecutePlayerAction.ExecuteIfBound(EActionState::Attack);
		if(bExecuted) TentacleOnRightHand->SetTentacleInvisible();
	}
	
}

// ====================================================== Evade ===============================================
void APlayerCharacter::TryEvade()
{
	// if player is able to dodge, make dodge
	//if(CheckCanPerformAction())
	if(IsPlayerCounterable && CounteringVictim && (CurrentActionState != EActionState::SpecialAttack && CurrentActionState != EActionState::Dodge))
		bool bExecuted = OnExecutePlayerAction.ExecuteIfBound(EActionState::Evade);
}


void APlayerCharacter::TryDodge()
{
	// if player is able to dodge, make dodge
	if(CanPerformDodge() && CurrentStamina > CostForEachDodge)
	{
		StopAnimMontage();	
		StopRegenerateStamina();
		bool bExecuted = OnExecutePlayerAction.ExecuteIfBound(EActionState::Dodge);

		// cost stamina and reset stamina
		CurrentStamina -= CostForEachDodge;
		
		WaitToRegenStamina();
	}
}

bool APlayerCharacter::CanPerformAttack()
{
	// only in idle or preaction state can perform attack
	return CurrentActionState == EActionState::Idle || CurrentActionState == EActionState::PreAction;
}

bool APlayerCharacter::CanPerformDodge()
{
	// only in before attack, idle or preaction state can perform dodge
	return CurrentActionState == EActionState::BeforeAttack || CurrentActionState == EActionState::Idle || CurrentActionState == EActionState::PreAction;
}

void APlayerCharacter::OnFinishCameraMovement()
{
	if(CurrentCameraType == EPlayerCameraType::InCombat)
	{
		// Enable Camera control
		AbleRotateVision = true;
	}
}

void APlayerCharacter::OnUpdatingCameraMovement(float Alpha)
{
	if(!PlayerCurrentController) TryCachePlayerController();

	const FRotator SettingRotation = UKismetMathLibrary::RLerp(CurrentCameraRotation, ExecutionCameraRotation, Alpha, true);

	PlayerCurrentController->SetControlRotation(SettingRotation);
}

void APlayerCharacter::TrySpecialAbility1()
{
	// if player is recovering from action or is dodging, return
	if(CanPerformAttack())
		bool bExecuted = OnExecutePlayerAction.ExecuteIfBound(EActionState::SpecialAbility1);
}

void APlayerCharacter::TrySpecialAbility2()
{
	// if player is recovering from action or is dodging, return
	if(CanPerformAttack())
		bool bExecuted = OnExecutePlayerAction.ExecuteIfBound(EActionState::SpecialAbility2);
}

// void APlayerCharacter::TrySpecialAbility3()
// {
// 	// if player is recovering from action or is dodging, return
// 	if(CheckCanPerformAction())
// 		bool bExecuted = OnExecutePlayerAction.ExecuteIfBound(EActionState::SpecialAbility3);
// }

// ================================================ Utility ===========================================================
void APlayerCharacter::UnsetCurrentTarget()
{
	if(TargetActor != nullptr)
	{
		// if target actor is not nullptr, unshow its target icon, and clear the reference of target actor
		if(TargetActor->GetClass()->ImplementsInterface(UEnemyWidgetInterface::StaticClass()))
		{
			IEnemyWidgetInterface::Execute_UnShowPlayerTargetIndicator(TargetActor);
			TargetActor = nullptr;
		}
	}

	
}

void APlayerCharacter::DebugTestFunction()
{
	if(DebugingBool)
	{
		DebugingBool = false;
		CameraSwitch_ShoulderView();
		
	}
	else
	{
		DebugingBool = true;
		CameraSwitch_CombatView();
	}
}

void APlayerCharacter::ResumeSimulatePhysic()
{
	UCapsuleComponent* PlayerCap = GetCapsuleComponent();
	if(!PlayerCap) return;

	PlayerCap->SetSimulatePhysics(false);
}

void APlayerCharacter::OnUpdateTentacleMaterial(float Alpha)
{
	if(!TentacleOnRightHand) return;

	TentacleOnRightHand->SetMaterialValue(Alpha);
}

void APlayerCharacter::OnCancelTentacleMaterialChange()
{
	if(!TentacleOnRightHand) return;

	TentacleMaterialChangingTimeline.Stop();
	TentacleOnRightHand->SetTentacleInvisible();
}

void APlayerCharacter::OnEnterCombatCameraUpdate(float Alpha)
{
	const FVector CurrentSocketOffset = UKismetMathLibrary::VLerp(ShoulderViewSocketOffset, InCombatCameraSocketOffset, Alpha);
	CombatSpringArm->SocketOffset = CurrentSocketOffset;
}

void APlayerCharacter::SetRangeAimingEnemy(AEnemyBase* NewRegisteringActor, float HUDRemainTime)
{
	if(RangeAimingEnemy != NewRegisteringActor)
		RangeAimingEnemy = NewRegisteringActor;

	IndicatorHUDRemainTime = HUDRemainTime;
}

void APlayerCharacter::SetTargetActor(AEnemyBase* NewTargetActor)
{
	UnsetCurrentTarget();
	
	if(NewTargetActor->GetClass()->ImplementsInterface(UEnemyWidgetInterface::StaticClass()))
		IEnemyWidgetInterface::Execute_ShowPlayerTargetIndicator(NewTargetActor);
	
	TargetActor = NewTargetActor;
}

// =============================================== Stamina Regen ================================================
void APlayerCharacter::WaitToRegenStamina()
{
	const UWorld* World = GetWorld();
	if(World == nullptr) return;
		
	World->GetTimerManager().SetTimer(RegenWaitingTimerHandle,this, &APlayerCharacter::BeginRegenerateStamina, MinTimeToStartRegen, false, -1);
}

void APlayerCharacter::BeginRegenerateStamina()
{
	const UWorld* World = GetWorld();
	if(World == nullptr) return;
	
	World->GetTimerManager().SetTimer(RegenStaminaTimerHandle,this, &APlayerCharacter::RegeneratingStamina, StaminaRegenTickTime, true, -1);
}

void APlayerCharacter::RegeneratingStamina()
{
	const float StaminaRegenPerCustomTick = StaminaRegenPerSecond * StaminaRegenTickTime;

	CurrentStamina = FMath::Clamp(CurrentStamina + StaminaRegenPerCustomTick, 0.f ,MaxStamina);
	
	if(CurrentStamina >= MaxStamina)
		StopRegenerateStamina();
}


void APlayerCharacter::OnDeath()
{
	bIsDead = true;
	FTimerHandle DeathResetTimer;
	
	GetWorldTimerManager().SetTimer(DeathResetTimer, this, &APlayerCharacter::ResetPostDeath, ResetTime);
}

void APlayerCharacter::ResetPostDeath()
{
	
	bIsDead = false;
	CharacterCurrentHealth = CharacterMaxHealth;
	
	TryCacheInstanceRef();
	if(InstanceRef) InstanceRef->ReloadLastSave();
	//SetActorLocation(GameModeRef->ResetAndGetCheckpointLocation());
}

void APlayerCharacter::TryCacheGameModeRef()
{
	if(GameModeRef) return;

	GameModeRef = Cast<AProjectTentacleGameModeBase>(GetWorld()->GetAuthGameMode());
}

void APlayerCharacter::TryCacheInstanceRef()
{
	if(InstanceRef) return;

	InstanceRef = Cast<UProjectTentacleGameInstance>(GetWorld()->GetGameInstance());
}

void APlayerCharacter::CameraSwitch_ShoulderView()
{
	//
	CombatCameraSwitchTimeline.ReverseFromEnd();

}

void APlayerCharacter::CameraSwitch_CombatView()
{
	CombatCameraSwitchTimeline.PlayFromStart();
}

bool APlayerCharacter::HasSpaceToLand(FVector KnockingDir)
{
	// line trace to check if player has space to land after getting knocked off
	FHitResult Hit;
	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(this);
	
	const FVector AssumeLandingStart = GetActorLocation() + (KnockingDir * 100.0f) + (GetActorUpVector() * 100.0f);
	const FVector AssumeLandingEnd =  AssumeLandingStart + (GetActorUpVector() * -1 * 300.0f);
	
	const bool IsFloorHit = UKismetSystemLibrary::SphereTraceSingle(GetWorld(), AssumeLandingStart, AssumeLandingEnd, 10.0f, UEngineTypes::ConvertToTraceType(ECC_Camera), false, IgnoreActors, EDrawDebugTrace::Persistent,Hit,true);
	
	return IsFloorHit;
}

void APlayerCharacter::StopRegenerateStamina()
{
	const UWorld* World = GetWorld();
	if(World == nullptr) return;
	
	World->GetTimerManager().ClearTimer(RegenWaitingTimerHandle);
	World->GetTimerManager().ClearTimer(RegenStaminaTimerHandle);
}

// =============================================== Camera ===================================================
void APlayerCharacter::HealthReduction(int32 ReducingAmount)
{
	CharacterCurrentHealth = FMath::Clamp((CharacterCurrentHealth - ReducingAmount),0.f, CharacterMaxHealth);

	if(CharacterCurrentHealth <= 0.f) OnDeath();
}

// =============================================== Special Ability ===================================================






// =============================================== Utility ================================================




// =========================================== Interface Function =========================================
void APlayerCharacter::DamagingTarget_Implementation()
{
	Super::DamagingTarget_Implementation();

	if(DamagingActor == nullptr) return;

	if(PunchSound && TentacleImpulseSound)
	{
		const FVector TentacleAttackPos = GetMesh()->GetSocketLocation("TentacleSocket");
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), PunchSound, TentacleAttackPos);
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), TentacleImpulseSound, TentacleAttackPos);
	}
	
	IDamageInterface::Execute_ReceiveDamageFromPlayer(DamagingActor, bIsOHKOEnabled ? OHKODamage: CurrentDamage, this, CurrentAttackType);

	if(CurrentAttackType == EPlayerAttackType::CounterAttack) UnsetCurrentTarget();

	if(DamagingActor->GetUnitType() == EEnemyType::Brute && CurrentAttackType == EPlayerAttackType::CounterAttack)
	{
		OnCounterStop.ExecuteIfBound();
	}
}

void APlayerCharacter::EnterUnableCancelAttack_Implementation()
{
	Super::EnterUnableCancelAttack_Implementation();

	CurrentActionState = EActionState::Attack;

}

void APlayerCharacter::TryStoreCounterTarget_Implementation(AEnemyBase* CounterTarget)
{
	Super::TryStoreCounterTarget_Implementation(CounterTarget);

	SetCounteringTarget(CounterTarget);

	TryTurnCounterCapable(true);
}

void APlayerCharacter::TryRemoveCounterTarget_Implementation(AEnemyBase* CounterTarget)
{
	Super::TryRemoveCounterTarget_Implementation(CounterTarget);

	ClearCounteringTarget(CounterTarget);

	TryTurnCounterCapable(false);
}


// void APlayerCharacter::ReceiveAttackInCounterState_Implementation(AActor* CounteringTarget)
// {
// 	Super::ReceiveAttackInCounterState_Implementation(CounteringTarget);
//
// 	if(CurrentActionState == EActionState::Evade)
// 		bool bExecuted = OnEnteringPreCounterState.ExecuteIfBound(CounteringTarget);
// 		
// 	// // if player is in evade state, it means player will trigger counter action
// 	// if(CurrentActionState == EActionState::Evade)
// 	// 	bool bExecuted = OnTriggeringCounter.ExecuteIfBound(CounteringTarget);
// }


void APlayerCharacter::ReceiveDamageFromEnemy_Implementation(int32 DamageAmount, AActor* DamageCauser,
                                                             EEnemyAttackType EnemyAttackType)
{
	IPlayerDamageInterface::ReceiveDamageFromEnemy_Implementation(DamageAmount, DamageCauser, EnemyAttackType);

	bool bExecuted = OnReceivingIncomingDamage.ExecuteIfBound(DamageAmount, DamageCauser, EnemyAttackType);
}

void APlayerCharacter::OnSwitchingToExecutionCamera_Implementation()
{
	IPlayerCameraInterface::OnSwitchingToExecutionCamera_Implementation();
	
	if(!PlayerCurrentController) TryCachePlayerController();
	
	USkeletalMeshComponent* PlayerMesh = GetMesh();
	if(!PlayerMesh) return;
	
	// set enum to execution
	CurrentCameraType = EPlayerCameraType::Execution;

	
	CurrentCameraRotation = PlayerCurrentController->PlayerCameraManager->GetCameraRotation(); 
	ExecutionCameraRotation = UKismetMathLibrary::MakeRotFromX(PlayerMesh->GetRightVector());

	
	PlayerCurrentController->SetViewTargetWithBlend(ExecutionCameraChild->GetChildActor(), CameraMoveTime, EViewTargetBlendFunction::VTBlend_EaseInOut, 1.0, false);
	CameraSwitchingTimeline.PlayFromStart();
	AbleRotateVision = false;
}

void APlayerCharacter::OnSwitchingBackToDefaultCamera_Implementation()
{
	IPlayerCameraInterface::OnSwitchingBackToDefaultCamera_Implementation();

	if(!PlayerCurrentController) TryCachePlayerController();
	
	if(CurrentActionState == EActionState::SpecialAttack)
	{
		OnCounterStop.ExecuteIfBound();
	}
	
	CurrentCameraType = EPlayerCameraType::InCombat;
	PlayerCurrentController->SetViewTargetWithBlend(CombatCameraChild->GetChildActor(), CameraMoveTime, EViewTargetBlendFunction::VTBlend_EaseInOut, 1.0, false);
	CameraSwitchingTimeline.ReverseFromEnd();
}

void APlayerCharacter::ActionEnd_Implementation(bool BufferingCheck)
{
	Super::ActionEnd_Implementation(BufferingCheck);
}

void APlayerCharacter::OnActivateComboResetTimer_Implementation()
{
	Super::OnActivateComboResetTimer_Implementation();

	const bool bExecuted = OnEnableComboResetTimer.ExecuteIfBound();
}

void APlayerCharacter::DetachEnemyTarget_Implementation()
{
	Super::DetachEnemyTarget_Implementation();

	// Unset Target
	UnsetCurrentTarget();

}

void APlayerCharacter::OnShowPlayerIndicatorHUD_Implementation(bool Show)
{
	Super::OnShowPlayerIndicatorHUD_Implementation(Show);

	if(Show)
	{
		if(!RangeAimingEnemy) return;
		ShowHitIndicator(IndicatorHUDRemainTime, RangeAimingEnemy->GetActorLocation());
	}
	else
		CollapseHitIndicator();
	
	
}


void APlayerCharacter::OnChangePlayerIndicatorHUD_Visibility_Implementation(bool IsVisible)
{
	Super::OnChangePlayerIndicatorHUD_Visibility_Implementation(IsVisible);

	HUDRef->ChangeVisibility(IsVisible);
}

void APlayerCharacter::OnApplyChargeKnockForce_Implementation(FVector ApplyingForce, FVector ForceDirection)
{
	Super::OnApplyChargeKnockForce_Implementation(ApplyingForce, ForceDirection);

	if(HasSpaceToLand(ForceDirection)) LaunchCharacter(ApplyingForce, true, true);
	else LaunchCharacter(GetActorUpVector() * 400.0f, true, true);
}

void APlayerCharacter::TryClearCounterVictim_Implementation(AEnemyBase* ClearingVictim)
{
	Super::TryClearCounterVictim_Implementation(ClearingVictim);

	ClearCounteringTarget(ClearingVictim);

}

void APlayerCharacter::OnMakingTentacleVisible_Implementation(bool bShowTentacle)
{
	Super::OnMakingTentacleVisible_Implementation(bShowTentacle);

	if(bShowTentacle) TentacleMaterialChangingTimeline.PlayFromStart();
	else TentacleMaterialChangingTimeline.ReverseFromEnd();
}

void APlayerCharacter::OnEnterOrExitCombat_Implementation(bool bEnterCombat)
{
	Super::OnEnterOrExitCombat_Implementation(bEnterCombat);

	if(bEnterCombat)
		CameraSwitch_CombatView();
	else
	{
		CameraSwitch_ShoulderView();	
		ClearCounteringTarget();
	}


}





