// Fill out your copyright notice in the Description page of Project Settings.
using System.IO;
using UnrealBuildTool;

public class NaturalPlay : ModuleRules
{
	public NaturalPlay(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Json", "JsonUtilities", "HTTP", "AudioCaptureCore", "AudioCapture" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });
	
		string SherpaPath = Path.Combine(ModuleDirectory, "../ThirdParty/SherpaOnnx");
		PublicIncludePaths.Add(Path.Combine(SherpaPath, "include"));
		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			string LibPath = Path.Combine(SherpaPath, "lib", "arm64-v8a");
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libonnxruntime.so"));
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libsherpa-onnx-c-api.so"));
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libsherpa-onnx-cxx-api.so"));
			// So that the .so files are packaged in the .apk
			AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(SherpaPath, "SherpaOnnx_APL.xml"));
		}
		
		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
