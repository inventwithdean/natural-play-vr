// Fill out your copyright notice in the Description page of Project Settings.


#include "SpatialRegistrySubsystem.h"

void USpatialRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ActiveVisionActorsCount = 0;
}

void USpatialRegistrySubsystem::Deinitialize()
{
	Super::Deinitialize();
}

int32 USpatialRegistrySubsystem::RegisterActor(AActor* InActor)
{
	int32 ActorID = ActiveVisionActorsCount;
	ActiveVisionActors.Add(ActorID, InActor);
	ActiveVisionActorsCount++;
	return ActorID;
}

AActor* USpatialRegistrySubsystem::GetActor(int32 ActorId)
{
	AActor** ActorPtr = ActiveVisionActors.Find(ActorId);
	if (ActorPtr == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("GetActor: ActorId not found"));
		return nullptr;
	}
	return *ActorPtr;
}
