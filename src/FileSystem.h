#pragma once
#include "Core.h"
#include "StringPool.h"
#include "CookingSystemIDs.h"

#include <thread>
#include <semaphore>
#include <optional>

// Forward declarations.
struct FileID;
struct FileInfo;
struct FileRepo;
struct FileDrive;
struct FileSystem;
struct FileTime;
struct SystemTime;
struct LocalTime;


static constexpr int cFileRepoIndexBits = 6;
static constexpr int cFileIndexBits     = 26;

static constexpr uint32 cMaxFileRepos   = (1u << cFileRepoIndexBits) - 1;
static constexpr uint32 cMaxFilePerRepo = (1u << cFileIndexBits) - 1;



// Wrapper for a HANDLE that closes it on destruction.
struct OwnedHandle : NoCopy
{
	static constexpr void* cInvalid = (void*)-1;

	OwnedHandle()									= default;
	OwnedHandle(void* inHandle)						{ mHandle = inHandle; }
	~OwnedHandle();									// Close the handle.
	OwnedHandle(OwnedHandle&& ioOther)				{ mHandle = ioOther.mHandle; ioOther.mHandle = cInvalid; }
	OwnedHandle& operator=(OwnedHandle&& ioOther)	{ mHandle = ioOther.mHandle; ioOther.mHandle = cInvalid; return *this; }

	operator void*() const							{ return mHandle; }
	bool IsValid() const							{ return mHandle != cInvalid; }

	void* mHandle = cInvalid;
};


// Forward declarations of Win32 types.
struct _FILE_ID_128;
struct _FILETIME;
struct _SYSTEMTIME;
using USN = int64;

constexpr USN cMaxUSN = 9223372036854775807;


// Alias for FILE_ID_128.
struct FileRefNumber
{
	uint64 mData[2]                                                          = { (uint64)-1, (uint64)-1 };

	constexpr FileRefNumber()                                                = default;
	constexpr FileRefNumber(const FileRefNumber&)                            = default;
	constexpr ~FileRefNumber()                                               = default;
	constexpr FileRefNumber& operator=(const FileRefNumber&)                 = default;
	constexpr auto           operator<=>(const FileRefNumber& inOther) const = default;

	// Conversion to/from FILE_ID_128.
	FileRefNumber(const _FILE_ID_128& inFileID128) { *this = inFileID128; }
	_FILE_ID_128                   ToWin32() const;
	FileRefNumber&                 operator=(const _FILE_ID_128&);

	constexpr bool                 IsValid() const { return *this != cInvalid(); }
	static constexpr FileRefNumber cInvalid() { return {}; }
};
static_assert(sizeof(FileRefNumber) == 16);

template <> struct ankerl::unordered_dense::hash<FileRefNumber> : MemoryHasher<FileRefNumber> {};


// Alias for FILETIME.
struct FileTime
{
	uint64 mDateTime                                              = 0;

	constexpr FileTime()                                          = default;
	constexpr FileTime(const FileTime&)                           = default;
	constexpr ~FileTime()                                         = default;
	constexpr FileTime& operator=(const FileTime&)                = default;
	constexpr bool      operator==(const FileTime& inOther) const = default;

	// Conversion to/from FILETIME.
	FileTime(const _FILETIME& inFileTime) { *this = inFileTime; }
	FileTime(uint64 inTime) { mDateTime = inTime; }
	_FILETIME ToWin32() const;
	FileTime& operator=(const _FILETIME&);
	FileTime& operator=(uint64 inTime)
	{
		mDateTime = inTime;
		return *this;
	}

	int64                     operator-(FileTime inOther) const { return ((int64)mDateTime - (int64)inOther.mDateTime) * 100; } // Difference in nano seconds.

	SystemTime                ToSystemTime() const;
	LocalTime                 ToLocalTime() const;

	constexpr bool            IsValid() const { return *this != cInvalid(); }
	static constexpr FileTime cInvalid() { return {}; }
};
static_assert(sizeof(FileTime) == 8);


