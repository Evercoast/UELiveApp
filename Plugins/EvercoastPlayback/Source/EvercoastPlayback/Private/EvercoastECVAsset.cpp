/*
* **********************************************
* Copyright Evercoast, Inc. All Rights Reserved.
* **********************************************
* @Author: Ye Feng
* @Date:   2021-11-16 01:37:43
* @Last Modified by:   Ye Feng
* @Last Modified time: 2021-12-07 04:52:42
*/
#include "EvercoastECVAsset.h"
#include "EvercoastConstants.h"
#include "Misc/SecureHash.h"
#include <fstream>
#include <memory>
#include <string>
#include "GhostTreeFormatReader.h"
#include "Interfaces/ITargetPlatform.h"

DEFINE_LOG_CATEGORY(EvercoastAssetLog);

#define DO_MD5 0

#if DO_MD5
constexpr int MD5_DIGEST_LENGTH = 16;
constexpr int READ_BUFFER_SIZE = 1024 * 1024 * 256;

static void FormatMD5(const uint8_t* digest, char* outDigestString )
{
	for(int i = 0; i < MD5_DIGEST_LENGTH; ++i)
	{
		std::snprintf(outDigestString + i * 2, 2+1, "%02x", digest[i]);
	}
	outDigestString[MD5_DIGEST_LENGTH*2] = 0; // null terminate
}
#endif


UEvercoastECVAsset::UEvercoastECVAsset(const FObjectInitializer& initializer) :
	ValidationDirty(true)
{

}

bool UEvercoastECVAsset::IsEmpty() const
{
	return DataURL.IsEmpty();
}

bool UEvercoastECVAsset::IsHttpStreaming() const
{
	if (!IsEmpty())
	{
		return DataURL.StartsWith("http://") || DataURL.StartsWith("https://");
	}

	return false;
}

FString UEvercoastECVAsset::GetDataURL() const
{
#if WITH_EDITOR
	return DataURL;
#else
	if (IsHttpStreaming())
		return DataURL;
	else
		return CookedDataURL;
#endif
}

void UEvercoastECVAsset::SetDataURL(const FString& url)
{
	if (url != DataURL)
	{
		DataURL = url.TrimStartAndEnd();

		if (!FPaths::IsRelative(DataURL))
		{
			UE_LOG(EvercoastAssetLog, Warning, TEXT("DataURL %s is absolute. The asset is not portable."), *DataURL);
		}
		ValidationDirty = true;
#if WITH_EDITOR
		GenerateCookedDataURL();
#endif
	}
}

void UEvercoastECVAsset::SetDataURLIgnoreValidation(const FString& url)
{
	if (url != DataURL)
	{
		DataURL = url.TrimStartAndEnd();

		if (!FPaths::IsRelative(DataURL))
		{
			UE_LOG(EvercoastAssetLog, Warning, TEXT("DataURL %s is absolute. The asset is not portable."), *DataURL);
		}

		// By pass validation
		ValidationDirty = false;
		CookedDataURL = FString(DataURL);
		Modify(true);
	}
}

void UEvercoastECVAsset::PreSave(const ITargetPlatform* TargetPlatform)
{
#if WITH_EDITOR
	GenerateCookedDataURL();
#endif
}

bool UEvercoastECVAsset::PreSaveRoot(const TCHAR* Filename)
{
	return false;
}

#if WITH_EDITOR
void UEvercoastECVAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.GetPropertyName() == FName(TEXT("DataURL")))
	{
		ValidationDirty = true;
		GenerateCookedDataURL();
	}
}

bool UEvercoastECVAsset::ValidateDataURL()
{
	if (!ValidationDirty)
	{
		return true;
	}
	if (_DoValidation(DataURL))
	{
		Modify(true);
		// force clear the dirty flag
		ValidationDirty = false;
		return true;
	}

	return false;
}

bool UEvercoastECVAsset::Modify(bool bAlwaysMarkDirty)
{
	return Super::Modify(bAlwaysMarkDirty);
}

EDataValidationResult UEvercoastECVAsset::IsDataValid(TArray< FText > & ValidationErrors)
{
	if (ValidateDataURL())
	{
		return EDataValidationResult::Valid;
	}

	ValidationErrors.Add(FText::Format(NSLOCTEXT("EvercoastText", "CannotLoadURL", "Cannot load Evercoast content from URL: {0}"), FText::FromString(DataURL)));

	return EDataValidationResult::Invalid;
}

