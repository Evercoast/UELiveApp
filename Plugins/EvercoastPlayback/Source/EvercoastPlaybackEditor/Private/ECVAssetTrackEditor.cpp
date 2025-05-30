#include "ECVAssetTrackEditor.h"
#include "Sequencer/ECVAssetTrack.h"
#include "Sequencer/ECVAssetTrackSection.h"
#include "EvercoastECVAsset.h"
#include "EvercoastVolcapActor.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserDelegates.h"
#include "ContentBrowserModule.h"
#include "SequencerUtilities.h"
#include "LevelSequence.h"
#include "ECVAssetTrackEditorStyle.h"
#include "TrackEditorThumbnail/TrackEditorThumbnailPool.h"
#include "ECVAssetThumbnailSection.h"

#define LOCTEXT_NAMESPACE "FECVAssetTrackEditor"


TSharedRef<ISequencerTrackEditor> FECVAssetTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer)
{
	return MakeShared<FECVAssetTrackEditor>(OwningSequencer);
}

TArray<FAnimatedPropertyKey, TInlineAllocator<1>> FECVAssetTrackEditor::GetAnimatedPropertyTypes()
{
	return TArray<FAnimatedPropertyKey, TInlineAllocator<1>>({ FAnimatedPropertyKey::FromObjectType(UEvercoastECVAsset::StaticClass()) });
}

FECVAssetTrackEditor::FECVAssetTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FMovieSceneTrackEditor(InSequencer)
{
	ThumbnailPool = MakeShared<FTrackEditorThumbnailPool>(InSequencer);
}

UMovieSceneTrack* FECVAssetTrackEditor::AddTrack(UMovieScene* FocusedMovieScene, const FGuid& ObjectHandle, TSubclassOf<class UMovieSceneTrack> TrackClass, FName UniqueTypeName)
{
	UMovieSceneTrack* Track = FocusedMovieScene->AddTrack(TrackClass, ObjectHandle);
	return Track;
}

void FECVAssetTrackEditor::BuildAddTrackMenu(FMenuBuilder& MenuBuilder)
{
}

void FECVAssetTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder,
	const TArray < FGuid >& ObjectBindings,
	const UClass* ObjectClass)
{
	if (ObjectClass == AEvercoastVolcapActor::StaticClass())
	{

		MenuBuilder.AddMenuEntry(
			LOCTEXT("AddTrack", "Evercoast VolCap Track"),
			LOCTEXT("AddTooltip", "Adds a new bounded Evercoast volcap track."),
			FSlateIcon(FECVAssetTrackEditorStyle::Get().GetStyleSetName(), "Sequencer.Tracks.ECV"),
			FUIAction(
				FExecuteAction::CreateLambda(
					[this, ObjectBindings, ObjectClass]()
					{
						HandleAddECVTrackBoundMenuEntryExecute(ObjectBindings, ObjectClass);
					}
		)));
	}
}


void FECVAssetTrackEditor::HandleAddECVTrackBoundMenuEntryExecute(const TArray < FGuid >& ObjectBindings, const UClass* ObjectClass)
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();
	if (FocusedMovieScene == nullptr)
	{
		return;
	}

	if (FocusedMovieScene->IsReadOnly())
	{
		return;
	}

	const TSharedPtr<ISequencer> TheSequencer = GetSequencer();

	

	const FScopedTransaction Transaction(NSLOCTEXT("Sequencer", "AddECVTrack_Transaction", "Add ECV Track"));
	FocusedMovieScene->Modify();
	for (int i = 0; i < ObjectBindings.Num(); ++i)
	{
		FMovieSceneBinding* ObjectBinding = FocusedMovieScene->FindBinding(ObjectBindings[i]);
		if (ObjectBinding)
		{
			// Create your custom track (a subclass of UMovieSceneNameableTrack)
			UECVAssetTrack* NewTrack = NewObject<UECVAssetTrack>(FocusedMovieScene, UECVAssetTrack::StaticClass());
			ensure(NewTrack);

			// Add it to the object binding so it appears as a sub-track of that actor/component.
#if ENGINE_MAJOR_VERSION == 5
			ObjectBinding->AddTrack(*NewTrack, FocusedMovieScene);
#else
			ObjectBinding->AddTrack(*NewTrack);
#endif
			NewTrack->SetDisplayName(LOCTEXT("UnnamedECVTrackName", "My ECV Track"));

			if (TheSequencer.IsValid())
			{
				TheSequencer->OnAddTrack(NewTrack, ObjectBindings[i]);
			}
		}
	}
}

void FECVAssetTrackEditor::HandleAddECVTrackMenuEntryExecute()
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();

	if (FocusedMovieScene == nullptr)
	{
		return;
	}

	if (FocusedMovieScene->IsReadOnly())
	{
		return;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("Sequencer", "AddECVTrack_Transaction", "Add ECV Track"));
	FocusedMovieScene->Modify();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
	auto NewTrack = FocusedMovieScene->AddTrack<UECVAssetTrack>();
