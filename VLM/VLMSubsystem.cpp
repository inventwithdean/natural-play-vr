// Fill out your copyright notice in the Description page of Project Settings.


#include "VLMSubsystem.h"

#include "HttpModule.h"
#include "ImageUtils.h"
#include "SpatialRegistrySubsystem.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Interfaces/IHttpResponse.h"
#include "Util/ProgressCancel.h"

void UVLMSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Model = "ministral-3-8b";
	TSharedPtr<FJsonValue> Tool_MoveTo = BuildTool_MoveTo();
	Tools.Add(Tool_MoveTo);
	TSharedPtr<FJsonValue> Tool_Turn = BuildTool_Turn();
	Tools.Add(Tool_Turn);
	// FString SystemPrompt = TEXT("You are an AI inside a virtual environment which can see in first person mode. Use the tools whenever necessary! If user is ambiguous in move commands, use your own brain to pick any one. Use turn tool to look around when necessary. When you execute a tool, you'll instantly get feedback if tool executes, then soon you'll get the new view of your eyes. Always be concise, like 1-2 sentences. At every turn you will be given the current view of your eyes, and previous views will be deleted. Have fun!");
	FString SystemPrompt = TEXT(R"(You are a loyal Medieval Knight bodyguard exploring with the player.
For your help, many objects have circular numerical ids on top of them which you can use with tools. Do NOT talk about those ids. They are only for your help.
If you want to move to the player, use object_id = -1 
And be concise in your responses, like 1-2 sentences.
You will get both views, from your eyes sometimes, and from what the player is seeing.
 )");
	TSharedPtr<FJsonValue> SystemMessage = BuildSystemMessage(SystemPrompt);
	Messages.Add(SystemMessage);
}

void UVLMSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UVLMSubsystem::SetRenderTarget_AI(UTextureRenderTarget2D* InRenderTarget)
{
	RenderTarget_AI = InRenderTarget;
}

void UVLMSubsystem::SetRenderTarget_Player(UTextureRenderTarget2D* InRenderTarget)
{
	RenderTarget_Player = InRenderTarget;
}

void UVLMSubsystem::SetPlayerActor(AActor* InActor)
{
	PlayerActor = InActor;
}

void UVLMSubsystem::SendUserMessage(FString Message)
{
	// Get The Image from AI NPC's Render Target
	// ⚠️ Remember to capture the environment before calling this function
	FString ImageURL = "";
	if (bAIVision)
	{
		if (RenderTarget_AI)
		{
			ImageURL = GetImageURLFromRenderTarget(RenderTarget_AI);
		}
	} else
	{
		if (RenderTarget_Player)
		{
			ImageURL = GetImageURLFromRenderTarget(RenderTarget_Player);
		}
	}
	// Prune Images first.
	PruneImages();
	TSharedPtr<FJsonValue> UserMessage = BuildUserMessage(Message, ImageURL);
	Messages.Add(UserMessage);
	
	SendChatCompletionRequest();
}

