// Copyright (C) The Tentacle Zone 2023. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Encounter/CheckpointSave.h"
#include "Engine/GameInstance.h"
#include "ProjectTentacleGameInstance.generated.h"

/**
 * 
 */
UCLASS()
class PROJECTTENTACLE_API UProjectTentacleGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	void ReloadLastSave();
	void SaveGame();

protected:
	virtual void Init() override;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UCheckpointSave> SaveObjectClass = UCheckpointSave::StaticClass();

	UPROPERTY()
	UCheckpointSave* SaveObject;

	UFUNCTION()
	void OnSaveLoad(const FString& SlotName, const int32 SlotID, USaveGame* Save);
	UFUNCTION()
	void OnLevelLoad();
	UFUNCTION()
	void WaitForLevelLoad();
	
private:
	class APlayerCharacter* PC;
	void TryCachePC(bool bForce = false);
};
