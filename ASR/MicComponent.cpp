// Fill out your copyright notice in the Description page of Project Settings.


#include "MicComponent.h"

#include "WhisperSubsystem.h"

// Sets default values for this component's properties
UMicComponent::UMicComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UMicComponent::BeginPlay()
{
	Super::BeginPlay();
	WhisperSubsystem = GetWorld()->GetGameInstance()->GetSubsystem<UWhisperSubsystem>();
	SetupMicrophone();
	InputAudio.Reserve(16000*10); // Reserve 10 seconds of input audio at 16000Hz
}

void UMicComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AudioCapture.IsStreamOpen())
	{
		if (AudioCapture.IsCapturing()) AudioCapture.StopStream();
		AudioCapture.CloseStream();
	}
	Super::EndPlay(EndPlayReason);
}


// Called every frame
void UMicComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UMicComponent::SetListening(bool bShouldListen)
{
	if (WhisperSubsystem->IsWorkerRunning()) return;
	bIsListening = bShouldListen;
	if (bShouldListen)
	{
		InputAudio.Empty();
		if (!AudioCapture.IsCapturing()) AudioCapture.StartStream();
	} else
	{
		if (AudioCapture.IsCapturing())
		{
			AudioCapture.StopStream();
			// Start Transcription
			if (InputAudio.Num() > 0) WhisperSubsystem->Transcribe(InputAudio.GetData(), InputAudio.Num());
		}
	}
}

bool UMicComponent::SetupMicrophone()
{
	TArray<Audio::FCaptureDeviceInfo> Devices;
	AudioCapture.GetCaptureDevicesAvailable(Devices);
	if (Devices.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("No Audio Devices Found!"));
		return false;
	}
	for (int32 i = 0; i < Devices.Num(); i++)
	{
		UE_LOG(LogTemp, Warning, TEXT("Device %d: %s"), i, *Devices[i].DeviceName);
	}
	Audio::FAudioCaptureDeviceParams DeviceParams;
	DeviceParams.DeviceIndex = 0;
	DeviceParams.SampleRate = Devices[0].PreferredSampleRate;
	DeviceParams.NumInputChannels = Devices[0].InputChannels;
	auto OnCapture = [this](const void* InAudio, int32 NumFrames, int32 NumChannels, int32 SampleRate, double StreamTime, bool bOverFlow)
	{
		this->OnAudioGenerate(InAudio, NumFrames, NumChannels, SampleRate);
	};
	if (AudioCapture.OpenAudioCaptureStream(DeviceParams, OnCapture, 1024))
	{
		UE_LOG(LogTemp, Warning, TEXT("Opened Audio Capture Stream!"))
		return true;
	}
	UE_LOG(LogTemp, Warning, TEXT("AudioCapture Failed to Open Stream!"));
	return false;
}


void UMicComponent::OnAudioGenerate(const void* InAudio, int32 NumFrames, int32 NumChannels, int32 SampleRate)
{
	if (!bIsListening) return;
	const float * FloatBuffer = static_cast<const float*>(InAudio);
	TArray<float> RawSamples;
	RawSamples.SetNumUninitialized(NumFrames);
	for (int32 i = 0; i < NumFrames; i++)
	{
		RawSamples[i] = FloatBuffer[i*NumChannels];
	}
	TArray<float> ResampledSamples;
	int32 TargetRate = 16000;
	if (SampleRate == TargetRate)
	{
		ResampledSamples = RawSamples;
		for (int32 i = 0; i < NumFrames; i++)
		{
			InputAudio.Add(ResampledSamples[i]);
		}
	} else
	{
		float Ratio = (float) SampleRate / (float) TargetRate;
		int32 NewNumFrames = NumFrames / Ratio;
		ResampledSamples.Reserve(NewNumFrames);
		float SourceIndex = 0.0f;
		while (SourceIndex < NumFrames)
		{
			ResampledSamples.Add(RawSamples[(int32)SourceIndex]);
			SourceIndex+=Ratio;
		}
		for (int32 i = 0; i < NewNumFrames; i++)
		{
			InputAudio.Add(ResampledSamples[i]);
		}
	}
}

