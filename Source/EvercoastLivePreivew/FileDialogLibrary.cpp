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

        FString ExtensionStr;
        ExtensionStr += TEXT("Evercoast Volcap Files|*.ecv;*.ecm;*.ecz|");
        ExtensionStr += TEXT("All files|*.*");

        TArray<FString> OutFiles;
        bool bOpened = DesktopPlatform->OpenFileDialog(
            ParentWindowHandle,
            TEXT("Open File"),
            FPaths::ProjectDir(),
            TEXT(""),
            ExtensionStr,
            EFileDialogFlags::None,
            OutFiles
        );

        if (bOpened && OutFiles.Num() > 0)
        {
            // Return an absolute path to avoid issues
            FilePath = FPaths::ConvertRelativePathToFull(OutFiles[0]);
        }
    }

    return FilePath;
}