void UEvercoastECVAsset::CookAdditionalFilesOverride(const TCHAR* PackageFilename, const ITargetPlatform* TargetPlatform,
		TFunctionRef<void(const TCHAR* Filename, void* Data, int64 Size)> WriteAdditionalFile)
{
	// We cannot use WriteAdditionalFile provided as the file can be too big to fit into memory
	bool* pRequiresCook = PlatformCookOverride.Find(TargetPlatform->IniPlatformName());
	// Cook when there's no override, or there's override and its value is true
	if (!pRequiresCook || (pRequiresCook && *pRequiresCook))
	{
		Cook();
	}
	else
	{
		Uncook();
	}
}

static FString NO_EXTERNAL_VIDEO_NEEDED("__NO_MP4__");

bool UEvercoastECVAsset::_DoValidation(const FString& url)
{
	// create a reader and see the callback results
	auto reader = UGhostTreeFormatReader::Create(true, nullptr, 2048);
	bool ret = reader->ValidateLocation(url, 60.0);

	if (reader->MeshRequiresExternalData())
	{
		m_externalPostfix = reader->GetExternalPostfix();
	}
	else
	{
		m_externalPostfix = NO_EXTERNAL_VIDEO_NEEDED;
	}
	return ret;
}


bool UEvercoastECVAsset::GenerateCookedDataURL()
{
	if (DataURL.IsEmpty())
	{
		UE_LOG(EvercoastAssetLog, Error, TEXT("DataURL is empty."));
		return false;
	}

	DataURL = DataURL.TrimStartAndEnd();
	if (!ValidateDataURL())
	{
		UE_LOG(EvercoastAssetLog, Error, TEXT("Validation for DataURL: %s failed."), *DataURL);
		return false;
	}
	
	if (IsHttpStreaming())
	{
		// no cooking needed for http streamed content
		if (CookedDataURL != DataURL)
		{
			CookedDataURL = FString(DataURL);
			Modify(true);
		}

		UE_LOG(EvercoastAssetLog, Log, TEXT("Generated cooked Data URL: %s"), *CookedDataURL);
		
		return true;
	}
	else
	{

		
		FString fullPathURL;
		bool isDataURLRelativePath = FPaths::IsRelative(DataURL);
		if (isDataURLRelativePath)
		{
			fullPathURL = FPaths::Combine(FPaths::ProjectDir(), DataURL);
			fullPathURL = FPaths::ConvertRelativePathToFull(fullPathURL);
		}
		else
		{
			fullPathURL = DataURL;
		}
#if DO_MD5
		std::ifstream ifs(*fullPathURL, std::ifstream::binary);
		if (ifs)
		{
			// md5 checksum calculation, so we can generate unique filenames for each content
			// if within two assets, the filename and the content remain the same, the cooked url of the two assets will be exactly the same
			FString extName = FPaths::GetExtension(DataURL, true); // probably just .ecv
			FMD5 md5;

			// Hash the file content, mind the content could be huge
			// so allocate 256MB at a time
			// Implementation copied from FMD5 and FMD5Hash
			FArchive* Ar = IFileManager::Get().CreateFileReader(*fullPathURL);
			if (Ar)
			{
				// TODO: this is time consuming potentially, result should be cached
				TArray<uint8> Buffer;
				Buffer.SetNumUninitialized(READ_BUFFER_SIZE);
				const int64 Size = Ar->TotalSize();
				int64 Position = 0;

				// Read in BufferSize chunks
				while (Position < Size)
				{
					const auto ReadNum = FMath::Min(Size - Position, (int64)Buffer.Num());
					Ar->Serialize(Buffer.GetData(), ReadNum);
					md5.Update(Buffer.GetData(), ReadNum);

					Position += ReadNum;
				}

				delete Ar;

				FMD5Hash hash;
				hash.Set(md5);

				const uint8_t* digest = hash.GetBytes(); // 16-bytes digest

				char digestString[MD5_DIGEST_LENGTH*2+1]; // null-terminated
				FormatMD5(digest, digestString);

				CookedDataURL = FPaths::Combine(EVERCOAST_VOLCAP_SOURCE_DATA_DIRECTORY, FString(ANSI_TO_TCHAR(digestString)) + extName);
				Modify(true);
				UE_LOG(EvercoastAssetLog, Log, TEXT("Generated cooked Data URL: %s"), *CookedDataURL);
				
				return true;
			}
		}
#else
		IPlatformFile& FileManager = FPlatformFileManager::Get().GetPlatformFile();
		if (FileManager.FileExists(*fullPathURL))
		{
			// Simply cut the basefilename and append it to the volcap source directory for staging
			FString baseFilename = FPaths::GetCleanFilename(fullPathURL); // with ext
			CookedDataURL = FPaths::Combine(EVERCOAST_VOLCAP_SOURCE_DATA_DIRECTORY, baseFilename);
			Modify(true);
			UE_LOG(EvercoastAssetLog, Log, TEXT("Generated cooked Data URL: %s"), *CookedDataURL);
			return true;
		}
#endif

		UE_LOG(EvercoastAssetLog, Error, TEXT("Reading from DataURL: %s failed."), *DataURL);
		CookedDataURL.Reset();
		Modify(true);
		return false;
	}
}

