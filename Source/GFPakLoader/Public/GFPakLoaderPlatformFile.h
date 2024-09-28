// Copyright GeoTech BV

#pragma once

#include "IPlatformFilePak.h"
#include "HAL/PlatformFileManager.h"

class GFPAKLOADER_API FGFPakLoaderPlatformFile : public IPlatformFile
{
public:
	FGFPakLoaderPlatformFile() : LowerLevel(nullptr), PakPlatformFile(nullptr) {}
	static const TCHAR* GetTypeName()
	{
		return TEXT("GFPakLoader");
	}
	virtual FPakPlatformFile* GetPakPlatformFile() { return PakPlatformFile; }
	
	void RegisterPakContentPath(const FString& MountPoint)
	{
		static FString Content{TEXT("Content/")};
		static int ContentLength{Content.Len()};
		
		if (ensure(MountPoint.EndsWith(Content)))
		{
			PakMountPaths.Add(MountPoint.Left(MountPoint.Len() - ContentLength));
		}
	}
	void UnregisterPakContentPath(const FString& MountPoint)
	{
		static FString Content{TEXT("Content/")};
		static int ContentLength{Content.Len()};
		
		if (ensure(MountPoint.EndsWith(Content)))
		{
			PakMountPaths.Remove(MountPoint.Left(MountPoint.Len() - ContentLength));
		}
	}
	/** Returns the PakPlatformFile if the Filename is from a PakFile, otherwise return the LowerLevel */
	IPlatformFile* GetPlatformFile(const TCHAR* Filename) const //todo: review, doesn't seem really needed anymore, but this might allow more efficient way to find the right pak
	{
		return GetPlatformFile();
	}
	/** Returns the PakPlatformFile if valid, otherwise return the LowerLevel */
	IPlatformFile* GetPlatformFile() const
	{
		if (IsEngineExitRequested())
		{
			// After Engine exit has been requested, the FPakPlatformFile might get destroyed, but we don't have callback so we check everytime
			FPakPlatformFile* PlatformFile = static_cast<FPakPlatformFile*>(FPlatformFileManager::Get().FindPlatformFile(FPakPlatformFile::GetTypeName()));
			return PlatformFile ? PlatformFile : LowerLevel;
		}
		return PakPlatformFile ? PakPlatformFile : LowerLevel;
	}

	/**
	 * Checks if the given
	 * @param OriginalFilename 
	 * @return 
	 */
	static FString GetPakAdjustedFilename(const TCHAR* OriginalFilename, bool* bFoundInPak = nullptr);
	
	// IPlatformFile
	virtual bool ShouldBeUsed(IPlatformFile* Inner, const TCHAR* CmdLine) const override { return true; }
	virtual bool Initialize(IPlatformFile* Inner, const TCHAR* CmdLine) override;
	virtual IPlatformFile* GetLowerLevel() override
	{
		return IsEngineExitRequested() ? LowerLevel : GetPlatformFile();
	}
	virtual void SetLowerLevel(IPlatformFile* NewLowerLevel) override { LowerLevel = NewLowerLevel; }
	virtual const TCHAR* GetName() const override { return FGFPakLoaderPlatformFile::GetTypeName(); }
	virtual void Tick() override;
	