// Alias for SYSTEMTIME.
// TODO all time stuff to its own file
struct SystemTime
{
	uint16 mYear                                                      = 0;
	uint16 mMonth                                                     = 0;
	uint16 mDayOfWeek                                                 = 0;
	uint16 mDay                                                       = 0;
	uint16 mHour                                                      = 0;
	uint16 mMinute                                                    = 0;
	uint16 mSecond                                                    = 0;
	uint16 mMilliseconds                                              = 0;

	constexpr SystemTime()                                            = default;
	constexpr SystemTime(const SystemTime&)                           = default;
	constexpr ~SystemTime()                                           = default;
	constexpr SystemTime& operator=(const SystemTime&)                = default;
	constexpr bool        operator==(const SystemTime& inOther) const = default;

	// Conversion to/from SYSTEMTIME.
	SystemTime(const _SYSTEMTIME& inSystemTime) { *this = inSystemTime; }
	_SYSTEMTIME                 ToWin32() const;
	SystemTime&                 operator=(const _SYSTEMTIME&);

	FileTime                    ToFileTime() const;
	LocalTime                   ToLocalTime() const;

	constexpr bool              IsValid() const { return *this != cInvalid(); }
	static constexpr SystemTime cInvalid() { return {}; }
};
static_assert(sizeof(SystemTime) == 16);


struct LocalTime : SystemTime
{
	constexpr LocalTime()                                           = default;
	constexpr LocalTime(const LocalTime&)                           = default;
	constexpr ~LocalTime()                                          = default;
	constexpr LocalTime& operator=(const LocalTime&)                = default;
	constexpr bool       operator==(const LocalTime& inOther) const = default;

private:
	// LocalTime is the final type used, don't allow converting to anything.
	using SystemTime::SystemTime;
	using SystemTime::ToLocalTime;
	using SystemTime::ToFileTime;
};
static_assert(sizeof(SystemTime) == 16);


SystemTime gGetSystemTime();
LocalTime  gGetLocalTime();
FileTime   gGetSystemTimeAsFileTime();


// Wrapper for a 128-bits hash value.
struct Hash128
{
	uint64 mData[2] = {};

	constexpr auto operator<=>(const Hash128&) const = default;
};


template <> struct ankerl::unordered_dense::hash<Hash128>
{
	using is_avalanching = void; // mark class as high quality avalanching hash

	// Hash128 is already a good quality hash, just return the lower 8 bytes.
    uint64 operator()(const Hash128& inValue) const noexcept { return inValue.mData[0]; }
};


// Identifier for a file. 4 bytes.
struct FileID
{
	uint32                  mRepoIndex : cFileRepoIndexBits = cMaxFileRepos;
	uint32                  mFileIndex : cFileIndexBits     = cMaxFilePerRepo;

	const FileInfo&         GetFile() const; // Convenience getter for the FileInfo itself.
	const FileRepo&         GetRepo() const; // Convenience getter for the FileRepo.

	bool                    IsValid() const { return *this != cInvalid(); }
	static constexpr FileID cInvalid() { return {}; }

	uint32                  AsUInt() const
	{
		uint32 i;
		memcpy(&i, this, sizeof(*this));
		return i;
	}

	auto operator<=>(const FileID& inOther) const = default;
};
static_assert(sizeof(FileID) == 4);

template <> struct ankerl::unordered_dense::hash<FileID> : MemoryHasher<FileID> {};


enum class FileType : int
{
	File,
	Directory
};


struct FileInfo : NoCopy
{
	const FileID                  mID;                  // Our ID for this file.
	const uint16                  mNamePos;             // Position in the path of the start of the file name (after the last '/').
	const uint16                  mExtensionPos;        // Position in the path of the first '.' in the file name.
	const StringView              mPath;                // Path relative to the root directory.
	const Hash128                 mPathHash;            // Case-insensitive hash of the path.