void UVLMSubsystem::SendChatCompletionRequest()
{
	TSharedPtr<FJsonObject> ChatCompletionRequestObject = MakeShareable(new FJsonObject());
	ChatCompletionRequestObject->SetStringField(TEXT("model"), Model);
	ChatCompletionRequestObject->SetArrayField(TEXT("messages"), Messages);
	ChatCompletionRequestObject->SetArrayField(TEXT("tools"), Tools);
	
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(ChatCompletionRequestObject.ToSharedRef(), Writer);
	UE_LOG(LogTemp, Warning, TEXT("%s"), *OutputString)
	// Build Request
	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(TEXT("http://192.168.137.1:8080/v1/chat/completions"));
	
	Request->SetVerb( TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(OutputString);
	
	Request->OnProcessRequestComplete().BindUObject(this, &UVLMSubsystem::OnResponseReceived);
	Request->ProcessRequest();
}



void UVLMSubsystem::OnResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccess)
{
	FString ContentString = Response->GetContentAsString();
	UE_LOG(LogTemp, Warning, TEXT("%s"), *ContentString);
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ContentString);
	TSharedPtr<FJsonObject> OutObject = MakeShareable(new FJsonObject());
	FJsonSerializer::Deserialize(Reader, OutObject);

	const TArray<TSharedPtr<FJsonValue>>* ChoicesArray = nullptr;
	if (OutObject->TryGetArrayField(TEXT("choices"), ChoicesArray))
	{
		for (auto Choice: *ChoicesArray)
		{
			const TSharedPtr<FJsonObject>* MessageObject = nullptr;
			FString FinishReason;
			if (!Choice->AsObject()->TryGetStringField(TEXT("finish_reason"), FinishReason)) return;
			UE_LOG(LogTemp, Warning, TEXT("Finish Reason: %s"), *FinishReason)
			if (!Choice->AsObject()->TryGetObjectField(TEXT("message"), MessageObject)) return;
			// AI Responded with normal text
			if (FinishReason.Equals(TEXT("stop")))
			{
				FString ExtractedText;
				UE_LOG(LogTemp, Warning, TEXT("Normal Response!"))
				MessageObject->Get()->TryGetStringField(TEXT("content"), ExtractedText);
				UE_LOG(LogTemp, Warning, TEXT("Extracted Text: %s"), *ExtractedText)
				TSharedPtr<FJsonValue> AssistantJson = BuildAssistantMessage(ExtractedText);
				Messages.Add(AssistantJson);
				ExtractedText = ExtractedText.Replace(TEXT("*"), TEXT(""));
				NormalAIResponseReceived.Broadcast(ExtractedText);
			}
			if (FinishReason.Equals(TEXT("tool_calls")))
			{
				/*
				"message":{
							"role":"assistant",
							"content":"",
							"tool_calls":[
								{
									"type":"function",
									"function":
										{
											"name":"move_to",
											"arguments":"{\"object_id\": 1}"
										},
									"id":"ZRrn12xER5l8J8R235urBgm6RQIIBgKb"
								}
							]
						}
				 */
				const TArray<TSharedPtr<FJsonValue>>* ToolCallsArray = nullptr;
				if (!MessageObject->Get()->TryGetArrayField(TEXT("tool_calls"), ToolCallsArray)) return;
				for (auto ToolCall: *ToolCallsArray)
				{
					TSharedPtr<FJsonValue> AssistantToolCallMessage = BuildAssistantToolCallMessage(ToolCall);
					Messages.Add(AssistantToolCallMessage);
					UE_LOG(LogTemp, Warning, TEXT("Handle Tool Calls!"))
					HandleToolCall(ToolCall->AsObject());
				}
			}
			
		}
	}
}


void UVLMSubsystem::HandleToolCall(TSharedPtr<FJsonObject> ToolObject)
{
	/* Example
	{
		"type":"function",
		"function":
			{
				"name":"move_to",
				"arguments":"{\"object_id\": 1}"
			},
		"id":"ZRrn12xER5l8J8R235urBgm6RQIIBgKb"
	}
	 */
	FString ToolId;
	if (!ToolObject->TryGetStringField(TEXT("id"), ToolId)) return;
	const TSharedPtr<FJsonObject>* FunctionObject = nullptr;
	if (!ToolObject->TryGetObjectField(TEXT("function"), FunctionObject)) return;
	FString FunctionName;
	if (!FunctionObject->Get()->TryGetStringField(TEXT("name"), FunctionName)) return;
	FString Arguments;
	if (!FunctionObject->Get()->TryGetStringField(TEXT("arguments"), Arguments)) return;
	UE_LOG(LogTemp, Warning, TEXT("Function Called: %s\nArguments: %s"), *FunctionName, *Arguments)
	if (FunctionName.Equals(TEXT("move_to")))
	{
		UE_LOG(LogTemp, Warning, TEXT("Moving to Object!"))
		TSharedPtr<FJsonValue> ToolResponse = HandleTool_MoveTo(ToolId, Arguments);
		Messages.Add(ToolResponse);
		if (ToolResponse->AsObject()->GetStringField(TEXT("content")).StartsWith("Failure"))
		{
			SendChatCompletionRequest();
			UE_LOG(LogTemp, Warning, TEXT("Sending Chat Completion Request Again."))
		}
	} else if (FunctionName.Equals(TEXT("turn")))
	{
		UE_LOG(LogTemp, Warning, TEXT("Turning!"))
		TSharedPtr<FJsonValue> ToolResponse = HandleTool_Turn(ToolId, Arguments);
		Messages.Add(ToolResponse);
	} else
	{
		UE_LOG(LogTemp, Warning, TEXT("Tool Call Not Implemented"));
	}
}

