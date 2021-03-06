// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "../FileUtils.h"
#include "../PathUtils.h"
#include "../../StringUtils.h"
#include <assert.h>
#include <utility>

#include "../../Core/WinAPI/IncludeWindows.h"

namespace Utility 
{
    static unsigned AsUnderlyingShareMode(BasicFile::ShareMode::BitField shareMode)
    {
        unsigned underlyingShareMode = 0;
        if (shareMode & BasicFile::ShareMode::Write)   { underlyingShareMode |= FILE_SHARE_WRITE; }
        if (shareMode & BasicFile::ShareMode::Read)    { underlyingShareMode |= FILE_SHARE_READ; }
        return underlyingShareMode;
    }

    namespace Internal
    {
        struct UnderlyingOpenMode
        {
            unsigned _underlyingAccessMode;
            unsigned _creationDisposition;
            unsigned _underlyingFlags;
        };
    }

    static Exceptions::IOException::Reason AsExceptionReason(DWORD dw)
    {
        switch (dw) {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
            return Exceptions::IOException::Reason::FileNotFound;

        case ERROR_ACCESS_DENIED:
            return Exceptions::IOException::Reason::AccessDenied;

        case ERROR_WRITE_PROTECT:
            return Exceptions::IOException::Reason::WriteProtect;

        default:
            return Exceptions::IOException::Reason::Complex;
        }
    }

    static Internal::UnderlyingOpenMode AsUnderlyingOpenMode(const char openMode[])
    {
        Internal::UnderlyingOpenMode result = {0,0,0};

        auto* i = openMode;
        while (*i != '\0') {
            switch (*i) {
            case 'w':
                if (*(i+1) == '+') {
                    ++i;
                    result._underlyingAccessMode = FILE_GENERIC_WRITE | FILE_GENERIC_READ;
                    result._creationDisposition = CREATE_ALWAYS;
                } else {
                    result._underlyingAccessMode = FILE_GENERIC_WRITE;
                    result._creationDisposition = CREATE_ALWAYS;
                }
                break;

            case 'r':
                if (*(i+1) == '+') {
                    ++i;
                    result._underlyingAccessMode = FILE_GENERIC_WRITE | FILE_GENERIC_READ;
                    result._creationDisposition = OPEN_EXISTING;
                } else {
                    result._underlyingAccessMode = FILE_GENERIC_READ;
                    result._creationDisposition = OPEN_EXISTING;
                }
                break;

            case 'b': break;    // binary mode -- actually the only supported mode

            case 'T':
            case 'D': result._underlyingFlags |= FILE_ATTRIBUTE_TEMPORARY; break;
            case 'R': result._underlyingFlags |= FILE_FLAG_RANDOM_ACCESS; break;
            case 'S': result._underlyingFlags |= FILE_FLAG_SEQUENTIAL_SCAN; break;
            
            case 'a':
                Throw(Exceptions::IOException(Exceptions::IOException::Reason::Complex, "Append file mode not supported"));

            case 't':
                Throw(Exceptions::IOException(Exceptions::IOException::Reason::Complex, "Text oriented file modes not supported"));

            case 'c':
                if (XlBeginsWith(MakeStringSection(i), MakeStringSection("ccs=")))
                    Throw(Exceptions::IOException(Exceptions::IOException::Reason::Complex, "Encoded text file modes supported"));
                // else, fall through...

            default:
                Throw(Exceptions::IOException(Exceptions::IOException::Reason::Complex, "Unknown characters found in open mode string (%s)", openMode));
            }
            ++i;
        }

        return result;
    }

