// The manual vertex fetch version of EvercoastVoxelRendererComp

#pragma once

#include <memory>
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/MeshComponent.h"
#include "VoxelRendererComponent.h"
#include "EvercoastMVFVoxelRendererComp.generated.h"


struct EvercoastLocalVoxelFrame;
class IEvercoastStreamingDataUploader;
/**
 * UEvercoastMVFVoxelRendererComp
 * Component accepts Evercoast's decoder's output and renders it out
 */
UCLASS(hidecategories = (Object, LOD), meta = (BlueprintSpawnableComponent), ClassGroup = EvercoastVoxel)
class EVERCOASTPLAYBACK_API UEvercoastMVFVoxelRendererComp : public UMeshComponent, public IVoxelRendererComponent
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetVoxelMaterial, Category = "Rendering")
	UMaterialInterface* VoxelMaterial;
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

protected:

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual int32 GetNumMaterials() const override { return 1; }
	virtual void GetUsedMaterials(TArray< UMaterialInterface* >& OutMaterials, bool bGetDebugMaterials ) const override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual FBoxSphereBounds CalcLocalBounds() const override;
	//~ End USceneComponent Interface.

	//~ Begin UActorComponent interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent interface
	
	//~ Begin UObject Interface.
	virtual void BeginDestroy() override;
	//~ End UObject Interface.

	std::shared_ptr<IEvercoastStreamingDataUploader> m_voxelUploader;
	std::shared_ptr<EvercoastLocalVoxelFrame> m_currLocalVoxelFrame;
	bool m_dirtyMark = true;
};
