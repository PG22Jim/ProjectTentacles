// Copyright (C) The Tentacle Zone 2023. All Rights Reserved.

#pragma once


#include "CoreMinimal.h"
#include "CharacterActionInterface.h"
#include "EnumClassesForCharacters.h"
#include "GameFramework/Character.h"
#include "BaseCharacter.generated.h"


UCLASS()
class PROJECTTENTACLE_API ABaseCharacter : public ACharacter , public ICharacterActionInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ABaseCharacter();
	void Heal(float Amount);

protected:

	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Camera)
	float BaseTurnRate = 1.0f;

	/** Base look up/down rate, in deg/sec. Other scaling may affect final rate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Camera)
	float BaseLookUpRate = 1.0f;

		
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=CharacterProperty)
	EActionState CurrentActionState = EActionState::Idle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=CharacterProperty)
	float CharacterCurrentHealth = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=CharacterProperty)
	float CharacterMaxHealth = 100;

	UPROPERTY()
	ACharacter* TargetCharacter;
	
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	ACharacter* GetCurrentTarget() const {return TargetCharacter;}
	void SetCurrentTarget(ACharacter* Target) {TargetCharacter = Target;}


	// ================================================= Get And Set Functions ============================================
	EActionState GetCurrentActionState() const {return CurrentActionState;}
	void SetCurrentActionState(EActionState NewActionState) {CurrentActionState = NewActionState;}
	
	
	
	// ================================================= Interface implementation =========================================
	
	virtual void ActionEnd_Implementation(bool BufferingCheck) override;

	virtual void StartWaitForCombo_Implementation() override;
	
	
};
