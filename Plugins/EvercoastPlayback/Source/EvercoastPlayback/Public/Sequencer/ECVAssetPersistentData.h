#pragma once
#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "UObject/SoftObjectPtr.h"
#include "Evaluation/MovieScenePropertyTemplate.h"

class UEvercoastStreamingReaderComp;
class UEvercoastRendererSelectorComp;
class AEvercoastVolcapActor;

// Persistent data passed along to execution tokens.
struct EVERCOASTPLAYBACK_API FECVAssetPersistentData :
	public PropertyTemplate::FSectionData
{
	FECVAssetPersistentData();
	virtual ~FECVAssetPersistentData();

public:
	//UEvercoastStreamingReaderComp* GetEvercoastReader() const;
	//UEvercoastRendererSelectorComp* GetEvercoastRenderer() const;
	/** Set up this persistent data object. */
	//void AcquireActor(TSoftObjectPtr<AEvercoastVolcapActor> theActor);
	//void GiveupActor();
private:
	//TSoftObjectPtr<AEvercoastVolcapActor> AcquiredActor;
};