﻿#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

namespace FUnrealSharpUtils
{
	UNREALSHARPUTILITIES_API FString GetNamespace(const UObject* Object);
	UNREALSHARPUTILITIES_API FString GetNamespace(FName PackageName);
	
	UNREALSHARPUTILITIES_API FName GetModuleName(const UObject* Object);

	template<typename T>
	static void GetAllCDOsOfClass(TArray<T*>& OutObjects)
	{
		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* ClassObject = *It;
		
			if (!ClassObject->IsChildOf(T::StaticClass()) || ClassObject->HasAnyClassFlags(CLASS_Abstract))
			{
				continue;
			}

			T* CDO = ClassObject->GetDefaultObject<T>();
			OutObjects.Add(CDO);
		}
	}
	
};
