// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <memory>
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/PrimitiveComponent.h"
#include "VoxelRendererComponent.h"
#include "EvercoastVoxelRendererComp.generated.h"


struct EvercoastLocalVoxelFrame;
class UTextureRenderTarget2D;
class IEvercoastStreamingDataUploader;
/**
 * EvercoastVoxelRendererComp
 * Component accepts Evercoast's decoder's output and renders it out
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class EVERCOASTPLAYBACK_API UEvercoastVoxelRendererComp : public UPrimitiveComponent, public IVoxelRendererComponent
{
	GENERATED_BODY()

public:
	UEvercoastVoxelRendererComp(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter=SetVoxelMaterial, Category = "Rendering")
	UMaterialInterface* VoxelMaterial;

	UPROPERTY(EditAnywhere, Category = "Rendering");
	bool bGenerateNormal = true;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "bGenerateNormal"), BlueprintSetter = SetUseIcosahedronForNormalGeneration, Category = "Rendering");
	bool bUseIcosahedronForNormalGeneration = true;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "bUseIcosahedronForNormalGeneration"), BlueprintSetter = SetNormalVoxelSizeFactor, Category = "Rendering");
	float NormalVoxelSizeFactor = 1.0f;

	// The initial capture size, will determine the quality of generated normal
	UPROPERTY(EditAnywhere, meta = (EditCondition = "bGenerateNormal"), BlueprintSetter = SetGenerateWorldNormalSize, Category = "Rendering");
	int32 GenerateWorldNormalSize = 1024;

	UPROPERTY(EditAnywhere, meta = (EditCondition = "bGenerateNormal"), Category = "Rendering");
	int32 WorldNormalSmoothIteration = 2;

	// The world normal size after filtering, will determine the quality of final output normal. It will be ignored if OverrideWorldNormalRenderTarget is set
	UPROPERTY(EditAnywhere, meta = (EditCondition = "bGenerateNormal && (OverrideWorldNormalRenderTarget[0] == nullptr || OverrideWorldNormalRenderTarget[1] == nullptr)"), BlueprintSetter = SetFilteredWorldNormalSize, Category = "Rendering");
	int32 FilteredWorldNormalSize = 512;

	// Allow user to provide their own normal render target, for outputting world normal purpose. Its dimension should be equal or smaller than GenerateWorldNormalSize
	UPROPERTY(EditAnywhere, meta = (EditCondition = "bGenerateNormal"), Category = "Rendering")
	UTextureRenderTarget2D* OverrideWorldNormalRenderTarget[2];
public:
	virtual std::shared_ptr<IEvercoastStreamingDataUploader> GetVoxelDataUploader() const override;
	virtual void SetVoxelData(std::shared_ptr<EvercoastLocalVoxelFrame> localVoxelFrame) override;
	virtual FPrimitiveSceneProxy* GetSceneProxy() const override
	{
		return SceneProxy;
	}
	void MarkDirty();

	UFUNCTION(BlueprintSetter)
	virtual void SetVoxelMaterial(UMaterialInterface* newMaterial) override;

	UFUNCTION(BlueprintSetter)
	virtual void SetUseIcosahedronForNormalGeneration(bool useIcosahedron);

	UFUNCTION(BlueprintSetter)
	virtual void SetNormalVoxelSizeFactor(float size);

	UFUNCTION(BlueprintSetter)
	void SetGenerateWorldNormalSize(int32 size);

	UFUNCTION(BlueprintSetter)
	void SetFilteredWorldNormalSize(int32 size);

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual int32 GetNumMaterials() const override;
	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const override;
	virtual void GetUsedMaterials(TArray< UMaterialInterface* >& OutMaterials, bool bGetDebugMaterials ) const override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual FBoxSphereBounds CalcLocalBounds() const override;
	//~ End USceneComponent Interface.

protected:
	//~ Begin UActorComponent interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent interface


private:
	UPROPERTY(Transient)
	UMaterialInstanceDynamic* VoxelMaterialDynamic;


	UPROPERTY(Transient)
	UTextureRenderTarget2D* CaptureWorldNormalRenderTarget[2];

	UPROPERTY(Transient)
	UTextureRenderTarget2D* FilteredWorldNormalRenderTarget[2];
	
	std::shared_ptr<IEvercoastStreamingDataUploader> m_voxelUploader;
	std::shared_ptr<EvercoastLocalVoxelFrame> m_currLocalVoxelFrame;
	bool m_dirtyMark;


};
