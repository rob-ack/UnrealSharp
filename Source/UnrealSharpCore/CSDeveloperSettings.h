﻿#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CSDeveloperSettings.generated.h"

UENUM()
enum EAutomaticHotReloadMethod : uint8
{
	// Automatically Hot Reloads when script changes are saved
	OnScriptSave,
	// Automatically Hot Reloads when the built .NET modules changed (build the C# project in your IDE and UnrealSharp will automatically reload)
	OnModuleChange,
	// Wait for the Editor to gain focus before Hot Reloading
	OnEditorFocus,
	// Will not Hot Reload automatically
	Off,
};

UCLASS(config = EditorPerProjectUserSettings, meta = (DisplayName = "UnrealSharp Settings"))
class UNREALSHARPCORE_API UCSDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	// Should we exit PIE when an exception is thrown in C#?
	UPROPERTY(EditDefaultsOnly, config, Category = "UnrealSharp | Debugging")
	bool bCrashOnException = true;
	
	// Whether Hot Reload should automatically start on script save, gaining Editor focus, or not at all.
	UPROPERTY(EditDefaultsOnly, config, Category = "UnrealSharp | Hot Reload")
	TEnumAsByte<EAutomaticHotReloadMethod> AutomaticHotReloading = OnScriptSave;

	// Should we suffix generated types' DisplayName with "TypeName (C#)"?
	// Needs restart to take effect.
	UPROPERTY(EditDefaultsOnly, config, Category = "UnrealSharp | Type Generation")
	bool bSuffixGeneratedTypes = false;
};
