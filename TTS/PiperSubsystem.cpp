// Fill out your copyright notice in the Description page of Project Settings.


#include "PiperSubsystem.h"

#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWaveProcedural.h"

using namespace sherpa_onnx::cxx;
static UPiperSubsystem* GlobalPiperSubsystem = nullptr;

void UPiperSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CopyAssetsToStorage();
	LoadPiperModel();
	ProceduralSoundWave = NewObject<USoundWaveProcedural>(this);
	if (ProceduralSoundWave)
	{
		ProceduralSoundWave->SetSampleRate(24000);
		ProceduralSoundWave->NumChannels = 1;
		ProceduralSoundWave->Duration = INDEFINITELY_LOOPING_DURATION;
		ProceduralSoundWave->bLooping = false;
		ProceduralSoundWave->bCanProcessAsync = true;
	}
}

void UPiperSubsystem::Deinitialize()
{
	if (AudioComponent)
	{
		AudioComponent->Stop();
	}
	Super::Deinitialize();
}

bool UPiperSubsystem::LoadPiperModel()
{
	FString StoragePath = GetStoragePath();
	if (!FPaths::FileExists(StoragePath / TEXT("tokens.txt")))
	{
		UE_LOG(LogTemp, Warning, TEXT("tokens.txt doesn't exist!"))
		return false;
	}
	OfflineTtsConfig Config;
	Config.model.vits.model = TCHAR_TO_ANSI(*(StoragePath / TEXT("en_US-hfc_male-medium.onnx")));
	Config.model.vits.data_dir = TCHAR_TO_ANSI(*(StoragePath / TEXT("espeak-ng-data")));
	Config.model.vits.tokens = TCHAR_TO_ANSI(*(StoragePath / TEXT("tokens.txt")));
	Config.model.vits.length_scale = 1.0f;
	Config.model.num_threads = 1;
	Config.model.debug = true;
	
	OfflineTts NewTTS = OfflineTts::Create(Config);
	Piper = MakeShared<OfflineTts>(MoveTemp(NewTTS));
	if (Piper.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Piper Model Loaded!"))
		return true;
	}
	return false;
}

void UPiperSubsystem::Speak(FString Text)
{
	if (!Piper.IsValid()) return;
	if (bIsSpeaking) return;
    
	bIsSpeaking = true;

	if (ProceduralSoundWave)
	{
		ProceduralSoundWave->ResetAudio(); 
		if (!AudioComponent)
		{
			UWorld* World = GetWorld();
			if (World) AudioComponent = UGameplayStatics::CreateSound2D(World, ProceduralSoundWave);
		}
		if (AudioComponent && !AudioComponent->IsPlaying()) AudioComponent->Play();
	}

	std::string TextToSpeak = TCHAR_TO_UTF8(*Text);
	TSharedPtr<OfflineTts> LocalTTS = Piper;
	UPiperSubsystem* RawThis = this;

	Async(EAsyncExecution::Thread, [RawThis, LocalTTS, TextToSpeak]()
	{
	   if (!LocalTTS.IsValid()) return;

	   GlobalPiperSubsystem = RawThis;

	   SherpaOnnxOfflineTtsGenerateWithCallback(
		   LocalTTS->Get(), 
		   TextToSpeak.c_str(), 
		   0, 
		   1.0f, 
		   PiperAudioCallback
	   );

	   GlobalPiperSubsystem = nullptr;

	   AsyncTask(ENamedThreads::GameThread, [RawThis](){
		   if (RawThis) RawThis->bIsSpeaking = false;
	   });
	});
}

// This is the callback Sherpa will fire repeatedly as it generates chunks
int32 UPiperSubsystem::PiperAudioCallback(const float* Samples, int32 NumSamples)
{
	// Safety check: Ensure the subsystem still exists
	if (!GlobalPiperSubsystem || !GlobalPiperSubsystem->IsValidLowLevel())
	{
		return 0; // Stop generation
	}

	if (GlobalPiperSubsystem->ProceduralSoundWave)
	{
		TArray<int16> PCMData;
		PCMData.SetNumUninitialized(NumSamples);
        
		for (int32 i = 0; i < NumSamples; ++i)
		{
			PCMData[i] = FMath::Clamp<int32>(Samples[i] * 32767.0f, -32768, 32767);
		}

		GlobalPiperSubsystem->ProceduralSoundWave->QueueAudio((uint8*)PCMData.GetData(), PCMData.Num() * sizeof(int16));
		return 1; // Continue
	}
    
	return 0; // Stop
}

FString UPiperSubsystem::GetStoragePath()
{
	return FPaths::ProjectPersistentDownloadDir() / TEXT("Piper");
}

void UPiperSubsystem::CopyAssetsToStorage()
{
	FString DestDir = GetStoragePath();
	FString SrcBase = FPaths::ProjectContentDir() / TEXT("OnnxModels") / TEXT("Piper");
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*DestDir))
	{
		PlatformFile.CreateDirectory(*DestDir);
	}
	TArray<FString> FilesToCopy = {
		TEXT("en_US-hfc_male-medium.onnx"),
		TEXT("en_US-hfc_male-medium.onnx.json"),
		TEXT("tokens.txt"),
	};
	for (const FString& FileName: FilesToCopy)
	{
		FString SrcPath = SrcBase / FileName;
		if (!PlatformFile.FileExists(*SrcPath))
		{
			UE_LOG(LogTemp, Error, TEXT("File: %s doesn't exist."), *SrcPath);
			return;
		}
		FString DestPath = DestDir / FileName;
		if (!PlatformFile.FileExists(*DestPath))
		{
			PlatformFile.CopyFile(*DestPath, *SrcPath);
		}
	}
	CopyDirectory(SrcBase / TEXT("espeak-ng-data"), DestDir / TEXT("espeak-ng-data"));
}

void UPiperSubsystem::CopyDirectory(const FString& SrcDir, const FString& DestDir)
{
	// UE_LOG(LogTemp, Warning, TEXT("Trying to Copy Directory: %s To %s"), *SrcDir, *DestDir);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*DestDir))
	{
		PlatformFile.CreateDirectoryTree(*DestDir);
	}
	TArray<FString> FoundFiles;
	PlatformFile.FindFilesRecursively(FoundFiles, *SrcDir, TEXT(""));
	for (const FString& FullSrcPath : FoundFiles)
	{
		FString RelativePath = FullSrcPath;
		// UE_LOG(LogTemp, Warning, TEXT("SrcDir: %s RelativePath: %s"), *FullSrcPath, *RelativePath);
		FPaths::MakePathRelativeTo(RelativePath, *(SrcDir + TEXT("/")));
		// UE_LOG(LogTemp, Warning, TEXT("RelativePath: %s"), *RelativePath);
		FString FullDestPath = FPaths::Combine(DestDir, RelativePath);
	
		FString DestSubDir = FPaths::GetPath(FullDestPath);
		if (!PlatformFile.DirectoryExists(*DestSubDir))
		{
			PlatformFile.CreateDirectoryTree(*DestSubDir);
		}
		if (!PlatformFile.FileExists(*FullDestPath))
		{
			PlatformFile.CopyFile(*FullDestPath, *FullSrcPath);
		}
	}
}
