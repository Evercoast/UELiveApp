#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include <memory>
#include "Components/MeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "CortoMeshRendererComp.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(EvercoastRendererLog, Log, All);

struct CortoLocalMeshFrame;
class CortoLocalTextureFrame;
class CortoDataUploader;
class IEvercoastStreamingDataUploader;
class USceneCaptureComponent2D;
class FCortoMeshSceneProxy;


UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class EVERCOASTPLAYBACK_API UCortoMeshRendererComp : public UMeshComponent
{
	GENERATED_BODY()

public:

	UCortoMeshRendererComp(const FObjectInitializer& ObjectInitializer);
	virtual ~UCortoMeshRendererComp();

	std::shared_ptr<IEvercoastStreamingDataUploader> GetMeshDataUploader() const;
	void SetMeshData(std::shared_ptr<CortoLocalMeshFrame> localMeshFrame);
	void SetTextureData(std::shared_ptr<CortoLocalTextureFrame> localTexture);

	UFUNCTION(BlueprintCallable, Category="Evercoast Playback")
	UTexture* GetTextureData();

	UFUNCTION(BlueprintSetter)
	void SetCortoMeshMaterial(UMaterialInterface* newMaterial);

	UFUNCTION(BlueprintSetter)
	void SetGenerateWorldNormalSize(int32 size);

	UFUNCTION(BlueprintSetter)
	void SetFilteredWorldNormalSize(int32 size);

	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void CommitImageEnhanceMaterialParams();

	//~ Begin UObject Interface.
	virtual void BeginDestroy() override;
	//~ End UObject Interface.

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual FBoxSphereBounds CalcLocalBounds() const override;
	//~ End USceneComponent Interface.

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin UMeshComponent Interface.
	virtual int32 GetNumMaterials() const override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	//~ End UMeshComponent Interface.

protected:
	//~ Begin UActorComponent interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent interface

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetCortoMeshMaterial, Category = "Rendering")
	UMaterialInterface* CortoMeshMaterial;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "-5.0", ClampMax = "5.0"), BlueprintReadWrite, Category = "Rendering")
	float ExposureCompensation = 0.0f;

	UPROPERTY(EditAnywhere, meta=(ClampMin="0.0", ClampMax ="1.0"), BlueprintReadWrite, Category = "Rendering")
	float HueShiftPercentage = 0.0f;

	UPROPERTY(EditAnywhere, meta=(ClampMin="0.0", ClampMax="2.0"), BlueprintReadWrite, Category = "Rendering")
	float SCurvePower = 1.0f;
	
	UPROPERTY(EditAnywhere, meta=(ClampMin="-1.0", ClampMax="1.0"), BlueprintReadWrite, Category = "Rendering")
	float Contrast = 0.0f;

	UPROPERTY(EditAnywhere, meta=(ClampMin = "-1.0", ClampMax = "1.0"), BlueprintReadWrite, Category = "Rendering")
	float Brightness = 0.0f;

	UPROPERTY(EditAnywhere, meta=(ClampMin = "-5.0", ClampMax = "5.0"), BlueprintReadWrite, Category = "Rendering")
	float AdditionalGamma = 1.0f;
	
	UPROPERTY(EditAnywhere, Category = "Rendering");
	bool bGenerateNormal = true;

	// The initial capture size, will determine the quality of generated normal
	UPROPERTY(EditAnywhere, meta = (EditCondition = "bGenerateNormal"), BlueprintSetter = SetGenerateWorldNormalSize, Category = "Rendering");
	int32 GenerateWorldNormalSize = 1024;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "bGenerateNormal"), Category = "Rendering");
	int32 WorldNormalSmoothIteration = 2;

	// The world normal size after filtering, will determine the quality of final output normal. It will be ignored if OverrideWorldNormalRenderTarget is set
	UPROPERTY(EditAnywhere, meta = (EditCondition = "bGenerateNormal"), BlueprintSetter = SetFilteredWorldNormalSize, Category = "Rendering");
	int32 FilteredWorldNormalSize = 512;

	// Allow user to provide their own normal render target, for outputting world normal purpose. Its dimension should be equal or smaller than GenerateWorldNormalSize
	UPROPERTY(EditAnywhere, meta = (EditCondition = "bGenerateNormal"), Category = "Rendering")
	UTextureRenderTarget2D* OverrideWorldNormalRenderTarget[2];


private:

	void ApplyImageEnhanceMaterialParams();
	void CheckImageEnhanceMaterialParams();

	std::shared_ptr<CortoDataUploader> m_meshUploader;
	std::shared_ptr<CortoLocalMeshFrame> m_currLocalMeshFrame;
	std::shared_ptr<CortoLocalTextureFrame> m_currLocalTextureFrame;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* m_materialInstance;

	mutable bool m_materialDirty;

	UPROPERTY(Transient)
	UTexture2D* m_mainTexture[2];			// this is a copy of texture set from SetTextureData()

	UPROPERTY()
	UTexture2D* m_mainTextureFirstFrame;	// copy of the copy of texture set from SetTextureData(), used for keeping the first frame when mesh hasn't started rendering, or in editor mode

	UPROPERTY(EditDefaultsOnly, Category="Evercoast Playback")
	bool bIsImageEnhancementMaterial = false;

	int m_mainTexturePtr = 0;

	TSharedPtr<TPromise<void>, ESPMode::ThreadSafe> m_copyPromise;

	UPROPERTY(Transient)
	UTextureRenderTarget2D* CaptureWorldNormalRenderTarget[2];

	UPROPERTY(Transient)
	UTextureRenderTarget2D* FilteredWorldNormalRenderTarget[2];

};