	bool                          mIsDirectory     : 1; // Is this a directory or a file. Note: could change if a file is deleted then a directory of the same name is created.
	bool                          mCommandsCreated : 1; // Are cooking commands already created for this file.
	FileRefNumber                 mRefNumber      = {}; // File ID used by Windows. Can change when the file is deleted and re-created.
	FileTime                      mCreationTime   = {}; // Time of the creation of this file (or its deletion if the file is deleted).
	USN                           mLastChangeUSN  = 0;  // Identifier of the last change to this file.
	FileTime                      mLastChangeTime = {}; // Time of the last change to this file.

	std::vector<CookingCommandID> mInputOf;             // List of commands that use this file as input.
	std::vector<CookingCommandID> mOutputOf;            // List of commands that use this file as output. There should be only one // TODO tiny vector optimization

	bool                          IsDeleted() const { return !mRefNumber.IsValid(); }
	bool                          IsDirectory() const { return mIsDirectory != 0; }
	FileType                      GetType() const { return mIsDirectory ? FileType::Directory : FileType::File; }
	StringView                    GetName() const { return mPath.substr(mNamePos); }
	StringView                    GetNameNoExt() const { return mPath.substr(mNamePos, mExtensionPos - mNamePos); }
	StringView                    GetExtension() const { return mPath.substr(mExtensionPos); }
	StringView                    GetDirectory() const { return mPath.substr(0, mNamePos); } // Includes the trailing slash.
	const FileRepo&               GetRepo() const { return mID.GetRepo(); }

	FileInfo(FileID inID, StringView inPath, Hash128 inPathHash, FileType inType, FileRefNumber inRefNumber);
};



// Top level container for files.
struct FileRepo : NoCopy
{
	FileRepo(uint32 inIndex, StringView inName, StringView inRootPath, FileDrive& inDrive);
	~FileRepo() = default;

	FileInfo&			GetFile(FileID inFileID)	{ gAssert(inFileID.mRepoIndex == mIndex); return mFiles[inFileID.mFileIndex]; }
	FileInfo&           GetOrAddFile(StringView inPath, FileType inType, FileRefNumber inRefNumber);
	void                MarkFileDeleted(FileInfo& ioFile, FileTime inTimeStamp);
	void                MarkFileDeleted(FileInfo& ioFile, FileTime inTimeStamp, const std::unique_lock<std::mutex>& inLock);

	StringView          RemoveRootPath(StringView inFullPath);

	void                ScanDirectory(std::vector<FileID>& ioScanQueue, Span<uint8> ioBuffer);

	uint32              mIndex = 0;  // The index of this repo.
	StringView          mName;       // A named used to identify the repo.
	StringView          mRootPath;   // Absolute path to the repo. Starts with the drive letter, ends with a slash.
	FileDrive&          mDrive;      // The drive this repo is on.
	FileID              mRootDirID;  // The FileID of the root dir.

	VMemArray<FileInfo> mFiles;      // All the files in this repo.

	StringPool          mStringPool; // Pool for storing all the paths.
};


struct FileDrive : NoCopy
{
	FileDrive(char inDriveLetter);

	bool                   ProcessMonitorDirectory(Span<uint8> ioUSNBuffer, Span<uint8> ioDirScanBuffer); // Check if files changed. Return false if there were no changes.
	FileRepo*              FindRepoForPath(StringView inFullPath);                                        // Return nullptr if not in any repo.

	OwnedHandle            OpenFileByRefNumber(FileRefNumber inRefNumber) const;
	OptionalStringView     GetFullPath(const OwnedHandle& inFileHandle, MutStringView ioBuffer) const;    // Get the full path of this file, including the drive letter part.
	USN                    GetUSN(const OwnedHandle& inFileHandle) const;

	char                   mLetter = 'C';
	OwnedHandle            mHandle;           // Handle to the drive, needed to open files with ref numbers.
	uint64                 mUSNJournalID = 0; // Journal ID, needed to query the USN journal.
	USN                    mNextUSN      = 0;
	std::vector<FileRepo*> mRepos;
};