    BasicFile::BasicFile(   const char filename[], const char openMode[], 
                            ShareMode::BitField shareMode)
    {
        assert(filename && filename[0]);
        assert(openMode);

        auto underlyingShareMode = AsUnderlyingShareMode(shareMode);
        auto underlyingOpenMode = AsUnderlyingOpenMode(openMode);

        auto handle = CreateFile(
            filename, 
            underlyingOpenMode._underlyingAccessMode,
            underlyingShareMode,
            nullptr, underlyingOpenMode._creationDisposition,
            underlyingOpenMode._underlyingFlags, nullptr);
        
        if (handle == INVALID_HANDLE_VALUE) {
                // use "FormatMessage" to get error code
                //  (as per msdn article: "Retrieving the Last-Error Code")
            LPVOID lpMsgBuf;
            DWORD dw = GetLastError(); 
            FormatMessage(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                dw,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR) &lpMsgBuf,
                0, NULL);

			// FORMAT_MESSAGE_FROM_SYSTEM will typically give us a new line
			// at the end of the string. We can strip it off here (assuming we have write
			// access to the buffer). We're going to get rid of any terminating '.' as well.
			// Note -- assuming 8 bit base character width here (ie, ASCII, UTF8)
			if (lpMsgBuf) {
				auto *end = XlStringEnd((char*)lpMsgBuf);
				while ((end - 1) > lpMsgBuf && (*(end - 1) == '\n' || *(end - 1) == '\r' || *(end-1) == '.')) {
					--end;
					*end = '\0';
				}
			}

            Exceptions::IOException except(
                AsExceptionReason(dw),
                "Failure during file open. Probably missing file or bad privileges: (%s), openMode: (%s), error string: (%s)", 
                filename, openMode, lpMsgBuf);
            LocalFree(lpMsgBuf);

            Throw(except);
        }

