// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SpatialRegistrySubsystem.generated.h"

/**
 * 
 */
UCLASS()
class NATURALPLAY_API USpatialRegistrySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
private:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
private:
	UPROPERTY()
	TMap<int32, AActor*> ActiveVisionActors;
	int32 ActiveVisionActorsCount;
public:
	UFUNCTION(BlueprintCallable, Category = "Spatial Registry")
	int32 RegisterActor(AActor* InActor);
	AActor* GetActor(int32 ActorId);
};
