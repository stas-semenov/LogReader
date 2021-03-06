// LogReader.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "CLogReader.h"
#include  <io.h>

inline bool isFileExists(LPCSTR filename)
{
	return (_access(filename, 0) != -1);
}

int main(int argc, char* argv[])
{
	if (argc < 3)
	{
		printf("Usage: %s", "LogReader.exe <file> <filter>");
		return 0;
	}

	if (!isFileExists(argv[1]))
	{
		printf("Couldn't access file: %s", argv[1]);
		return 0;
	}

	//CLogReader reader(true); // useMemoryMapping = true
	CLogReader reader(false); // useMemoryMapping = false

	LARGE_INTEGER freq, start, end;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&start);

	LPCSTR fileName = argv[1];
	if (reader.Open(fileName))
	{
		printf("File name: %s\nFile size: %llu\n", fileName, reader.getLength());

		if (reader.SetFilter(argv[2]))
		{
			const int _bufsize = 8192;
			char _buf[_bufsize];
			while (true)
			{
				if (reader.GetNextLine(_buf, _bufsize))
				{
					printf("%s\n", _buf);
				}
				else
					break;
			}
		}
		else
			printf("%s", "Invalid filter.\n");
	}
	else
		printf("%s", "Couldn't open file or zero size file.\n");

	QueryPerformanceCounter(&end);
	printf("Execution time: %.3f ms", (end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart);

    return 0;
}

