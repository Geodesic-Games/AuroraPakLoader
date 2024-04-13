// Copyright GeoTech BV


#include "GFPakLoaderPlatformFile.h"

#include "IPlatformFilePak.h"

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
			PakPlatformFile->Initialize(Inner, TEXT("")); // TEXT("-UseIoStore")
			PakPlatformFile->InitializeNewAsyncIO(); // needed in Game builds to ensure PakPrecacherSingleton is valid
			// FPlatformFileManager::Get().SetPlatformFile(*PakPlatformFile);
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


bool FGFPakLoaderPlatformFile::FileExists(const TCHAR* Filename)
{
	IPlatformFile* PlatformFile = GetPlatformFile(Filename);
	return PlatformFile ? PlatformFile->FileExists(Filename) : false;
}
int64 FGFPakLoaderPlatformFile::FileSize(const TCHAR* Filename)
{
	IPlatformFile* PlatformFile = GetPlatformFile(Filename);
	return PlatformFile ? PlatformFile->FileSize(Filename) : -1LL;
}
bool FGFPakLoaderPlatformFile::IsReadOnly(const TCHAR* Filename)
{
	IPlatformFile* PlatformFile = GetPlatformFile(Filename);
	return PlatformFile ? PlatformFile->IsReadOnly(Filename) : false;
}
bool FGFPakLoaderPlatformFile::DeleteFile(const TCHAR* Filename)
{
	IPlatformFile* PlatformFile = GetPlatformFile(Filename);
	return PlatformFile ? PlatformFile->DeleteFile(Filename) : false;
}
bool FGFPakLoaderPlatformFile::MoveFile(const TCHAR* To, const TCHAR* From)
{
	IPlatformFile* PlatformFile = GetPlatformFile(From);
	return PlatformFile ? PlatformFile->MoveFile(To, From) : false;
}
bool FGFPakLoaderPlatformFile::SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue)
{
	IPlatformFile* PlatformFile = GetPlatformFile(Filename);
	return PlatformFile ? PlatformFile->SetReadOnly(Filename, bNewReadOnlyValue) : false;
}
FDateTime FGFPakLoaderPlatformFile::GetTimeStamp(const TCHAR* Filename)
{
	IPlatformFile* PlatformFile = GetPlatformFile(Filename);
	return PlatformFile ? PlatformFile->GetTimeStamp(Filename) : FDateTime::MinValue();
}
void FGFPakLoaderPlatformFile::SetTimeStamp(const TCHAR* Filename, FDateTime DateTime)
{
	if (IPlatformFile* PlatformFile = GetPlatformFile(Filename))
	{
		PlatformFile->SetTimeStamp(Filename, DateTime);
	}
}
FDateTime FGFPakLoaderPlatformFile::GetAccessTimeStamp(const TCHAR* Filename)
{
	IPlatformFile* PlatformFile = GetPlatformFile(Filename);
	return PlatformFile ? PlatformFile->GetAccessTimeStamp(Filename) : FDateTime::MinValue();
}
FString FGFPakLoaderPlatformFile::GetFilenameOnDisk(const TCHAR* Filename)
{
	IPlatformFile* PlatformFile = GetPlatformFile(Filename);
	return PlatformFile ? PlatformFile->GetFilenameOnDisk(Filename) : FString{};
}
IFileHandle* FGFPakLoaderPlatformFile::OpenRead(const TCHAR* Filename, bool bAllowWrite)
{
	IPlatformFile* PlatformFile = GetPlatformFile(Filename);
	return PlatformFile ? PlatformFile->OpenRead(Filename, bAllowWrite) : nullptr;
}
IAsyncReadFileHandle* FGFPakLoaderPlatformFile::OpenAsyncRead(const TCHAR* Filename)
{
	IPlatformFile* PlatformFile = GetPlatformFile(Filename);
	return PlatformFile ? PlatformFile->OpenAsyncRead(Filename) : nullptr;
}
IMappedFileHandle* FGFPakLoaderPlatformFile::OpenMapped(const TCHAR* Filename)
{
	IPlatformFile* PlatformFile = GetPlatformFile(Filename);
	return PlatformFile ? PlatformFile->OpenMapped(Filename) : nullptr;
}
IFileHandle* FGFPakLoaderPlatformFile::OpenReadNoBuffering(const TCHAR* Filename, bool bAllowWrite)
{
	IPlatformFile* PlatformFile = GetPlatformFile(Filename);
	return PlatformFile ? PlatformFile->OpenReadNoBuffering(Filename, bAllowWrite) : nullptr;
}
IFileHandle* FGFPakLoaderPlatformFile::OpenWrite(const TCHAR* Filename, bool bAppend, bool bAllowRead)
{
	IPlatformFile* PlatformFile = GetPlatformFile(Filename);
	return PlatformFile ? PlatformFile->OpenWrite(Filename, bAppend, bAllowRead) : nullptr;
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
	IPlatformFile* PlatformFile = GetPlatformFile(Filename);
	return PlatformFile ? PlatformFile->ConvertToAbsolutePathForExternalAppForRead(Filename) : FString{};
}
FString FGFPakLoaderPlatformFile::ConvertToAbsolutePathForExternalAppForWrite(const TCHAR* Filename)
{
	IPlatformFile* PlatformFile = GetPlatformFile(Filename);
	return PlatformFile ? PlatformFile->ConvertToAbsolutePathForExternalAppForWrite(Filename) : FString{};
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
	IPlatformFile* PlatformFile = GetPlatformFile(Filename);
	return PlatformFile ? PlatformFile->GetTimeStampLocal(Filename) : FDateTime::MinValue();
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
bool FGFPakLoaderPlatformFile::HasMarkOfTheWeb(FStringView Filename, FString* OutSourceURL)
{
	IPlatformFile* PlatformFile = GetPlatformFile(Filename.GetData());
	return PlatformFile ? PlatformFile->HasMarkOfTheWeb(Filename) : false;
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
	IPlatformFile* PlatformFile = GetPlatformFile(Filename);
	return PlatformFile ? PlatformFile->IsSymlink(Filename) : ESymlinkResult::Unimplemented;
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
bool FGFPakLoaderPlatformFile::SetMarkOfTheWeb(FStringView Filename, bool bNewStatus, const FString* InSourceURL)
{
	IPlatformFile* PlatformFile = GetPlatformFile(Filename.GetData());
	return PlatformFile ? PlatformFile->SetMarkOfTheWeb(Filename, bNewStatus, InSourceURL) : false;
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