FString UVLMSubsystem::GetImageURLFromRenderTarget(UTextureRenderTarget2D* InRenderTarget)
{
	TArray<FColor> OutArray;
	InRenderTarget->GameThread_GetRenderTargetResource()->ReadPixels(OutArray);
	
	FImageView ImageView(OutArray.GetData(), 1366, 768, ERawImageFormat::BGRA8);
	
	TArray64<uint8> OutData;
	FImageUtils::CompressImage(OutData, TEXT("jpeg"), ImageView, 85);
	// Save Image
	
	FString SavedDir = FPaths::ProjectPersistentDownloadDir() / TEXT("RenderTargets");
	FString FileName = FString::Printf(TEXT("Capture_%s.jpg"), *FDateTime::Now().ToString());
	FString FullPath = SavedDir / FileName;
	if (FFileHelper::SaveArrayToFile(OutData, *FullPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("Image successfully saved to: %s"), *FullPath)
	} else
	{
		UE_LOG(LogTemp, Warning, TEXT("Image Failed to Save"))
	}
	
	FString Base64Image = FBase64::Encode(OutData.GetData(), (uint32)OutData.Num());
	FString ImageURL = TEXT("data:image/jpeg;base64,") + Base64Image;
	return ImageURL;
}

TSharedPtr<FJsonValue> UVLMSubsystem::BuildSystemMessage(FString Message)
{
	// Builds and returns
	// {"role": "system", "content": "System Prompt"}
	TSharedPtr<FJsonObject> SystemMessageObject = MakeShareable( new FJsonObject );
	SystemMessageObject->SetStringField(TEXT("role"), TEXT("system"));
	SystemMessageObject->SetStringField(TEXT("content"), Message);
	
	TSharedPtr<FJsonValueObject> SystemMessage = MakeShareable(new FJsonValueObject(SystemMessageObject));
	return SystemMessage;
}

TSharedPtr<FJsonValue> UVLMSubsystem::BuildUserMessage(FString Text, FString ImageURL)
{
	// Builds and returns
	// {"role": "user", "content": [...]}
	TSharedPtr<FJsonObject> UserMessageObject = MakeShareable( new FJsonObject );
	UserMessageObject->SetStringField(TEXT("role"), TEXT("user"));
	// content array "content": [{text_thing}, {image_thing}]
	TArray<TSharedPtr<FJsonValue>>  ContentArray;
	if (!Text.IsEmpty())
	{
		// Build Text object
		// { "type": "text", "text": "what's in this image?" }
		TSharedPtr<FJsonObject> TextContentObject = MakeShareable( new FJsonObject );
		TextContentObject->SetStringField(TEXT("type"), TEXT("text"));
		TextContentObject->SetStringField(TEXT("text"), Text);
		// Wrap the object in FJsonValue
		TSharedPtr<FJsonValueObject> TextContent = MakeShareable(new FJsonValueObject(TextContentObject));
		ContentArray.Add(TextContent);
	}
	
	if (!ImageURL.IsEmpty())
	{
		// Build Image Object
		// { "type": "image_url", "image_url":  { "url": "data:image/jpeg;base64,{base64_image}" } }
		TSharedPtr<FJsonObject> ImageContentObject = MakeShareable( new FJsonObject );
		ImageContentObject->SetStringField(TEXT("type"), TEXT("image_url"));
		TSharedPtr<FJsonObject> ImageURLObject = MakeShareable(new FJsonObject);
		ImageURLObject->SetStringField(TEXT("url"), ImageURL);
		ImageContentObject->SetObjectField(TEXT("image_url"), ImageURLObject);
		// Wrap the object in FJsonValue
		TSharedPtr<FJsonValueObject> ImageContent = MakeShareable(new FJsonValueObject(ImageContentObject));
		ContentArray.Add(ImageContent);
	}
	UserMessageObject->SetArrayField(TEXT("content"), ContentArray);
	TSharedPtr<FJsonValueObject> UserMessage = MakeShareable(new FJsonValueObject(UserMessageObject));
	return UserMessage;
}