        _file = (void*)handle;
    }

    auto BasicFile::TryOpen(const char filename[], const char openMode[], ShareMode::BitField shareMode) never_throws -> Exceptions::IOException::Reason
    {
        assert(_file == INVALID_HANDLE_VALUE);
        assert(filename && filename[0]);
        assert(openMode);

        auto underlyingShareMode = AsUnderlyingShareMode(shareMode);
        auto underlyingOpenMode = AsUnderlyingOpenMode(openMode);

        _file = CreateFile(
            filename, 
            underlyingOpenMode._underlyingAccessMode,
            underlyingShareMode,
            nullptr, underlyingOpenMode._creationDisposition,
            underlyingOpenMode._underlyingFlags, nullptr);

        if (_file != INVALID_HANDLE_VALUE && _file != nullptr)
            return Exceptions::IOException::Reason::Success;

        return AsExceptionReason(GetLastError());
    }

    BasicFile::BasicFile(BasicFile&& moveFrom) never_throws
    {
        _file = moveFrom._file;
        moveFrom._file = INVALID_HANDLE_VALUE;
    }

    BasicFile& BasicFile::operator=(BasicFile&& moveFrom) never_throws
    {
        if (_file != INVALID_HANDLE_VALUE) {
            CloseHandle(_file);
        }
        _file = moveFrom._file;
        moveFrom._file = INVALID_HANDLE_VALUE;
        return *this;
    }

    BasicFile::BasicFile(const BasicFile& copyFrom) never_throws
    {
        _file = INVALID_HANDLE_VALUE;
        auto currentProc = GetCurrentProcess();
        auto hresult = DuplicateHandle(
            currentProc, copyFrom._file,
            currentProc, &_file,
            0, FALSE,
            DUPLICATE_SAME_ACCESS);

        if (!SUCCEEDED(hresult)) {
            LPVOID lpMsgBuf;
            DWORD dw = GetLastError(); 
            FormatMessage(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR) &lpMsgBuf, 0, NULL);

			if (lpMsgBuf) {
				auto *end = XlStringEnd((char*)lpMsgBuf);
				while ((end - 1) > lpMsgBuf && (*(end - 1) == '\n' || *(end - 1) == '\r' || *(end - 1) == '.')) {
					--end;
					*end = '\0';
				}
			}

            Exceptions::IOException except(
                AsExceptionReason(dw), 
                "Failure while attempting to duplicate file handle. Error string: (%s)", lpMsgBuf);
            LocalFree(lpMsgBuf);
            Throw(except);
        }
    }

    BasicFile& BasicFile::operator=(const BasicFile& copyFrom) never_throws
    {
        if (_file != INVALID_HANDLE_VALUE)
            CloseHandle(_file);
        _file = INVALID_HANDLE_VALUE;
        auto currentProc = GetCurrentProcess();
        auto hresult = DuplicateHandle(
            currentProc, copyFrom._file,
            currentProc, &_file,
            0, FALSE,
            DUPLICATE_SAME_ACCESS);

        if (!SUCCEEDED(hresult)) {
            LPVOID lpMsgBuf;
            DWORD dw = GetLastError(); 
            FormatMessage(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR) &lpMsgBuf, 0, NULL);

			if (lpMsgBuf) {
				auto *end = XlStringEnd((char*)lpMsgBuf);
				while ((end - 1) > lpMsgBuf && (*(end - 1) == '\n' || *(end - 1) == '\r' || *(end - 1) == '.')) {
					--end;
					*end = '\0';
				}
			}

            Exceptions::IOException except(
                AsExceptionReason(dw), 
                "Failure while attempting to duplicate file handle. Error string: (%s)", lpMsgBuf);
            LocalFree(lpMsgBuf);
            Throw(except);
        }
        return *this;
    }

    BasicFile::BasicFile()
    {
        _file = INVALID_HANDLE_VALUE;
    }

    BasicFile::~BasicFile()
    {
        if (_file != INVALID_HANDLE_VALUE) {
            CloseHandle(_file);
        }
    }

    size_t   BasicFile::Read(void *buffer, size_t size, size_t count) const never_throws
    {
        if (!(size * count)) return 0;
        DWORD bytesRead = 0;
        auto result = ReadFile(_file, buffer, DWORD(size * count), &bytesRead, nullptr);
        auto errorCode = GetLastError();
        assert((bytesRead%size)==0); (void)errorCode;
        return result?(bytesRead/size):0;
    }
    
    size_t   BasicFile::Write(const void *buffer, size_t size, size_t count) never_throws
    {
        if (!(size * count)) return 0;
        DWORD bytesWritten = 0;
        auto result = WriteFile(_file, buffer, DWORD(size * count), &bytesWritten, nullptr);
        assert((bytesWritten%size)==0);
        return result?(bytesWritten/size):0;
    }

    size_t   BasicFile::Seek(size_t offset, int origin) never_throws
    {
        unsigned underlingMoveMethod = 0;
        switch (origin) {
        case SEEK_SET: underlingMoveMethod = FILE_BEGIN; break;
        case SEEK_CUR: underlingMoveMethod = FILE_CURRENT; break;
        case SEEK_END: underlingMoveMethod = FILE_END; break;
        default: assert(0);
        }
        return SetFilePointer(_file, LONG(offset), nullptr, underlingMoveMethod);
    }

    size_t   BasicFile::TellP() const never_throws
    {
        return SetFilePointer(_file, 0, nullptr, FILE_CURRENT);
    }

    void    BasicFile::Flush() const never_throws
    {
        FlushFileBuffers(_file);
    }

    uint64      BasicFile::GetSize() never_throws
    {
        if (_file == INVALID_HANDLE_VALUE) return 0;
        DWORD highWord = 0;
        auto lowWord = ::GetFileSize(_file, &highWord);
        return (uint64(highWord)<<32ull) | uint64(lowWord);
    }

