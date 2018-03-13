#pragma once

#include <Windows.h>

struct HandleTypeFile
{
	static HANDLE invalidValue()
	{
		return INVALID_HANDLE_VALUE;
	}
};

struct HandleTypeMemoryMapping
{
	static HANDLE invalidValue()
	{
		return nullptr;
	}
};

template <typename HandleType>
class FileHandle
{
public:
	FileHandle() : handle(HandleType::invalidValue())
	{
	}

	FileHandle(HANDLE h) : handle(h)
	{
	}

	~FileHandle()
	{
		close();
	}

	HANDLE getHandle() const
	{
		return handle;
	}

	void reset(HANDLE h)
	{
		if (h != handle)
		{
			close();
			handle = h;
		}
	}

	bool isValid() const
	{
		return (handle != HandleType::invalidValue());
	}

	void close()
	{
		if (isValid())
			CloseHandle(handle);

		handle = HandleType::invalidValue();
	}

private:
	HANDLE handle;
};

typedef FileHandle<HandleTypeFile> HandleFile;
typedef FileHandle<HandleTypeMemoryMapping> HandleMemoryMapping;

class SimpleFile
{
public:
	SimpleFile(LPCSTR filename) : fileLength(0)
	{
		HANDLE handle = ::CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
		hFile.reset(handle);
		if (!hFile.isValid())
			return;

		LARGE_INTEGER sz;
		if (!GetFileSizeEx(hFile.getHandle(), &sz))
			return;

		if (sz.QuadPart == 0)
			return;

		fileLength = sz.QuadPart;
	}

	virtual ~SimpleFile()
	{
		hFile.close();
	}

	HANDLE getHandle() const
	{
		return hFile.getHandle();
	}

	ULONGLONG length() const
	{
		return fileLength;
	}

	virtual bool isValid() const
	{
		return hFile.isValid();
	}

protected:
	HandleFile hFile;
	unsigned long long fileLength;
};

class MapFile : public SimpleFile
{
public:
	MapFile(LPCSTR filename) : SimpleFile(filename), fileView(nullptr)
	{
		if (fileLength == 0)
			return;

		HANDLE handle = ::CreateFileMapping(hFile.getHandle(), nullptr, PAGE_READONLY, 0, 0, nullptr);
		hMemoryMapping.reset(handle);
		if (!hMemoryMapping.isValid())
			return;

		LPVOID pointer = ::MapViewOfFile(hMemoryMapping.getHandle(), FILE_MAP_READ, 0, 0, 0);
		if (pointer == NULL)
		{
			LPVOID lpMsgBuf;
			DWORD dw = GetLastError();
			FormatMessageA(
				FORMAT_MESSAGE_ALLOCATE_BUFFER |
				FORMAT_MESSAGE_FROM_SYSTEM |
				FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL,
				dw,
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				(LPSTR)&lpMsgBuf,
				0, NULL);
			printf("%s", (LPSTR)lpMsgBuf);
			LocalFree(lpMsgBuf);
		}

		fileView = reinterpret_cast<char*>(pointer);
		if (!fileView)
			return;
	}

	virtual ~MapFile()
	{
		if (fileView)
			UnmapViewOfFile(fileView);
		hMemoryMapping.close();
	}

	LPCSTR begin() const
	{
		return fileView;
	}

	LPCSTR end() const
	{
		return begin() + length();
	}

	bool isValid() const override
	{
		return (fileView != nullptr);
	}

private:
	HandleMemoryMapping  hMemoryMapping;
	LPSTR fileView;
};


#define BUF_SIZE 16384

class CLogReader
{
public:
	CLogReader(bool useMemoryMapping = false) : bMemoryMapping(useMemoryMapping), pFile(NULL), _mask(NULL), iter(0)
	{
		cur_pos = 0;
		last_pos = 0;
		cur_count = 0;
	}

	~CLogReader()
	{
		if (_mask != NULL)
			free((LPVOID)_mask);
		Close();
	}

	bool Open(LPCSTR filename)
	{
		Close();

		if (bMemoryMapping)
			pFile = new MapFile(filename);
		else
			pFile = new SimpleFile(filename);

		if (!pFile->isValid())
		{
			Close();
			return false;
		}

		return true;
	}

	void Close()
	{
		cur_pos = 0;
		last_pos = 0;
		cur_count = 0;
		iter = 0;
		if (pFile != NULL)
		{
			delete pFile;
			pFile = NULL;
		}
	}

	bool GetNextLine(char * buf, const int bufsize);
	bool SetFilter(const char * filter);

	ULONGLONG getLength() const
	{
		if (pFile != NULL)
			return pFile->length();

		return 0;
	}

private:
	bool GetNextFullLine(char * &buf, int * bufsize);
	bool MatchFilter(LPCSTR str, LPCSTR mask);

	bool bMemoryMapping;
	SimpleFile * pFile;
	LPCSTR iter;
	LPSTR _mask;
	DWORD cur_pos, last_pos;
	DWORD cur_count;
	char _linebuf[BUF_SIZE];
};
