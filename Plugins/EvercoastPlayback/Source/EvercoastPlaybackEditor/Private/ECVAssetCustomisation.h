#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;
class IPropertyHandle;

class FECVAssetCustomisation : public IDetailCustomization
{
public:

	//~ IDetailCustomization interface

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable(new FECVAssetCustomisation());
	}
protected:

	TSharedRef<SWidget> MakePlatformCookOverrideValueWidget();

private:

	/** Pointer to the DefaultPlayers property handle. */
	TSharedPtr<IPropertyHandle> PlatformCookingOverrideProperty;
};