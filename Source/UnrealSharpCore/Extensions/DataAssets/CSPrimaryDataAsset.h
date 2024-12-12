﻿#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CSPrimaryDataAsset.generated.h"

UCLASS()
class UNREALSHARPCORE_API UCSPrimaryDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	// UPrimaryDataAsset interface implementation
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	// End of implementation

protected:

	// The name of the asset which AssetManager will use to identify this asset
	UPROPERTY(BlueprintReadWrite)
	FName AssetName;
	
};
