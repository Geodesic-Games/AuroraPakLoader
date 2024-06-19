// Copyright GeoTech BV


#include "PluginMountPoint.h"

#include "GFPakLoaderLog.h"

FPluginMountPoint::FPluginMountPoint(const FString& InRootPath, const FString& InContentPath)
	: RootPath(InRootPath), ContentPath(InContentPath), bNeedsUnregistering(false)
{
	RegisterMountPoint();
}

FPluginMountPoint::FPluginMountPoint(FPluginMountPoint&& Other) noexcept
	: RootPath(Other.RootPath), ContentPath(Other.ContentPath), bNeedsUnregistering(Other.bNeedsUnregistering)
{
	Other.bNeedsUnregistering = false; // We don't want to unregister the mount point when the moved instance is deleted
}


FPluginMountPoint::~FPluginMountPoint()
{
	UnregisterMountPoint();
}

TOptional<FPluginMountPoint> FPluginMountPoint::CreateFromContentPath(const FString& InContentPath)
{
	FString ContentPath;
	const TOptional<FString> MountPointName = GetMountPointFromContentPath(InContentPath, &ContentPath);
	if (MountPointName.IsSet())
	{
		return TOptional<FPluginMountPoint>{InPlace, MountPointName.GetValue(), ContentPath};
	}
	else
	{
		UE_LOG(LogGFPakLoader, Warning, TEXT("Unable to create a PluginMountPoint for ContentPath '%s' because this path does not contain '/Content/'."), *InContentPath)
	}
	return {};
}

TOptional<FString> FPluginMountPoint::GetMountPointFromContentPath(const FString& InContentPath, FString* OutContentPath)
{
	FString ContentPath;
	if (InContentPath.Split(TEXT("/Content/"), &ContentPath, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromStart))
	{
		// At this point, ContentPath is something like '.../.../Engine'. We want to find 'Engine' so we split at the last '/'
		FString MountPointName;
		if (!ensure(ContentPath.Split(TEXT("/"), nullptr, &MountPointName, ESearchCase::IgnoreCase, ESearchDir::FromEnd)))
		{
			// If we do not find the '/', that would mean that all the path before is the new MountPoint. Haven't seen this case so we issue a warning
			UE_LOG(LogGFPakLoader, Warning, TEXT("The ContentPath doesn't look regular (no '/' before the the MountPointName), this might not work as expected:  '%s'."), *InContentPath)
			MountPointName = ContentPath;
		}
		if (OutContentPath)
		{
			*OutContentPath = ContentPath + TEXT("/Content/");
		}
		return TEXT("/") + MountPointName + TEXT("/");
	}
	return {};
}

TOptional<FString> FPluginMountPoint::TryGetMountPointRootForPath(const FString& InContentPath, FPackageName::EErrorCode* OutFailureReason)
{
	TStringBuilder<256> MountPointNameBuilder;
	TStringBuilder<256> MountPointPathBuilder;
	TStringBuilder<256> RelativePathBuilder;
	FPackageName::EFlexNameType FlexNameType;
	if (FPackageName::TryGetMountPointForPath(InContentPath, MountPointNameBuilder, MountPointPathBuilder, RelativePathBuilder, &FlexNameType, OutFailureReason))
	{
		const FString MountPointName {MountPointNameBuilder}; // ex: "/Engine/"
		const FString MountPointPath {MountPointPathBuilder}; // ex: "../../../Engine/Content/"
		const FString RelativePath {RelativePathBuilder}; // ex: ""
		return MountPointName;
	}
	else
	{
		return {};
	}
}

bool FPluginMountPoint::RegisterMountPoint()
{
	if (bNeedsUnregistering)
	{
		return true;
	}
	
	FPackageName::EErrorCode FailureReason;
	const TOptional<FString> Root = TryGetMountPointRootForPath(ContentPath, &FailureReason);
	if (Root.IsSet())
	{
		if (Root.GetValue() == RootPath)
		{
			return true;
		}
		UE_LOG(LogGFPakLoader, Warning, TEXT("Tried the register the MountPoint '%s' => '%s' but this content path is already registered to another Root Path '%s'. This previous MountPoint will NOT BE be overriden"), *RootPath, *ContentPath, *Root.GetValue())
		return true;
	}
	if (Root.IsSet() || FailureReason == FPackageName::EErrorCode::PackageNamePathNotMounted)
	{
		UE_LOG(LogGFPakLoader, Verbose, TEXT("About to register Mount Point  '%s' => '%s'"), *RootPath, *ContentPath);
		if (!FPackageName::MountPointExists(RootPath))
		{
			FPackageName::RegisterMountPoint(RootPath, ContentPath);
			bNeedsUnregistering = true;
			const FString RegisteredContentPath = FPackageName::GetContentPathForPackageRoot(RootPath);
			UE_CLOG(RegisteredContentPath != ContentPath, LogGFPakLoader, Error, TEXT("  The Content Path actually registered for '%s' is not matching what was requested:  Requested: '%s'  Actual: '%s'"), *RootPath, *ContentPath, *RegisteredContentPath)
			return true;
		}
		else
		{
			const FString ExistingContentPath = FPackageName::GetContentPathForPackageRoot(RootPath);
			UE_LOG(LogGFPakLoader, Warning, TEXT("Mount Point will not be registered because it is already registered and points to another Content Path:  Existing: '%s' => '%s'  Proposed: '%s'"), *RootPath, *ExistingContentPath, *ContentPath);
			return false;
		}
	}
	
	UE_LOG(LogGFPakLoader, Warning, TEXT(" => Unable to register the Mount Point '%s' => '%s' because the failure was not 'PackageNamePathNotMounted'."), *RootPath, *ContentPath)
	return false;
}

bool FPluginMountPoint::UnregisterMountPoint()
{
	if (bNeedsUnregistering)
	{
		UE_LOG(LogGFPakLoader, Verbose, TEXT("About to unregister Mount Point  '%s' => '%s'"), *RootPath, *ContentPath);
		FPackageName::UnRegisterMountPoint(RootPath, ContentPath);
		bNeedsUnregistering = false;
		return true;
	}
	return false;
}