	virtual bool FileExists(const TCHAR* Filename) override;
	virtual int64 FileSize(const TCHAR* Filename) override;
	virtual bool IsReadOnly(const TCHAR* Filename) override;
	virtual bool DeleteFile(const TCHAR* Filename) override;
	virtual bool MoveFile(const TCHAR* To, const TCHAR* From) override;
	virtual bool SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue) override;
	virtual FDateTime GetTimeStamp(const TCHAR* Filename) override;
	virtual void SetTimeStamp(const TCHAR* Filename, FDateTime DateTime) override;
	virtual FDateTime GetAccessTimeStamp(const TCHAR* Filename) override;
	virtual FString GetFilenameOnDisk(const TCHAR* Filename) override;
	virtual FFileOpenResult OpenRead(const TCHAR* Filename, EOpenReadFlags Flags) override;
	virtual IFileHandle* OpenRead(const TCHAR* Filename, bool bAllowWrite = false) override;
	virtual IFileHandle* OpenReadNoBuffering(const TCHAR* Filename, bool bAllowWrite = false) override;
	virtual FFileOpenAsyncResult OpenAsyncRead(const TCHAR* Filename, EOpenReadFlags Flags) override;
	virtual IAsyncReadFileHandle* OpenAsyncRead(const TCHAR* Filename, bool bAllowWrite = false) override;
	virtual IMappedFileHandle* OpenMapped(const TCHAR* Filename) override;
	virtual IFileHandle* OpenWrite(const TCHAR* Filename, bool bAppend = false, bool bAllowRead = false) override;
	virtual bool DirectoryExists(const TCHAR* Directory) override;
	virtual bool CreateDirectory(const TCHAR* Directory) override;
	virtual bool DeleteDirectory(const TCHAR* Directory) override;
	virtual FFileStatData GetStatData(const TCHAR* FilenameOrDirectory) override;
	virtual bool IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor) override;
	virtual bool IterateDirectory(const TCHAR* Directory, FDirectoryVisitorFunc Visitor) override;
	virtual bool IterateDirectoryRecursively(const TCHAR* Directory, FDirectoryVisitor& Visitor) override;
	virtual bool IterateDirectoryRecursively(const TCHAR* Directory, FDirectoryVisitorFunc Visitor) override;
	virtual bool IterateDirectoryStat(const TCHAR* Directory, FDirectoryStatVisitor& Visitor) override;
	virtual bool IterateDirectoryStat(const TCHAR* Directory, FDirectoryStatVisitorFunc Visitor) override;
	virtual bool IterateDirectoryStatRecursively(const TCHAR* Directory, FDirectoryStatVisitor& Visitor) override;
	virtual bool IterateDirectoryStatRecursively(const TCHAR* Directory, FDirectoryStatVisitorFunc Visitor) override;
	virtual void AddLocalDirectories(TArray<FString>& LocalDirectories) override;
	virtual void BypassSecurity(bool bInBypass) override;
	virtual FString ConvertToAbsolutePathForExternalAppForRead(const TCHAR* Filename) override;
	virtual FString ConvertToAbsolutePathForExternalAppForWrite(const TCHAR* Filename) override;
	virtual bool CopyDirectoryTree(const TCHAR* DestinationDirectory, const TCHAR* Source, bool bOverwriteAllExisting) override;
	virtual bool CopyFile(const TCHAR* To, const TCHAR* From, EPlatformFileRead ReadFlags, EPlatformFileWrite WriteFlags) override;
	virtual bool CreateDirectoryTree(const TCHAR* Directory) override;
	virtual bool DeleteDirectoryRecursively(const TCHAR* Directory) override;
	virtual bool DoesCreatePublicFiles() override;
	virtual void FindFiles(TArray<FString>& FoundFiles, const TCHAR* Directory, const TCHAR* FileExtension) override;
	virtual void FindFilesRecursively(TArray<FString>& FoundFiles, const TCHAR* Directory, const TCHAR* FileExtension) override;
	virtual int64 GetAllowedBytesToWriteThrottledStorage(const TCHAR* DestinationPath) override;
	virtual FDateTime GetTimeStampLocal(const TCHAR* Filename) override;
	virtual void GetTimeStampPair(const TCHAR* PathA, const TCHAR* PathB, FDateTime& OutTimeStampA, FDateTime& OutTimeStampB) override;
	virtual bool HasMarkOfTheWeb(FStringView Filename, FString* OutSourceURL) override;
	virtual void InitializeAfterProjectFilePath() override;
	virtual void InitializeAfterSetActive() override;
	virtual void InitializeNewAsyncIO() override;
	virtual bool IsSandboxEnabled() const override;
	virtual ESymlinkResult IsSymlink(const TCHAR* Filename) override;
	virtual void MakeUniquePakFilesForTheseFiles(const TArray<TArray<FString>>& InFiles) override;
	virtual bool SendMessageToServer(const TCHAR* Message, IFileServerMessageHandler* Handler) override;
	virtual void SetAsyncMinimumPriority(EAsyncIOPriorityAndFlags MinPriority) override;
	virtual void SetCreatePublicFiles(bool bCreatePublicFiles) override;
	virtual bool SetMarkOfTheWeb(FStringView Filename, bool bNewStatus, const FString* InSourceURL) override;
	virtual void SetSandboxEnabled(bool bInEnabled) override;
	// ~IPlatformFile
private:
	IPlatformFile* LowerLevel;
	FPakPlatformFile* PakPlatformFile;
	TArray<FString> PakMountPaths;
};
