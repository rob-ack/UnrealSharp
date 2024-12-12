#include "CSTypeRegistry.h"
#include "UnrealSharpCore/UnrealSharpCore.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "TypeInfo/CSClassInfo.h"
#include "UnrealSharpUtilities/UnrealSharpUtils.h"

template<typename T>
void InitializeBuilders(TMap<FName, T>& Map)
{
	for (auto It = Map.CreateIterator(); It; ++It)
	{
		It->Value->InitializeBuilder();
	}
}

bool FCSTypeRegistry::ProcessMetaData(const FString& FilePath)
{
	if (!FPaths::FileExists(FilePath))
	{
		UE_LOG(LogUnrealSharp, Fatal, TEXT("Couldn't find metadata file at: %s"), *FilePath);
		return false;
	}

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		UE_LOG(LogUnrealSharp, Fatal, TEXT("Failed to load MetaDataPath at: %s"), *FilePath);
		return false;
	}

	TSharedPtr<FJsonObject> JsonObject;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(JsonString), JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogUnrealSharp, Fatal, TEXT("Failed to parse JSON at: %s"), *FilePath);
		return false;
	}
	
	for (const TSharedPtr<FJsonValue>& MetaData : JsonObject->GetArrayField(TEXT("ClassMetaData")))
	{
		TSharedPtr<FCSharpClassInfo> ClassInfo = MakeShared<FCSharpClassInfo>(MetaData);
		ManagedClasses.Add(ClassInfo->TypeMetaData->Name, ClassInfo);
	}

	const TArray<TSharedPtr<FJsonValue>>& StructMetaData = JsonObject->GetArrayField(TEXT("StructMetaData"));
	for (const TSharedPtr<FJsonValue>& MetaData : StructMetaData)
	{
		TSharedPtr<FCSharpStructInfo> StructInfo = MakeShared<FCSharpStructInfo>(MetaData);
		ManagedStructs.Add(StructInfo->TypeMetaData->Name, StructInfo);
	}

	const TArray<TSharedPtr<FJsonValue>>& EnumMetaData = JsonObject->GetArrayField(TEXT("EnumMetaData"));
	for (const TSharedPtr<FJsonValue>& MetaData : EnumMetaData)
	{
		TSharedPtr<FCSharpEnumInfo> EnumInfo = MakeShared<FCSharpEnumInfo>(MetaData);
		ManagedEnums.Add(EnumInfo->TypeMetaData->Name, EnumInfo);
	}

	const TArray<TSharedPtr<FJsonValue>>& InterfacesMetaData = JsonObject->GetArrayField(TEXT("InterfacesMetaData"));
	for (const TSharedPtr<FJsonValue>& MetaData : InterfacesMetaData)
	{
		TSharedPtr<FCSharpInterfaceInfo> InterfaceInfo = MakeShared<FCSharpInterfaceInfo>(MetaData);
		ManagedInterfaces.Add(InterfaceInfo->TypeMetaData->Name, InterfaceInfo);
	}

	InitializeBuilders(ManagedClasses);
	InitializeBuilders(ManagedStructs);
	InitializeBuilders(ManagedEnums);
	InitializeBuilders(ManagedInterfaces);
	return true;
}

TSharedRef<FCSharpClassInfo> FCSTypeRegistry::FindManagedType(UClass* Class)
{
	FString Name = Class->GetName();
	Name.RemoveFromEnd(TEXT("_C"));
	
	TSharedPtr<FCSharpClassInfo> FoundClassInfo = ManagedClasses.FindRef(*Name);

	// Native classes are populated on the go as they are needed for managed code.
	if (!FoundClassInfo.IsValid())
	{
		FoundClassInfo = MakeShared<FCSharpClassInfo>();
		FoundClassInfo->TypeHandle = UCSManager::Get().GetTypeHandle(nullptr,
		 	FUnrealSharpUtils::GetNamespace(Class),
		 	Class->GetName());
		FoundClassInfo->Field = Class;
		ManagedClasses.Add(Class->GetFName(), FoundClassInfo);
	}
	
	return FoundClassInfo.ToSharedRef();
}

void FCSTypeRegistry::AddPendingClass(FName ParentClass, FCSharpClassInfo* NewClass)
{
	FPendingClasses& PendingClass = PendingClasses.FindOrAdd(ParentClass);
	PendingClass.Classes.Add(NewClass);
}

UClass* FCSTypeRegistry::GetClassFromName(FName Name)
{
	UClass* FoundType;
	TSharedPtr<FCSharpClassInfo> TypeInfo = Get().ManagedClasses.FindRef(Name);
	if (TypeInfo.IsValid())
	{
		FoundType = TypeInfo->InitializeBuilder();
	}
	else
	{
		FoundType = FindFirstObjectSafe<UClass>(*Name.ToString());
	}

	if (!IsValid(FoundType))
	{
		FoundType = GetInterfaceFromName(Name);
	}
	
	return FoundType;
}

UScriptStruct* FCSTypeRegistry::GetStructFromName(FName Name)
{
	UScriptStruct* FoundType;
	TSharedPtr<FCSharpStructInfo> TypeInfo = Get().ManagedStructs.FindRef(Name);
	if (TypeInfo.IsValid())
	{
		FoundType = TypeInfo->InitializeBuilder();
	}
	else
	{
		FoundType = FindFirstObjectSafe<UScriptStruct>(*Name.ToString());
	}
	return FoundType;
}

UEnum* FCSTypeRegistry::GetEnumFromName(FName Name)
{
	UEnum* FoundType;
	TSharedPtr<FCSharpEnumInfo> TypeInfo = Get().ManagedEnums.FindRef(Name);
	if (TypeInfo.IsValid())
	{
		FoundType = TypeInfo->InitializeBuilder();
	}
	else
	{
		FoundType = FindFirstObjectSafe<UEnum>(*Name.ToString());
	}
	
	return FoundType;
}

UClass* FCSTypeRegistry::GetInterfaceFromName(FName Name)
{
	UClass* FoundType;
	TSharedPtr<FCSharpInterfaceInfo> TypeInfo = Get().ManagedInterfaces.FindRef(Name);
	if (TypeInfo.IsValid())
	{
		FoundType = TypeInfo->InitializeBuilder();
	}
	else
	{
		FoundType = FindFirstObjectSafe<UClass>(*Name.ToString());
	}
	
	return FoundType;
}

void FCSTypeRegistry::OnModulesChanged(FName InModuleName, EModuleChangeReason InModuleChangeReason)
{
	if (InModuleChangeReason != EModuleChangeReason::ModuleLoaded)
	{
		return;
	}
	
	for (auto Itr = PendingClasses.CreateIterator(); Itr; ++Itr)
	{
		UClass* Class = GetClassFromName(Itr.Key());
		
		if (!Class)
		{
			// Class still not loaded from this module.
			continue;
		}

		for (FCSharpClassInfo* PendingClass : Itr.Value().Classes)
		{
			PendingClass->InitializeBuilder();
		}

		Itr.RemoveCurrent();
	}
}
