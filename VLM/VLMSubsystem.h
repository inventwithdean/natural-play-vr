// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VLMSubsystem.generated.h"



UENUM(BlueprintType)
enum class ETurnType : uint8
{
	ETT_Left UMETA(DisplayName = "Left"),
	ETT_Right UMETA(DisplayName = "Right"),
	ETT_Around UMETA(DisplayName = "Around"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSetTargetActorToFaceForWidget, AActor*, ActorToFace);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCaptureRenderTargetAIVision);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCaptureRenderTargetPlayerVision);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMoveToRequest, AActor*, TargetActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTurnRequest, ETurnType, TurnType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNormalAIResponseReceived, FString, AIResponse);

/**
 * 
 */
UCLASS()
class NATURALPLAY_API UVLMSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
private:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
private:
	TArray<TSharedPtr<FJsonValue>> Messages;
	TArray<TSharedPtr<FJsonValue>> Tools;
	FString Model;
	UPROPERTY()
	UTextureRenderTarget2D* RenderTarget_AI;
	UPROPERTY()
	UTextureRenderTarget2D* RenderTarget_Player;
	AActor* PlayerActor;
public:
	UPROPERTY(BlueprintAssignable, BlueprintCallable)
	FCaptureRenderTargetAIVision CaptureRenderTargetAIVision;
	UPROPERTY(BlueprintAssignable, BlueprintCallable)
	FCaptureRenderTargetPlayerVision CaptureRenderTargetPlayerVision;
	UPROPERTY(BlueprintAssignable, BlueprintCallable)
	FSetTargetActorToFaceForWidget SetTargetActorToFaceForWidget;
	UFUNCTION(BlueprintCallable)
	void SetRenderTarget_AI(UTextureRenderTarget2D* InRenderTarget);
	UFUNCTION(BlueprintCallable)
	void SetRenderTarget_Player(UTextureRenderTarget2D* InRenderTarget);
	UFUNCTION(BlueprintCallable)
	void SetPlayerActor(AActor* InActor);
	UPROPERTY(BlueprintReadWrite)
	bool bAIVision = true;
	UFUNCTION(BlueprintCallable)
	void SendUserMessage(FString Message);
	void SendChatCompletionRequest();
private:
	TSharedPtr<FJsonValue> BuildSystemMessage(FString Message);
	TSharedPtr<FJsonValue> BuildUserMessage(FString Text, FString ImageBase64);
	TSharedPtr<FJsonValue> BuildAssistantMessage(FString Text);
	TSharedPtr<FJsonValue> BuildAssistantToolCallMessage(TSharedPtr<FJsonValue> ToolCall);
	void OnResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccess);
	FString GetImageURLFromRenderTarget(UTextureRenderTarget2D *InRenderTarget);
public:
	UPROPERTY(BlueprintAssignable)
	FNormalAIResponseReceived NormalAIResponseReceived;
private:
	// Tools 
	TSharedPtr<FJsonValue> BuildTool_MoveTo();
	TSharedPtr<FJsonValue> BuildTool_Turn();
	TSharedPtr<FJsonValue> BuildTool_SaveMemory();
	void HandleToolCall(TSharedPtr<FJsonObject> ToolObject);
	// Individual Tool Handling
	TSharedPtr<FJsonValue> HandleTool_MoveTo(FString ToolId, FString Arguments);
	TSharedPtr<FJsonValue> HandleTool_Turn(FString ToolId, FString Arguments);
	void PruneImages();
public:
	// Broadcasts Move Commands
	UPROPERTY(BlueprintAssignable)
	FMoveToRequest MoveToRequest;
	UPROPERTY(BlueprintAssignable)
	FTurnRequest TurnRequest;
	// Called from AI blueprint, to send new view as user messsage
	UFUNCTION(BlueprintCallable)
	void MoveRequestCompleted();
	UFUNCTION(BlueprintCallable)
	void TurnCompleted();
};
