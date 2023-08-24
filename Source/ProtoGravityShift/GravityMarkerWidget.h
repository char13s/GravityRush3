// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GravityMarkerWidget.generated.h"

/**
 * 
 */
UCLASS()
class PROTOGRAVITYSHIFT_API UGravityMarkerWidget : public UUserWidget
{
	GENERATED_BODY()

	virtual void NativeConstruct() override;

};
