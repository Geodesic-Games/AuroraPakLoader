// Copyright GeoTech BV


#include "GFPakLoaderPlatformFile.h"

#include "GFPakLoaderLog.h"
#include "GFPakLoaderSubsystem.h"
#include "IPlatformFilePak.h"

FString FGFPakLoaderPlatformFile::GetPakAdjustedFilename(const TCHAR* OriginalFilename, bool* bFoundInPak)
{
	if (UGFPakLoaderSubsystem* Subsystem = UGFPakLoaderSubsystem::Get())
	{
		FString Filename;
		if (UGFPakPlugin* Plugin = Subsystem->FindMountedPakContainingFile(OriginalFilename, &Filename))
		{
			UE_LOG(LogGFPakLoader, VeryVerbose, TEXT(" ... GetPakAdjustedFilename FOUND ( `%s` ) => `%s`"), OriginalFilename, *Filename)
			if (bFoundInPak)
			{
				*bFoundInPak = true;
			}
			return Filename;
		}
	}
	
	UE_LOG(LogGFPakLoader, VeryVerbose, TEXT(" ... GetPakAdjustedFilename NOT FOUND( `%s` )"), OriginalFilename)
	
	if (bFoundInPak)
	{
		*bFoundInPak = false;
	}
	return {OriginalFilename};
}

bool FGFPakLoaderPlatformFile::Initialize(IPlatformFile* Inner, const TCHAR* CmdLine)
{
	LowerLevel = Inner;
	if (!PakPlatformFile)
	{
		PakPlatformFile = static_cast<FPakPlatformFile*>(FPlatformFileManager::Get().FindPlatformFile(FPakPlatformFile::GetTypeName()));
		if (!PakPlatformFile)
		{
			// Create a pak platform file and mount the feature pack file.
			PakPlatformFile = static_cast<FPakPlatformFile*>(FPlatformFileManager::Get().GetPlatformFile(FPakPlatformFile::GetTypeName()));
			PakPlatformFile->Initialize(Inner, CmdLine);
			PakPlatformFile->InitializeNewAsyncIO(); // needed in Game builds to ensure PakPrecacherSingleton is valid
		}
	}
	return true;
}

void FGFPakLoaderPlatformFile::Tick()
{
	if (PakPlatformFile && PakPlatformFile == GetPlatformFile())
	{
		PakPlatformFile->Tick();
	}
	if (ensure(LowerLevel))
	{
		LowerLevel->Tick();
	}
}

/**
 * Macro to call the given Function on the right PlatformFile, depending if the file is within a pak or not.
 * ex: CALL_PAK_PLATFORM_FILE_FIRST_ON_FILE(bool, false, FileExists(Filename))
 * todo: Currently we are trying first to call the function in the PakPlatform file and if not successful, call it in the Inner platform file which is not the most efficient. Check if this can be improved
 * todo: also do not bother if no paks are loaded, go straight to the Lowerlevel?
 * @param Type The type of the return value of the Function
 * @param DefaultValue The DefaultValue to return if no PlatformFile is valid
 * @param Function The complete function call to pass to PlatformFile->
 */
#define CALL_PAK_PLATFORM_FILE_FIRST_ON_FILE(Type, DefaultValue, Function) \
	Type Value = DefaultValue; \
	if (IPlatformFile* PlatformFile = GetPlatformFile(Filename)) \
	{ \
		FString AdjustedFilename = GetPakAdjustedFilename(Filename); \
		Filename = *AdjustedFilename; \
		Value = PlatformFile->Function; \
		if (Value == DefaultValue && LowerLevel != nullptr && PakPlatformFile == PlatformFile) \
		{ \
			Value = LowerLevel->Function; \
		} \
	} \
	return Value;

