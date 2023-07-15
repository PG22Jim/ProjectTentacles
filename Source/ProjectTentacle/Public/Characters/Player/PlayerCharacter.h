// Copyright (C) The Tentacle Zone 2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttachingTentacle.h"
#include "GenericTeamAgentInterface.h"
#include "PlayerCameraInterface.h"
#include "PlayerDamageInterface.h"
#include "Characters/Base/BaseCharacter.h"
#include "Characters/Enemies/EnemyBase.h"
#include "Components/TimelineComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "PlayerCharacter.generated.h"


USTRUCT(BlueprintType)
struct FInputDirection
{
	GENERATED_BODY()

protected:

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float InputDirectionX;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float InputDirectionY;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float PreviousInputDirectionX;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float PreviousInputDirectionY;
	
	
public:
	
	FInputDirection()
	{
		PreviousInputDirectionX = 0;
		PreviousInputDirectionY = 0;
		InputDirectionX = 0;
		InputDirectionY = 0;
	}

	float GetInputDirectionX() const {return InputDirectionX;}
	float GetInputDirectionY() const {return InputDirectionY;}
	void SetInputDirectionX(float InputX) {InputDirectionX = InputX;}
	void SetInputDirectionY(float InputY) {InputDirectionY = InputY;}

	float GetPreviousInputDirectionX() const {return PreviousInputDirectionX;}
	float GetPreviousInputDirectionY() const {return PreviousInputDirectionY;}
	void SetPreviousInputDirectionX(float NewInputX) {PreviousInputDirectionX = NewInputX;}
	void SetPreviousInputDirectionY(float NewInputY) {PreviousInputDirectionY = NewInputY;}
};


DECLARE_DYNAMIC_DELEGATE_OneParam(FOnExecutingPlayerAction, EActionState, ExecutingAction);
DECLARE_DYNAMIC_DELEGATE_ThreeParams(FOnReceivingIncomingDamage, int32, DamageAmount, AActor*, DamageCauser, EEnemyAttackType, ReceivingAttackType);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnTriggeringCounter, AActor*, DamageCauser);
DECLARE_DYNAMIC_DELEGATE(FCounterStart);
DECLARE_DYNAMIC_DELEGATE(FCounterStop);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnEnteringPreCounterState, AActor*, CounterTarget);


DECLARE_DYNAMIC_DELEGATE(FOnEnableComboResetTimer);


/**
 * 
 */
UCLASS()
class PROJECTTENTACLE_API APlayerCharacter : public ABaseCharacter, public IGenericTeamAgentInterface, public IPlayerDamageInterface, public IPlayerCameraInterface
{
	GENERATED_BODY()

private:
	static FGenericTeamId TeamId;
	virtual FGenericTeamId GetGenericTeamId() const override;

	void CreatCameraComponents();

	void TryCachePlayerController();
	
	void StopRegenerateStamina();
	void WaitToRegenStamina();
	void BeginRegenerateStamina();
	void RegeneratingStamina();

	bool AbleRotateVision = true; 
	void OnDeath();
	void ResetPostDeath();
	void TryCacheGameModeRef();
	void TryCacheInstanceRef();

	void SwitchToNormalCamera();
	void SwitchToCombatCamera();

	bool HasSpaceToLand(FVector KnockingDir);
	
	FTimeline TentacleAppearingTimeline;
	
	class AProjectTentacleGameModeBase* GameModeRef;
	class UProjectTentacleGameInstance* InstanceRef;

	unsigned int bIsDead:1;
	
	unsigned int bTakingSwampDamage:1;
	FTimerDelegate SwampDamageDelegate;
	unsigned int bIsOHKOEnabled:1;

	AAttachingTentacle* TentacleOnRightHand;

	void TimelineInitialization();
	void TentacleAttachment();

