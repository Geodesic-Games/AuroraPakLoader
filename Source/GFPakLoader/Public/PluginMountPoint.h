// Copyright GeoTech BV

#pragma once

#include "CoreMinimal.h"
#include "Misc/PackageName.h"

/**
 * Class managing a MountPoint for a Pak Plugin, automatically Registering it if required on construction, and Unregistering it on destruction
 */
struct GFPAKLOADER_API FPluginMountPoint : public TSharedFromThis<FPluginMountPoint>
{
public:
	/**
	 * Create a new MountPoint and registers it if not yet existing.
	 * @param InRootPath The logical Root Path of this MountPoint. Needs to Start and end with '/', for example, '/Engine/'
	 * @param InContentPath The Content Path on disk for this MountPoint. For example, '.../.../Engine/Content/'
	 */
	FPluginMountPoint(const FString& InRootPath, const FString& InContentPath);
	FPluginMountPoint(const FPluginMountPoint& Other) = delete;
	/** Only implementing the Move Constructor to make sure the moved FPluginMountPoint does not Unregister its MountPoint on destruction. */
	FPluginMountPoint(FPluginMountPoint&& Other) noexcept;
	FPluginMountPoint& operator=(const FPluginMountPoint& Other) = delete;
	FPluginMountPoint& operator=(FPluginMountPoint&& Other) = delete;
	/** Destructor that unregisters the MountPoint if bNeedsUnregistering is true */
	~FPluginMountPoint();

	/**
	 * Create a PluginMountPoint from a Content Path.
	 * The Root Path is derived from the Content Path, so the Content Path '.../.../Engine/Content/' will create a Root Path '/Engine/'
	 * @param InContentPath The Content Path on disk for this MountPoint. For example, '.../.../Engine/Content/'.
	 * Needs to have '/Content/' in the path, representing the content folder.
	 * @return 
	 */
	static TOptional<FPluginMountPoint> CreateFromContentPath(const FString& InContentPath);
	
	/** Returns the logical Root Path of this MountPoint. For example, '/Engine/' */
	const FString& GetRootPath() const { return RootPath; }
	/** Returns the Content Path on disk of this MountPoint. For example, '.../.../Engine/Content/' */
	const FString& GetContentPath() const { return ContentPath; }

	/**
	 * Returns true if this Mount Point is currently Registered with the FPackageName.
	 * Does not mean this instance added this Mount Point, it could have been created somewhere else.
	 */
	bool IsRegistered() const
	{
		return bNeedsUnregistering || TryGetMountPointRootForPath(ContentPath).Get(FString()) == RootPath;
	}
	/**
	 * Returns true if this Mount Point was registered by this instance and will need to be Unregistered with the FPackageName.
	 * Implies that IsRegistered returns true.
	 * Does not check if the MountPoint was removed manually outside of this struct
	 */
	bool NeedsUnregistering() const { return bNeedsUnregistering;}

	/**
	 * Check if the given Content Path on disk is registered as a Root Path, and if yes returns the RootPath
	 * @param InContentPath The Content Path on disk of this MountPoint. For example, '.../.../Engine/Content/'
	 * @param OutFailureReason If non-null, will be set to the reason InContentPath was not found, or to EErrorCode::Unknown if the function was successful.
	 * @return Returns an empty Optional if not found, otherwise the Root Path of this MountPoint. For example, '/Engine/'
	 */
	static TOptional<FString> TryGetMountPointRootForPath(const FString& InContentPath, FPackageName::EErrorCode* OutFailureReason = nullptr);
private:
	/**
	 * Registers the MountPoint.
	 * @return true if the Mount Point is registered, otherwise false
	 */
	bool RegisterMountPoint();
	/**
	 * Unregisters the MountPoint only if bNeedsUnregistering is true;
	 * @return true if the Mount Point is unregistered, otherwise false
	 */
	bool UnregisterMountPoint();
	
	const FString RootPath;
	const FString ContentPath;
	bool bNeedsUnregistering;
};