#else
	auto NewTrack = FocusedMovieScene->AddMasterTrack<UECVAssetTrack>();
#endif
	ensure(NewTrack);
	NewTrack->SetDisplayName(LOCTEXT("UnnamedECVTrackName", "My ECV Track"));

	if (GetSequencer().IsValid())
	{
		GetSequencer()->OnAddTrack(NewTrack, FGuid());
	}
}

TSharedPtr<SWidget> FECVAssetTrackEditor::BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params)
{
	UECVAssetTrack* ECVTrack = Cast<UECVAssetTrack>(Track);

	if (ECVTrack == nullptr)
	{
		return SNullWidget::NullWidget;
	}

	auto CreatePicker = [this, ECVTrack]
	{
		FAssetPickerConfig AssetPickerConfig;
		{
			AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateRaw(this, &FECVAssetTrackEditor::AddNewSection, ECVTrack);
			AssetPickerConfig.OnAssetEnterPressed = FOnAssetEnterPressed::CreateRaw(this, &FECVAssetTrackEditor::AddNewSectionEnterPressed, ECVTrack);
			AssetPickerConfig.bAllowNullSelection = false;
			AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
			AssetPickerConfig.Filter.bRecursiveClasses = true;
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
			AssetPickerConfig.Filter.ClassPaths.Add(FTopLevelAssetPath(UEvercoastECVAsset::StaticClass()));
#else
			AssetPickerConfig.Filter.ClassNames.Add(UEvercoastECVAsset::StaticClass()->GetFName());
#endif
		}

		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

		TSharedRef<SBox> Picker = SNew(SBox)
			.WidthOverride(300.0f)
			.HeightOverride(300.f)
			[
				ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
			];

		return Picker;
	};

	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			FSequencerUtilities::MakeAddButton(LOCTEXT("AddECVTrackSection_Text", "ECV/ECM"), FOnGetContent::CreateLambda(CreatePicker), Params.NodeIsHovered, GetSequencer())
		];
}


void FECVAssetTrackEditor::AddNewSection(const FAssetData& AssetData, UECVAssetTrack* ECVTrack)
{
	FSlateApplication::Get().DismissAllMenus();

	UObject* SelectedObject = AssetData.GetAsset();

	if (SelectedObject)
	{
		auto ECVAsset = CastChecked<UEvercoastECVAsset>(AssetData.GetAsset());

		if (ECVAsset != nullptr)
		{
			const FScopedTransaction Transaction(NSLOCTEXT("Sequencer", "AddECVTrackSection_Transaction", "Add ECV"));

			ECVAsset->Modify();

			FFrameTime KeyTime = GetSequencer()->GetLocalTime().Time;
			UECVAssetTrackSection* NewSection = ECVTrack->AddNewECVAsset(*ECVAsset, KeyTime.FrameNumber);

			GetSequencer()->EmptySelection();
			GetSequencer()->SelectSection(NewSection);
			GetSequencer()->ThrobSectionSelection();

			GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
		}
	}
}

void FECVAssetTrackEditor::AddNewSectionEnterPressed(const TArray<FAssetData>& AssetData, UECVAssetTrack* Track)
{
	if (AssetData.Num() > 0)
	{
		AddNewSection(AssetData[0].GetAsset(), Track);
	}
}

bool FECVAssetTrackEditor::HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid)
{
	if ((Asset == nullptr) || !Asset->IsA<UEvercoastECVAsset>())
	{
		return false;
	}

	UEvercoastECVAsset* ECVAsset = Cast<UEvercoastECVAsset>(Asset);

	if (TargetObjectGuid.IsValid())
	{
		TArray<TWeakObjectPtr<>> OutObjects;

		for (TWeakObjectPtr<> Object : GetSequencer()->FindObjectsInCurrentSequence(TargetObjectGuid))
		{
			OutObjects.Add(Object);
		}

		int32 RowIndex = INDEX_NONE;
		AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(this, &FECVAssetTrackEditor::AddAttachedECVAsset, ECVAsset, OutObjects, RowIndex));
	}
	else
	{
		int32 RowIndex = INDEX_NONE;
		AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(this, &FECVAssetTrackEditor::AddMasterECVAsset, ECVAsset, RowIndex));
	}

	return true;
}