bool FGFPakLoaderPlatformFile::FileExists(const TCHAR* Filename)
{
	CALL_PAK_PLATFORM_FILE_FIRST_ON_FILE(bool, false, FileExists(Filename))
}
int64 FGFPakLoaderPlatformFile::FileSize(const TCHAR* Filename)
{
	CALL_PAK_PLATFORM_FILE_FIRST_ON_FILE(int64, -1LL, FileSize(Filename))
}
bool FGFPakLoaderPlatformFile::IsReadOnly(const TCHAR* Filename)
{
	CALL_PAK_PLATFORM_FILE_FIRST_ON_FILE(bool, false, IsReadOnly(Filename))
}
bool FGFPakLoaderPlatformFile::DeleteFile(const TCHAR* Filename)
{
	CALL_PAK_PLATFORM_FILE_FIRST_ON_FILE(bool, false, DeleteFile(Filename))
}
bool FGFPakLoaderPlatformFile::MoveFile(const TCHAR* To, const TCHAR* Filename)
{
	CALL_PAK_PLATFORM_FILE_FIRST_ON_FILE(bool, false, MoveFile(To, Filename))
}
bool FGFPakLoaderPlatformFile::SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue)
{
	CALL_PAK_PLATFORM_FILE_FIRST_ON_FILE(bool, false, SetReadOnly(Filename, bNewReadOnlyValue))
}
FDateTime FGFPakLoaderPlatformFile::GetTimeStamp(const TCHAR* Filename)
{
	CALL_PAK_PLATFORM_FILE_FIRST_ON_FILE(FDateTime, FDateTime::MinValue(), GetTimeStamp(Filename))
}
void FGFPakLoaderPlatformFile::SetTimeStamp(const TCHAR* Filename, FDateTime DateTime)
{
	if (IPlatformFile* PlatformFile = GetPlatformFile(Filename))
	{
		PlatformFile->SetTimeStamp(Filename, DateTime);
		if (LowerLevel != nullptr && PakPlatformFile == PlatformFile)
		{
			LowerLevel->SetTimeStamp(Filename, DateTime);
		}
	}
}
FDateTime FGFPakLoaderPlatformFile::GetAccessTimeStamp(const TCHAR* Filename)
{
	CALL_PAK_PLATFORM_FILE_FIRST_ON_FILE(FDateTime, FDateTime::MinValue(), GetAccessTimeStamp(Filename))
}
FString FGFPakLoaderPlatformFile::GetFilenameOnDisk(const TCHAR* Filename)
{
	CALL_PAK_PLATFORM_FILE_FIRST_ON_FILE(FString, FString{}, GetFilenameOnDisk(Filename))
}
FFileOpenResult FGFPakLoaderPlatformFile::OpenRead(const TCHAR* Filename, EOpenReadFlags Flags)
{
	if (IPlatformFile* PlatformFile = GetPlatformFile(Filename))
	{
		const FString AdjustedFilename = GetPakAdjustedFilename(Filename);
		Filename = *AdjustedFilename;
		FFileOpenResult Value = PlatformFile->OpenRead(Filename, Flags);
		if (Value.HasError() && LowerLevel != nullptr && PakPlatformFile == PlatformFile)
		{
			Value = LowerLevel->OpenRead(Filename, Flags);
		}
		return Value;
	}
	return MakeError(TEXTVIEW("Unable to get the Platform File"));;
}
IFileHandle* FGFPakLoaderPlatformFile::OpenRead(const TCHAR* Filename, bool bAllowWrite)
{
	UE_LOG(LogGFPakLoader, VeryVerbose, TEXT(" ... FGFPakLoaderPlatformFile::OpenRead ( `%s` )"), Filename)
	// todo: as the macro calls GetPakAdjustedFilename which ends up calling FindMountedPakContainingFile,
	// we are already have a handle of the right plugin containing the file, could we somehow use it here and in the other functions? 
	CALL_PAK_PLATFORM_FILE_FIRST_ON_FILE(IFileHandle*, nullptr, OpenRead(Filename, bAllowWrite))
}