///////////////////////////////////////////////////////////////////////////////////////////////////
    bool DoesFileExist(const char filename[])
    {
        DWORD dwAttrib = GetFileAttributes(filename);
        return dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY);
    }

    bool DoesDirectoryExist(const char filename[])
    {
        DWORD dwAttrib = GetFileAttributes(filename);
        return (dwAttrib != INVALID_FILE_ATTRIBUTES) && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY);
    }

    void CreateDirectory_Int(const char* dn)    { CreateDirectoryA(dn, nullptr); }
    void CreateDirectory_Int(const wchar_t* dn) { CreateDirectoryW(dn, nullptr); }

    template<typename Char>
        void CreateDirectoryRecursive_Int(StringSection<Char> filename)
    {
                // note that because our input string may not have a null 
                // terminator at the very end, we have to copy at least
                // once... So might as well copy and then we can safely
                // modify the copy as we go through
        Char buffer[MaxPath];
        XlCopyString(buffer, filename);

        SplitPath<Char> split(buffer);
        for (const auto& section:split.GetSections()) {
            Char q = 0;
            std::swap(q, *const_cast<Char*>(section.end()));
            CreateDirectory_Int(buffer);
            std::swap(q, *const_cast<Char*>(section.end()));
        }
    }

    void CreateDirectoryRecursive(const StringSection<char> filename)
    {
        CreateDirectoryRecursive_Int(filename);
    }

    uint64 GetFileModificationTime(const char filename[])
    {
        WIN32_FILE_ATTRIBUTE_DATA attribData;
        auto result = GetFileAttributesEx(filename, GetFileExInfoStandard, &attribData);
        if (!result) return 0ull;
        return (uint64(attribData.ftLastWriteTime.dwHighDateTime) << 32ull) | uint64(attribData.ftLastWriteTime.dwLowDateTime);
    }

    uint64 GetFileSize(const char filename[])
    {
        WIN32_FILE_ATTRIBUTE_DATA attribData;
        auto result = GetFileAttributesEx(filename, GetFileExInfoStandard, &attribData);
        if (!result) return 0ull;
        return (uint64(attribData.nFileSizeHigh) << 32ull) | uint64(attribData.nFileSizeLow);
    }

    std::vector<std::string> FindFiles(const std::string& searchPath, FindFilesFilter::BitField filter)
    {
        std::vector<std::string> result;

        char buffer[256];
        XlDirname(buffer, dimof(buffer), searchPath.c_str());
        std::string basePath = buffer;
        if (!basePath.empty() && basePath[basePath.size()-1]!='/') {
            basePath += "/";
        }
        WIN32_FIND_DATAA findData;
        memset(&findData, 0, sizeof(findData));
        HANDLE findHandle = FindFirstFileA(searchPath.c_str(), &findData);
        if (findHandle != INVALID_HANDLE_VALUE) {
            do {
                bool isDir = !!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
                if (filter & (1<<unsigned(isDir))) {
                    result.push_back(basePath + findData.cFileName);
                }
            } while (FindNextFileA(findHandle, &findData));
            FindClose(findHandle);
        }

        return std::move(result);
    }

    static std::vector<std::string> FindAllDirectories(const std::string& rootDirectory)
    {
        std::string basePath = rootDirectory;
        if (!basePath.empty() && basePath[basePath.size()-1]!='/') {
            basePath += "/";
        }
        std::vector<std::string> result;
        result.push_back(basePath);

        WIN32_FIND_DATAA findData;
        memset(&findData, 0, sizeof(findData));
        HANDLE findHandle = FindFirstFileA((basePath + "*").c_str(), &findData);
        if (findHandle != INVALID_HANDLE_VALUE) {
            do {
                if (    (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    &&  (findData.cFileName[0] != '.')) {
                    auto sub = FindAllDirectories(basePath + findData.cFileName);
                    result.insert(result.end(), sub.begin(), sub.end());
                }
            } while (FindNextFileA(findHandle, &findData));
            FindClose(findHandle);
        }

        return std::move(result);
    }

    std::vector<std::string> FindFilesHierarchical(const std::string& rootDirectory, const std::string& filePattern, FindFilesFilter::BitField filter)
    {
        auto dirs = FindAllDirectories(rootDirectory);

        std::vector<std::string> result;
        for(const auto&d:dirs) {
            auto files = FindFiles(d + filePattern, filter);
            result.insert(result.end(), files.begin(), files.end());
        }

        return std::move(result);
    }

    MemoryMappedFile::MemoryMappedFile(
        const char filename[], uint64 size, Access::BitField access, 
        BasicFile::ShareMode::BitField shareMode)
    {
        _mapping = INVALID_HANDLE_VALUE;
        _fileHandle = INVALID_HANDLE_VALUE;
        _mappedData = nullptr;

        unsigned underlyingAccess = 0;
        if (access & Access::Read)  underlyingAccess |= GENERIC_READ;
        if (access & Access::Write) underlyingAccess |= GENERIC_WRITE|GENERIC_READ;

        auto underlyingShareMode = AsUnderlyingShareMode(shareMode);

        unsigned creationDisposition = OPEN_EXISTING;
        if (access & Access::Write && (!(access & Access::Read))) {
            creationDisposition = CREATE_ALWAYS;
        } else if (access & Access::OpenAlways) {
            creationDisposition = OPEN_ALWAYS;
        }

        auto fileHandle = CreateFile(
            filename, underlyingAccess, underlyingShareMode, nullptr, creationDisposition, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (fileHandle == INVALID_HANDLE_VALUE) {
            return;
        }

        unsigned pageAccessMode = (access & Access::Write) ? PAGE_READWRITE : PAGE_READONLY;
        auto mapping = CreateFileMapping(
            fileHandle, nullptr, pageAccessMode, DWORD(size>>32), DWORD(size), nullptr);
        if (!mapping || mapping == INVALID_HANDLE_VALUE) {
            CloseHandle(fileHandle);
            return;
        }

        unsigned mapAccess = (access & Access::Write) ? FILE_MAP_WRITE : FILE_MAP_READ;
        auto mappingStart = MapViewOfFile(mapping, mapAccess, 0, 0, 0);
        if (!mappingStart) {
            CloseHandle(mapping);
            CloseHandle(fileHandle);
            return;
        }

        _mappedData = mappingStart;
        _mapping = mapping;
        _fileHandle = fileHandle;
    }

    MemoryMappedFile::~MemoryMappedFile()
    {
        if (_mappedData != nullptr) {
            UnmapViewOfFile(_mappedData);
        }
        CloseHandle(_mapping);
        CloseHandle(_fileHandle);
    }

    MemoryMappedFile::MemoryMappedFile()
    {
        _mapping = INVALID_HANDLE_VALUE;
        _fileHandle = INVALID_HANDLE_VALUE;
        _mappedData = nullptr;
    }

    MemoryMappedFile::MemoryMappedFile(MemoryMappedFile&& moveFrom) never_throws
    {
        _mapping = moveFrom._mapping;
        _fileHandle = moveFrom._fileHandle;
        _mappedData = moveFrom._mappedData;
        moveFrom._mapping = INVALID_HANDLE_VALUE;
        moveFrom._fileHandle = INVALID_HANDLE_VALUE;
        moveFrom._mappedData = nullptr;
    }

    MemoryMappedFile& MemoryMappedFile::operator=(MemoryMappedFile&& moveFrom) never_throws
    {
        if (_mappedData != nullptr) {
            UnmapViewOfFile(_mappedData);
        }
        CloseHandle(_mapping);
        CloseHandle(_fileHandle);

        _mapping = moveFrom._mapping;
        _fileHandle = moveFrom._fileHandle;
        _mappedData = moveFrom._mappedData;
        moveFrom._mapping = INVALID_HANDLE_VALUE;
        moveFrom._fileHandle = INVALID_HANDLE_VALUE;
        moveFrom._mappedData = nullptr;
        return *this;
    }

    size_t MemoryMappedFile::GetSize() const
    {
        LARGE_INTEGER fileSize;
        GetFileSizeEx(_fileHandle, &fileSize);
        return (size_t)fileSize.QuadPart;
    }
}

