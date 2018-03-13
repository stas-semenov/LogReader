#include "stdafx.h"
#include "CLogReader.h"


bool CLogReader::GetNextLine(char * buf, const int bufsize)
{
	if (buf == NULL || bufsize <= 0)
		return false;

	while (true)
	{
		LPSTR _buf = NULL;
		int _bufsize = 0;
		if (GetNextFullLine(_buf, &_bufsize))
		{
			if (MatchFilter(_buf, _mask))
			{
				int length = min(bufsize, _bufsize) - 1;
				if (length > 0)
					memcpy(buf, _buf, length);
				buf[length] = 0;
				free(_buf);

				return true;
			}
			else
			{
				free(_buf);
				continue;
			}
		}
		else
			break;
	}

	return false;
}

bool CLogReader::GetNextFullLine(char * &buf, int * bufsize)
{
	if (bufsize == NULL)
		return false;

	if (pFile == NULL)
		return false;

	if (bMemoryMapping)
	{
		MapFile * pMapFile = (MapFile *)pFile;
		if (iter == 0)
			iter = pMapFile->begin();
		else if (iter == pMapFile->end())
			return false;

		LPCSTR begin = iter;
		int specials = 0;
		for (; iter != pMapFile->end(); iter++)
		{
			__try
			{
				if (*iter == '\r')
				{
					specials++;
					continue;
				}
				else if (*iter == '\n')
				{
					specials++;
					iter++;
					break;
				}
			}
			__except (GetExceptionCode() == EXCEPTION_IN_PAGE_ERROR ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
			{
				Close();

				// e.g. accessing a mapped file on a remote server if the connection to the server is lost.
				printf("%s", "Failed to read from the view.\n");
				return false;
			}
		}

		*bufsize = int(iter - begin) - specials + 1;
		if (*bufsize > 0)
		{
			buf = (LPSTR)malloc((*bufsize) * sizeof(char));
			if (buf == NULL)
			{
				Close();

				printf("%s", "Couldn't allocate memory.\n");
				return false;
			}

			__try
			{
				memcpy(buf, begin, *bufsize - 1);
			}
			__except (GetExceptionCode() == EXCEPTION_IN_PAGE_ERROR ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
			{
				Close();
				return false;
			}

			buf[*bufsize - 1] = 0;

			return true;
		}
	}
	else
	{
		buf = NULL;
		DWORD cur_line_size = 0;

		bool _end = false;
		int specials = 0;
		while (!_end)
		{
			bool line_break = false;
			if (last_pos == 0)
			{
				if (!ReadFile(pFile->getHandle(), _linebuf, BUF_SIZE * sizeof(char), &cur_count, nullptr))
				{
					if (buf != NULL)
						free(buf);

					Close();

					printf("%s", "Failed to read from the file.\n");
					return false;
				}
			}

			for (; cur_pos < cur_count; cur_pos++)
			{
				if (_linebuf[cur_pos] == '\r')
				{
					specials++;
					continue;
				}
				else if (_linebuf[cur_pos] == '\n')
				{
					line_break = true;
					specials++;
					cur_pos++;
					break;
				}
			}

			buf = (LPSTR)realloc(buf, (cur_line_size + (cur_pos - last_pos) + 1) * sizeof(char));
			if (buf == NULL)
			{
				Close();

				printf("%s", "Couldn't allocate memory.\n");
				return false;
			}

			memcpy(buf + cur_line_size, _linebuf + last_pos, cur_pos - last_pos);
			buf[cur_line_size + cur_pos - last_pos - specials] = 0;
			cur_line_size += cur_pos - last_pos;

			last_pos = cur_pos;
			if (last_pos == cur_count)
			{
				if (cur_count < BUF_SIZE)
					_end = true;

				cur_pos = 0;
				last_pos = 0;
				cur_count = 0;
			}

			if (line_break || _end)
			{
				if (cur_line_size > 0)
				{
					buf[cur_line_size - 1] = 0;
					*bufsize = cur_line_size;
					return true;
				}
			}
		}
	}

	return false;
}

bool CLogReader::SetFilter(const char * filter)
{
	if (filter == NULL)
		return false;

	if (_mask != NULL)
	{
		free((LPVOID)_mask);
		_mask = NULL;
	}

	size_t lenght = strlen(filter) + 1;
	_mask = (LPSTR)malloc(lenght);
	if (_mask == NULL)
	{
		printf("%s", "Couldn't allocate memory.\n");
		return false;
	}

	strcpy_s(_mask, lenght * sizeof(char), filter);

	return true;
}

bool CLogReader::MatchFilter(LPCSTR str, LPCSTR mask)
{
	if (str == NULL || mask == NULL)
		return false;

	LPCSTR asterisk_after_pos = NULL;
	size_t shift = 0;

	while (*str != 0)
	{
		if (*mask == '*')
		{
			while (*mask == '*')
				mask++;

			if (*mask == 0)
				return true;

			asterisk_after_pos = mask;
			continue;
		}

		if (*mask == *str || *mask == '?')
		{
			shift++;
			str++;
			mask++;
			continue;
		}

		if (asterisk_after_pos == NULL)
			return false;

		if (shift > 0)
		{
			str -= shift;
			shift = 0;
		}

		str++;
		mask = asterisk_after_pos;
	}

	return *mask == 0;
}