TSharedPtr<FJsonValue> UVLMSubsystem::BuildAssistantMessage(FString Text)
{
	// Builds and returns
	// {"role": "assistant", "content": "assistant's response"}
	TSharedPtr<FJsonObject> AssistantMessageObject = MakeShareable( new FJsonObject );
	//	AssistantMessageObject->SetStringField(TEXT("type"), TEXT("message"));
	AssistantMessageObject->SetStringField(TEXT("role"), TEXT("assistant"));
	AssistantMessageObject->SetStringField(TEXT("content"), Text);
	
	TSharedPtr<FJsonValueObject> AssistantMessage = MakeShareable(new FJsonValueObject(AssistantMessageObject));
	return AssistantMessage;
}

TSharedPtr<FJsonValue> UVLMSubsystem::BuildAssistantToolCallMessage(TSharedPtr<FJsonValue> ToolCall)
{
	TSharedPtr<FJsonObject> AssistantToolCallMessage = MakeShareable(new FJsonObject());
	AssistantToolCallMessage->SetStringField(TEXT("role"), TEXT("assistant"));
	AssistantToolCallMessage->SetStringField(TEXT("content"), TEXT(""));
	TArray<TSharedPtr<FJsonValue>> ToolCallsArray;
	ToolCallsArray.Add(ToolCall);
	AssistantToolCallMessage->SetArrayField(TEXT("tool_calls"), ToolCallsArray);
	TSharedPtr<FJsonValue> AssistantToolCall = MakeShareable(new FJsonValueObject(AssistantToolCallMessage));
	return AssistantToolCall;
}

void UVLMSubsystem::PruneImages()
{
	/*
	{
	  role: 'user',
	  content: [
		{
		  type: 'text',
		  text: "What's in this image?",
		},
		{
		  type: 'image_url',
		  imageUrl: {
			url: 'https://upload.wikimedia.org/wikipedia/commons/thumb/d/dd/Gfp-wisconsin-madison-the-nature-boardwalk.jpg/2560px-Gfp-wisconsin-madison-the-nature-boardwalk.jpg',
		  },
		},
	  ],
	},
	 */
	
	for (auto& Message: Messages)
	{
		FString Role;
		if (!Message->AsObject()->TryGetStringField(TEXT("role"), Role)) continue;
		if (Role.Equals(TEXT("user")))
		{
			TArray<TSharedPtr<FJsonValue>> ContentArrayPruned;
			const TArray<TSharedPtr<FJsonValue>>* ContentArray = nullptr;
			if (Message->AsObject()->TryGetArrayField(TEXT("content"), ContentArray))
			{
				for (auto ContentObject: *ContentArray)
				{
					FString ContentObjectType;
					if (!ContentObject->AsObject()->TryGetStringField(TEXT("type"), ContentObjectType))
					{
						UE_LOG(LogTemp, Error, TEXT("This should not have happened!"))
						continue;
					}
					if (ContentObjectType.Equals(TEXT("text")))
					{
						ContentArrayPruned.Add(ContentObject);
					}
				}
			}
			Message->AsObject()->SetArrayField(TEXT("content"), ContentArrayPruned);
		}
	}
}

void UVLMSubsystem::MoveRequestCompleted()
{
	// Adding fake assistant messages were necessary for stability. Otherwise, the model kept calling tools again, if we 
	// just let it respond to the instant tool initiation feedback.
	TSharedPtr<FJsonValue> AssistantMessage = BuildAssistantMessage(TEXT("I am moving towards the object and am ready to see the new view."));
	Messages.Add(AssistantMessage);
	SendUserMessage(TEXT("You arrived at the object!"));
}

void UVLMSubsystem::TurnCompleted()
{
	// Add a fake assistant message
	TSharedPtr<FJsonValue> AssistantMessage = BuildAssistantMessage(TEXT("I have turned around and am ready to see the new view."));
	Messages.Add(AssistantMessage);
	SendUserMessage(TEXT("You successfully turned!"));
}

/* Build Tools and Handle Tools */


