#include "microtar.h"
#include <windows.h>
#include <tchar.h> 
#include <stdio.h>
#include <strsafe.h>

#define _CRT_SECURE_NO_WARNINGS
#undef UNICODE
#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))



void ListFilesInDirectory(const char* directoryPath) {
	WIN32_FIND_DATAA findFileData;
	HANDLE hFind;
	TCHAR szDir[MAX_PATH];
	// Create the search path

	StringCchCopy(szDir, MAX_PATH, directoryPath);
	StringCchCat(szDir, MAX_PATH, TEXT("\\*"));

	// Start searching for files
	hFind = FindFirstFileA(directoryPath, &findFileData);

	if (hFind == INVALID_HANDLE_VALUE) {
		printf("Error: Unable to open directory %s. Error code: %d\n", directoryPath, GetLastError());
		return;
	}

	printf("Files in directory '%s':\n", directoryPath);

	do {
		// Skip "." and ".." entries
		if (strcmp(findFileData.cFileName, ".") != 0 && strcmp(findFileData.cFileName, "..") != 0) {
			if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				printf("[DIR]  %s\n", findFileData.cFileName);
			}
			else {
				printf("[FILE] %s\n", findFileData.cFileName);
			}
		}
	} while (FindNextFileA(hFind, &findFileData) != 0);

	// Check for errors during the FindNextFile loop
	if (GetLastError() != ERROR_NO_MORE_FILES) {
		printf("Error occurred while listing files. Error code: %d\n", GetLastError());
	}

	// Close the handle
	// Close the handle
	FindClose(hFind);
}

int getnextfile(HANDLE searchhandle, const WIN32_FIND_DATAA *ffd) {
	//WIN32_FIND_DATAA findFileData;
	HANDLE hFind;
	TCHAR szDir[MAX_PATH];
	int err;
	
	// Start searching for files
	err=FindNextFileA(searchhandle, ffd);

	if (ffd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				printf("Only files will be added, skipping directory:  %s\n", ffd->cFileName); //skipping subdirs for now
				FindNextFileA(searchhandle, ffd); //get next item, doen't handle several subfolders in a row yet.
		}
	
	printf("Adding [FILE] %s\n", ffd->cFileName);
	
	return err;
	
}

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

HANDLE preparetapedrive() {
	//function not in use yet
	printf("Preparing tapoe drive\n");
	HANDLE tapeDrive=CreateFileA("\\\\.\\Tape0", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	return tapeDrive;
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
	

	
	HANDLE h_tape;
	HANDLE hfile = INVALID_HANDLE_VALUE;
	HANDLE s_file1 = INVALID_HANDLE_VALUE;
	HANDLE s_file2 = INVALID_HANDLE_VALUE;
	HANDLE searchhandle;
	WIN32_FIND_DATAA findFileData;


	DWORD dwBytesRead = 0;
	DWORD dwBytesWritten = 0;
	
	char ReadBuffer[16384] = { 0 };

	unsigned int open_flags = 0;
	TAPE_GET_DRIVE_PARAMETERS drive;
	TAPE_GET_MEDIA_PARAMETERS media;
	int have_drive_info = 0;
	int have_media_info = 0;
	

	TAPE_SET_MEDIA_PARAMETERS tsmp;
	DWORD error;
	DWORD dstatus;

	//Blocksize 0 means variable block size on the tape drive, make sure it supports it.
	tsmp.BlockSize = 0;
	


	//define source directory, all files will be added to the tar archive, but no recursion yet.
	searchhandle = FindFirstFileA("c:\\temp\\test\\*", &findFileData);

	hfile = CreateFileA("c:\\temp\\test3.tar", GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
	h_tape = CreateFileA("\\\\.\\Tape0", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (h_tape != INVALID_HANDLE_VALUE)
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
			printf("Drive default block size: %d\n", drive.DefaultBlockSize);
			printf("Drive featureslow: %d\n", CHECK_BIT(drive.FeaturesLow, 2));
		}
		else if (error != ERROR_NO_MEDIA_IN_DRIVE) {
			printf("Can't get media information:\n");
			int success = 0;
		}

	}

	//setting block size to variable size(0), this was important if i remember correctly, otherwise it would fill out a whole tape block for the remaining bytes of the file.
	SetTapeParameters(h_tape, SET_TAPE_MEDIA_INFORMATION, &tsmp);
	PrepareTape(h_tape, TAPE_LOAD, FALSE);
	SetTapePosition(h_tape, TAPE_FILEMARKS, 0, 0, 0, TRUE);
	mtar_t tar;
	mtar_open(&tar, h_tape, "w");
	LPDWORD dwFileSizeHi=0;
	DWORD dwFileSizeLo = 0;
	LARGE_INTEGER lpFileSize;
	unsigned int size = 0;

	//loop through directory 
	while (getnextfile(searchhandle, &findFileData) != 0)
	{
		
		printf("filename: %s\n", &findFileData.cFileName);
		char* filename = (char*)malloc(strlen(&findFileData.cFileName) + strlen("c:\\temp\\test\\"));
		sprintf(filename, "c:\\temp\\test\\%s", &findFileData.cFileName);
		s_file1=CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		printf("Result from opening %s :%d\n", filename, GetLastError());
		size = (unsigned int)GetFileSize(s_file1, dwFileSizeHi);
		//Write filename, filesize and a few other things to the tar header
		mtar_write_file_header(&tar, &findFileData.cFileName, size);
		while (ReadFile(s_file1, ReadBuffer, sizeof(ReadBuffer), &dwBytesRead, NULL))
		{
			if (dwBytesRead == 0) {
				break;
			}
			mtar_write_data(&tar, ReadBuffer, dwBytesRead);
			printf("bytes read; %d", dwBytesRead);
		}
		//close source file
		CloseHandle(s_file1);
		printf("Next \n");
	}

		
	mtar_finalize(&tar);

	//close the search HANDLE
	CloseHandle(searchhandle);
	
	//closing the h_tape HANDLE in tar->stream
	mtar_close(&tar);
	return 0;


}