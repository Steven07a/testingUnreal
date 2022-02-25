// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "EggplantyverseGameModeBase.generated.h"

/**
 *
 */
UCLASS()
class MPSTREAM01_API AEggplantyverseGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

		AEggplantyverseGameModeBase();

public:

	UFUNCTION(BlueprintCallable, Category = "DLC")
		void TryServerTravel(const FString& URL);
};
