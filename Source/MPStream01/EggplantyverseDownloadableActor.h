// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/TextRenderComponent.h"
#include "EggplantyverseDownloadableActor.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEggplantyverseRefreshDelegate);

UCLASS()
class MPSTREAM01_API AEggplantyverseDownloadableActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AEggplantyverseDownloadableActor();



	////////////////////////////////////////////////////////////////////
	//PB nametag can be possibly loaded from here
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		FString EggplantyverseUsername;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		FString EggplantyverseID;

	UPROPERTY(BlueprintAssignable)
	//UFUNCTION(BlueprintCallable)
		FEggplantyverseRefreshDelegate OnEggplantyverseRefresh;

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Debug)
	//	FText Example;

	//UPROPERTY(VisibleDefaultsOnly, Category = Components)
	//	UTextRenderComponent* EggplantyverseOwnerText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		UTextRenderComponent* EggplantyverseUsernameText; //this can probably be deleted, not changing before final presentation

	////////////////////////////////////////////////////////////////////



protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
