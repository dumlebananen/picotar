#include "microtar.h"
#include <windows.h>
#include <tchar.h> 
#include <stdio.h>
#include <strsafe.h>
#include <wchar.h>

#define _CRT_SECURE_NO_WARNINGS
#undef _UNICODE
#undef UNICODE
#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))


typedef struct {
	HANDLE hFind;
	WIN32_FIND_DATAA findFileData;
	char folderPath[MAX_PATH];
	int initialized;
} FileIterator;

char *ConvertToUnixPath(char *winpath, char *unixpath) {
	if (winpath == NULL) {
		return NULL; // Handle null input
	}
	size_t length = strlen(winpath);
	

	// Convert backslashes to forward slashes
	for (size_t i = 0; i < length; i++) {
		if (winpath[i] == '\\') {
			unixpath[i] = '/';
		}
		else {
			unixpath[i] = winpath[i];
		}
	}
	unixpath[length] = '\0'; // Null-terminate the string

	return unixpath;
}

int GetNextFileOrFolder(FileIterator* iterator) {
	// Initialize the search if it's the first call
	if (!iterator->initialized) {
		char searchPath[MAX_PATH];
		snprintf(searchPath, MAX_PATH, "%s\\*", iterator->folderPath);

		iterator->hFind = FindFirstFileA(searchPath, &iterator->findFileData);
		if (iterator->hFind == INVALID_HANDLE_VALUE) {
			return 0; // No files found
		}
		iterator->initialized = 1;
		return 1; // Found the first file/folder
	}

	// Continue to the next file/folder
	if (FindNextFileA(iterator->hFind, &iterator->findFileData)) {
		return 1; // Found the next file/folder
	}

	// Clean up if no more files/folders
	FindClose(iterator->hFind);
	iterator->hFind = INVALID_HANDLE_VALUE;
	return 0;
}

void ProcessFilesAndFolders(const char* startPath, mtar_t *tar) {
	LPDWORD dwFileSizeHi = 0;
	DWORD dwFileSizeLo = 0;
	LARGE_INTEGER lpFileSize;
	unsigned int size = 0;
	DWORD dwBytesRead = 0;
	DWORD dwBytesWritten = 0;
	HANDLE s_file = INVALID_HANDLE_VALUE;
	char ReadBuffer[16384] = { 0 };
	unsigned int open_flags = 0;
	FileIterator iterator = { 0 };
	char unixpath[MAX_PATH];
	char fullPath[MAX_PATH];
	snprintf(iterator.folderPath, MAX_PATH, "%s", startPath);

	while (GetNextFileOrFolder(&iterator)) {
		
		// Skip "." and ".."
		if (strcmp(iterator.findFileData.cFileName, ".") == 0 ||
			strcmp(iterator.findFileData.cFileName, "..") == 0) {
			continue;
		}
		
		snprintf(fullPath, MAX_PATH, "%s\\%s", startPath, iterator.findFileData.cFileName);
		ConvertToUnixPath(fullPath, unixpath);

		if (iterator.findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			// It's a directory
			printf("Directory: %s\n", fullPath);
			//mtar_write_file_header(tar, unixpath, 0);
			ProcessFilesAndFolders(fullPath, tar);
		}
		else {
			// It's a file
			printf("File: %s\n", fullPath);
			s_file = CreateFileA(fullPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			printf("Result from opening %s :%d\n", fullPath, GetLastError());
			size = (unsigned int)GetFileSize(s_file, dwFileSizeHi);
			//Write filename, filesize and a few other things to the tar header
			//mtar_write_file_header(tar, iterator.findFileData.cFileName, size);
			
			//testing with unix-style paths in tar header
			mtar_write_file_header(tar, unixpath, size);
			
			while (ReadFile(s_file, ReadBuffer, sizeof(ReadBuffer), &dwBytesRead, NULL))
			{
				if (dwBytesRead == 0) {
					break;
				}
				mtar_write_data(tar, &ReadBuffer, dwBytesRead);
				//printf("bytes read; %d", dwBytesRead);
			}
			//close source file
			CloseHandle(s_file);
		}

}
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
	HANDLE h_file = INVALID_HANDLE_VALUE;
	HANDLE searchhandle;
	WIN32_FIND_DATAA findFileData;
	char *escaped_path = NULL;
	DWORD error;
	DWORD dstatus;

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
		if (argc != 4) {
			printf("Usage: %s backup <source_file> <destination_tar>\n", argv[0]);
			return 1;
		}
		else {
			const char* dest_path = escape_windows_filepath(argv[3]);
			h_file = CreateFileA(dest_path, GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, NULL);
			mtar_open(&tar, h_file, "w");
		}
	}
		
	char* fixedpath = escape_windows_filepath(src_path);
	printf("Escaped path: %s", fixedpath);
	ProcessFilesAndFolders(src_path, &tar);

	mtar_finalize(&tar);
		
	//closing the h_tape HANDLE in tar->stream
	mtar_close(&tar);
	return 0;


}