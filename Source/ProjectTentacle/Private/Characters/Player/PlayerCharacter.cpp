// Copyright (C) The Tentacle Zone 2023. All Rights Reserved.


#include "Characters/Player/PlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"

FGenericTeamId APlayerCharacter::TeamId = FGenericTeamId(1);
// ==================================================== Constructor =========================================

APlayerCharacter::APlayerCharacter()
{
	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm
	
	StimuliSource = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("Stimuli Source"));
	StimuliSource->bAutoRegister = true;
	StimuliSource->RegisterForSense(UAISense_Sight::StaticClass());
}

FGenericTeamId APlayerCharacter::GetGenericTeamId() const
{
	return TeamId;
}

// =================================== Begin Play, Set up InputComponent, Tick ==============================
void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	CharacterCurrentHealth = CharacterMaxHealth;
}

void APlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const APlayerController* PlayerControl = GetWorld()->GetFirstPlayerController();
	// reset input vector
	if(PlayerControl->WasInputKeyJustReleased(MovingForwardKey) || PlayerControl->WasInputKeyJustReleased(MovingBackKey))
		InputDirection.SetInputDirectionY(0.0f);
	
	if(PlayerControl->WasInputKeyJustReleased(MovingLeftKey) || PlayerControl->WasInputKeyJustReleased(MovingRightKey))
		InputDirection.SetInputDirectionX(0.0f);

	
}


// ==================================================== Movement ==============================================
void APlayerCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void APlayerCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
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

		InputDirection.SetInputDirectionX(Value);
		
		// add movement in that direction
		if(CurrentActionState == EActionState::Idle) AddMovementInput(Direction, Value);
	}
}