	float UpdatedAttackingSpeedBonus = 1.0f;
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Death)
	float ResetTime = 5.f;
	
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	class UUserWidget_HitIndicator* HUDRef;

	UPROPERTY()
	AEnemyBase* RangeAimingEnemy;

	UPROPERTY()
	float IndicatorHUDRemainTime = 0;

	UPROPERTY()
	APlayerController* PlayerCurrentController;
	
	
	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	
	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* ShoulderViewSpringArm;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* CombatSpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* ExecutionSpringArm;
	
	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* NormalCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UChildActorComponent* NormalCameraChild;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* CombatCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UChildActorComponent* CombatCameraChild;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* ExecutionCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UChildActorComponent* ExecutionCameraChild;

	// Current playing reference to be check if valid
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UAnimMontage* CurrentPlayingMontage;

	// Actor variables to set targeting and damaging actor for the incoming damage 
	UPROPERTY()
	AEnemyBase* TargetActor;

	UPROPERTY()
	AEnemyBase* DamagingActor;

	UPROPERTY()
	AEnemyBase* CounteringVictim;

	bool IsPlayerCounterable = false;

	EPlayerCameraType CurrentCameraType = EPlayerCameraType::InCombat;
	
	
	UPROPERTY(BlueprintReadOnly)
	EPlayerAttackType CurrentAttackType;
	
	UPROPERTY()
	EPlayerAttackAnimations CurrentAttackingAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= ExecutionCameraSetting)
	float CameraMoveTime = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= ExecutionCameraSetting)
	UCurveFloat* CameraRotationCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= TentacleAttachingSetting)
	UCurveFloat* MaterialChangingCurve;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category= TentacleAttachingSetting)
	TSubclassOf<AAttachingTentacle> StunTentacleClass;
	
	
	// timeline
	FTimeline CameraSwitchingTimeline;
	FTimeline TentacleMaterialChangingTimeline;
	
	FRotator CurrentCameraRotation = FRotator(0,0,0);
	FRotator ExecutionCameraRotation = FRotator(0,0,0);
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= DamageSetting)
	float BaseDamage = 3;

	float CurrentDamage = 3;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= DamageSetting)
	float DamageMultiplier = 0.5;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= DamageSetting)
	float MaxDamage =  10;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= DamageSetting)
	float OHKODamage =  1000;
	
	// Animation montages
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= AnimMontages)
	UAnimMontage* EvadeAnimMontage;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= AnimMontages)
	TArray<UAnimMontage*> MeleeAttackMontages;
	

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= StaminaSetting)
	float CurrentStamina = 100.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= StaminaSetting)
	float MaxStamina = 100.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= StaminaSetting)
	float CostForEachDodge = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= StaminaSetting)
	float MinTimeToStartRegen = 3.0f;

	float StaminaRegenTickTime = 0.1f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= StaminaSetting)
	float StaminaRegenPerSecond = 10.0f;

	// Timer handle for waiting to regen and regening
	FTimerHandle RegenWaitingTimerHandle;
	FTimerHandle RegenStaminaTimerHandle;


	// Timer Handle for resume simulate physics setting
	FTimerHandle ResumeSimulatePhysicTimer;
	
	
	// input key variables to check
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InputKey)
	FKey MovingForwardKey;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InputKey)
	FKey MovingBackKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InputKey)
	FKey MovingLeftKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = InputKey)
	FKey MovingRightKey;

	// Current Input direction structure	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FInputDirection InputDirection;
	
	// Register as visual stimulus for enemies
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	class UAIPerceptionStimuliSourceComponent* StimuliSource;

	UFUNCTION()
	void DealSwampDamage(float Damage, float TickTime);

