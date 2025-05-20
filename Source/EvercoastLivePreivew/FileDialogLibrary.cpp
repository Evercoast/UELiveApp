// FileDialogLibrary.cpp
#include "FileDialogLibrary.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Misc/Paths.h"
#include "Framework/Application/SlateApplication.h"

FString UFileDialogLibrary::OpenFileDialog()
{
    FString FilePath;
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

    if (DesktopPlatform)
    {
        const void* ParentWindowHandle = FSlateApplication::IsInitialized()
            ? FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr)
            : nullptr;

        TArray<FString> OutFiles;
        bool bOpened = DesktopPlatform->OpenFileDialog(
            ParentWindowHandle,
            TEXT("Open File"),
            FPaths::ProjectDir(),
            TEXT(""),
            TEXT("Evercoast Splats Files (*.ecz)|*.ecz"),
            EFileDialogFlags::None,
            OutFiles
        );

        if (bOpened && OutFiles.Num() > 0)
        {
            FilePath = OutFiles[0];
        }
    }

    return FilePath;
}
