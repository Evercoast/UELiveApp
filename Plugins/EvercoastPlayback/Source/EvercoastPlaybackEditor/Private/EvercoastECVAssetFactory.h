/*
* **********************************************
* Copyright Evercoast, Inc. All Rights Reserved.
* **********************************************
* @Author: Ye Feng
* @Date:   2021-11-16 01:45:30
* @Last Modified by:   Ye Feng
* @Last Modified time: 2021-12-03 05:04:19
*/
#pragma once

#include "Factories/Factory.h"
#include "UObject/ObjectMacros.h"

#include "EvercoastECVAssetFactory.generated.h"

UCLASS()
class UEvercoastECVAssetFactory
	: public UFactory
{
	GENERATED_UCLASS_BODY()

public:

	//~ UFactory Interface
	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;
};