// // ====================================================== Attack ==============================================
void APlayerCharacter::TryMeleeAttack()
{
	// if player is recovering from action or is dodging, return
	if(CurrentActionState == EActionState::Idle || CurrentActionState == EActionState::WaitForCombo)
		bool bExecuted = OnExecutePlayerAction.ExecuteIfBound(EActionState::Attack);
	
}
//
// void APlayerCharacter::BeginMeleeAttack()
// {
// 	// Get max number of attack animation montage array
// 	const int32 MAttackMontagesNum = MeleeAttackMontages.Num();
//
// 	// Get random number from 0 to max number of attack animation montage array
// 	const int32 MAttackRndIndex = UKismetMathLibrary::RandomIntegerInRange(0, MAttackMontagesNum - 1);
// 	
// 	// Validation Check
// 	if(MeleeAttackMontages[MAttackRndIndex] == nullptr) return;
//
// 	// 
// 	const EPlayerAttackType SelectedAttackType = GetAttackTypeByRndNum(MAttackRndIndex);
//
// 	// Get All enemy around player
// 	TArray<AAttackTargetTester*> OpponentAroundSelf = GetAllOpponentAroundSelf();
//
// 	// if there is no opponent around, simply return
// 	if(OpponentAroundSelf.Num() == 0) return;
//
// 	// Get target direction to face to
// 	AAttackTargetTester* ResultFacingEnemy = GetTargetEnemy(OpponentAroundSelf);
//
// 	// if there is no direction, return
// 	if(ResultFacingEnemy == nullptr) return;
// 	
//
// 	// Store target actor and selected attack type references for later anim notify usage
// 	TargetActor = ResultFacingEnemy;
// 	const FVector TargetActorPos = TargetActor->GetActorLocation();
// 	const FVector PlayerPos = GetActorLocation();
//
// 	CurrentAttackType = SelectedAttackType;
//
// 	
// 	
// 	// simply make player and targeted enemy rotate to each other
// 	const FVector FacingEnemyDir = UKismetMathLibrary::Normal(TargetActorPos - PlayerPos);
// 	const FVector FacingPlayerDir = UKismetMathLibrary::Normal(PlayerPos - TargetActorPos);
// 	InstantRotation(FacingEnemyDir);
// 	TargetActor->InstantRotation(FacingPlayerDir);
//
// 	// set lerping start and end position to variable
// 	SetAttackMovementPositions(TargetActorPos);
// 	
// 	// change current action state enum
// 	CurrentActionState = EActionState::Attack;
//
// 	// TODO: Need To ReadWrite 
// 	// Check if Enemy is dying or now, if is, finish him
// 	int32 EnemyCurrentHealth = TargetActor->GetEnemyHealth();
//
// 	if(EnemyCurrentHealth <= 1)
// 	{
// 		FinishEnemy();
// 		return;
// 	}
// 	
// 	EnemyCurrentHealth--;
// 	TargetActor->SetEnemyHealth(EnemyCurrentHealth);
//
// 	
// 	// Player attack montage
// 	CurrentPlayingMontage = MeleeAttackMontages[MAttackRndIndex];
// 	PlayAnimMontage(CurrentPlayingMontage, 1, "Default");
//
// 	// Start attack movement timeline depends on the result of playering montage
// 	StartAttackMovementTimeline(SelectedAttackType);
// }
//
// void APlayerCharacter::FinishEnemy()
// {
// 	const FVector TargetPos = TargetActor->GetActorLocation();
// 	const FVector SelfPos = GetActorLocation();
// 	
// 	const FVector TargetToSelfDir = UKismetMathLibrary::Normal(SelfPos - TargetPos);
//
// 	// Player attack montage
// 	CurrentPlayingMontage = FinisherAnimMontages;
// 	PlayAnimMontage(CurrentPlayingMontage, 1, "Default");
//
// 	TargetActor->PlayFinishedAnimation();
// 	CloseToPerformFinisherTimeLine.PlayFromStart();
// }
//
// void APlayerCharacter::SetAttackMovementPositions(FVector TargetPos)
// {
// 	MovingStartPos = GetActorLocation();
//
// 	// Get direction from target to player
// 	FVector OffsetWithoutZ = MovingStartPos - TargetPos;
// 	OffsetWithoutZ.Z = 0;
// 	const FVector DirFromTargetToPlayer = UKismetMathLibrary::Normal(OffsetWithoutZ);
//
// 	// Get lerp end position
// 	MovingEndPos = TargetPos + (DirFromTargetToPlayer * 80);
// }
//
// EPlayerAttackType APlayerCharacter::GetAttackTypeByRndNum(int32 RndNum)
// {
// 	return static_cast<EPlayerAttackType>(RndNum);
// }
//
// void APlayerCharacter::StartAttackMovementTimeline(EPlayerAttackType AttackType)
// {
// 	switch (AttackType)
// 	{
// 		case EPlayerAttackType::ShortFlipKick:
// 			ShortFlipKickTimeLine.PlayFromStart();
// 			break;
// 		case EPlayerAttackType::FlyingKick:
// 			FlyingKickTimeLine.PlayFromStart();
// 			break;
// 		case EPlayerAttackType::FlyingPunch:
// 			FlyingPunchTimeLine.PlayFromStart();
// 			break;
// 		case EPlayerAttackType::SpinKick:
// 			SpinKickTimeLine.PlayFromStart();
// 			break;
// 		case EPlayerAttackType::DashingDoubleKick:
// 			DashingDoubleKickTimeLine.PlayFromStart();
// 			break;
// 		default:
// 			break;
// 	}
// }
//
// // ====================================================== Evade ===============================================
void APlayerCharacter::TryEvade()
{
	// if player is able to dodge, make dodge
	if(CurrentActionState == EActionState::Idle)
		bool bExecuted = OnExecutePlayerAction.ExecuteIfBound(EActionState::Evade);
}
//
// void APlayerCharacter::BeginEvade()
// {
// 	// if evade montage is null pointer, just return
// 	if(EvadeAnimMontage == nullptr) return;
//
// 	CurrentActionState = EActionState::Evade;
//
// 	CurrentPlayingMontage = EvadeAnimMontage;
//
// 	const int32 RndPerformIndex = UKismetMathLibrary::RandomIntegerInRange(0,1);
//
// 	if(RndPerformIndex == 0)
// 	{
// 		PlayAnimMontage(CurrentPlayingMontage, 1, "DodgeRight");
// 		return;
// 	}
// 	
// 	PlayAnimMontage(CurrentPlayingMontage, 1, "DodgeLeft");
// }
//
//
//
// // ================================================== Counter ======================================================
// void APlayerCharacter::BeginCounterAttack(AActor* CounteringTarget)
// {
// 	// if Dodge montage is null pointer, just return
// 	if(CounterAttackMontages == nullptr) return;
//
// 	CurrentActionState = EActionState::SpecialAttack;
//
// 	AAttackTargetTester* CastedTarget = Cast<AAttackTargetTester>(CounteringTarget);
// 	if(CastedTarget == nullptr) return;
//
// 	TargetActor = CastedTarget;
// 	
// 	CurrentPlayingMontage = CounterAttackMontages;
//
// 	const FVector FacingEnemyDir = UKismetMathLibrary::Normal(TargetActor->GetActorLocation() - GetActorLocation());
// 	InstantRotation(FacingEnemyDir);
// 	
// 	PlayAnimMontage(CurrentPlayingMontage, 1, NAME_None);
// }
//
// // ==================================================== Dodge ===============================================
//
void APlayerCharacter::TryDodge()
{
	// if player is able to dodge, make dodge
	if(CurrentActionState == EActionState::Idle)
	{
		StopAnimMontage();	
		bool bExecuted = OnExecutePlayerAction.ExecuteIfBound(EActionState::Dodge);
	}
}