TSharedPtr<FJsonValue> UVLMSubsystem::BuildTool_MoveTo()
{
	/* Builds MoveTo tool
	{
	  "type": "function",
	  "function": {
		"name": "move_to",
		"description": "Moves to an object with a specific id.",
		"parameters": {
		  "type": "object",
		  "properties": {
			"object_id": {
			  "type": "integer",
			  "description": "Id of the object to move to."
			}
		  },
		  "required": ["object_id"]
		}
	  }
	}
	 */
	TSharedPtr<FJsonObject> ToolObject = MakeShareable(new FJsonObject());
	ToolObject->SetStringField(TEXT("type"), TEXT("function"));
	
	TSharedPtr<FJsonObject> FunctionObject = MakeShareable(new FJsonObject());
	FunctionObject->SetStringField(TEXT("name"), TEXT("move_to"));
	FunctionObject->SetStringField(TEXT("description"), TEXT("Moves to an object with a specific id."));
	
	TSharedPtr<FJsonObject> ParametersObject = MakeShareable(new FJsonObject());
	ParametersObject->SetStringField(TEXT("type"), TEXT("object"));
	/*
	"properties": {
			"object_id": {
			  "type": "int",
			  "description": "Id of the object to move to."
			}
		  },
	 */
	TSharedPtr<FJsonObject> PropertiesObject = MakeShareable(new FJsonObject());
	
	TSharedPtr<FJsonObject> ObjectIdObject = MakeShareable(new FJsonObject());
	ObjectIdObject->SetStringField(TEXT("type"), TEXT("integer"));
	ObjectIdObject->SetStringField(TEXT("description"), TEXT("Id of the object to move to."));
	
	PropertiesObject->SetObjectField(TEXT("object_id"), ObjectIdObject);
	
	ParametersObject->SetObjectField(TEXT("properties"), PropertiesObject);
	
	TArray<TSharedPtr<FJsonValue>> RequiredArray;
	RequiredArray.Add(MakeShareable(new FJsonValueString(TEXT("object_id"))));
	
	ParametersObject->SetArrayField(TEXT("required"), RequiredArray);
	FunctionObject->SetObjectField(TEXT("parameters"), ParametersObject);
	
	ToolObject->SetObjectField(TEXT("function"), FunctionObject);
	
	TSharedPtr<FJsonValue> Tool = MakeShareable(new FJsonValueObject(ToolObject));
	return Tool;
}

TSharedPtr<FJsonValue> UVLMSubsystem::BuildTool_Turn()
{
	/* Builds Turn tool
	{
	  "type": "function",
	  "function": {
		"name": "turn",
		"description": "Allows you to turn left, right and around.",
		"parameters": {
		  "type": "object",
		  "properties": {
			"direction": {
			  "type": "string",
			  "enum": ["left", "right" "around"],
			  "description": "The direction to turn. 'left' is 90 degrees left, 'right' is 90 degrees right, 'around' is 180 degrees."
			}
		  },
		  "required": ["direction"]
		}
	  }
	}
	 */
	TSharedPtr<FJsonObject> ToolObject = MakeShareable(new FJsonObject());
	ToolObject->SetStringField(TEXT("type"), TEXT("function"));
	
	TSharedPtr<FJsonObject> FunctionObject = MakeShareable(new FJsonObject());
	FunctionObject->SetStringField(TEXT("name"), TEXT("turn"));
	FunctionObject->SetStringField(TEXT("description"), TEXT("Allows you to turn left, right and around."));
	
	TSharedPtr<FJsonObject> ParametersObject = MakeShareable(new FJsonObject());
	ParametersObject->SetStringField(TEXT("type"), TEXT("object"));
	/*
	"properties": {
		"direction": {
			  "type": "string",
			  "enum": ["left", "right" "around"],
			  "description": "The direction to turn. 'left' is 90 degrees left, 'right' is 90 degrees right, 'around' is 180 degrees."
			}
		},
	*/
	TSharedPtr<FJsonObject> PropertiesObject = MakeShareable(new FJsonObject());
	
	TSharedPtr<FJsonObject> DirectionObject = MakeShareable(new FJsonObject());
	DirectionObject->SetStringField(TEXT("type"), TEXT("string"));
	// Enum Array
	TArray<TSharedPtr<FJsonValue>> EnumArray;
	EnumArray.Add(MakeShareable(new FJsonValueString(TEXT("left"))));
	EnumArray.Add(MakeShareable(new FJsonValueString(TEXT("right"))));
	EnumArray.Add(MakeShareable(new FJsonValueString(TEXT("around"))));
	DirectionObject->SetArrayField(TEXT("enum"), EnumArray);
	DirectionObject->SetStringField(TEXT("description"), TEXT("The direction to turn. 'left' is 90 degrees left, 'right' is 90 degrees right, 'around' is 180 degrees."));
	
	PropertiesObject->SetObjectField(TEXT("direction"), DirectionObject);
	
	ParametersObject->SetObjectField(TEXT("properties"), PropertiesObject);
	
	TArray<TSharedPtr<FJsonValue>> RequiredArray;
	RequiredArray.Add(MakeShareable(new FJsonValueString(TEXT("direction"))));
	
	ParametersObject->SetArrayField(TEXT("required"), RequiredArray);
	FunctionObject->SetObjectField(TEXT("parameters"), ParametersObject);
	
	ToolObject->SetObjectField(TEXT("function"), FunctionObject);
	
	TSharedPtr<FJsonValue> Tool = MakeShareable(new FJsonValueObject(ToolObject));
	return Tool;
}

