// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AudioCaptureCore.h"
#include "Components/ActorComponent.h"
#include "MicComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class NATURALPLAY_API UMicComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UMicComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	UFUNCTION( BlueprintCallable, Category="AI|ASR")
	void SetListening(bool bShouldListen);
	UPROPERTY(BlueprintReadOnly, Category="AI|ASR")
	bool bIsListening = false;
private:
	bool SetupMicrophone();
	TArray<float> InputAudio;
private:
	UPROPERTY()
	class UWhisperSubsystem* WhisperSubsystem;
	Audio::FAudioCapture AudioCapture;
	void OnAudioGenerate(const void* InAudio, int32 NumFrames, int32 NumChannels, int32 SampleRate);
};
