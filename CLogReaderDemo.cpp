#include "stdafx.h"
#include "CLogReader.h"

int main(int argc, char* argv[])
{
	if (argc < 3) {
		printf("Usage: <exename> <mask> <filepath> \n");
		return -1;
	}

	printf("Name: %s Mask: %s Path: %s \n", argv[0], argv[1], argv[2]);
	CLogReader cl;

	if (!cl.SetFilter(argv[1]))
	{
		printf("Error: Can't set filter [%s]\n", argv[1]);
		return -1;
	}

	if (!cl.Open(argv[2])) {
		printf("Error: Can't open file [%s]\n", argv[2]);
		return -1;
	}

	unsigned int lines = 0;
	char buffer[2048];
	while (cl.GetNextLine(buffer, sizeof(buffer))) {
		printf("%s\n", buffer);
		lines++;
	}

	printf("%d lines found for mask [%s] \n", lines, argv[1]);
	return 0;
}

