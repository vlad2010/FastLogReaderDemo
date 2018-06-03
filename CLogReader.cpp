#include "stdafx.h"
#include "CLogReader.h"
#include <Windows.h>
#include <string.h>

CLogReader::CLogReader() {
	m_memoryGranularity = GetGranularity();
};

CLogReader::~CLogReader() {
	Close();
}

// открытие файла, false - ошибка
bool CLogReader::Open(const char* filename) {
	if (m_isFileOpened) {
		Close();
	}

	m_fileHandle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (m_fileHandle == INVALID_HANDLE_VALUE) {
		return false;
	};

	bool res = GetFileSizeEx(m_fileHandle, &m_fileSize);
	if (!res || !m_fileSize.QuadPart) {
		CloseHandle(m_fileHandle);
		return false;
	}

	m_mappingHandle = CreateFileMapping(m_fileHandle, 0, PAGE_READONLY, 0, 0, 0);
	if (m_mappingHandle == 0) {
		CloseHandle(m_fileHandle);
		return false;
	}

	m_isFileOpened = true;
	return true;
}

// закрытие файла
void CLogReader::Close() {
	if (m_isFileOpened) {

		if (m_baseAddress)
			UnmapFragment();

		m_currentFilePos.QuadPart = 0;
		CloseHandle(m_mappingHandle);
		CloseHandle(m_fileHandle);
		m_isFileOpened = false;
	}
}

// установка фильтра строк, false - ошибка
bool CLogReader::SetFilter(const char *filter) {

	char prev = 0;
	size_t mask_pos = 0;
	if (m_isMaskSet)
		memcpy(m_maskCopy, m_mask, m_maskSize);

	while (*filter != 0) {

		if (mask_pos >= m_maskSize) {
			//copy saved mask back 
			if (m_isMaskSet)
				memcpy(m_mask, m_maskCopy, m_maskSize);
			return false;
		}

		if (prev != '*' || *filter != '*') {
			m_mask[mask_pos] = *filter;
			mask_pos++;
		}

		prev = *filter;
		filter++;
	}

	if (mask_pos == 0) {
		//copy saved mask back 
		if (m_isMaskSet)
			memcpy(m_mask, m_maskCopy, m_maskSize);
		return false;
	}

	//end of line for our mask 
	m_mask[mask_pos] = 0;
	m_isMaskSet = true;
	return true;
}

bool CLogReader::GetNextLine(char *buf, const int bufsize) {

	bool matchResult = false;
	if (m_isFileOpened && buf != nullptr && bufsize > 0) {

		if (!m_data) {
			if (!MapNextFragmentWithOffset(0))
				return false;
		}

		while ((matchResult == false) && (m_currentFilePos.QuadPart < m_fileSize.QuadPart)) {
			int bufPos = 0;
			char symbol = 0;
			const char *stringStart = nullptr;
			while (1) {

				if (m_currentMappingPos.QuadPart == m_mappedSize) {
					if (!UnmapFragment())
						return false;

					if (m_currentFilePos.QuadPart < m_fileSize.QuadPart) {

						if (!MapNextFragmentWithOffset(bufPos)) {
							return false;
						}
						bufPos = 0;
						symbol = 0;
						stringStart = nullptr;
						continue;
					}
					else {
						return false;  //no more lines
					}
				}
				if (symbol == 0) {
					stringStart = &m_data[m_currentMappingPos.QuadPart];
				}

				symbol = m_data[m_currentMappingPos.QuadPart];
				m_currentMappingPos.QuadPart++;
				m_currentFilePos.QuadPart++;

				if (symbol == '\r' || symbol == '\n' || symbol == 0) {
					break;
				}

				if (bufPos < (int)(m_stringBufSize - 1)) {
					bufPos++;
				}
			}

			if (bufPos > 0) {
				matchResult = Match(m_mask, stringStart);
				if (matchResult) {
					unsigned int sizeToCopy = (bufPos<bufsize-1)? bufPos : bufsize;
					memcpy(buf, stringStart, sizeToCopy);
					buf[sizeToCopy] = 0;
				}
			}
			else {
				matchResult = false;
			}
		}

		if (m_currentFilePos.QuadPart == m_fileSize.QuadPart) {
			UnmapFragment();
		}
	}
	return matchResult;
}

bool CLogReader::Match(const char* mask, const char* str)
{
	const char *strP, *maskP;
	bool star = false;
loopStart:
	for (strP = str, maskP = mask; *strP != '\r' && *strP != '\n'; ++strP, ++maskP) {
		switch (*maskP) {
		case '?':
			if (*strP == '.')
				goto starCheck;
			break;
		case '*':
			star = true;
			str = strP, mask = maskP;
			if (!*++mask)
				return true;
			goto loopStart;
		default:
			if (*strP != *maskP)
				goto starCheck;
			break;
		}
	}
	if (*maskP == '*')
		++maskP;
	return (!*maskP);

starCheck:
	if (!star)
		return false;
	str++;
	goto loopStart;
}

bool CLogReader::UnmapFragment() {
	bool result = true;

	if (m_baseAddress) {
		result = UnmapViewOfFile(m_baseAddress);

		m_baseAddress = nullptr;
		m_data = nullptr;
		m_currentMappingPos.QuadPart = 0;
		m_mappedSize = 0;

		if (!result)
			return false;
	}
	return result;
};

bool CLogReader::MapNextFragmentWithOffset(unsigned int offset) {

	LARGE_INTEGER nextMapAddress = getProperAddressForReMap(m_currentFilePos.QuadPart, offset);
	m_currentFilePos.QuadPart -= offset; //we go back to the beginning of the string

	long long howMuchLeft = m_fileSize.QuadPart - nextMapAddress.QuadPart;
	m_mappedSize = (unsigned int)(howMuchLeft > m_pageSize ? m_pageSize : m_fileSize.QuadPart - nextMapAddress.QuadPart);

	m_baseAddress = MapViewOfFile(m_mappingHandle, FILE_MAP_READ, nextMapAddress.HighPart, nextMapAddress.LowPart, m_mappedSize);
	if (m_baseAddress == 0) {
		return false;
	}

	if (offset>0) {
		m_currentMappingPos.QuadPart = m_memoryGranularity - offset;
	}
	else {
		m_currentMappingPos.QuadPart = 0;
	}
	m_data = (const char*)m_baseAddress;
	return true;
};

DWORD CLogReader::GetGranularity() {
	SYSTEM_INFO si;
	::GetNativeSystemInfo(&si);
	return si.dwAllocationGranularity;
}

LARGE_INTEGER CLogReader::getProperAddressForReMap(long long currentFilePos, unsigned int offset) {

	LARGE_INTEGER newMappingAddress{ 0 };
	if (((currentFilePos-offset) % m_memoryGranularity) == 0) {
		newMappingAddress.QuadPart = currentFilePos - offset;
	}
	else {
		newMappingAddress.QuadPart = (((currentFilePos - offset) / m_memoryGranularity) * m_memoryGranularity);
	}

	return newMappingAddress;
}