public:
	UFUNCTION(BlueprintCallable)
	void ToggleOHKO();
	
	void StartSwampDamageTick(float Damage, float TickInterval);
	void StopSwampDamageTick();
	void ShowHitIndicator(float CounterTime, const FVector HitLocation) const;
	void CollapseHitIndicator() const;
	
	// Delegate signature
	FOnExecutingPlayerAction OnExecutePlayerAction;
	FOnReceivingIncomingDamage OnReceivingIncomingDamage;
	FOnTriggeringCounter OnTriggeringCounter;
	FCounterStart OnCounterStart;
	FCounterStop OnCounterStop;
	FOnEnteringPreCounterState OnEnteringPreCounterState;
	FOnEnableComboResetTimer OnEnableComboResetTimer;

	
	
	APlayerCharacter();

	// ================================================= 
	
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	

	// ================================================= Input ===================================================
	void LookUpAtRate(float Rate);
	
	virtual void TurnAtRate(float Rate);

	/** Called for forwards/backward input */
	void MoveForward(float Value);

	/** Called for side to side input */
	void MoveRight(float Value);

	void TryMeleeAttack();
	
	void TryEvade();
	
	void TryDodge();

	bool CanPerformAttack();

	bool CanPerformDodge();


	// =================================================== Camera ========================================================
	UFUNCTION()
	void OnFinishCameraMovement();

	UFUNCTION()
	void OnUpdatingCameraMovement(float Alpha);
	

	// =============================================== Special Ability ===================================================

	void TrySpecialAbility1();
	
	void TrySpecialAbility2();
	

	
	// ================================================= Utility Functions ================================================

	void UnsetCurrentTarget();

	UFUNCTION(BlueprintCallable)
	void DebugTestFunction();

	bool DebugingBool = false;

	UFUNCTION()
	void ResumeSimulatePhysic();

	UFUNCTION()
	void OnUpdateTentacleMaterial(float Alpha);

	void OnCancelTentacleMaterialChange();
	
	
	// ================================================= Get And Set Functions ============================================
	bool GetAbleRotateVision() const {return AbleRotateVision;}
	
	FInputDirection GetPlayerInputDir() const {return InputDirection;}

	void SetRangeAimingEnemy(AEnemyBase* NewRegisteringActor, float HUDRemainTime);
	void TryClearStoredRange(AEnemyBase* ClearingEnemy) {if(RangeAimingEnemy == ClearingEnemy) RangeAimingEnemy = nullptr;}

	float GetUpdatedAttackingSpeedBonus() const {return UpdatedAttackingSpeedBonus;}
	void SetUpdatedAttackingSpeedBonus(float NewSpeed) {UpdatedAttackingSpeedBonus = NewSpeed;}
	

	float GetCurrentDamage() const {return CurrentDamage;}
	void SetCurrentDamage(int32 CurrentComboCount) {CurrentDamage = UKismetMathLibrary::FClamp((BaseDamage * (1 + (CurrentComboCount * DamageMultiplier))), BaseDamage, MaxDamage);}
	
	float GetCurrentStamina() const {return CurrentStamina;}
	void SetStamina(float NewStamina) {CurrentStamina = NewStamina;}
	
	AEnemyBase* GetTargetActor() const {return TargetActor;}
	void SetTargetActor(AEnemyBase* NewTargetActor);

	AEnemyBase* GetDamagingActor() const {return DamagingActor;}
	void SetDamagingActor(AEnemyBase* NewDamagingActor) {DamagingActor = NewDamagingActor;}

	bool GetIsPlayerCounterable() const {return IsPlayerCounterable;}
	void TryTurnCounterCapable(bool IsOn) {if(IsPlayerCounterable != IsOn) IsPlayerCounterable = IsOn;}

	UFUNCTION(BlueprintCallable)
	AEnemyBase* GetCounteringTarget() const {return CounteringVictim;}
	void SetCounteringTarget(AEnemyBase* NewCounterTarget) {if(CounteringVictim != NewCounterTarget) CounteringVictim = NewCounterTarget;}
	void ClearCounteringTarget() {if(CounteringVictim) CounteringVictim = nullptr;}
	void ClearCounteringTarget(AEnemyBase* RemovingCounterTarget) {if(CounteringVictim == RemovingCounterTarget) CounteringVictim = nullptr;}

	EPlayerAttackType GetCurrentAttackType() const {return CurrentAttackType;}
	void SetCurrentAttackType(EPlayerAttackType NewAttackType) {CurrentAttackType = NewAttackType;}

	EPlayerAttackAnimations GetCurrentAttackAnim() const {return CurrentAttackingAnim;}
	void SetCurrentAttackAnim(EPlayerAttackAnimations NewAttackAnim) {CurrentAttackingAnim = NewAttackAnim;}

	
	int32 GetCurrentCharacterHealth() const {return CharacterCurrentHealth;}
	void SetCurrentCharacterHealth(float CurrentHealth) {CharacterCurrentHealth = FMath::Clamp(CurrentHealth, 0.f, CharacterMaxHealth);}
	void HealthReduction(int32 ReducingAmount); 

	// ================================================= Interface implementation =========================================
	
	UFUNCTION()
	virtual void DamagingTarget_Implementation() override;

	UFUNCTION()
	virtual void EnterUnableCancelAttack_Implementation() override;
	
	UFUNCTION()
	virtual void TryStoreCounterTarget_Implementation(AEnemyBase* CounterTarget) override;

	UFUNCTION()
	virtual void TryRemoveCounterTarget_Implementation(AEnemyBase* CounterTarget) override;
	
	UFUNCTION()
	virtual void ReceiveDamageFromEnemy_Implementation(int32 DamageAmount, AActor* DamageCauser, EEnemyAttackType EnemyAttackType) override;

	UFUNCTION()
	virtual void OnSwitchingToExecutionCamera_Implementation() override;

	UFUNCTION()
	virtual void OnSwitchingBackToDefaultCamera_Implementation() override;

	UFUNCTION()
	virtual void ActionEnd_Implementation(bool BufferingCheck) override;

	UFUNCTION()
	virtual void OnActivateComboResetTimer_Implementation() override;
	
	UFUNCTION()
	virtual void DetachEnemyTarget_Implementation() override;

	UFUNCTION()
	virtual void OnShowPlayerIndicatorHUD_Implementation(bool Show) override;	
	
	UFUNCTION()
	virtual void OnChangePlayerIndicatorHUD_Visibility_Implementation(bool IsVisible) override;

	UFUNCTION()
	virtual void OnApplyChargeKnockForce_Implementation(FVector ApplyingForce, FVector ForceDirection) override;

	UFUNCTION()
	virtual void TryClearCounterVictim_Implementation(AEnemyBase* ClearingVictim) override;

	UFUNCTION()
	virtual void OnMakingTentacleVisible_Implementation(bool bShowTentacle) override;

	UFUNCTION()
	virtual void OnEnterOrExitCombat_Implementation(bool bEnterCombat) override;
};
