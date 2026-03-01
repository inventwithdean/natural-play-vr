// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "sherpa-onnx/c-api/cxx-api.h"

#include "WhisperSubsystem.generated.h"

/**
 * 
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTranscriptionCompleteSignature, FString, Transcription);

UCLASS()
class NATURALPLAY_API UWhisperSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
private:
	TSharedPtr<sherpa_onnx::cxx::OfflineRecognizer> Whisper;
public:
	UPROPERTY(BlueprintAssignable)
	FOnTranscriptionCompleteSignature OnTranscriptionComplete;
private:
	bool LoadWhisperModel();
	bool bIsWorkerRunning = false;
public:
	void Transcribe(const float* InData, int32 NumSamples);
	bool IsWorkerRunning() const { return bIsWorkerRunning; };
private:
	FString GetStoragePath();
	// Copies model files from virtual space (.paks) to android file system (persistent storage dir)
	void CopyAssetsToStorage();
};
