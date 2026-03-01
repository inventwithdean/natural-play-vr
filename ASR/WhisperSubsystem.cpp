// Fill out your copyright notice in the Description page of Project Settings.


#include "WhisperSubsystem.h"

using namespace sherpa_onnx::cxx;

void UWhisperSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CopyAssetsToStorage();
	LoadWhisperModel();
}

void UWhisperSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

bool UWhisperSubsystem::LoadWhisperModel()
{
	FString StoragePath = GetStoragePath();
	OfflineRecognizerConfig Config;
	Config.model_config.whisper.encoder = TCHAR_TO_ANSI(*(StoragePath / "encoder.onnx"));
	Config.model_config.whisper.decoder = TCHAR_TO_ANSI(*(StoragePath / "decoder.onnx"));
	Config.model_config.tokens = TCHAR_TO_ANSI(*(StoragePath / "tokens.txt"));
	
	Config.model_config.num_threads = 1;
	Config.model_config.debug = false;
	OfflineRecognizer NewRecognizer = OfflineRecognizer::Create(Config);
	Whisper = MakeShared<OfflineRecognizer>(MoveTemp(NewRecognizer));
	if (Whisper.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Loaded Whisper Model!"))
		return true;
	}
	return false;
}

void UWhisperSubsystem::Transcribe(const float* InData, int32 NumSamples)
{
	if (!Whisper.IsValid() ) return;
	if (bIsWorkerRunning) return;
	TWeakObjectPtr<UWhisperSubsystem> WeakThis(this);
	TSharedPtr<OfflineRecognizer> LocalWhisper = Whisper;
	Async(EAsyncExecution::Thread, [WeakThis, LocalWhisper, InData, NumSamples]()
	{
		OfflineStream LocalStream = LocalWhisper->CreateStream();
		LocalStream.AcceptWaveform(16000, InData, NumSamples);
		LocalWhisper->Decode(&LocalStream);
		if (const SherpaOnnxOfflineRecognizerResult* Result = SherpaOnnxGetOfflineStreamResult(LocalStream.Get()))
		{
			FString Transcription = UTF8_TO_TCHAR(Result->text);
			SherpaOnnxDestroyOfflineRecognizerResult(Result);
			if (!Transcription.IsEmpty())
			{
				AsyncTask(ENamedThreads::GameThread, [WeakThis, Transcription]()
				{
					if (UWhisperSubsystem* StrongThis = WeakThis.Get())
					{
						UE_LOG(LogTemp, Warning, TEXT("Transcription: %s"), *Transcription);
						StrongThis->OnTranscriptionComplete.Broadcast(Transcription);
						StrongThis->bIsWorkerRunning = false; // Allow microphone to accept input again!
					}
				});
			}
		}
	});
}


FString UWhisperSubsystem::GetStoragePath()
{
	return FPaths::ProjectPersistentDownloadDir() / TEXT("Whisper");
}

void UWhisperSubsystem::CopyAssetsToStorage()
{
	FString DestDir = GetStoragePath();
	FString SrcBase = FPaths::ProjectContentDir() / TEXT("OnnxModels") / TEXT("Whisper");
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*DestDir))
	{
		PlatformFile.CreateDirectory(*DestDir);
	}
	TArray<FString> FilesToCopy = {
		TEXT("encoder.onnx"),
		TEXT("decoder.onnx"),
		TEXT("tokens.txt")
	};
	
	for (const FString& FileName : FilesToCopy)
	{
		FString SrcPath = SrcBase / FileName;
		if (!PlatformFile.FileExists(*SrcPath))
		{
			UE_LOG(LogTemp, Error, TEXT("File Does Not Exists: %s"), *SrcPath);
			return;
		}
		FString DestPath = DestDir / FileName;
		PlatformFile.CopyFile(*DestPath, *SrcPath);
	}
}
