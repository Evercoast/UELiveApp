/*
* **********************************************
* Copyright Evercoast, Inc. All Rights Reserved.
* **********************************************
* @Author: Ye Feng
* @Date:   2021-11-16 01:49:36
* @Last Modified by:   Ye Feng
* @Last Modified time: 2021-12-03 05:04:11
*/
#include "EvercoastECVAssetFactory.h"
#include "EvercoastECVAsset.h"

UEvercoastECVAssetFactory::UEvercoastECVAssetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Formats.Add(FString(TEXT("ecv;")) + NSLOCTEXT("EvercoastText", "FormatECV", "Evercoast ECV File").ToString());
	Formats.Add(FString(TEXT("ecm;")) + NSLOCTEXT("EvercoastText", "FormatECM", "Evercoast ECM File").ToString());
	SupportedClass = UEvercoastECVAsset::StaticClass();
	bCreateNew = false;
	bEditorImport = true;
}

UObject* UEvercoastECVAssetFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	UEvercoastECVAsset* ecvAsset;

	ecvAsset = NewObject<UEvercoastECVAsset>(InParent, InClass, InName, Flags);

	FString projDir = FPaths::ProjectDir();
	if (FPaths::IsRelative(Filename))
	{
		// The imported file should have the same base dir as the engine, so it comes as a relative path

		FString dummyAbsoluteFilename = FPaths::ConvertRelativePathToFull(Filename);
		FString dummyAbsoluteProjDir = FPaths::ConvertRelativePathToFull(projDir);

		// FPaths::MakePathRelativeTo returns proper true/false when both params are abs paths
		if (FPaths::MakePathRelativeTo(dummyAbsoluteFilename, *dummyAbsoluteProjDir))
		{
			FString relativeFilename(Filename);
			// The import file have the same base dir as the project too
			// convert it to relative path
			if (FPaths::MakePathRelativeTo(relativeFilename, *projDir))
			{
				ecvAsset->SetDataURL(relativeFilename);
			}
			else
			{
				// This should not happen, but convert the path to abs path then set anyway
				ecvAsset->SetDataURL(FPaths::ConvertRelativePathToFull(Filename));
			}
		}
		else
		{
			// Relative path but cannot converted relative to project,
			// So this happens when the imported file having the same base dir as the engine(e.g. imported file on D drive, engine on D drive)
			// but NOT having the same base dir as the project(e.g. project on C drive)
			// In this case, we have to convert it to absolute path and pass it down to ECVAsset
			FString absoluteFilename = FPaths::ConvertRelativePathToFull(Filename);
			ecvAsset->SetDataURL(absoluteFilename);
		}
	}
	else
	{
		// Provided is an absolute path, 
		// Imported file has different base dir than the engine

		// Check if the imported file has the same base dir as the project
		FString dummyAbsoluteFilename = Filename;
		FString dummyAbsoluteProjDir = FPaths::ConvertRelativePathToFull(projDir);
		// FPaths::MakePathRelativeTo returns proper true/false when both params are abs paths
		if (!Filename.StartsWith("//"))
		{
			if (FPaths::MakePathRelativeTo(dummyAbsoluteFilename, *dummyAbsoluteProjDir))
			{
				FString relativeFilename(Filename);
				// The import file have the same base dir as the project
				// convert it to relative path
				if (FPaths::MakePathRelativeTo(relativeFilename, *projDir))
				{
					ecvAsset->SetDataURL(relativeFilename);
				}
				else
				{
					// This should not happen, but convert the path to abs path then set anyway
					ecvAsset->SetDataURL(Filename);
				}
			}
			else
			{
				// Set the absolute path as there's no way to convert
				// i.e. imported file <-> project dir = different base dir
				// And  imported file <-> engine = different base dir
				ecvAsset->SetDataURL(Filename);
			}
		}
		else
		{
			// Network path
			ecvAsset->SetDataURL(Filename);
		}
	}
	

	return ecvAsset;
}

