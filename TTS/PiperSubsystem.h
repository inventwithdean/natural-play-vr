// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "sherpa-onnx/c-api/cxx-api.h"

#include "PiperSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class NATURALPLAY_API UPiperSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	UFUNCTION(BlueprintCallable, Category = "AI|TTS")
	void Speak(FString Text);
private:
	UPROPERTY()
	class USoundWaveProcedural* ProceduralSoundWave;
	UPROPERTY()
	UAudioComponent* AudioComponent;
private:
	TSharedPtr<sherpa_onnx::cxx::OfflineTts> Piper;
	bool bIsSpeaking = false;
	bool LoadPiperModel();
	static int32 PiperAudioCallback(const float* Samples, int32 NumSamples);
private:
	// Helpers
	// Get Storage Path where models are stored (C++ can only access actual files on disk,
	// rather than virtual files like those stored in .pak from FPaths::ProjectContentDir() )
	FString GetStoragePath();
	// Copies from Virtual Space (Project Content Dir) to App's Persistent Download Dir
	void CopyAssetsToStorage();
	// Copies whole directory from source to destination
	void CopyDirectory(const FString& SrcDir, const FString &DestDir);
};