// Hardcode three video extensions for now
static FString videoExtensions[] = {
	FString(TEXT(".mp4")),
	FString(TEXT(".15fps.mp4")),
	FString(TEXT(".30fps.mp4"))
};


void UEvercoastECVAsset::Cook()
{
	if (m_externalPostfix.IsEmpty())
	{
		ForceInvalidateFlags();
	}

	if (GenerateCookedDataURL())
	{
		if (IsHttpStreaming())
		{
			UE_LOG(EvercoastAssetLog, Log, TEXT("ECV Asset(HTTP streaming) cooked: %s"), *CookedDataURL);
			return;
		}
		// copy data from DataURL to CookedDataURL
		FString fullPathURL;
		if (FPaths::IsRelative(DataURL))
		{
			fullPathURL = FPaths::Combine(FPaths::ProjectDir(), DataURL);
			fullPathURL = FPaths::ConvertRelativePathToFull(fullPathURL);
		}
		else
		{
			fullPathURL = DataURL;
		}

#if DO_MD5
		FArchive* ArReader = IFileManager::Get().CreateFileReader(*fullPathURL);

		// CookedDataURL should always be relative
		check(FPaths::IsRelative(CookedDataURL));
		FString WriteFullPath = FPaths::Combine(FPaths::ProjectDir(), CookedDataURL);
		WriteFullPath = FPaths::ConvertRelativePathToFull(WriteFullPath);
		FArchive* ArWriter = IFileManager::Get().CreateFileWriter(*WriteFullPath);
		if (ArReader && ArWriter)
		{
			TArray<uint8> Buffer;
			Buffer.SetNumUninitialized(READ_BUFFER_SIZE);
			const int64 Size = ArReader->TotalSize();
			int64 Position = 0;

			// Read in BufferSize chunks
			while (Position < Size)
			{
				const auto ReadNum = FMath::Min(Size - Position, (int64)Buffer.Num());
				ArReader->Serialize(Buffer.GetData(), ReadNum);
				ArWriter->Serialize(Buffer.GetData(), ReadNum);

				Position += ReadNum;
			}

			ArWriter->Flush();
			delete ArWriter;
			delete ArReader;

			UE_LOG(EvercoastAssetLog, Log, TEXT("ECV Asset(local streaming) cooked: %s -> %s"), *fullPathURL, *WriteFullPath);
		}
		else
		{
			UE_LOG(EvercoastAssetLog, Error, TEXT("ECV Asset(local streaming) failed cooking: %s -> %s"), *fullPathURL, *WriteFullPath);
		}

		FString extName = FPaths::GetExtension(DataURL, true);
		if (extName.ToLower() == TEXT(".ecm"))
		{
			if (m_externalPostfix != NO_EXTERNAL_VIDEO_NEEDED)
			{
				// need to copy mp4 as well
				FString videoUrl(DataURL);
				if (videoUrl.RemoveFromEnd(TEXT(".ecm"), ESearchCase::IgnoreCase))
				{
					videoUrl.Append(m_externalPostfix);
				}
				else
				{
					UE_LOG(EvercoastReaderLog, Warning, TEXT("Abnormal ECVAsset data URL: %s"), *DataURL);
				}

				if (FPaths::IsRelative(videoUrl))
				{
					fullPathURL = FPaths::Combine(FPaths::ProjectDir(), videoUrl);
					fullPathURL = FPaths::ConvertRelativePathToFull(fullPathURL);
				}
				else
				{
					fullPathURL = videoUrl;
				}

				ArReader = IFileManager::Get().CreateFileReader(*fullPathURL);

				FString cookedVideoUrl(CookedDataURL);
				if (cookedVideoUrl.RemoveFromEnd(TEXT(".ecm"), ESearchCase::IgnoreCase))
				{
					cookedVideoUrl.Append(m_externalPostfix);
				}
				else
				{
					UE_LOG(EvercoastReaderLog, Warning, TEXT("Abnormal ECVAsset cooked data URL: %s"), *CookedDataURL);
				}

				// cookedVideoUrl should always be relative
				check(FPaths::IsRelative(cookedVideoUrl));
				WriteFullPath = FPaths::Combine(FPaths::ProjectDir(), cookedVideoUrl);
				WriteFullPath = FPaths::ConvertRelativePathToFull(WriteFullPath);
				ArWriter = IFileManager::Get().CreateFileWriter(*WriteFullPath);
				if (ArReader && ArWriter)
				{
					TArray<uint8> Buffer;
					Buffer.SetNumUninitialized(READ_BUFFER_SIZE);
					const int64 Size = ArReader->TotalSize();
					int64 Position = 0;

					// Read in BufferSize chunks
					while (Position < Size)
					{
						const auto ReadNum = FMath::Min(Size - Position, (int64)Buffer.Num());
						ArReader->Serialize(Buffer.GetData(), ReadNum);
						ArWriter->Serialize(Buffer.GetData(), ReadNum);

						Position += ReadNum;
					}

					ArWriter->Flush();
					delete ArWriter;
					delete ArReader;

					UE_LOG(EvercoastAssetLog, Log, TEXT("ECM accompanying video(local streaming) cooked: %s -> %s"), *fullPathURL, *WriteFullPath);
				}
				else
				{
					UE_LOG(EvercoastAssetLog, Error, TEXT("ECM accompanying video(local streaming) failed cooking: %s -> %s"), *fullPathURL, *WriteFullPath);
				}
			}
			else
			{
				UE_LOG(EvercoastAssetLog, Log, TEXT("No video needs cooking for: %s"), *fullPathURL);

			}

		}
#else
		// CookedDataURL should always be relative
		check(FPaths::IsRelative(CookedDataURL));
		FString WriteFullPath = FPaths::Combine(FPaths::ProjectDir(), CookedDataURL);
		WriteFullPath = FPaths::ConvertRelativePathToFull(WriteFullPath);

		// Copy the ecm/ecv file
		IPlatformFile& FileManager = FPlatformFileManager::Get().GetPlatformFile();
		if (FileManager.CopyFile(*WriteFullPath, *fullPathURL, EPlatformFileRead::None, EPlatformFileWrite::None))
		{
			UE_LOG(EvercoastAssetLog, Log, TEXT("ECV Asset(local streaming) cooked: %s -> %s"), *fullPathURL, *WriteFullPath);
		}
		else
		{
			UE_LOG(EvercoastAssetLog, Error, TEXT("ECV Asset(local streaming) failed cooking: %s -> %s"), *fullPathURL, *WriteFullPath);
		}

		// If it's ecm, then copy its accompanying mp4 files if any
		FString extName = FPaths::GetExtension(DataURL, true);
		if (extName.ToLower() == TEXT(".ecm"))
		{
			// We have to copy mp4 no matter whether video source it is selected or not
			// Now the reader can decide to use it or not
			for (int i = 0; i < 3; ++i)
			{
				// need to copy mp4 as well
				FString videoUrl(DataURL);
				if (videoUrl.RemoveFromEnd(TEXT(".ecm"), ESearchCase::IgnoreCase))
				{
					videoUrl.Append(videoExtensions[i]);
				}
				else
				{
					UE_LOG(EvercoastReaderLog, Warning, TEXT("Abnormal ECVAsset data URL: %s"), *DataURL);
				}

				if (FPaths::IsRelative(videoUrl))
				{
					fullPathURL = FPaths::Combine(FPaths::ProjectDir(), videoUrl);
					fullPathURL = FPaths::ConvertRelativePathToFull(fullPathURL);
				}
				else
				{
					fullPathURL = videoUrl;
				}

				FString cookedVideoUrl(CookedDataURL);
				if (cookedVideoUrl.RemoveFromEnd(TEXT(".ecm"), ESearchCase::IgnoreCase))
				{
					cookedVideoUrl.Append(videoExtensions[i]);
				}
				else
				{
					UE_LOG(EvercoastReaderLog, Warning, TEXT("Abnormal ECVAsset cooked data URL: %s"), *CookedDataURL);
				}

				// cookedVideoUrl should always be relative
				check(FPaths::IsRelative(cookedVideoUrl));
				WriteFullPath = FPaths::Combine(FPaths::ProjectDir(), cookedVideoUrl);
				WriteFullPath = FPaths::ConvertRelativePathToFull(WriteFullPath);
				
				if (FileManager.FileExists(*fullPathURL))
				{
					if (FileManager.CopyFile(*WriteFullPath, *fullPathURL, EPlatformFileRead::None, EPlatformFileWrite::None))
					{
						UE_LOG(EvercoastAssetLog, Log, TEXT("ECM accompanying video(local streaming) cooked: %s -> %s"), *fullPathURL, *WriteFullPath);
					}
					else
					{
						UE_LOG(EvercoastAssetLog, Error, TEXT("ECM accompanying video(local streaming) failed cooking: %s -> %s"), *fullPathURL, *WriteFullPath);
					}
				}
				else
				{
					UE_LOG(EvercoastAssetLog, Log, TEXT("ECM accompanying video(local streaming) %s not found, skipping"), *fullPathURL);
				}
			}
		}

#endif
	}
	else
	{
		UE_LOG(EvercoastAssetLog, Error, TEXT("Generating cooked data URL failed. Original url: %s"), *DataURL);
	}
}

