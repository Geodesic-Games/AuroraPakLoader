// Copyright GeoTech BV


#include "GFPakLoaderSettings.h"

#include "GFPakLoaderSubsystem.h"
#include "Misc/Paths.h"

UGFPakLoaderSettings::UGFPakLoaderSettings()
{
	SetPakLoadPath(TEXT("/DLC/"));
}

#if WITH_EDITOR
void UGFPakLoaderSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// The RelativePath meta specifier doesn't seem to do what we want, so we manually make the path relative
	if(PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(UGFPakLoaderSettings, StartupPakLoadDirectory))
	{
		SetPakLoadPath(StartupPakLoadDirectory.Path);
	}
	if(PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(UGFPakLoaderSettings, bEnsureWorldIsLoadedInMemoryBeforeLoadingMap))
	{
		if (UGFPakLoaderSubsystem* Subsystem = UGFPakLoaderSubsystem::Get())
		{
			Subsystem->OnEnsureWorldIsLoadedInMemoryBeforeLoadingMapChanged();
		}
	}
}
#endif

void UGFPakLoaderSettings::SetPakLoadPath(const FString& Path)
{
	// Here we want to make sure the path is relative if it is within the Project Directory, otherwise the path will be absolute
	StartupPakLoadDirectory.Path = Path;
	StartupPakLoadDirectory.Path.RemoveFromStart(TEXT("/"));
	StartupPakLoadDirectory.Path.RemoveFromStart(TEXT("\\"));
	
	const FString ProjectDir = FPaths::ProjectDir();
	StartupPakLoadDirectory.Path = FPaths::ConvertRelativePathToFull(ProjectDir, StartupPakLoadDirectory.Path);
	FPaths::NormalizeDirectoryName(StartupPakLoadDirectory.Path);
	FPaths::CollapseRelativeDirectories(StartupPakLoadDirectory.Path);

	// We finally check if the path is within the project directory, and if it is, we make the path relative
	FString AbsolutePath = StartupPakLoadDirectory.Path;
	if (FPaths::MakePathRelativeTo(AbsolutePath, *ProjectDir))
	{
		if (!AbsolutePath.StartsWith(TEXT("../")))
		{
			StartupPakLoadDirectory.Path = TEXT("/") + AbsolutePath;
		}
	}
}

FString UGFPakLoaderSettings::GetAbsolutePakLoadPath() const
{
	FString Path = StartupPakLoadDirectory.Path;
	Path.RemoveFromStart(TEXT("/"));
	Path.RemoveFromStart(TEXT("\\"));
	
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), Path);
}
