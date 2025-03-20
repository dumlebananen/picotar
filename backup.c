#include "microtar.h"
#include <windows.h>
#include <tchar.h> 
#include <stdio.h>
#include <strsafe.h>

#define _CRT_SECURE_NO_WARNINGS
#undef UNICODE
#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))

// Function to escape a Windows file path
char* escape_windows_filepath(const char* filepath) {
	// Calculate the length of the escaped string
	size_t length = 0;
	for (const char* p = filepath; *p; ++p) {
		if (*p == '\\' || *p == '\"' || *p == '\'') {
			length += 2; // Escape character and the character itself
		}
		else {
			length += 1; // Normal character
		}
	}

	// Allocate memory for the escaped string
	char* escaped = (char*)malloc(length + 1); // +1 for null terminator
	if (!escaped) {
		fprintf(stderr, "Memory allocation failed.\n");
		return NULL;
	}

	// Fill the escaped string
	const char* src = filepath;
	char* dest = escaped;
	while (*src) {
		if (*src == '\\') {
			*dest++ = '\\';
			*dest++ = '\\';
		}
		else if (*src == '\"') {
			*dest++ = '\\';
			*dest++ = '\"';
		}
		else if (*src == '\'') {
			*dest++ = '\\';
			*dest++ = '\'';
		}
		else {
			*dest++ = *src;
		}
		src++;
	}

	*dest = '\0'; // Null terminate the escaped string
	return escaped;
}


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

int getnextfile(HANDLE *searchhandle, const WIN32_FIND_DATAA *ffd) {
	//WIN32_FIND_DATAA findFileData;
	HANDLE hFind = searchhandle;
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

int preptape(HANDLE h_tape) {
	TAPE_GET_DRIVE_PARAMETERS drive;
	TAPE_GET_MEDIA_PARAMETERS media;
	int have_drive_info = 0;
	int have_media_info = 0;
	TAPE_SET_MEDIA_PARAMETERS tsmp;
	tsmp.BlockSize = 0;
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
			//setting block size to variable size(0), this was important if i remember correctly, otherwise it would fill out a whole tape block for the remaining bytes of the file.
			SetTapeParameters(h_tape, SET_TAPE_MEDIA_INFORMATION, &tsmp);
			PrepareTape(h_tape, TAPE_LOAD, FALSE);
			SetTapePosition(h_tape, TAPE_FILEMARKS, 0, 0, 0, TRUE);
		}
		else if (error != ERROR_NO_MEDIA_IN_DRIVE) {
			printf("Can't get media information:\n");
			int success = 0;
		}

	}

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
	else {
		printf("Tape drive found!\n");
	}
	return 0;

}




int main(int argc, char* argv[]) {
	

	
	mtar_t tar;
	HANDLE h_tape = INVALID_HANDLE_VALUE;
	HANDLE s_file1 = INVALID_HANDLE_VALUE;
	HANDLE h_file = INVALID_HANDLE_VALUE;
	HANDLE searchhandle;
	WIN32_FIND_DATAA findFileData;
	char *escaped_path = NULL;

	LPDWORD dwFileSizeHi = 0;
	DWORD dwFileSizeLo = 0;
	LARGE_INTEGER lpFileSize;
	unsigned int size = 0;
	DWORD dwBytesRead = 0;
	DWORD dwBytesWritten = 0;
	
	char ReadBuffer[16384] = { 0 };

	unsigned int open_flags = 0;


	DWORD error;
	DWORD dstatus;

	//Blocksize 0 means variable block size on the tape drive, make sure it supports it.
	
	
	//h_tape = CreateFileA("\\\\.\\Tape0", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	//h_file = CreateFileA("c:\\temp\\testing.tar", GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, NULL);
	
	const char* command = argv[1];
	const char* src_path = argv[2];
	if (strcmp(command, "tapebackup") == 0) {
		if (argc != 3) {
			printf("Usage: %s tapebackup <source_file>\n", argv[0]);
			
			return 1;
		}
		else {
			h_tape = CreateFileA("\\\\.\\Tape0", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
			error = checkTapeDrive(h_tape);
			if (error != 0) {
				printf("Tape error: %d ", error);
				return 1;
			}
			else {
				preptape(h_tape);
				mtar_open(&tar, h_tape, "w");
			}
		}
	}


	if (strcmp(command, "backup") == 0) {
		if (argc != 3) {
			printf("Usage: %s backup <source_file>\n", argv[0]);
			return 1;
		}
		else {
			h_file = CreateFileA("c:\\temp\\testing.tar", GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, NULL);
			mtar_open(&tar, h_file, "w");
		}
	}
		
	char* fixedpath = escape_windows_filepath(src_path);
	printf("Escaped path: %s", fixedpath);
	//escaped_path = &fixedpath;
	//searchhandle = FindFirstFileA(fixedpath, &findFileData);
	//printf("reurned path: %s\n", *escaped_path);
	
	
	
	//define source directory, all files will be added to the tar archive, but no recursion yet.
	//searchhandle = FindFirstFileA("c:\\temp\\test\\*", &findFileData);
	
	//h_tape = CreateFileA("\\\\.\\Tape0", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	
	

	
	


	searchhandle = FindFirstFileA(fixedpath, &findFileData);

	if (findFileData.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY) {

	}
	//loop through directory 
	//while (getnextfile(searchhandle, &findFileData) != 0)
	//{
		
		printf("filename: %s\n", &findFileData.cFileName);
		//char* filename = (char*)malloc(strlen(&findFileData.cFileName) + strlen(fixedpath));
		//sprintf(filename, "%s%s",fixedpath, &findFileData.cFileName);
		
		//to tape
		//s_file1=CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		//printf("Result from opening %s :%d\n", filename, GetLastError());
	
		//to file
		s_file1 = CreateFileA(fixedpath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		printf("Result from opening %s :%d\n", fixedpath, GetLastError());
		size = (unsigned int)GetFileSize(s_file1, dwFileSizeHi);
		//Write filename, filesize and a few other things to the tar header
		mtar_write_file_header(&tar, &findFileData.cFileName, size);
		while (ReadFile(s_file1, ReadBuffer, sizeof(ReadBuffer), &dwBytesRead, NULL))
		{
			if (dwBytesRead == 0) {
				break;
			}
			mtar_write_data(&tar, ReadBuffer, dwBytesRead);
			//printf("bytes read; %d", dwBytesRead);
		}
		//close source file
		CloseHandle(s_file1);
		printf("Next \n");
	/*}*/

		
	mtar_finalize(&tar);

	//close the search HANDLE
	FindClose(searchhandle);

	
	//closing the h_tape HANDLE in tar->stream
	mtar_close(&tar);
	return 0;


}