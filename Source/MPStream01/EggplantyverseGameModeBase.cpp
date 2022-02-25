// Fill out your copyright notice in the Description page of Project Settings.


#include "EggplantyverseGameModeBase.h"

AEggplantyverseGameModeBase::AEggplantyverseGameModeBase()
{
	bUseSeamlessTravel = true;
}

//EggplantyverseGameModeBase::~EggplantyverseGameModeBase()
//{
//}

void AEggplantyverseGameModeBase::TryServerTravel(const FString& URL)
{
	GetWorld()->ServerTravel(URL, true, true);
}