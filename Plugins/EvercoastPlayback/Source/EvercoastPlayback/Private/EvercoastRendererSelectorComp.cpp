#include "EvercoastRendererSelectorComp.h"
#include "EvercoastVoxelRendererComp.h"
#include "EvercoastMVFVoxelRendererComp.h"
#include "VoxelRendererComponent.h"
#include "CortoMeshRendererComp.h"

// Change this type to either UEvercoastVoxelRendererComp or UEvercoastMVFVoxelRendererComp, as well as in the header(UHT forbids me to do both in one place)
typedef UEvercoastVoxelRendererComp CHOSEN_VOXEL_RENDERER_COMPONENT;

UEvercoastRendererSelectorComp::UEvercoastRendererSelectorComp(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	ECVMaterial(nullptr),
	bKeepRenderedFrameWhenStopped(true),
	m_voxelRenderer(nullptr),
	m_meshRenderer(nullptr),
	m_currRenderer(nullptr)
{

}

void UEvercoastRendererSelectorComp::SetECVMaterial(UMaterialInterface* newMaterial)
{
	if (IsUsingVoxelRenderer())
	{
		m_voxelRenderer->SetVoxelMaterial(newMaterial);
	}
	else if (IsUsingMeshRenderer())
	{
		m_meshRenderer->SetCortoMeshMaterial(newMaterial);
	}
	else
	{
		UE_LOG(EvercoastRendererLog, Warning, TEXT("Undecided renderer type. ECV material will be actually assigned when the renderer component is created."));
	}

	ECVMaterial = newMaterial;
}

UMaterialInterface* UEvercoastRendererSelectorComp::GetECVMaterial() const
{
	if (IsUsingVoxelRenderer())
	{
		return m_voxelRenderer->VoxelMaterial;
	}
	else if (IsUsingMeshRenderer())
	{
		return m_meshRenderer->CortoMeshMaterial;
	}
	else
	{
		// Actuall we should allow null material returned to allow Blueprint construction script
		return nullptr;
	}
}

bool UEvercoastRendererSelectorComp::IsUsingVoxelRenderer() const
{
	if (!m_currRenderer)
		return false;

	return m_currRenderer->GetClass() == CHOSEN_VOXEL_RENDERER_COMPONENT::StaticClass();
}

bool UEvercoastRendererSelectorComp::IsUsingMeshRenderer() const
{
	if (!m_currRenderer)
		return false;

	return m_currRenderer->GetClass() == UCortoMeshRendererComp::StaticClass();
}

void UEvercoastRendererSelectorComp::ResetRendererSelection()
{
    m_currRenderer = nullptr;
}

void UEvercoastRendererSelectorComp::ChooseCorrespondingSubRenderer(DecoderType decoderType)
{
	auto Actor = GetOwner();
	if (decoderType == DT_EvercoastVoxel)
	{
		if (!m_voxelRenderer)
		{
			auto existingVoxelComponent = Actor->FindComponentByClass<CHOSEN_VOXEL_RENDERER_COMPONENT>();
			if (existingVoxelComponent)
			{
				m_voxelRenderer = existingVoxelComponent;
				m_voxelRenderer->RegisterComponent();
				Actor->AddInstanceComponent(m_voxelRenderer);
			}
			else
			{
				m_voxelRenderer = NewObject<CHOSEN_VOXEL_RENDERER_COMPONENT>(Actor, CHOSEN_VOXEL_RENDERER_COMPONENT::StaticClass());
				m_voxelRenderer->RegisterComponent();
				m_voxelRenderer->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);

				Actor->AddInstanceComponent(m_voxelRenderer);
			}

			m_voxelRenderer->SetVoxelMaterial(ECVMaterial);
		}

		if (m_currRenderer != m_voxelRenderer && m_currRenderer != nullptr)
		{
			Actor->RemoveInstanceComponent(m_currRenderer);
			m_currRenderer->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
			m_currRenderer->DestroyComponent();

			m_meshRenderer = nullptr;
		}
		m_currRenderer = m_voxelRenderer;
        m_currRenderer->SetVisibility(true, true);

		
        if (m_meshRenderer != nullptr)
        {
            m_meshRenderer->SetVisibility(false, true);
        }
	}
	else if (decoderType == DT_CortoMesh)
	{
		if (!m_meshRenderer)
		{
			auto existingMeshComponent = Actor->FindComponentByClass<UCortoMeshRendererComp>();
			if (existingMeshComponent)
			{
				m_meshRenderer = existingMeshComponent;
				m_meshRenderer->RegisterComponent();
				Actor->AddInstanceComponent(m_meshRenderer);
			}
			else
			{
				m_meshRenderer = NewObject<UCortoMeshRendererComp>(Actor, UCortoMeshRendererComp::StaticClass());
				m_meshRenderer->RegisterComponent();
				m_meshRenderer->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);

				Actor->AddInstanceComponent(m_meshRenderer);
			}

			m_meshRenderer->SetCortoMeshMaterial(ECVMaterial);
		}

		if (m_currRenderer != m_meshRenderer && m_currRenderer != nullptr)
		{
			Actor->RemoveInstanceComponent(m_currRenderer);
			m_currRenderer->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
			m_currRenderer->DestroyComponent();

			m_voxelRenderer = nullptr;
		}
		m_currRenderer = m_meshRenderer;
        m_currRenderer->SetVisibility(true, true);
        
        // hide other renderer if any
        if (m_voxelRenderer != nullptr)
        {
            m_voxelRenderer->SetVisibility(false, true);
        }
	}
	else
	{
		ensureMsgf(false, TEXT("Unexpected decoder type. Unable to choose renderer!"));
	}

}

std::shared_ptr<IEvercoastStreamingDataUploader> UEvercoastRendererSelectorComp::GetDataUploader() const
{
	if (!m_currRenderer)
		return nullptr;

	// TODO: remove the casting, general interface
	if (IsUsingVoxelRenderer())
	{
		return Cast<CHOSEN_VOXEL_RENDERER_COMPONENT>(m_currRenderer)->GetVoxelDataUploader();
	}
	else if (IsUsingMeshRenderer())
	{
		return Cast<UCortoMeshRendererComp>(m_currRenderer)->GetMeshDataUploader();
	}
	else
	{
		ensureMsgf(false, TEXT("Unexpected renderer type. Unable to get uploader!"));
		return nullptr;
	}
}

#if WITH_EDITOR
void UEvercoastRendererSelectorComp::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (!m_currRenderer)
	{
		Super::PostEditChangeProperty(PropertyChangedEvent);
		return;
	}

	if (PropertyChangedEvent.GetPropertyName() == FName(TEXT("ECVMaterial")))
	{
		SetECVMaterial(ECVMaterial);
	}

	//return m_currRenderer->PostEditChangeProperty(PropertyChangedEvent);
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif


void UEvercoastRendererSelectorComp::OnRegister()
{
	Super::OnRegister();
}

void UEvercoastRendererSelectorComp::OnUnregister()
{
	Super::OnUnregister();
}