struct FileSystem : NoCopy
{
	void            AddRepo(StringView inName, StringView inRootPath);

	void            StartMonitoring(); // Only call after adding all repos.
	void            StopMonitoring();

	bool            IsMonitoringStarted() const			{ return mMonitorDirThread.joinable(); }

	FileRepo&		GetRepo(FileID inFileID)			{ return mRepos[inFileID.mRepoIndex]; }
	FileInfo&		GetFile(FileID inFileID)			{ return mRepos[inFileID.mRepoIndex].GetFile(inFileID); }

	FileRepo*       FindRepo(StringView inRepoName);	// Return nullptr if not found.

	FileID			FindFileID(FileRefNumber inRefNumber) const;		// Return an invalid FileID if not found.
	FileInfo*		FindFile(FileRefNumber inRefNumber);				// Return nullptr if not found.

	bool            CreateDirectory(FileID inFileID);

	void			KickMonitorDirectoryThread();
private:
	void            InitialScan(std::stop_token inStopToken, Span<uint8> ioBuffer);
	void			MonitorDirectoryThread(std::stop_token inStopToken);

	FileDrive&		GetOrAddDrive(char inDriveLetter);

	friend void     gDrawDebugWindow();

	using FilesByRefNumberMap = SegmentedHashMap<FileRefNumber, FileID>;
	using FilesByPathHash = SegmentedHashMap<Hash128, FileID>;

	SegmentedVector<FileRepo>  mRepos;
	SegmentedVector<FileDrive> mDrives;					// All the drives that have at least one repo on them.

	friend struct FileRepo;
	FilesByRefNumberMap      mFilesByRefNumber;         // Map to find files by ref number.
	FilesByPathHash          mFilesByPathHash;          // Map to find files by path hash.
	mutable std::mutex       mFilesMutex;				// Mutex to protect access to the maps.

	bool                     mInitialScanCompleted = false;

	std::mutex               mChangedFilesMutex;
	SegmentedHashSet<FileID> mChangedFiles;

	std::jthread             mMonitorDirThread;
	std::binary_semaphore    mMonitorDirThreadSignal = std::binary_semaphore(0);
};


inline FileSystem gFileSystem;


inline const FileInfo& FileID::GetFile() const
{
	return gFileSystem.GetFile(*this);
}


inline const FileRepo& FileID::GetRepo() const
{
	return gFileSystem.GetRepo(*this);
}


// Formatter for FileRefNumber.
template <> struct std::formatter<FileRefNumber> : std::formatter<std::string_view>
{
	auto format(FileRefNumber inRefNumber, format_context& ioCtx) const
	{
		return std::format_to(ioCtx.out(), "0x{:X}{:016X}", inRefNumber.mData[1], inRefNumber.mData[0]);
	}
};



// Formatter for FileInfo.
template <> struct std::formatter<FileInfo> : std::formatter<std::string_view>
{
	auto format(const FileInfo& inFileInfo, format_context& ioCtx) const
	{
		return std::format_to(ioCtx.out(), "{}:{}", 
			//inFileInfo.IsDirectory() ? "Dir" : "File", 
			inFileInfo.GetRepo().mName,
			inFileInfo.mPath);
	}
};


// Formatter for FileTime.
template <> struct std::formatter<FileTime> : std::formatter<std::string_view>
{
	auto format(FileTime inFileTime, format_context& ioCtx) const
	{
		if (inFileTime.IsValid())
		{
			SystemTime local_time = inFileTime.ToLocalTime();
			return std::format_to(ioCtx.out(), "{:04}/{:02}/{:02} {:02}:{:02}:{:02}", 
				local_time.mYear,
				local_time.mMonth,
				local_time.mDay,
				local_time.mHour,
				local_time.mMinute,
				local_time.mSecond);
		}
		else
		{
			return std::format_to(ioCtx.out(), "Unknown Time");
		}
	}
};