void APlayerCharacter::SetTargetActor(AAttackTargetTester* NewTargetActor)
{
	// if target actor is not null ptr, unshow its target icon
	if(TargetActor != nullptr)
	{
		if(TargetActor->GetClass()->ImplementsInterface(UEnemyWidgetInterface::StaticClass()))
			IEnemyWidgetInterface::Execute_UnShowPlayerTargetIndicator(TargetActor);
	}
	
	
	if(NewTargetActor->GetClass()->ImplementsInterface(UEnemyWidgetInterface::StaticClass()))
		IEnemyWidgetInterface::Execute_ShowPlayerTargetIndicator(NewTargetActor);
	
	TargetActor = NewTargetActor;
}


//
// void APlayerCharacter::BeginDodge()
// {
// 	// Get Player facing direction
// 	const FVector PlayerFaceDir = GetActorForwardVector();
//
// 	// Get dodging direction
// 	const FVector PlayerDodgingDir = DecideDodgingDirection(PlayerFaceDir);
//
// 	// Get dodging montage depends on dodging direction
// 	UAnimMontage* PlayerDodgingMontage = FrontRollingMontage;
// 	InstantRotation(PlayerDodgingDir);
// 	//UAnimMontage* PlayerDodgingMontage = DecideDodgingMontage(PlayerDodgingDir);
//
// 	// Set action state and playing montage to dodging
// 	CurrentActionState = EActionState::Dodge;
// 	CurrentPlayingMontage = PlayerDodgingMontage;
//
// 	// Set moving locations
// 	const FVector DodgeStart = GetActorLocation();
// 	const FVector DodgingDest = DodgeStart + (PlayerDodgingDir * DodgeDistance);
// 	MovingStartPos = DodgeStart;
// 	MovingEndPos = DodgingDest;
//
// 	
//
// 	// Play both lerping timeline and montage
// 	PlayAnimMontage(CurrentPlayingMontage, 1 , NAME_None);
// 	DodgeLerpingTimeLine.PlayFromStart();
// }
//
// FVector APlayerCharacter::DecideDodgingDirection(FVector PlayerFaceDir)
// {
// 	
// 	// if input direction is both 0, it means player didn't 
// 	if(InputDirection.GetInputDirectionX() == 0 && InputDirection.GetInputDirectionY() == 0)
// 	{
// 		const FVector PlayerBackDir = -1 * PlayerFaceDir;
// 		return PlayerBackDir;
// 	}
// 	
// 	// Get input direction
// 	const FVector SelfPos = GetActorLocation();
// 	const FRotator Rotation = Controller->GetControlRotation();
// 	const FRotator YawRotation(0, Rotation.Yaw, 0);
// 	const FVector RightX = UKismetMathLibrary::GetRightVector(YawRotation);
// 	const FVector ForwardY = UKismetMathLibrary::GetForwardVector(YawRotation);
// 	const FVector RightInputDir = RightX * InputDirection.GetInputDirectionX();
// 	const FVector ForwardInputDir = ForwardY * InputDirection.GetInputDirectionY();
// 	const FVector InputDest = SelfPos + ((RightInputDir * 50) + (ForwardInputDir * 50));
// 	const FVector SelfToInputDestDir = UKismetMathLibrary::GetDirectionUnitVector(SelfPos, InputDest);
//
// 	return SelfToInputDestDir;
// }
//
// UAnimMontage* APlayerCharacter::DecideDodgingMontage(FVector PlayerDodgingDirection)
// {
// 	// Get Player facing direction
// 	const FVector PlayerFaceDir = GetActorForwardVector();
// 	
// 	const float DotProduct = UKismetMathLibrary::Dot_VectorVector(PlayerFaceDir, PlayerDodgingDirection);
//
// 	// Dot product is negative = player is dodging backward
// 	if(DotProduct < 0)
// 	{
// 		InstantRotation((-1 * PlayerDodgingDirection));
// 		return BackFlipMontage;
// 	}
//
// 	InstantRotation(PlayerDodgingDirection);
// 	return FrontRollingMontage;
// }
//
// // ==================================================== Utility ===============================================
// TArray<AAttackTargetTester*> APlayerCharacter::GetAllOpponentAroundSelf()
// {
// 	TArray<AActor*> FoundActorList;
// 	TArray<AAttackTargetTester*> ReturnActors;
// 	
// 	const UWorld* World = GetWorld();
// 	if(World == nullptr) return ReturnActors;
//
// 	const FVector SelfPos = GetActorLocation();
//
// 	TArray<AActor*> IgnoreActors;
// 	IgnoreActors.Add(this);
// 	
// 	UKismetSystemLibrary::SphereOverlapActors(World,SelfPos, DetectionRange, FilterType, FilteringClass, IgnoreActors,FoundActorList);
//
// 	if(FoundActorList.Num() != 0)
// 	{
// 		for (AActor* EachFoundActor : FoundActorList)
// 		{
// 			AAttackTargetTester* FoundCharacter = Cast<AAttackTargetTester>(EachFoundActor);
// 			if(FoundCharacter != nullptr) ReturnActors.Add(FoundCharacter);
// 		}
// 	}
// 	
// 	return ReturnActors;
// }
//
// void APlayerCharacter::InstantRotation(FVector RotatingVector)
// {
// 	const FRotator InputRotation = UKismetMathLibrary::MakeRotFromX(RotatingVector);
//
// 	SetActorRotation(InputRotation);
// }
//
// AAttackTargetTester* APlayerCharacter::GetTargetEnemy(TArray<AAttackTargetTester*> OpponentsAroundSelf)
// {
// 	const FVector SelfPos = GetActorLocation();
//
// 	FVector SelfToInputDestDir;
//
// 	const float InputXValue = InputDirection.GetInputDirectionX(); 
// 	const float InputYValue = InputDirection.GetInputDirectionY(); 
// 	
// 	// find out which way is right
// 	if(InputXValue != 0 || InputYValue != 0)
// 	{
// 		const FRotator Rotation = Controller->GetControlRotation();
// 		const FRotator YawRotation(0, Rotation.Yaw, 0);
//
// 		const FVector RightX = UKismetMathLibrary::GetRightVector(YawRotation);
// 		const FVector ForwardY = UKismetMathLibrary::GetForwardVector(YawRotation);
//
// 		const FVector RightInputDir = RightX * InputDirection.GetInputDirectionX();
// 		const FVector ForwardInputDir = ForwardY * InputDirection.GetInputDirectionY();
// 		
// 		const FVector InputDest = SelfPos + ((RightInputDir * 50) + (ForwardInputDir * 50));
//
// 		SelfToInputDestDir = UKismetMathLibrary::GetDirectionUnitVector(SelfPos, InputDest);
// 	}
// 	else
// 	{
// 		// if there is no input direction, it means player didn't press movement key, it means attack will happen in front of player
// 		SelfToInputDestDir = GetActorForwardVector();
// 	}
// 	
// 	// set first one as closest target and iterating from opponents list
// 	AAttackTargetTester* ReturnTarget = OpponentsAroundSelf[0];
// 	
// 	// Set a fake dot product
// 	float TargetDotProduct = -1.0f;
//
// 	// TODO: Use dot product to check instead of angle
// 	for (int32 i = 0; i < OpponentsAroundSelf.Num(); i++)
// 	{
// 		FVector EachCharacterPos = OpponentsAroundSelf[i]->GetActorLocation();
//
// 		EachCharacterPos.Z = SelfPos.Z;
//
// 		const FVector SelfToCharacterDir = UKismetMathLibrary::GetDirectionUnitVector(SelfPos, EachCharacterPos);
//
//
// 		const float DotProduct = UKismetMathLibrary::Dot_VectorVector(SelfToInputDestDir, SelfToCharacterDir);
// 		
// 		// if iterating dot product is not correct
// 		if(DotProduct < 0.70f) continue;
//
// 		// if iterating dotproduct is bigger than current target dot product, it means iterating actor will more likely be our target
// 		if(DotProduct > TargetDotProduct)
// 		{
// 			ReturnTarget = OpponentsAroundSelf[i];
// 			TargetDotProduct = DotProduct;
// 		}
// 	}
//
// 	if(TargetDotProduct < 0.70f)
// 	{
// 		return nullptr;
// 	}
//
// 	
// 	return ReturnTarget;
// }
//
// // ========================================== Timeline Function =========================================
// void APlayerCharacter::MovingAttackMovement(float Alpha)
// {
// 	const FVector CharacterCurrentPos = GetActorLocation();
// 	
// 	const FVector MovingPos = UKismetMathLibrary::VLerp(MovingStartPos, MovingEndPos, Alpha);
//
// 	const FVector LaunchingPos = FVector(MovingPos.X, MovingPos.Y, CharacterCurrentPos.Z);
//
// 	SetActorLocation(LaunchingPos);
// }


// =============================================== Utility ================================================




// =========================================== Interface Function =========================================
void APlayerCharacter::DamagingTarget_Implementation()
{
	Super::DamagingTarget_Implementation();

	if(DamagingActor == nullptr) return;

	IDamageInterface::Execute_ReceiveDamageFromPlayer(DamagingActor, 1, this, CurrentAttackType);
}

void APlayerCharacter::ReceiveDamageFromEnemy_Implementation(int32 DamageAmount, AActor* DamageCauser,
	EEnemyAttackType EnemyAttackType)
{
	IDamageInterface::ReceiveDamageFromEnemy_Implementation(DamageAmount, DamageCauser, EnemyAttackType);

	bool bExecuted = OnReceivingIncomingDamage.ExecuteIfBound(DamageAmount, DamageCauser, EnemyAttackType);
}





