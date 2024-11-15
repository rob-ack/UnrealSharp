﻿#include "CSReinstancer.h"
#include "BlueprintActionDatabase.h"
#include "K2Node_CallFunction.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UnrealSharpCore/TypeGenerator/Register/CSTypeRegistry.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/ReloadUtilities.h"
#include "UnrealSharpBlueprint/K2Node_CSAsyncAction.h"

FCSReinstancer& FCSReinstancer::Get()
{
	static FCSReinstancer Instance;
	return Instance;
}

void FCSReinstancer::Initialize()
{
	FCSTypeRegistry::Get().GetOnNewClassEvent().AddRaw(this, &FCSReinstancer::AddPendingClass);
	FCSTypeRegistry::Get().GetOnNewStructEvent().AddRaw(this, &FCSReinstancer::AddPendingStruct);
}

void FCSReinstancer::AddPendingClass(UClass* OldClass, UClass* NewClass)
{
	ClassesToReinstance.Add(MakeTuple(OldClass, NewClass));
}

void FCSReinstancer::AddPendingStruct(UScriptStruct* OldStruct, UScriptStruct* NewStruct)
{
	StructsToReinstance.Add(MakeTuple(OldStruct, NewStruct));
}

void FCSReinstancer::AddPendingInterface(UClass* OldInterface, UClass* NewInterface)
{
	InterfacesToReinstance.Add(MakeTuple(OldInterface, NewInterface));
}