FKeyPropertyResult FECVAssetTrackEditor::AddAttachedECVAsset(FFrameNumber KeyTime, UEvercoastECVAsset* Asset, TArray<TWeakObjectPtr<UObject>> ObjectsToAttachTo, int32 RowIndex)
{
	FKeyPropertyResult KeyPropertyResult;

	for (int32 ObjectIndex = 0; ObjectIndex < ObjectsToAttachTo.Num(); ++ObjectIndex)
	{
		UObject* Object = ObjectsToAttachTo[ObjectIndex].Get();

		FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToObject(Object);
		FGuid ObjectHandle = HandleResult.Handle;
		KeyPropertyResult.bHandleCreated |= HandleResult.bWasCreated;

		if (ObjectHandle.IsValid())
		{
			FFindOrCreateTrackResult TrackResult = FindOrCreateTrackForObject(ObjectHandle, UECVAssetTrack::StaticClass());
			UMovieSceneTrack* Track = TrackResult.Track;
			KeyPropertyResult.bTrackCreated |= TrackResult.bWasCreated;

			if (ensure(Track))
			{
				UECVAssetTrack* ECVTrack = Cast<UECVAssetTrack>(Track);
				UECVAssetTrackSection* NewSection = ECVTrack->AddNewECVAsset(*Asset, KeyTime);
				ECVTrack->SetDisplayName(LOCTEXT("ECVTrackName", "ECV"));
				KeyPropertyResult.bTrackModified = true;
				KeyPropertyResult.SectionsCreated.Add(NewSection); 

				GetSequencer()->EmptySelection();
				GetSequencer()->SelectSection(NewSection);
				GetSequencer()->ThrobSectionSelection();
			}
		}
	}

	return KeyPropertyResult;
}

FKeyPropertyResult FECVAssetTrackEditor::AddMasterECVAsset(FFrameNumber KeyTime, UEvercoastECVAsset* Asset, int32 RowIndex)
{
	FKeyPropertyResult KeyPropertyResult;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
	FFindOrCreateRootTrackResult<UECVAssetTrack> TrackResult = FindOrCreateRootTrack<UECVAssetTrack>();
#else
	FFindOrCreateMasterTrackResult<UECVAssetTrack> TrackResult = FindOrCreateMasterTrack<UECVAssetTrack>();
#endif
	UMovieSceneTrack* Track = TrackResult.Track;
	UECVAssetTrack* ECVTrack = Cast<UECVAssetTrack>(Track);

	UECVAssetTrackSection* NewSection = ECVTrack->AddNewECVAsset(*Asset, KeyTime);

	if (TrackResult.bWasCreated)
	{
		ECVTrack->SetDisplayName(LOCTEXT("ECVTrackName", "ECV"));
	}

	KeyPropertyResult.bTrackModified = true;
	KeyPropertyResult.SectionsCreated.Add(NewSection);

	return KeyPropertyResult;
}


// interfacing between MovieScene and Sequencer: UMovieSceneSection -> ISequencerSection
TSharedRef<ISequencerSection> FECVAssetTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	// Use ObjectBinding to find bound UObject of the track. Then FECVAssetThumbnailSection should know which EvercoastVolcapActor to operate on.
	TSharedPtr<ISequencer> TheSequencer = GetSequencer();

	// We used to provide EvercoastStreamingReader to ECVAssetThumbnailSection but spawnable ones are created later, so the thumbnail has to
	// query for the reader itself
	/*
	if (TheSequencer.IsValid())
	{
		FMovieSceneSequenceID SequenceId = TheSequencer->GetFocusedTemplateID();
		for (TWeakObjectPtr<> WeakObject : TheSequencer->FindBoundObjects(ObjectBinding, SequenceId))
		{
			UObject* BoundObject = WeakObject.Get();
			if (!BoundObject)
				continue;

			if (!BoundObject->IsA(AEvercoastVolcapActor::StaticClass()))
				continue;

			AEvercoastVolcapActor* VolcapActor = Cast<AEvercoastVolcapActor>(BoundObject);
			return MakeShared<FECVAssetThumbnailSection>(*CastChecked<UECVAssetTrackSection>(&SectionObject), ThumbnailPool, TheSequencer, VolcapActor->Reader);
		}
	}
	*/

	return MakeShared<FECVAssetThumbnailSection>(*CastChecked<UECVAssetTrackSection>(&SectionObject), ThumbnailPool, TheSequencer);
}


bool FECVAssetTrackEditor::SupportsSequence(UMovieSceneSequence* InSequence) const
{
	return InSequence && InSequence->IsA(ULevelSequence::StaticClass());
}
bool FECVAssetTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> TrackClass) const
{
	return TrackClass.Get() && TrackClass.Get()->IsChildOf(UECVAssetTrack::StaticClass());
}
void FECVAssetTrackEditor::Tick(float DeltaTime)
{
	ThumbnailPool->DrawThumbnails();
}
const FSlateBrush* FECVAssetTrackEditor::GetIconBrush() const
{
	return FECVAssetTrackEditorStyle::Get().GetBrush("Sequencer.Tracks.ECV");
}

#undef LOCTEXT_NAMESPACE
