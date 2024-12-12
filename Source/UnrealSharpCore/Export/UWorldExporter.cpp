﻿#include "UWorldExporter.h"
#include "UnrealSharpCore/CSManager.h"
#include "Kismet/KismetSystemLibrary.h"

void UUWorldExporter::ExportFunctions(FRegisterExportedFunction RegisterExportedFunction)
{
	EXPORT_FUNCTION(SetTimer)
	EXPORT_FUNCTION(InvalidateTimer)
	EXPORT_FUNCTION(GetWorldSubsystem)
}

void UUWorldExporter::SetTimer(UObject* Object, FName FunctionName, float Rate, bool Loop, float InitialDelay, FTimerHandle* TimerHandle)
{
	FTimerDynamicDelegate Delegate;
	Delegate.BindUFunction(Object, FunctionName);
	*TimerHandle = UKismetSystemLibrary::K2_SetTimerDelegate(Delegate, Rate, Loop, false, InitialDelay);
}

void UUWorldExporter::InvalidateTimer(UObject* Object, FTimerHandle* TimerHandle)
{
	if (!IsValid(Object))
	{
		return;
	}

	Object->GetWorld()->GetTimerManager().ClearTimer(*TimerHandle);
}

void* UUWorldExporter::GetWorldSubsystem(UClass* SubsystemClass, UObject* WorldContextObject)
{
	if (!IsValid(WorldContextObject))
	{
		return nullptr;
	}
	
	UWorldSubsystem* WorldSubsystem = WorldContextObject->GetWorld()->GetSubsystemBase(SubsystemClass);
	return UCSManager::Get().FindManagedObject(WorldSubsystem).GetIntPtr();
}
