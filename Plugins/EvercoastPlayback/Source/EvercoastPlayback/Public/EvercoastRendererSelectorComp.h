#pragma once
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/PrimitiveComponent.h"
#include "GenericDecoder.h"
#include "EvercoastRendererSelectorComp.generated.h"


class IEvercoastStreamingDataUploader;
class UEvercoastVoxelRendererComp;
class UEvercoastMVFVoxelRendererComp;
class IVoxelRendererComponent;
class UCortoMeshRendererComp;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class EVERCOASTPLAYBACK_API UEvercoastRendererSelectorComp : public USceneComponent
{
	GENERATED_BODY()
public:
	UEvercoastRendererSelectorComp(const FObjectInitializer& ObjectInitializer);

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintGetter = GetECVMaterial, BlueprintSetter = SetECVMaterial, Category = "Rendering")
	UMaterialInterface* ECVMaterial;

	UPROPERTY(EditAnywhere, Category = "Rendering")
	bool bKeepRenderedFrameWhenStopped;

	UFUNCTION(BlueprintSetter)
	void SetECVMaterial(UMaterialInterface* newMaterial);

	UFUNCTION(BlueprintGetter)
	UMaterialInterface* GetECVMaterial() const;
    
    UFUNCTION(BlueprintCallable, Category = "Rendering")
    void ResetRendererSelection();

public:

	void ChooseCorrespondingSubRenderer(DecoderType decoderType);
	std::shared_ptr<IEvercoastStreamingDataUploader> GetDataUploader() const;

private:
	bool IsUsingVoxelRenderer() const;
	bool IsUsingMeshRenderer() const;

protected:
	//~ Begin UActorComponent interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
public:
	//~ End UActorComponent interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

private:
	// sub renderer components
	// .ecv voxel renderer
	UPROPERTY()
	UEvercoastVoxelRendererComp* m_voxelRenderer;
	// .ecm corto mesh renderer
	UPROPERTY()
	UCortoMeshRendererComp* m_meshRenderer;

	UPrimitiveComponent* m_currRenderer;
};
