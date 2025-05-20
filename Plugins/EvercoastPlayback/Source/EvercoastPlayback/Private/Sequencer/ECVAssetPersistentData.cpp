#include "Sequencer/ECVAssetPersistentData.h"
#include "EvercoastStreamingReaderComp.h"
#include "EvercoastRendererSelectorComp.h"
#include "EvercoastVolcapActor.h"

FECVAssetPersistentData::FECVAssetPersistentData() //: AcquiredActor(nullptr)
{

}

FECVAssetPersistentData::~FECVAssetPersistentData()
{
}

/*
UEvercoastStreamingReaderComp* FECVAssetPersistentData::GetEvercoastReader() const
{
	if (AcquiredActor && !AcquiredActor->IsActorBeingDestroyed())
	{
		return AcquiredActor.LoadSynchronous()->Reader;
	}
	return nullptr;
}


UEvercoastRendererSelectorComp* FECVAssetPersistentData::GetEvercoastRenderer() const
{
	if (AcquiredActor && !AcquiredActor->IsActorBeingDestroyed())
	{
		return AcquiredActor.LoadSynchronous()->RendererSelector;
	}
	return nullptr;
}

void FECVAssetPersistentData::AcquireActor(TSoftObjectPtr<AEvercoastVolcapActor> theActor)
{
	AcquiredActor = theActor;
	// Must NOT call AddToRoot()
}
void FECVAssetPersistentData::GiveupActor()
{
	// Must NOT call RemoveFromRoot()
	AcquiredActor = nullptr;
}
*/