FFileOpenAsyncResult FGFPakLoaderPlatformFile::OpenAsyncRead(const TCHAR* Filename, EOpenReadFlags Flags)
{
	if (IPlatformFile* PlatformFile = GetPlatformFile(Filename))
	{
		const FString AdjustedFilename = GetPakAdjustedFilename(Filename);
		Filename = *AdjustedFilename;
		FFileOpenAsyncResult Value = PlatformFile->OpenAsyncRead(Filename, Flags);
		if (Value.HasError() && LowerLevel != nullptr && PakPlatformFile == PlatformFile)
		{
			Value = LowerLevel->OpenAsyncRead(Filename, Flags);
		}
		return Value;
	}
	return MakeError(TEXTVIEW("Unable to get the Platform File"));;
}

IAsyncReadFileHandle* FGFPakLoaderPlatformFile::OpenAsyncRead(const TCHAR* Filename, bool bAllowWrite)
{
	// We need to be careful in OpenAsyncRead because it works a bit differently than what we might expect.
	// Calling `PakPlatformFile->OpenAsyncRead` in Editor always ends up calling IPlatformFile::OpenAsyncRead(Filename) without calling the function on the LowerLevel.
	// This is needed for files within a Pak folder but if the file is outside a Pak, the behaviour might not work.
	// For example, in Game build, the LowerLevel might be a FSandboxPlatformFile, which needs to tweak the given filepath for the path to be found.
	// This works as expected in FileExists as we end up calling LowerLevel::FileExists, but if we try to open the same file via OpenAsyncRead,
	// the PakPlatformFile will never call FSandboxPlatformFile::OpenAsyncRead and will just return the handle of the given filename that will not exist at that path
	IAsyncReadFileHandle* Value = nullptr;
	if (IPlatformFile* PlatformFile = GetPlatformFile(Filename))
	{
		bool bFoundInPak;
		FString AdjustedFilename = GetPakAdjustedFilename(Filename, &bFoundInPak);
		Filename = *AdjustedFilename;
		
		if (bFoundInPak && PakPlatformFile == PlatformFile)
		{
			UE_LOG(LogGFPakLoader, VeryVerbose, TEXT(" ... FGFPakLoaderPlatformFile::OpenAsyncRead ( `%s` )  =>  PakPlatformFile"), Filename)
			return PakPlatformFile->OpenAsyncRead(Filename, bAllowWrite);
		}
		UE_LOG(LogGFPakLoader, VeryVerbose, TEXT(" ... FGFPakLoaderPlatformFile::OpenAsyncRead ( `%s` )"), Filename)
		return LowerLevel->OpenAsyncRead(Filename, bAllowWrite);
	}
	return Value;
}
IMappedFileHandle* FGFPakLoaderPlatformFile::OpenMapped(const TCHAR* Filename)
{
	CALL_PAK_PLATFORM_FILE_FIRST_ON_FILE(IMappedFileHandle*, nullptr, OpenMapped(Filename))
}
IFileHandle* FGFPakLoaderPlatformFile::OpenReadNoBuffering(const TCHAR* Filename, bool bAllowWrite)
{
	CALL_PAK_PLATFORM_FILE_FIRST_ON_FILE(IFileHandle*, nullptr, OpenReadNoBuffering(Filename, bAllowWrite))
}
IFileHandle* FGFPakLoaderPlatformFile::OpenWrite(const TCHAR* Filename, bool bAppend, bool bAllowRead)
{
	CALL_PAK_PLATFORM_FILE_FIRST_ON_FILE(IFileHandle*, nullptr, OpenWrite(Filename, bAppend, bAllowRead))
}
bool FGFPakLoaderPlatformFile::DirectoryExists(const TCHAR* Directory)
{
	IPlatformFile* PlatformFile = GetPlatformFile();
	return PlatformFile ? PlatformFile->DirectoryExists(Directory) : false;
}
bool FGFPakLoaderPlatformFile::CreateDirectory(const TCHAR* Directory)
{
	IPlatformFile* PlatformFile = GetPlatformFile();
	return PlatformFile ? PlatformFile->CreateDirectory(Directory) : false;
}
bool FGFPakLoaderPlatformFile::DeleteDirectory(const TCHAR* Directory)
{
	IPlatformFile* PlatformFile = GetPlatformFile();
	return PlatformFile ? PlatformFile->DeleteDirectory(Directory) : false;
}
FFileStatData FGFPakLoaderPlatformFile::GetStatData(const TCHAR* FilenameOrDirectory)
{
	if (IPlatformFile* PlatformFile = GetPlatformFile())
	{
		return PlatformFile->GetStatData(FilenameOrDirectory);
	}
	return {};
}
bool FGFPakLoaderPlatformFile::IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor)
{
	IPlatformFile* PlatformFile = GetPlatformFile();
	return PlatformFile ? PlatformFile->IterateDirectory(Directory, Visitor) : false;
}
bool FGFPakLoaderPlatformFile::IterateDirectory(const TCHAR* Directory, FDirectoryVisitorFunc Visitor)
{
	IPlatformFile* PlatformFile = GetPlatformFile();
	return PlatformFile ? PlatformFile->IterateDirectory(Directory, Visitor) : false;
}
bool FGFPakLoaderPlatformFile::IterateDirectoryRecursively(const TCHAR* Directory, FDirectoryVisitor& Visitor)
{
	IPlatformFile* PlatformFile = GetPlatformFile();
	return PlatformFile ? PlatformFile->IterateDirectoryRecursively(Directory, Visitor) : false;
}
bool FGFPakLoaderPlatformFile::IterateDirectoryRecursively(const TCHAR* Directory, FDirectoryVisitorFunc Visitor)
{
	IPlatformFile* PlatformFile = GetPlatformFile();
	return PlatformFile ? PlatformFile->IterateDirectoryRecursively(Directory, Visitor) : false;
}