TSharedPtr<FJsonValue> UVLMSubsystem::BuildTool_SaveMemory()
{
	/* Builds Memory tool
	{
	  "type": "function",
	  "function": {
		"name": "save_memory",
		"description": "Allows you to save memory of important things.",
		"parameters": {
		  "type": "object",
		  "properties": {
			"memory": {
			  "type": "string",
			  "description": "Short description of things that you think are important."
			}
		  },
		  "required": ["memory"]
		}
	  }
	}
	 */
	TSharedPtr<FJsonObject> ToolObject = MakeShareable(new FJsonObject());
	ToolObject->SetStringField(TEXT("type"), TEXT("function"));
	
	TSharedPtr<FJsonObject> FunctionObject = MakeShareable(new FJsonObject());
	FunctionObject->SetStringField(TEXT("name"), TEXT("memory"));
	FunctionObject->SetStringField(TEXT("description"), TEXT("Allows you to save memory of important things."));
	
	TSharedPtr<FJsonObject> ParametersObject = MakeShareable(new FJsonObject());
	ParametersObject->SetStringField(TEXT("type"), TEXT("object"));
	/*
	"properties": {
			"memory": {
			  "type": "string",
			  "description": "Short description of things that you think are important."
			}
		  },
	*/
	TSharedPtr<FJsonObject> PropertiesObject = MakeShareable(new FJsonObject());
	
	TSharedPtr<FJsonObject> MemoryObject = MakeShareable(new FJsonObject());
	MemoryObject->SetStringField(TEXT("type"), TEXT("string"));
	MemoryObject->SetStringField(TEXT("description"), TEXT("Short description of things that you think are important."));
	
	PropertiesObject->SetObjectField(TEXT("memory"), MemoryObject);
	
	ParametersObject->SetObjectField(TEXT("properties"), PropertiesObject);
	
	TArray<TSharedPtr<FJsonValue>> RequiredArray;
	RequiredArray.Add(MakeShareable(new FJsonValueString(TEXT("memory"))));
	
	ParametersObject->SetArrayField(TEXT("required"), RequiredArray);
	FunctionObject->SetObjectField(TEXT("parameters"), ParametersObject);
	
	ToolObject->SetObjectField(TEXT("function"), FunctionObject);
	
	TSharedPtr<FJsonValue> Tool = MakeShareable(new FJsonValueObject(ToolObject));
	return Tool;
}