// Force remove the cooked file from the cache directory
void UEvercoastECVAsset::Uncook()
{
	if (GenerateCookedDataURL())
	{
		if (IsHttpStreaming())
		{
			return;
		}
		// copy data from DataURL to CookedDataURL
		FString fullPathURL;
		if (FPaths::IsRelative(DataURL))
		{
			fullPathURL = FPaths::Combine(FPaths::ProjectDir(), DataURL);
			fullPathURL = FPaths::ConvertRelativePathToFull(fullPathURL);
		}
		else
		{
			fullPathURL = DataURL;
		}

		// CookedDataURL should always be relative
		check(FPaths::IsRelative(CookedDataURL));
		FString WriteFullPath = FPaths::Combine(FPaths::ProjectDir(), CookedDataURL);
		WriteFullPath = FPaths::ConvertRelativePathToFull(WriteFullPath);

		// Copy the ecm/ecv file
		IPlatformFile& FileManager = FPlatformFileManager::Get().GetPlatformFile();
		if (FileManager.DeleteFile(*WriteFullPath))
		{
			UE_LOG(EvercoastAssetLog, Log, TEXT("Removed ECV Asset(local streaming) because it was excluded from current platform: %s"), *WriteFullPath);
		}

		// If it's ecm, then copy its accompanying mp4 files if any
		FString extName = FPaths::GetExtension(DataURL, true);
		if (extName.ToLower() == TEXT(".ecm"))
		{
			// We have to delete mp4 no matter whether video source it is selected or not
			// Now the reader can decide to use it or not
			for (int i = 0; i < 3; ++i)
			{
				// need to copy mp4 as well
				FString videoUrl(DataURL);
				if (videoUrl.RemoveFromEnd(TEXT(".ecm"), ESearchCase::IgnoreCase))
				{
					videoUrl.Append(videoExtensions[i]);
				}
				else
				{
					UE_LOG(EvercoastReaderLog, Warning, TEXT("Abnormal ECVAsset data URL: %s"), *DataURL);
				}

				if (FPaths::IsRelative(videoUrl))
				{
					fullPathURL = FPaths::Combine(FPaths::ProjectDir(), videoUrl);
					fullPathURL = FPaths::ConvertRelativePathToFull(fullPathURL);
				}
				else
				{
					fullPathURL = videoUrl;
				}

				FString cookedVideoUrl(CookedDataURL);
				if (cookedVideoUrl.RemoveFromEnd(TEXT(".ecm"), ESearchCase::IgnoreCase))
				{
					cookedVideoUrl.Append(videoExtensions[i]);
				}
				else
				{
					UE_LOG(EvercoastReaderLog, Warning, TEXT("Abnormal ECVAsset cooked data URL: %s"), *CookedDataURL);
				}

				// cookedVideoUrl should always be relative
				check(FPaths::IsRelative(cookedVideoUrl));
				WriteFullPath = FPaths::Combine(FPaths::ProjectDir(), cookedVideoUrl);
				WriteFullPath = FPaths::ConvertRelativePathToFull(WriteFullPath);


				if (FileManager.DeleteFile(*WriteFullPath))
				{
					UE_LOG(EvercoastAssetLog, Log, TEXT("Removed ECM accompanying video(local streaming) because it was excluded from current platform: %s"), *WriteFullPath);
				}
			}
		}
	}
}


void UEvercoastECVAsset::ForceInvalidateFlags()
{
	ValidationDirty = true;
	Modify(true);
	UE_LOG(EvercoastAssetLog, Log, TEXT("Force invalidate content: %s -> %s"), *DataURL, *CookedDataURL);
}
#endif