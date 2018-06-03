#pragma once
#include <Windows.h>
#include <memory>

#define DEFAULT_MASK_SIZE 1024
#define DEFAULT_STRBUF_SIZE 4096
#define DEFAULT_PAGE_SIZE 134217728   //128M

class CLogReader {
public:
	CLogReader();
	~CLogReader();

	bool    Open(const char *filename);                       // открытие файла, false - ошибка
	void    Close();									      // закрытие файла

	bool    SetFilter(const char *filter);                    // установка фильтра строк, false - ошибка
	bool    GetNextLine(char *buf, const int bufsize);  // запрос очередной найденной строки,
														// buf - буфер, bufsize - максимальная длина
														// false - конец файла или ошибка
private:
	bool m_isFileOpened = false;
	bool m_isMaskSet = false;

	LPVOID m_baseAddress = nullptr;
	const char* m_data = nullptr;
	const char *m_filename = nullptr;

	LARGE_INTEGER m_fileSize{ 0 };
	LARGE_INTEGER m_currentFilePos{ 0 };
	LARGE_INTEGER m_currentMappingPos{ 0 };

	HANDLE m_mappingHandle = nullptr;
	HANDLE m_fileHandle = nullptr;
	const unsigned int m_pageSize = DEFAULT_PAGE_SIZE;

	size_t  m_mappedSize = 0;
	const unsigned int m_maskSize = DEFAULT_MASK_SIZE;
	const unsigned int m_stringBufSize = DEFAULT_STRBUF_SIZE;

	DWORD m_memoryGranularity = 0;

	char m_mask[DEFAULT_MASK_SIZE];
	char m_maskCopy[DEFAULT_MASK_SIZE];

	bool Match(const char* wildcard, const char* string);
	bool UnmapFragment();
	bool MapNextFragmentWithOffset(unsigned int offset);

	DWORD GetGranularity();
	LARGE_INTEGER getProperAddressForReMap(long long currentFilePos, unsigned int offset);
};