bool FGFPakLoaderPlatformFile::IterateDirectoryStat(const TCHAR* Directory, FDirectoryStatVisitor& Visitor)
{
	IPlatformFile* PlatformFile = GetPlatformFile();
	return PlatformFile ? PlatformFile->IterateDirectoryStat(Directory, Visitor) : false;
}
bool FGFPakLoaderPlatformFile::IterateDirectoryStat(const TCHAR* Directory, FDirectoryStatVisitorFunc Visitor)
{
	IPlatformFile* PlatformFile = GetPlatformFile();
	return PlatformFile ? PlatformFile->IterateDirectoryStat(Directory, Visitor) : false;
}
bool FGFPakLoaderPlatformFile::IterateDirectoryStatRecursively(const TCHAR* Directory, FDirectoryStatVisitor& Visitor)
{
	IPlatformFile* PlatformFile = GetPlatformFile();
	return PlatformFile ? PlatformFile->IterateDirectoryStatRecursively(Directory, Visitor) : false;
}
bool FGFPakLoaderPlatformFile::IterateDirectoryStatRecursively(const TCHAR* Directory, FDirectoryStatVisitorFunc Visitor)
{
	IPlatformFile* PlatformFile = GetPlatformFile();
	return PlatformFile ? PlatformFile->IterateDirectoryStatRecursively(Directory, Visitor) : false;
}