TSharedPtr<FJsonValue> UVLMSubsystem::HandleTool_MoveTo(FString ToolId, FString Arguments)
{
	// "arguments":"{\"object_id\": 1}"
	
	/* Need to Add this into messages
	{
	  "role": "tool",
	  "tool_call_id": "call_abc123",
	  "content": "[{\"id\": 4300, \"title\": \"Ulysses\", \"authors\": [{\"name\": \"Joyce, James\"}]}]"
	}
	*/
	// Query the Spatial Registry Subsystem to get associated actor with this id, and then fire a delegate to let the 
	// AI know it needs to move to this object
	
	TSharedPtr<FJsonObject> ToolResponseObject = MakeShareable(new FJsonObject());
	ToolResponseObject->SetStringField(TEXT("role"), TEXT("tool"));
	ToolResponseObject->SetStringField(TEXT("tool_call_id"), ToolId);
	
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Arguments);
	TSharedPtr<FJsonObject> ArgumentsObject;
	// Have to use multiple if-else blocks because we need to send tool response in every case.
	if (!FJsonSerializer::Deserialize(Reader, ArgumentsObject))
	{
		ToolResponseObject->SetStringField(TEXT("content"), TEXT("Failure: Malformed arguments!"));
	} else
	{
		int32 ObjectId;
		if (!ArgumentsObject->TryGetNumberField(TEXT("object_id"), ObjectId))
		{
			ToolResponseObject->SetStringField(TEXT("content"), TEXT("Failure: Could not find id in arguments!"));
		} else
		{
			if (ObjectId == -1)
			{
				if (PlayerActor) MoveToRequest.Broadcast(PlayerActor);
				ToolResponseObject->SetStringField(TEXT("content"), TEXT("You're moving towards player now!"));
			} else
			{
				USpatialRegistrySubsystem* SpatialRegistrySubsystem = GetGameInstance()->GetSubsystem<USpatialRegistrySubsystem>();
				if (!SpatialRegistrySubsystem)
				{
					ToolResponseObject->SetStringField(TEXT("content"), TEXT("Failure: Internal Error. Spatial Registry Subsystem not found."));
				} else
				{
					AActor* Actor = SpatialRegistrySubsystem->GetActor(ObjectId);
					if (Actor)
					{
						MoveToRequest.Broadcast(Actor);
						ToolResponseObject->SetStringField(TEXT("content"), TEXT("You're moving towards the object now!"));
					} else
					{
						ToolResponseObject->SetStringField(TEXT("content"), TEXT("Failure: Make sure you're using the right id!"));
					}
				}
			}
		}
	}
	TSharedPtr<FJsonValue> ToolResponse = MakeShareable(new FJsonValueObject(ToolResponseObject));
	return ToolResponse;
}

TSharedPtr<FJsonValue> UVLMSubsystem::HandleTool_Turn(FString ToolId, FString Arguments)
{
	// "arguments":"{\"direction\": \"around\"}"
	
	/* Need to Add this into messages
	{
	  "role": "tool",
	  "tool_call_id": "call_abc123",
	  "content": "[{\"id\": 4300, \"title\": \"Ulysses\", \"authors\": [{\"name\": \"Joyce, James\"}]}]"
	}
	*/
	
	TSharedPtr<FJsonObject> ToolResponseObject = MakeShareable(new FJsonObject());
	ToolResponseObject->SetStringField(TEXT("role"), TEXT("tool"));
	ToolResponseObject->SetStringField(TEXT("tool_call_id"), ToolId);
	
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Arguments);
	TSharedPtr<FJsonObject> ArgumentsObject;
	// Have to use multiple if-else blocks because we need to send tool response in every case.
	if (!FJsonSerializer::Deserialize(Reader, ArgumentsObject))
	{
		ToolResponseObject->SetStringField(TEXT("content"), TEXT("Malformed arguments!"));
	} else
	{
		FString Direction;
		if (!ArgumentsObject->TryGetStringField(TEXT("direction"), Direction))
		{
			ToolResponseObject->SetStringField(TEXT("content"), TEXT("Could not find direction in arguments!"));
		} else
		{
			ETurnType DirectionType;
			if (Direction.Equals(TEXT("left")))
			{
				DirectionType = ETurnType::ETT_Left;	
			} else if (Direction.Equals(TEXT("right")))
			{
				DirectionType = ETurnType::ETT_Right;
			} else // around
			{
				DirectionType = ETurnType::ETT_Around;
			}
			UE_LOG(LogTemp, Warning, TEXT("Broadcasting Direction: %s"), *Direction)
			TurnRequest.Broadcast(DirectionType);
			ToolResponseObject->SetStringField(TEXT("content"), TEXT("Initiating Turn. You will soon get the view."));
		}
	}
	TSharedPtr<FJsonValue> ToolResponse = MakeShareable(new FJsonValueObject(ToolResponseObject));
	return ToolResponse;
}
