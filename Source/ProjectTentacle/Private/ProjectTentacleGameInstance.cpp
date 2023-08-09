// Copyright (C) The Tentacle Zone 2023. All Rights Reserved.


#include "ProjectTentacleGameInstance.h"

#include "EngineUtils.h"
#include "Characters/Player/PlayerCharacter.h"
#include "Encounter/EncounterVolume.h"
#include "Engine/LevelStreaming.h"
#include "Kismet/GameplayStatics.h"

// Create a blank save object instance and setup for first save flag
void UProjectTentacleGameInstance::Init()
{
	Super::Init();
	bCompletedFirstSave = false;
	SaveObject = Cast<UCheckpointSave>(UGameplayStatics::CreateSaveGameObject(SaveObjectClass));
}

// Sets up environment after save object gets loaded into memory
void UProjectTentacleGameInstance::OnSaveLoad(const FString& SlotName, const int32 SlotID, USaveGame* Save)
{
	// Don't load if save object doesn't exist or isn't of right type
	if(!Save) return;

	UCheckpointSave* AsCheckpointSave = Cast<UCheckpointSave>(Save);
	if(!AsCheckpointSave) return;

	SaveObject = AsCheckpointSave;

	// Clear all encounters and reload the map
	AllEncounterVolumes.Empty();
	const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(this, true);
	UGameplayStatics::OpenLevel(GetWorld(), FName(*CurrentLevelName));

	// Set a timer for next tick to update map from save data (Doesn't work if done in sync so it needs a timer)
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UProjectTentacleGameInstance::WaitForLevelLoad);
}

// Set up world from save data
void UProjectTentacleGameInstance::OnLevelLoad()
{
	// Get PC reference and update location/rotation/health
	TryCachePC();
	PC->SetActorLocation(SaveObject->PlayerLocation);
	PC->SetActorRotation(SaveObject->PlayerRotation);
	PC->SetCurrentCharacterHealth(SaveObject->PlayerHealth);

	// Mark any completed encounters to stop them reactivating
	for(AEncounterVolume* Volume: AllEncounterVolumes)
	{
		if(SaveObject->CompletedEncounters.Contains(Volume->GetName())) Volume->MarkComplete();
	}
}

// Checks if the map has successfully reloaded, loops on a timer if not
void UProjectTentacleGameInstance::WaitForLevelLoad()
{
	APlayerCharacter* InitialRef = PC;
	TryCachePC(true);
	if(PC == InitialRef)
	{
		GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UProjectTentacleGameInstance::WaitForLevelLoad);
		return;
	}
	OnLevelLoad();
}

// Cache reference to PC
void UProjectTentacleGameInstance::TryCachePC(bool bForce)
{
	if(PC && !bForce) return;
	
	for (TActorIterator<APlayerCharacter> It(GetWorld(), APlayerCharacter::StaticClass()); It; ++It)
	{
		PC = *It;
		if(PC) break;
	}
}

// Write data into SaveObject and save to file
void UProjectTentacleGameInstance::SaveGame()
{
	TryCachePC();
	if(!PC) return;
	SaveObject->PlayerHealth = PC->GetCurrentCharacterHealth();
	SaveObject->PlayerLocation = PC->GetActorLocation();
	SaveObject->PlayerRotation = PC->GetActorRotation();
	
	for(const AEncounterVolume* Volume: AllEncounterVolumes)
	{
		if(Volume->IsComplete()) SaveObject->CompletedEncounters.Add(Volume->GetName());	
	}
	
	UGameplayStatics::AsyncSaveGameToSlot(SaveObject, SaveObject->GetSlotName(), SaveObject->GetSlotIndex());
	if(!bCompletedFirstSave) bCompletedFirstSave = true;
}

void UProjectTentacleGameInstance::RegisterEncounterVolume(AEncounterVolume* Volume)
{
	AllEncounterVolumes.Add(Volume);
}

bool UProjectTentacleGameInstance::ShouldSaveAtPCSpawn() const
{
	return !bCompletedFirstSave;
}

void UProjectTentacleGameInstance::KillActiveUnits()
{
	for(AEncounterVolume* Volume: AllEncounterVolumes)
	{
		if(Volume->IsActive()) Volume->KillUnits();
	}
}

void UProjectTentacleGameInstance::ReloadLastSave()
{
	TryCachePC();
	if(!PC) return;

	PC->OnResetPlayerProperty();
	
	FAsyncLoadGameFromSlotDelegate LoadDelegate;
	LoadDelegate.BindUFunction(this, FName("OnSaveLoad"));

	
	UGameplayStatics::AsyncLoadGameFromSlot(SaveObject->GetSlotName(), SaveObject->GetSlotIndex(), LoadDelegate);
}
