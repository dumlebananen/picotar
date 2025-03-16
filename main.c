#include "microtar.h"
#include <windows.h>
#include <stdio.h>

#define _CRT_SECURE_NO_WARNINGS
#undef UNICODE
#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))



//void ListFilesInDirectory(const char* directoryPath) {
//	WIN32_FIND_DATA findFileData;
//	HANDLE hFind;
//
//	// Create the search path
//	char searchPath[MAX_PATH];
//	snprintf(searchPath, MAX_PATH, "%s\\*", directoryPath);
//	int result = snprintf(searchPath, MAX_PATH, "%s\\*", directoryPath);
//	if (result < 0 || result >= MAX_PATH) {
//		printf("Error: Path is too long or snprintf failed.\n");
//		return;
//	}
//	// Start searching for files
//	hFind = FindFirstFileA(directoryPath, &findFileData);
//
//	if (hFind == INVALID_HANDLE_VALUE) {
//		printf("Error: Unable to open directory %s. Error code: %d\n", directoryPath, GetLastError());
//		return;
//	}
//
//	printf("Files in directory '%s':\n", directoryPath);
//
//	do {
//		// Skip "." and ".." entries
//		if (strcmp(findFileData.cFileName, ".") != 0 && strcmp(findFileData.cFileName, "..") != 0) {
//			if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
//				printf("[DIR]  %s\n", findFileData.cFileName);
//			}
//			else {
//				printf("[FILE] %s\n", findFileData.cFileName);
//			}
//		}
//	} while (FindNextFile(hFind, &findFileData) != 0);
//
//	// Check for errors during the FindNextFile loop
//	if (GetLastError() != ERROR_NO_MORE_FILES) {
//		printf("Error occurred while listing files. Error code: %d\n", GetLastError());
//	}
//
//	// Close the handle
//	// Close the handle
//	FindClose(hFind);
//}

int checkTapeDrive(HANDLE h_tape) {
	/* Check for errors */
	if (h_tape == INVALID_HANDLE_VALUE)
	{
		DWORD error = GetLastError();
		switch (error)
		{
		case ERROR_FILE_NOT_FOUND:
			printf("Can't open tape: device not found!\n");
			return error;
			break;
		case ERROR_ACCESS_DENIED:
			printf("Can't open tape: access denied!\n");
			return error;
			break;
		case ERROR_SHARING_VIOLATION:
			printf("Can't open tape: locked by other program\n");
			return error;
			break;
		default:
			printf("Can't open tape: generic error\n");
			return error;
			break;
		}

	}
	return 0;

}


int writetotape(HANDLE tapedrive) {

	mtar_t tar;
	const char* str1 = "Hello world";
	const char* str2 = "Goodbye world";
	/* Open archive for writing */
	mtar_open(&tar, tapedrive, "w");

	/* Write strings to files `test1.txt` and `test2.txt` */
	mtar_write_file_header(&tar, "test1.txt", strlen(str1));
	mtar_write_data(&tar, str1, strlen(str1));
	mtar_write_file_header(&tar, "test2.txt", strlen(str2));
	mtar_write_data(&tar, str2, strlen(str2));

	/* Finalize -- this needs to be the last thing done before closing */
	mtar_finalize(&tar);

	/* Close archive */
	mtar_close(&tar);
	return 0;
}

int main(int argc, char* argv[]) {

	HANDLE h_tape = INVALID_HANDLE_VALUE;
	HANDLE h_file = INVALID_HANDLE_VALUE;
	unsigned int open_flags = 0;
	TAPE_GET_DRIVE_PARAMETERS drive;
	TAPE_GET_MEDIA_PARAMETERS media;
	int have_drive_info = 0;
	int have_media_info = 0;

	TAPE_SET_MEDIA_PARAMETERS tsmp;
	DWORD error;
	DWORD dstatus;
	tsmp.BlockSize = 0;
	printf("Argument passed: %s", argv[1]);
	const char* directoryPath = "C:\\temp";
	//ListFilesInDirectory(directoryPath);
	if (argc > 1) {
		if (strcmp(argv[1], "w") == 0) {
			h_tape = CreateFileA("\\\\.\\Tape0", GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
			h_file = CreateFileA("c:\\temp\\test2.tar", GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
			if (!checkTapeDrive(h_tape))
			{
				DWORD size, error;
				printf("tape opened \nGetting tape info\n");
				size = sizeof(drive);
				error = GetTapeParameters(h_tape, GET_TAPE_DRIVE_INFORMATION, &size, &drive);
				printf("Error: %d\n", error);

				printf("Querying media information...\n");
				size = sizeof(media);
				error = GetTapeParameters(h_tape, GET_TAPE_MEDIA_INFORMATION, &size, &media);
				if (error == 0) {
					have_media_info = 1;
					printf("Drive Max block size: %d\n", drive.MaximumBlockSize);
					printf("Drive Min block size: %d\n", drive.MinimumBlockSize);
					printf("Drive ECC status: %d\n", drive.ECC);
					printf("Drive compression status: %d\n", drive.Compression);
					//printf("Drive compression status: %d\n", drive.);
					printf("Drive default block size: %d\n", drive.DefaultBlockSize);
					printf("Drive featureslow: %d\n", CHECK_BIT(drive.FeaturesLow, 2));
				}
				else if (error != ERROR_NO_MEDIA_IN_DRIVE) {
					printf("Can't get media information:\n");
					int success = 0;
				}

			}
			SetTapeParameters(h_tape, SET_TAPE_MEDIA_INFORMATION, &tsmp);
			dstatus = GetTapeStatus(h_tape);
			error = PrepareTape(h_tape, TAPE_LOAD, FALSE);
			dstatus = GetTapeStatus(h_tape);
			writetotape(h_file);
			exit(1);
		}
	}

	//LPWSTR szFileName = "c:\\temp\\test.tar";
	//HANDLE src_file;
	//src_file = CreateFileA(szFileName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	//mtar_t tar;
	//mtar_header_t h;
	//char* p;

	///* Open archive for reading */
	//mtar_open(&tar, src_file, "r");

	///* Print all file names and sizes */
	//while ((mtar_read_header(&tar, &h)) != MTAR_ENULLRECORD) {
	//	printf("%s (%d bytes)\n", h.name, h.size);
	//	mtar_next(&tar);
	//}

	///* Load and print contents of file "test.txt" */
	//mtar_find(&tar, "test.txt", &h);
	//p = calloc(1, h.size + 1);
	//mtar_read_data(&tar, p, h.size);
	//printf("%s", p);
	//free(p);

	///* Close archive */
	//mtar_close(&tar);
}