void FGFPakLoaderPlatformFile::AddLocalDirectories(TArray<FString>& LocalDirectories)
{
	if (IPlatformFile* PlatformFile = GetPlatformFile())
	{
		PlatformFile->AddLocalDirectories(LocalDirectories);
	}
}
void FGFPakLoaderPlatformFile::BypassSecurity(bool bInBypass)
{
	if (IPlatformFile* PlatformFile = GetPlatformFile())
	{
		PlatformFile->BypassSecurity(bInBypass);
	}
}
FString FGFPakLoaderPlatformFile::ConvertToAbsolutePathForExternalAppForRead(const TCHAR* Filename)
{
	CALL_PAK_PLATFORM_FILE_FIRST_ON_FILE(FString, FString{}, ConvertToAbsolutePathForExternalAppForRead(Filename))
}
FString FGFPakLoaderPlatformFile::ConvertToAbsolutePathForExternalAppForWrite(const TCHAR* Filename)
{
	CALL_PAK_PLATFORM_FILE_FIRST_ON_FILE(FString, FString{}, ConvertToAbsolutePathForExternalAppForWrite(Filename))
}
bool FGFPakLoaderPlatformFile::CopyDirectoryTree(const TCHAR* DestinationDirectory, const TCHAR* Source, bool bOverwriteAllExisting)
{
	IPlatformFile* PlatformFile = GetPlatformFile();
	return PlatformFile ? PlatformFile->CopyDirectoryTree(DestinationDirectory, Source, bOverwriteAllExisting) : false;
}
bool FGFPakLoaderPlatformFile::CopyFile(const TCHAR* To, const TCHAR* From, EPlatformFileRead ReadFlags, EPlatformFileWrite WriteFlags)
{
	IPlatformFile* PlatformFile = GetPlatformFile();
	return PlatformFile ? PlatformFile->CopyFile(To, From, ReadFlags, WriteFlags) : false;
}
bool FGFPakLoaderPlatformFile::CreateDirectoryTree(const TCHAR* Directory)
{
	IPlatformFile* PlatformFile = GetPlatformFile();
	return PlatformFile ? PlatformFile->CreateDirectoryTree(Directory) : false;
}
bool FGFPakLoaderPlatformFile::DeleteDirectoryRecursively(const TCHAR* Directory)
{
	IPlatformFile* PlatformFile = GetPlatformFile();
	return PlatformFile ? PlatformFile->DeleteDirectoryRecursively(Directory) : false;
}
bool FGFPakLoaderPlatformFile::DoesCreatePublicFiles()
{
	IPlatformFile* PlatformFile = GetPlatformFile();
	return PlatformFile ? PlatformFile->DoesCreatePublicFiles() : false;
}
void FGFPakLoaderPlatformFile::FindFiles(TArray<FString>& FoundFiles, const TCHAR* Directory, const TCHAR* FileExtension)
{
	if (IPlatformFile* PlatformFile = GetPlatformFile())
	{
		PlatformFile->FindFiles(FoundFiles, Directory, FileExtension);
	}
}
void FGFPakLoaderPlatformFile::FindFilesRecursively(TArray<FString>& FoundFiles, const TCHAR* Directory, const TCHAR* FileExtension)
{
	if (IPlatformFile* PlatformFile = GetPlatformFile())
	{
		PlatformFile->FindFilesRecursively(FoundFiles, Directory, FileExtension);
	}
}
int64 FGFPakLoaderPlatformFile::GetAllowedBytesToWriteThrottledStorage(const TCHAR* DestinationPath)
{
	IPlatformFile* PlatformFile = GetPlatformFile();
	return PlatformFile ? PlatformFile->GetAllowedBytesToWriteThrottledStorage(DestinationPath) : -1LL;
}
FDateTime FGFPakLoaderPlatformFile::GetTimeStampLocal(const TCHAR* Filename)
{
	CALL_PAK_PLATFORM_FILE_FIRST_ON_FILE(FDateTime, FDateTime::MinValue(), GetTimeStampLocal(Filename))
}
void FGFPakLoaderPlatformFile::GetTimeStampPair(const TCHAR* PathA, const TCHAR* PathB, FDateTime& OutTimeStampA, FDateTime& OutTimeStampB)
{
	OutTimeStampA = FDateTime::MinValue();
	OutTimeStampB = FDateTime::MinValue();
	if (IPlatformFile* PlatformFile = GetPlatformFile())
	{
		PlatformFile->GetTimeStampPair(PathA, PathB, OutTimeStampA,OutTimeStampB);
	}
}
bool FGFPakLoaderPlatformFile::HasMarkOfTheWeb(FStringView FilenameView, FString* OutSourceURL)
{
	const TCHAR* Filename = FilenameView.GetData();
	CALL_PAK_PLATFORM_FILE_FIRST_ON_FILE(bool, false, HasMarkOfTheWeb(Filename))
}
void FGFPakLoaderPlatformFile::InitializeAfterProjectFilePath()
{
	if (PakPlatformFile && PakPlatformFile == GetPlatformFile())
	{
		PakPlatformFile->InitializeAfterProjectFilePath();
	}
	if (ensure(LowerLevel))
	{
		LowerLevel->InitializeAfterProjectFilePath();
	}
}
void FGFPakLoaderPlatformFile::InitializeAfterSetActive()
{
	if (PakPlatformFile && PakPlatformFile == GetPlatformFile())
	{
		PakPlatformFile->InitializeAfterSetActive();
	}
	if (ensure(LowerLevel))
	{
		LowerLevel->InitializeAfterSetActive();
	}
}
void FGFPakLoaderPlatformFile::InitializeNewAsyncIO()
{
	if (PakPlatformFile && PakPlatformFile == GetPlatformFile())
	{
		PakPlatformFile->InitializeNewAsyncIO();
	}
	if (ensure(LowerLevel))
	{
		LowerLevel->InitializeNewAsyncIO();
	}
}
bool FGFPakLoaderPlatformFile::IsSandboxEnabled() const
{
	IPlatformFile* PlatformFile = GetPlatformFile();
	return PlatformFile ? PlatformFile->IsSandboxEnabled() : false;
}
ESymlinkResult FGFPakLoaderPlatformFile::IsSymlink(const TCHAR* Filename)
{
	CALL_PAK_PLATFORM_FILE_FIRST_ON_FILE(ESymlinkResult, ESymlinkResult::Unimplemented, IsSymlink(Filename))
}
void FGFPakLoaderPlatformFile::MakeUniquePakFilesForTheseFiles(const TArray<TArray<FString>>& InFiles)
{
	if (IPlatformFile* PlatformFile = GetPlatformFile())
	{
		PlatformFile->MakeUniquePakFilesForTheseFiles(InFiles);
	}
}
bool FGFPakLoaderPlatformFile::SendMessageToServer(const TCHAR* Message, IFileServerMessageHandler* Handler)
{
	IPlatformFile* PlatformFile = GetPlatformFile();
	return PlatformFile ? PlatformFile->SendMessageToServer(Message, Handler) : false;
}
void FGFPakLoaderPlatformFile::SetAsyncMinimumPriority(EAsyncIOPriorityAndFlags MinPriority)
{
	if (PakPlatformFile && PakPlatformFile == GetPlatformFile())
	{
		PakPlatformFile->SetAsyncMinimumPriority(MinPriority);
	}
	if (ensure(LowerLevel))
	{
		LowerLevel->SetAsyncMinimumPriority(MinPriority);
	}
}
void FGFPakLoaderPlatformFile::SetCreatePublicFiles(bool bCreatePublicFiles)
{
	if (PakPlatformFile && PakPlatformFile == GetPlatformFile())
	{
		PakPlatformFile->SetCreatePublicFiles(bCreatePublicFiles);
	}
	if (ensure(LowerLevel))
	{
		LowerLevel->SetCreatePublicFiles(bCreatePublicFiles);
	}
}
bool FGFPakLoaderPlatformFile::SetMarkOfTheWeb(FStringView FilenameView, bool bNewStatus, const FString* InSourceURL)
{
	const TCHAR* Filename = FilenameView.GetData();
	CALL_PAK_PLATFORM_FILE_FIRST_ON_FILE(bool, false, SetMarkOfTheWeb(Filename, bNewStatus, InSourceURL))
}
void FGFPakLoaderPlatformFile::SetSandboxEnabled(bool bInEnabled)
{
	if (PakPlatformFile && PakPlatformFile == GetPlatformFile())
	{
		PakPlatformFile->SetSandboxEnabled(bInEnabled);
	}
	if (ensure(LowerLevel))
	{
		LowerLevel->SetSandboxEnabled(bInEnabled);
	}
}

#undef CALL_PAK_PLATFORM_FILE_FIRST_ON_FILE