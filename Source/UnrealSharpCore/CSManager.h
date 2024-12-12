﻿#pragma once

#include <coreclr_delegates.h>
#include <hostfxr.h>
#include "CSAssembly.h"
#include "CSManagedCallbacksCache.h"
#include "CSManager.generated.h"

struct FCSTypeReferenceMetaData;

using FInitializeRuntimeHost = bool (*)(const TCHAR*, FCSManagedPluginCallbacks*, FCSManagedCallbacks::FManagedCallbacks*, const void*);

UCLASS()
class UNREALSHARPCORE_API UCSManager : public UObject, public FUObjectArray::FUObjectDeleteListener
{
	GENERATED_BODY()
public:
	
	static UCSManager& GetOrCreate();
	static UCSManager& Get();
	static void Shutdown();
	
	UPackage* GetUnrealSharpPackage() const { return UnrealSharpPackage; }

	bool LoadAllUserAssemblies();
	
	TSharedPtr<FCSAssembly> LoadAssembly(const FString& AssemblyPath);
	bool UnloadAssembly(const FString& AssemblyName);

	FGCHandle CreateNewManagedObject(UObject* Object, UClass* Class);
	FGCHandle CreateNewManagedObject(UObject* Object, uint8* TypeHandle);
	FGCHandle FindManagedObject(UObject* Object);

	uint8* GetTypeHandle(uint8* AssemblyHandle, const FString& Namespace, const FString& TypeName) const;
	uint8* GetTypeHandle(const FString& AssemblyName, const FString& Namespace, const FString& TypeName) const;
	uint8* GetTypeHandle(const FCSTypeReferenceMetaData& TypeMetaData) const;

	void SetCurrentWorldContext(UObject* WorldContext) { CurrentWorldContext = WorldContext; }
	UObject* GetCurrentWorldContext() const { return CurrentWorldContext; }

	const FCSManagedPluginCallbacks& GetManagedPluginsCallbacks() const { return ManagedPluginsCallbacks; }

private:

	void Initialize();

	load_assembly_and_get_function_pointer_fn InitializeHostfxr() const;
	load_assembly_and_get_function_pointer_fn InitializeHostfxrSelfContained() const;
	
	bool LoadRuntimeHost();
	bool InitializeRuntime();
	
	void RemoveManagedObject(const UObjectBase* Object);

	// Begin FUObjectArray::FUObjectDeleteListener interface
	virtual void NotifyUObjectDeleted(const UObjectBase *Object, int32 Index) override;
	virtual void OnUObjectArrayShutdown() override;
	// End FUObjectArray::FUObjectDeleteListener interface

	void OnEnginePreExit();

	static UCSManager* Instance;

	UPROPERTY()
	TObjectPtr<UPackage> UnrealSharpPackage;

	UPROPERTY()
	TObjectPtr<UObject> CurrentWorldContext;

	TMap<const UObjectBase*, FGCHandle> UnmanagedToManagedMap;
	TMap<FName, TSharedPtr<FCSAssembly>> LoadedPlugins;

	FCSManagedPluginCallbacks ManagedPluginsCallbacks;
	
	//.NET Core Host API
	hostfxr_initialize_for_dotnet_command_line_fn Hostfxr_Initialize_For_Dotnet_Command_Line = nullptr;
	hostfxr_initialize_for_runtime_config_fn Hostfxr_Initialize_For_Runtime_Config = nullptr;
	hostfxr_get_runtime_delegate_fn Hostfxr_Get_Runtime_Delegate = nullptr;
	hostfxr_close_fn Hostfxr_Close = nullptr;

	void* RuntimeHost = nullptr;
	void* UnrealSharpLibraryDLL = nullptr;
	void* UserScriptsDLL = nullptr;
	//End
};
