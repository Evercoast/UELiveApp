/*
* **********************************************
* Copyright Evercoast, Inc. All Rights Reserved.
* **********************************************
* @Author: Ye Feng
* @Date:   2021-11-16 03:13:02
* @Last Modified by:   Ye Feng
* @Last Modified time: 2021-12-03 05:04:23
*/
#pragma once

#include "Factories/Factory.h"
#include "UObject/ObjectMacros.h"

#include "EvercoastECVAssetFactoryNew.generated.h"

UCLASS()
class UEvercoastECVAssetFactoryNew
	: public UFactory
{
	GENERATED_UCLASS_BODY()

public:
	//~ UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;

	virtual uint32 GetMenuCategories() const override;
	virtual bool ShouldShowInNewMenu() const override;
};
