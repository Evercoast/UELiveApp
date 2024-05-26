#pragma once

#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "EvercoastECVAsset.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(EvercoastAssetLog, Log, All);

UCLASS(BlueprintType, Blueprintable)
class EVERCOASTPLAYBACK_API UEvercoastECVAsset : public UObject
{
	GENERATED_BODY()

public:
	UEvercoastECVAsset(const FObjectInitializer& initializer);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Data Source", BlueprintGetter=GetDataURL, BlueprintSetter=SetDataURL)
	FString DataURL;

	UPROPERTY(BlueprintReadonly, Category = "Data Source")
	FString CookedDataURL;

	UPROPERTY()
	bool ValidationDirty;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Platforms, Meta = (DisplayName = "Per-platform Cooking Overrides"))
	TMap<FString, bool> PlatformCookOverride;

	UFUNCTION(BlueprintCallable, Category = "Data Source", meta = (DisplayName = "GetDataURL", CallInEditor = "true"))
	FString GetDataURL() const;

	UFUNCTION(BlueprintCallable, Category = "Data Source", meta = (DisplayName = "SetDataURL", CallInEditor = "true"))
	void SetDataURL(const FString& url);

	UFUNCTION(BlueprintCallable, Category = "Data Source", meta = (DisplayName = "SetDataURLIgnoreValidation", CallInEditor = "true"))
	void SetDataURLIgnoreValidation(const FString& url);

	bool IsEmpty() const;
	bool IsHttpStreaming() const;

	virtual void PreSave(const ITargetPlatform* TargetPlatform) override;
	virtual bool PreSaveRoot(const TCHAR* Filename) override;

#if WITH_EDITOR
	virtual bool Modify(bool bAlwaysMarkDirty = true) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual EDataValidationResult IsDataValid(TArray <FText>& ValidationErrors) override;
	bool ValidateDataURL();
	//bool ValidateCookedDataURL();
	void Cook();
	void Uncook();
	void ForceInvalidateFlags();
#endif
private:
#if WITH_EDITOR
	virtual void CookAdditionalFilesOverride(const TCHAR* PackageFilename, const ITargetPlatform* TargetPlatform,
		TFunctionRef<void(const TCHAR* Filename, void* Data, int64 Size)> WriteAdditionalFile) override;
	bool _DoValidation(const FString& url);
	bool GenerateCookedDataURL();
#endif

	FString m_externalPostfix;
};