bool FCSReinstancer::TryUpdatePin(FEdGraphPinType& PinType) const
{
	UObject* PinSubCategoryObject = PinType.PinSubCategoryObject.Get();
	
	if (PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
	{
		UScriptStruct* Struct = Cast<UScriptStruct>(PinSubCategoryObject);
		if (UScriptStruct* const * FoundStruct = StructsToReinstance.Find(Struct))
		{
			PinType.PinSubCategoryObject = *FoundStruct;
			return true;
		}
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Enum || PinType.PinCategory == UEdGraphSchema_K2::PC_Byte)
	{
		UEnum* Enum = Cast<UEnum>(PinSubCategoryObject);
		
		if (!Enum)
		{
			return false;
		}

		// Enums are not reinstanced, so we need to check if the enum is still valid
		if (FCSTypeRegistry::Get().GetEnumFromName(Enum->GetFName()))
		{
			PinType.PinSubCategoryObject = Enum;
			return true;
		}
	}
	else if (PinType.PinSubCategory == UEdGraphSchema_K2::PC_Class
		|| PinType.PinSubCategory == UEdGraphSchema_K2::PC_Object
		|| PinType.PinSubCategory == UEdGraphSchema_K2::PC_SoftObject
		|| PinType.PinSubCategory == UEdGraphSchema_K2::PC_SoftClass)
	{
		UClass* Interface = Cast<UClass>(PinSubCategoryObject);
		if (UClass* const * FoundInterface = InterfacesToReinstance.Find(Interface))
		{
			PinType.PinSubCategoryObject = *FoundInterface;
			return true;
		}
	}
	return false;
}

void FCSReinstancer::StartReinstancing()
{
	TUniquePtr<FReload> Reload = MakeUnique<FReload>(EActiveReloadType::Reinstancing, TEXT(""), *GWarn);
	Reload->SetSendReloadCompleteNotification(false);

	auto NotifyChanges = [&Reload](const auto& Container)
	{
		for (const auto& [Old, New] : Container)
		{
			if (!Old || !New)
			{
				continue;
			}

			Reload->NotifyChange(New, Old);
		}
	};

	NotifyChanges(InterfacesToReinstance);
	NotifyChanges(StructsToReinstance);
	NotifyChanges(ClassesToReinstance);

	// Before we reinstance, we want the BP to know about the new types
	UpdateBlueprints();
	
	Reload->Reinstance();
	PostReinstance();

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	
	auto CleanOldTypes = [](const auto& Container, IAssetRegistry& AssetRegistry)
	{
		for (const auto& [Old, New] : Container)
		{
			if (!Old || !New)
			{
				continue;
			}

			AssetRegistry.AssetDeleted(Old);
			Old->SetFlags(RF_NewerVersionExists);
			Old->RemoveFromRoot();
		}
	};

	CleanOldTypes(InterfacesToReinstance, AssetRegistry);
	CleanOldTypes(StructsToReinstance, AssetRegistry);
	CleanOldTypes(ClassesToReinstance, AssetRegistry);

	InterfacesToReinstance.Empty();
	StructsToReinstance.Empty();
	ClassesToReinstance.Empty();
	
	FCoreUObjectDelegates::ReloadCompleteDelegate.Broadcast(EReloadCompleteReason::None);
	Reload->Finalize(true);
}

void FCSReinstancer::PostReinstance()
{
	FBlueprintActionDatabase& ActionDB = FBlueprintActionDatabase::Get();
	for (auto& Element : ClassesToReinstance)
	{
		ActionDB.ClearAssetActions(Element.Key);
		ActionDB.RefreshClassActions(Element.Value);
	}

	for (auto& Struct : StructsToReinstance)
	{
		TArray<UDataTable*> Tables;
		GetTablesDependentOnStruct(Struct.Key, Tables);
		
		for (UDataTable*& Table : Tables)
		{
			auto Data = Table->GetTableAsJSON();
			Struct.Key->StructFlags = static_cast<EStructFlags>(STRUCT_NoDestructor | Struct.Key->StructFlags);
			Table->CleanBeforeStructChange();
			Table->RowStruct = Struct.Value;
			Table->CreateTableFromJSONString(Data);
		}
	}
	
	for (const auto& StructToReinstancePair : StructsToReinstance)
	{
		StructToReinstancePair.Key->ChildProperties = nullptr;
		StructToReinstancePair.Key->ConditionalBeginDestroy();
	}
	
	if (FPropertyEditorModule* PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
	{
		PropertyModule->NotifyCustomizationModuleChanged();
	}

	if (!GEngine)
	{
		return;
	}
	
	GEditor->BroadcastBlueprintCompiled();	
}

UFunction* FCSReinstancer::FindMatchingMember(const FMemberReference& FunctionReference) const
{
	const UClass* CurrentClassType = FunctionReference.GetMemberParentClass();
	if (!CurrentClassType)
	{
		return nullptr;
	}

	if (UClass* const * FoundNewClassType = ClassesToReinstance.Find(CurrentClassType))
	{
		if (UFunction* Function = (*FoundNewClassType)->FindFunctionByName(FunctionReference.GetMemberName()))
		{
			return Function;
		}
	}
	return nullptr;
}

bool FCSReinstancer::UpdateMemberCall(UK2Node_CallFunction* Node) const
{
	if (UFunction* NewMember = FindMatchingMember(Node->FunctionReference))
	{
		Node->SetFromFunction(NewMember);
		return true;
	}
	return false;
}

bool FCSReinstancer::UpdateMemberCall(UK2Node_CSAsyncAction * Node) const
{
	const TObjectPtr<UClass> CurrentProxyClass = Node->GetProxyClass();
	if (!CurrentProxyClass)
	{
		return false;
	}
	
	if (UClass* const * FoundNewClassType = ClassesToReinstance.Find(CurrentProxyClass))
	{
		if (UFunction* Function = (*FoundNewClassType)->FindFunctionByName(Node->GetFactoryFunctionName()))
		{
			UK2Node_CSAsyncAction::SetNodeFunc(Node, false, Function);
			return true;
		}
	}
	return false;
}

void FCSReinstancer::UpdateBlueprints()
{
	TArray<UK2Node*> NodesToUpdate;
	
	for (TObjectIterator<UBlueprint> BlueprintIt; BlueprintIt; ++BlueprintIt)
	{
		UBlueprint* Blueprint = *BlueprintIt;
		for (FBPVariableDescription& NewVariable : Blueprint->NewVariables)
		{
			TryUpdatePin(NewVariable.VarType);
		}

		TArray<UK2Node*> AllNodes;
		FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, AllNodes);
		
		for (UK2Node* Node : AllNodes)
		{
			bool bNeedsReconstruction = false;
			if (UK2Node_CallFunction* CallFunction = Cast<UK2Node_CallFunction>(Node))
			{
				if(UpdateMemberCall(CallFunction))
				{
					bNeedsReconstruction = true;
				}
			}
			else if (UK2Node_CSAsyncAction* CustomEvent = Cast<UK2Node_CSAsyncAction>(Node))
			{
				if(UpdateMemberCall(CustomEvent))
				{
					bNeedsReconstruction = true;
				}
			}
			
			if (UK2Node_EditablePinBase* EditableNode = Cast<UK2Node_EditablePinBase>(Node))
			{
				for (const TSharedPtr<FUserPinInfo>& Pin : EditableNode->UserDefinedPins)
				{
					if (TryUpdatePin(Pin->PinType))
					{
						bNeedsReconstruction = true;
					}
				}
			}
			else
			{
				for (UEdGraphPin* Pin : Node->Pins)
				{
					if (TryUpdatePin(Pin->PinType))
					{
						bNeedsReconstruction = true;
					}
				}
			}

			if (bNeedsReconstruction)
			{
				NodesToUpdate.Push(Node);
			}
		}
	}

	for (UK2Node* Node : NodesToUpdate)
	{
		Node->ReconstructNode();
	}
}

void FCSReinstancer::GetTablesDependentOnStruct(UScriptStruct* Struct, TArray<UDataTable*>& DataTables)
{
	TArray<UDataTable*> Result;
	if (Struct)
	{
		TArray<UObject*> FoundDataTables;
		GetObjectsOfClass(UDataTable::StaticClass(), FoundDataTables);
		for (UObject* DataTableObj : DataTables)
		{
			UDataTable* DataTable = Cast<UDataTable>(DataTableObj);
			if (DataTable && Struct == DataTable->RowStruct)
			{
				Result.Add(DataTable);
			}
		}
	}
}
