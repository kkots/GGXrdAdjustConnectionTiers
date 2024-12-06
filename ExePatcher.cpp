#include "ExePatcher.h"

// The purpose of this file is to patch GuiltyGearXrd.exe to add instructions to it so that
// it stops rejecting the -1 (mirror) color and loops it around properly
#include "framework.h"
#include <iostream>
#include <vector>
#include "SharedFunctions.h"
#include "WinError.h"

using namespace std::literals;

// they banned strerror so I could do the exact same thing it did
static char errstr[1024] { '\0' };
static char sprintfbuf[1024] { '\0' };
extern const wchar_t* ERROR_STR;
extern bool somethingWentWrongWhenPatching;

// A class for simulating std::wcout << things.
// Prints everything into the debug console.
// Should I just extend wostream?.. Naah.
class OutputMachine {
public:
	OutputMachine& operator<<(char c) {
		char str[2] { '\0' };
		str[0] = c;
		OutputDebugStringA(str);
		return *this;
	}
	OutputMachine& operator<<(wchar_t c) {
		wchar_t wstr[2] { L'\0' };
		wstr[0] = c;
		OutputDebugStringW(wstr);
		return *this;
	}
	OutputMachine& operator<<(const wchar_t* wstr) {
		OutputDebugStringW(wstr);
		return *this;
	}
	OutputMachine& operator<<(const char* str) {
		OutputDebugStringA(str);
		return *this;
	}
	OutputMachine& operator<<(std::wostream& (*func)(std::wostream& in)) {
		if (func == std::endl<wchar_t, std::char_traits<wchar_t>>) {
			OutputDebugStringW(L"\n");
		}
		return *this;
	}
	OutputMachine& operator<<(std::ios_base& (*func)(std::ios_base& in)) {
		if (func == std::hex) {
			isHex = true;
		} else if (func == std::dec) {
			isHex = false;
		}
		return *this;
	}
	OutputMachine& operator<<(int i) {
		if (isHex) {
			sprintf_s(sprintfbuf, sizeof sprintfbuf, "%x", i);
			OutputDebugStringA(sprintfbuf);
		} else {
			OutputDebugStringW(std::to_wstring(i).c_str());
		}
		return *this;
	}
private:
	bool isHex = false;
} dout;

// byteSpecification is of the format "00 8f 1e ??". ?? means unknown byte.
// Converts a "00 8f 1e ??" string into two vectors:
// sig vector will contain bytes '00 8f 1e' for the first 3 bytes and 00 for every ?? byte.
// sig vector will be terminated with an extra 0 byte.
// mask vector will contain an 'x' character for every non-?? byte and a '?' character for every ?? byte.
// mask vector will be terminated with an extra 0 byte.
static void byteSpecificationToSigMask(const char* byteSpecification, std::vector<char>& sig, std::vector<char>& mask) {
	unsigned long long accumulatedNibbles = 0;
	int nibbleCount = 0;
	bool nibblesUnknown = false;
	const char* byteSpecificationPtr = byteSpecification;
	while (true) {
		char currentChar = *byteSpecificationPtr;
		if (currentChar != ' ' && currentChar != '\0') {
			char currentNibble = 0;
			if (currentChar >= '0' && currentChar <= '9' && !nibblesUnknown) {
				currentNibble = currentChar - '0';
			} else if (currentChar >= 'a' && currentChar <= 'f' && !nibblesUnknown) {
				currentNibble = currentChar - 'a' + 10;
			} else if (currentChar >= 'A' && currentChar <= 'F' && !nibblesUnknown) {
				currentNibble = currentChar - 'A' + 10;
			} else if (currentChar == '?' && (nibbleCount == 0 || nibblesUnknown)) {
				nibblesUnknown = true;
			} else {
				std::wstring errorMsg = L"Wrong byte specification: " + strToWStr(byteSpecification);
				MessageBoxW(NULL, errorMsg.c_str(), ERROR_STR, MB_OK);
				throw errorMsg;
			}
			accumulatedNibbles = (accumulatedNibbles << 4) | currentNibble;
			++nibbleCount;
			if (nibbleCount > 16) {
				std::wstring errorMsg = L"Wrong byte specification: " + strToWStr(byteSpecification);
				MessageBoxW(NULL, errorMsg.c_str(), ERROR_STR, MB_OK);
				throw errorMsg;
			}
		} else if (nibbleCount) {
			do {
				if (!nibblesUnknown) {
					sig.push_back(accumulatedNibbles & 0xff);
					mask.push_back('x');
					accumulatedNibbles >>= 8;
				} else {
					sig.push_back(0);
					mask.push_back('?');
				}
				nibbleCount -= 2;
			} while (nibbleCount > 0);
			nibbleCount = 0;
			nibblesUnknown = false;
			if (currentChar == '\0') {
				break;
			}
		}
		++byteSpecificationPtr;
	}
	sig.push_back('\0');
	mask.push_back('\0');
}

// 
// 

/// <summary>
/// A sigscan is an operation that looks for a pattern of bytes in a given region of binary data.
// The function returns the position of the start of the pattern within the region, relative to the start of the region,
// or -1 if the pattern is not found.
// The pattern is formed using -sig- and -mask- vectors and is described in byteSpecificationToSigMask function.
/// </summary>
/// <param name="start">The start of the searched region.</param>
/// <param name="end">The non-inclusive end of the searched region.</param>
/// <param name="sig">Signature, for ex. "\x80\x9f\xaa\x00\x00". Described in byteSpecificationToSigMask function.</param>
/// <param name="mask">Mask, for ex. "xxx??". Described in byteSpecificationToSigMask function.</param>
/// <returns>Position of the first matching pattern within the region, relative to start.
/// -1 if the pattern is not found.</returns>
static int sigscanSigMask(const char* start, const char* end, const char* sig, const char* mask) {
    const char* startPtr = start;
    const size_t maskLen = strlen(mask);
    const size_t seekLength = end - start - maskLen + 1;
    for (size_t seekCounter = seekLength; seekCounter != 0; --seekCounter) {
        const char* stringPtr = startPtr;

        const char* sigPtr = sig;
        for (const char* maskPtr = mask; true; ++maskPtr) {
            const char maskPtrChar = *maskPtr;
            if (maskPtrChar != '?') {
                if (maskPtrChar == '\0') return (int)(startPtr - start);
                if (*sigPtr != *stringPtr) break;
            }
            ++sigPtr;
            ++stringPtr;
        }
        ++startPtr;
    }
    return -1;
}

/// <summary>
/// A sigscan is an operation that looks for a pattern of bytes in a given region of binary data.
// The function returns the position of the start of the pattern relative to -fileStart-,
// or -1 if the pattern is not found.
// The pattern is formed using -sig- and -mask- vectors and is described in byteSpecificationToSigMask function.
/// </summary>
/// <param name="fileStart">The address of the start of the file data that was wholly read and loaded into memory.
/// Is not used to limit the searched region. Is only used to calculate the offset of the start of the pattern
/// relative to this -fileStart-.</param>
/// <param name="start">The start of the memory region to be searched. This start is expected to be no less than
/// -fileStart-.</param>
/// <param name="end">The non-inclusive end of the memory region to be searched.</param>
/// <param name="sig">Signature, for ex. "\x80\x9f\xaa\x00\x00". Described in byteSpecificationToSigMask function.</param>
/// <param name="mask">Mask, for ex. "xxx??". Described in byteSpecificationToSigMask function.</param>
/// <returns>Position of the first matching pattern within the in-memory (wholly read) file, relative to -fileStart-.
/// -1 if the pattern is not found.</returns>
static int sigscan(const char* fileStart, const char* start, const char* end, const char* sig, const char* mask) {
    int result = sigscanSigMask(start, end, sig, mask);
	if (result == -1) {
		return -1;
	} else {
		return result + (int)(start - fileStart);
	}
}

/// <summary>
/// A sigscan is an operation that looks for a pattern of bytes in a given region of binary data.
/// The function returns the position of the start of the pattern relative to -fileStart-,
/// or -1 if the pattern is not found.
/// The pattern is formed using byteSpecification.
/// </summary>
/// <param name="fileStart">The address of the start of the file data that was wholly read and loaded into memory.
/// Is not used to limit the searched region. Is only used to calculate the offset of the start of the pattern
/// relative to this -fileStart-.</param>
/// <param name="start">The start of the memory region to be searched. This start is expected to be no less than
/// -fileStart-.</param>
/// <param name="end">The non-inclusive end of the memory region to be searched.</param>
/// <param name="byteSpecification">byteSpecification is of the format "00 8f 1e ??". ?? means unknown byte, which can match any byte.</param>
/// <returns>Position of the first matching pattern within the in-memory (wholly read) file, relative to -fileStart-.
/// -1 if the pattern is not found.</returns>
static int sigscan(const char* fileStart, const char* start, const char* end, const char* byteSpecification) {
	
	std::vector<char> sig;
	std::vector<char> mask;
	byteSpecificationToSigMask(byteSpecification, sig, mask);

    int result = sigscanSigMask(start, end, sig.data(), mask.data());
	if (result == -1) {
		return -1;
	} else {
		return result + (int)(start - fileStart);
	}
}

static bool readWholeFile(FILE* file, std::vector<char>& wholeFile) {
    fseek(file, 0, SEEK_END);
    size_t fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    wholeFile.resize(fileSize);
    char* wholeFilePtr = wholeFile.data();
    size_t readBytesTotal = 0;
    while (true) {
        size_t sizeToRead = 1024;
        if (fileSize - readBytesTotal < 1024) sizeToRead = fileSize - readBytesTotal;
        if (sizeToRead == 0) break;
        size_t readBytes = fread(wholeFilePtr, 1, sizeToRead, file);
        if (readBytes != sizeToRead) {
            if (ferror(file)) {
                return false;
            }
            // assume feof
            break;
        }
        wholeFilePtr += sizeToRead;
        readBytesTotal += sizeToRead;
    }
    return true;
}

// Writes bytes into the file at the given -pos- without doing any shifting, meaning it overwrites any data
// at this region.
static bool writeBytesToFile(FILE* file, int pos, const char* bytesToWrite, size_t bytesToWriteCount) {
    fseek(file, pos, SEEK_SET);
    size_t writtenBytes = fwrite(bytesToWrite, 1, bytesToWriteCount, file);
    if (writtenBytes != bytesToWriteCount) {
        return false;
    }
    return true;
}

/// <summary>
/// Writes bytes into the file at the given -pos- without doing any shifting, meaning it overwrites any data
/// at this region.
/// </summary>
/// <param name="file">The file to write into.</param>
/// <param name="pos">The position to write at in the file.</param>
/// <param name="byteSpecification">byteSpecification is of the format "00 8f 1e". ?? bytes are interpreted as 00 here.</param>
/// <returns>true if no error encountered.</returns>
static bool writeBytesToFile(FILE* file, int pos, const char* byteSpecification) {
	std::vector<char> sig;
	std::vector<char> mask;
	byteSpecificationToSigMask(byteSpecification, sig, mask);

    fseek(file, pos, SEEK_SET);
    size_t writtenBytes = fwrite(sig.data(), 1, sig.size() - 1, file);
    if (writtenBytes != sig.size() - 1) {
        return false;
    }
    return true;
}

// A section of an .exe file, for example .text, .reloc section, etc.
struct Section {
    std::string name;

	// RVA. Virtual address offset relative to the virtual address start of the entire .exe.
	// So let's say the whole .exe starts at 0x400000 and RVA is 0x400.
	// That means the non-relative VA is 0x400000 + RVA = 0x400400.
	// Note that the .exe, although it does specify a base virtual address for itself on the disk,
	// may actually be loaded anywhere in the RAM once it's launched, and that RAM location will
	// become its base virtual address.
    uintptr_t relativeVirtualAddress = 0;

	// VA. Virtual address within the .exe.
	// A virtual address is the location of something within the .exe once it's loaded into memory.
	// An on-disk, file .exe is usually smaller than when it's loaded so it creates this distinction
	// between raw address and virtual address.
    uintptr_t virtualAddress = 0;

	// The size in terms of virtual address space.
    uintptr_t virtualSize = 0;

	// Actual position of the start of this section's data within the file.
    uintptr_t rawAddress = 0;

	// Size of this section's data on disk in the file.
    uintptr_t rawSize = 0;
};

// Reads info about sections such as .text, .reloc, etc.
static std::vector<Section> readSections(FILE* file, uintptr_t* outImageBase = nullptr) {

    std::vector<Section> result;

    uintptr_t peHeaderStart = 0;
    fseek(file, 0x3C, SEEK_SET);
    fread(&peHeaderStart, 4, 1, file);

    unsigned short numberOfSections = 0;
    fseek(file, (int)(peHeaderStart + 0x6), SEEK_SET);
    fread(&numberOfSections, 2, 1, file);

    uintptr_t optionalHeaderStart = peHeaderStart + 0x18;

    unsigned short optionalHeaderSize = 0;
    fseek(file, (int)(peHeaderStart + 0x14), SEEK_SET);
    fread(&optionalHeaderSize, 2, 1, file);

    uintptr_t imageBase = 0;
    fseek(file, (int)(peHeaderStart + 0x34), SEEK_SET);
    fread(&imageBase, 4, 1, file);
    if (outImageBase) *outImageBase = imageBase;

    uintptr_t sectionsStart = optionalHeaderStart + optionalHeaderSize;
    uintptr_t sectionStart = sectionsStart;
    for (size_t sectionCounter = numberOfSections; sectionCounter != 0; --sectionCounter) {
        Section newSection;
        fseek(file, (int)sectionStart, SEEK_SET);
        newSection.name.resize(8);
        fread(&newSection.name.front(), 1, 8, file);
        newSection.name.resize(strlen(newSection.name.c_str()));
        fread(&newSection.virtualSize, 4, 1, file);
        fread(&newSection.relativeVirtualAddress, 4, 1, file);
        newSection.virtualAddress = imageBase + newSection.relativeVirtualAddress;
        fread(&newSection.rawSize, 4, 1, file);
        fread(&newSection.rawAddress, 4, 1, file);
        result.push_back(newSection);
        sectionStart += 40;
    }

    return result;
}

uintptr_t rawToVa(const std::vector<Section>& sections, uintptr_t rawAddr) {
    if (sections.empty()) return 0;
    auto it = sections.cend();
    --it;
    while (true) {
        const Section& section = *it;
        if (rawAddr >= section.rawAddress) {
            return rawAddr - section.rawAddress + section.virtualAddress;
        }
        if (it == sections.cbegin()) break;
        --it;
    }
    return 0;
}

uintptr_t vaToRaw(const std::vector<Section>& sections, uintptr_t va) {
    if (sections.empty()) return 0;
    auto it = sections.cend();
    --it;
    while (true) {
        const Section& section = *it;
        if (va >= section.virtualAddress) {
            return va - section.virtualAddress + section.rawAddress;
        }
        if (it == sections.cbegin()) break;
        --it;
    }
    return 0;
}

// relativeCallAddr points to the start of a 'relative call' instruction, relative to the fileStart.
// The 'relative call' instruction is one byte of the instruction command, followed by 4 bytes relative offset. Offset can be negative (start with FF...).
// The relative offset is relative to the instruction that goes after the call smh, and the call instruction itself is 5 bytes in total
DWORD followRelativeCall(const char* fileStart, const char* textSectionBegin, const char* textSectionEnd, const std::vector<Section>& sections, DWORD relativeCallAddr) {
	if (fileStart + relativeCallAddr + 1 < textSectionBegin || fileStart + relativeCallAddr + 1 + 4 > textSectionEnd) {
		return 0;
	}
	DWORD nextInstVa = (DWORD)rawToVa(sections, relativeCallAddr + 5);
	DWORD result = nextInstVa + *(int*)(fileStart + relativeCallAddr + 1);
	// Calls can also have absolute addresses so check which one you got
	result = (DWORD)vaToRaw(sections, result);
	if (fileStart + result < textSectionBegin || fileStart + result > textSectionEnd) {
		return 0;
	}
	return result;
}

bool parseCmpInstr(char* instrStart, const char* textSectionEnd, int* outValue, int* outSize) {
	if (instrStart == textSectionEnd - 1) return false;
	BYTE instr = (BYTE)*instrStart;
	if (instr && (BYTE)*(instrStart + 1) == 0xf8) {
		*outValue = (int)(*(instrStart + 2));
		*outSize = 3;
		return true;
	} else if (instr == 0x3d) {
		if (instrStart + 5 > textSectionEnd) return false;
		*outValue = *(int*)(instrStart + 1);
		*outSize = 5;
		return true;
	}
	return false;
}

#define assertWithBox(stuff) if (!(stuff)) { \
	MessageBoxW(mainWindow, mismatchMsg, ERROR_STR, MB_OK); \
	return false; \
}

#define assertBytes(bytes) \
	sig.clear(); \
	byteSpecificationToSigMask(bytes, sig, mask); \
	assertWithBox(funcPtr + sig.size() - 1 < textSectionEnd) \
	assertWithBox(memcmp(funcPtr, sig.data(), sig.size() - 1) == 0) \
	funcPtr += sig.size() - 1;

#define assertBytesNoBox(bytes) \
	sig.clear(); \
	byteSpecificationToSigMask(bytes, sig, mask); \
	myAssert(funcPtr + sig.size() - 1 < textSectionEnd) \
	myAssert(memcmp(funcPtr, sig.data(), sig.size() - 1) == 0) \
	funcPtr += sig.size() - 1;

#define assertBytesNoBoxNoAssignment \
	myAssert(funcPtr + sig.size() - 1 < textSectionEnd) \
	myAssert(memcmp(funcPtr, sig.data(), sig.size() - 1) == 0) \
	funcPtr += sig.size() - 1;

bool parseFunction(HWND mainWindow, char* funcPtr, const char* textSectionEnd, int* pingValues, size_t* functionSize) {
	char* funcPtrStart = funcPtr;
	std::vector<char> sig;
	std::vector<char> mask;
	
	#define myAssert(stuff) if (!(stuff)) { \
		return false; \
	}
	
	assertBytesNoBox("85 c0 78")
	// TEST EAX,EAX
	// JS ...
	funcPtr += 1;  // skip jmp instruction's offset (1 byte)
	
	sig.clear();
	byteSpecificationToSigMask("7d 06 b8 04 00 00 00 c3", sig, mask);  // will add one extra 00 byte to the end of sig
	// JGE 0x6  // jump 6 bytes from the next instruction
	// MOV EAX,0x4
	// RET
	
	for (int i = 4; i >= 1; --i) {
		
		int cmpVal;
		int cmpSize;
		myAssert(parseCmpInstr(funcPtr, textSectionEnd, &cmpVal, &cmpSize))
		funcPtr += cmpSize;
		
		if (pingValues) pingValues[i] = cmpVal;
		
		sig[3] = i;
		assertBytesNoBoxNoAssignment
		
	}
	
	assertBytesNoBox("33 c0 c3")
	// XOR EAX,EAX
	// RET
	#undef myAssert
	
	if (functionSize) *functionSize = funcPtr - funcPtrStart;
	
	return true;
}

bool regenerateFunction(HWND mainWindow, char* funcPtr, const char* textSectionEnd, int* pingValues, size_t* oldFuncSize, std::vector<char>& output) {
	output.clear();
	
	if (!parseFunction(mainWindow, funcPtr, textSectionEnd, nullptr, oldFuncSize)) {
		return false;
	}
	
	int totalJumpDistance = 0x2c;
	for (int i = 1; i < 5; ++i) {
		if (pingValues[i] + 1 > 127) {
			totalJumpDistance += 2;
		}
	}
	
	std::vector<char> sig;
	std::vector<char> mask;
	byteSpecificationToSigMask("85 c0 78", sig, mask);  // will add one extra 00 byte to the end of sig
	// TEST EAX,EAX
	// JS ...
	
	output.insert(output.end(), sig.begin(), sig.end() - 1);
	output.push_back((char)totalJumpDistance);  // the jump offset
	
	#define setBytes(name, bytes) \
		name.clear(); \
		byteSpecificationToSigMask(bytes, name, mask);  // will add one extra 00 byte to the end of sig
	
	#define insertBytes(bytes) \
		setBytes(sig, bytes) \
		output.insert(output.end(), sig.begin(), sig.end() - 1);
	
	#define insertWithoutSetBytes \
		output.insert(output.end(), sig.begin(), sig.end() - 1);
	
	setBytes(sig, "7d 06 b8 04 00 00 00 c3")
	// JGE 0x6  // jump 6 bytes from the next instruction
	// MOV EAX,0x4
	// RET
	
	for (int i = 4; i >= 1; --i) {
		int pingValue = pingValues[i] + 1;
		if (pingValue <= 127) {
			output.insert(output.end(), {'\x83', '\xf8'});
			// CMP ...
			output.push_back((char)pingValue);  // the value to compare to (1 byte)
		} else {
			output.push_back('\x3d');
			output.resize(output.size() + 4);
			memcpy(output.data() + output.size() - 4, &pingValue, 4);
		}
		sig[3] = i;
		insertWithoutSetBytes
	}
	
	insertBytes("33 c0 c3")
	// XOR EAX,EAX
	// RET
	
	#undef setBytes
	#undef insertBytes
	#undef insertWithoutSetBytes
	
	return true;
}

bool findFunction1(HWND mainWindow,
			char* wholeFileBegin,
			char* textSectionBegin,
			char* textSectionEnd,
			char* rdataSectionBegin,
			char* rdataSectionEnd,
			const std::vector<Section>& sections,
			char** foundFunc) {
	const wchar_t* sigTxt = L"Net_Room_07_%02d";
	size_t sigTxtSize = (wcslen(sigTxt) + 1) * 2;
	std::vector<char> sig(sigTxtSize + 1, '\0');
	memcpy(sig.data(), sigTxt, sigTxtSize);
	std::vector<char> mask(sigTxtSize + 1, 'x');
	mask[sigTxtSize] = '\0';
	int netRoomStringPos = sigscan(wholeFileBegin, rdataSectionBegin, rdataSectionEnd, sig.data(), mask.data());
	if (netRoomStringPos == -1) {
		MessageBoxW(mainWindow, (L"Could not find '" + std::wstring(sigTxt) + L"' string in the EXE file.").c_str(), ERROR_STR, MB_OK);
		return false;
	}
	
	sig.clear();
	mask.clear();
	byteSpecificationToSigMask("e8 ?? ?? ?? ?? 50 8d 8c 24 70 03 00 00 68 ?? ?? ?? ??", sig, mask);
	*(DWORD*)(sig.data() + 14) = (DWORD)rawToVa(sections, netRoomStringPos);
	strncpy_s(mask.data() + 14, mask.size() - 14, "xxxx", 4);
	int func1UsagePlace = sigscan(wholeFileBegin, textSectionBegin, textSectionEnd, sig.data(), mask.data());
	if (func1UsagePlace == -1) {
		MessageBoxW(mainWindow, L"Could not find function 1 in the EXE file.", ERROR_STR, MB_OK);
		return false;
	}
	
	DWORD func1Addr = followRelativeCall(wholeFileBegin, textSectionBegin, textSectionEnd, sections, func1UsagePlace);
	if (!func1Addr) {
		MessageBoxW(mainWindow, L"Could not follow a call to function 1 in the EXE file.", ERROR_STR, MB_OK);
		return false;
	}
	char* funcPtr = wholeFileBegin + func1Addr;
	
	const wchar_t* mismatchMsg = L"Function 1 is not what is expected in the EXE file.";
	
	assertBytes("8b 44 24 04 50 e8")
	// MOV EAX,dword ptr [ESP + param_1]
	// PUSH EAX
	// CALL ?? ?? ?? ?? (FUN_00f7f880)
	funcPtr += 4;  // skip call offset (4 bytes)
	
	assertBytes("98 83 c4 04")
	// CWDE
	// ADD ESP,0x4
	
	if (foundFunc) *foundFunc = funcPtr;
	return true;
}

bool findFunction2(HWND mainWindow,
			char* wholeFileBegin,
			char* textSectionBegin,
			char* textSectionEnd,
			char* rdataSectionBegin,
			char* rdataSectionEnd,
			const std::vector<Section>& sections,
			char** foundFunc) {
	
	std::vector<char> sig;
	std::vector<char> mask;
	byteSpecificationToSigMask("8d 8c 81 98 bc 00 00 8b 42 08 ff d0 8b 48 44 51 e8", sig, mask);
	int func2UsagePlace = sigscan(wholeFileBegin, textSectionBegin, textSectionEnd, sig.data(), mask.data());
	if (func2UsagePlace == -1) {
		MessageBoxW(mainWindow, L"Could not find function 2 in the EXE file.", ERROR_STR, MB_OK);
		return false;
	}
	
	DWORD func2Addr = followRelativeCall(wholeFileBegin, textSectionBegin, textSectionEnd, sections, func2UsagePlace + 16);
	if (!func2Addr) {
		MessageBoxW(mainWindow, L"Could not follow a call to function 2 in the EXE file.", ERROR_STR, MB_OK);
		return false;
	}
	char* funcPtr = wholeFileBegin + func2Addr;
	
	const wchar_t* mismatchMsg = L"Function 2 is not what is expected in the EXE file.";
	
	assertBytes("8b 44 24 04")
	// MOV EAX,dword ptr [ESP + param_1]
	
	if (foundFunc) *foundFunc = funcPtr;
	return true;
}

bool obtainValuesFromExe(const std::wstring& szFile, PingValues* outValues, std::wstring& importantRemarks, HWND mainWindow) {
	std::wstring fileName = getFileName(szFile);
	
	
    FILE* file = nullptr;
    if (_wfopen_s(&file, szFile.c_str(), L"rb") || !file) {
		if (errno) {
			strerror_s(errstr, errno);
			MessageBoxW(mainWindow, (L"Failed to open " + fileName + L" file: " + strToWStr(errstr)).c_str(), ERROR_STR, MB_OK);
		} else {
			MessageBoxW(mainWindow, (L"Failed to open " + fileName + L" file.").c_str(), ERROR_STR, MB_OK);
		}
        if (file) {
            fclose(file);
        }
        return false;
    }
    
    class CloseFileOnExit {
	public:
		~CloseFileOnExit() {
			if (file) fclose(file);
		}
		FILE* file = NULL;
	} closeFileOnExit;

	closeFileOnExit.file = file;
	
	std::vector<Section> sections = readSections(file);
    if (sections.empty()) {
		MessageBoxW(mainWindow, (L"Error reading " + fileName + L": Failed to read sections.").c_str(), ERROR_STR, MB_OK);
        return false;
    }
    
	Section* textSection = nullptr;
	Section* rdataSection = nullptr;
	for (Section& section : sections) {
		if (section.name == ".text" ) {
			textSection = &section;
		}
		if (section.name == ".rdata" ) {
			rdataSection = &section;
		}
	}
	if (textSection == nullptr) {
		MessageBoxW(mainWindow, (L"Error reading " + fileName + L": .text section not found.").c_str(), ERROR_STR, MB_OK);
        return false;
	}
	if (rdataSection == nullptr) {
		MessageBoxW(mainWindow, (L"Error reading " + fileName + L": .rdata section not found.").c_str(), ERROR_STR, MB_OK);
        return false;
	}
	
    std::vector<char> wholeFile;
    if (!readWholeFile(file, wholeFile)) {
        strerror_s(errstr, sizeof errstr, errno);
		MessageBoxW(mainWindow, (L"Error reading file: " + strToWStr(errstr)).c_str(), ERROR_STR, MB_OK);
		return false;
	}
    char* wholeFileBegin = wholeFile.data();
	char* textSectionBegin = wholeFileBegin + textSection->rawAddress;
	char* textSectionEnd = wholeFileBegin + textSection->rawAddress + textSection->rawSize;
	char* rdataSectionBegin = wholeFileBegin + rdataSection->rawAddress;
	char* rdataSectionEnd = wholeFileBegin + rdataSection->rawAddress + rdataSection->rawSize;
	
	char* funcPtr;
	if (!findFunction1(mainWindow,
			wholeFileBegin,
			textSectionBegin,
			textSectionEnd,
			rdataSectionBegin,
			rdataSectionEnd,
			sections,
			&funcPtr)) {
		return false;
	}
	
	const wchar_t* mismatchMsg = L"Function 1 is not what is expected in the EXE file.";
	int pingValues[5];
	assertWithBox(parseFunction(mainWindow, funcPtr, textSectionEnd, pingValues, nullptr))
	
	for (int i = 1; i < 4; ++i) {
		if (pingValues[i] < pingValues[i + 1]) {
			importantRemarks += L"The values parsed from the function in the EXE file are out of order.";
			break;
		}
	}
	
	(*outValues)[0].max = INT_MAX;
	(*outValues)[0].min = pingValues[1];
	for (int i = 1; i < 4; ++i) {
		(*outValues)[i].max = pingValues[i] - 1;
		(*outValues)[i].min = pingValues[i + 1];
	}
	(*outValues)[4].max = pingValues[4] - 1;
	(*outValues)[4].min = 0;
	
	return true;
}

bool overwriteFunction(HWND mainWindow, char* funcPtr, char* textSectionEnd, int* newPings, const wchar_t* name, FILE* file, char* wholeFileBegin, bool* changedSomething) {
	std::vector<char> funcData;
	size_t oldFuncSize;
	if (!regenerateFunction(mainWindow, funcPtr, textSectionEnd, newPings, &oldFuncSize, funcData)) {
		std::wstring errorStr = L"Failed to find or parse ";
		errorStr += name;
		errorStr += L" in the EXE.";
		MessageBoxW(mainWindow, errorStr.c_str(), ERROR_STR, MB_OK);
        return false;
	}
	bool nothingWasChanged = false;
	int positionToWriteFrom = (int)(funcPtr - wholeFileBegin);
	size_t sizeToWrite = 0;
	if (funcData.size() < oldFuncSize) {
		nothingWasChanged = memcmp(funcPtr, funcData.data(), funcData.size()) == 0;
		memcpy(funcPtr, funcData.data(), funcData.size());
		char* cEnd = funcPtr + oldFuncSize;
		for (char* c = funcPtr + funcData.size(); c != cEnd; ++c) {
			if (*c != '\xCC') {
				nothingWasChanged = false;
				*c = '\xCC';
			}
		}
		sizeToWrite = cEnd - funcPtr;
	} else if (funcData.size() > oldFuncSize) {
		int diff = (int)(funcData.size() - oldFuncSize);
		for (char* c = funcPtr + oldFuncSize; diff && c < textSectionEnd; ++c) {
			if (*c != '\xCC') {
				std::wstring errorStr = L"Not enough 0xCC bytes to write ";
				errorStr += name;
				errorStr += L" in the EXE.";
				MessageBoxW(mainWindow, errorStr.c_str(), ERROR_STR, MB_OK);
		        return false;
			}
			--diff;
		}
		nothingWasChanged = memcmp(funcPtr, funcData.data(), funcData.size()) == 0;
		memcpy(funcPtr, funcData.data(), funcData.size());
		sizeToWrite = funcData.size();
	} else {
		nothingWasChanged = memcmp(funcPtr, funcData.data(), funcData.size()) == 0;
		memcpy(funcPtr, funcData.data(), funcData.size());
		sizeToWrite = funcData.size();
	}
	if (!nothingWasChanged) {
		if (changedSomething) *changedSomething = true;
		fseek(file, positionToWriteFrom, SEEK_SET);
		size_t bytesWritten = fwrite(funcPtr, 1, sizeToWrite, file);
		if (bytesWritten != sizeToWrite) {
			*errstr = '\0';
			if (ferror(file)) {
				strerror_s(errstr, sizeof errstr, errno);
			}
			std::wstring errorStr = L"An error occured when writing into the GuiltyGearXrd.exe file";
			if (*errstr != '\0') {
				errorStr += L": " + strToWStr(errstr) + L"\n";
			} else {
				errorStr += L". ";
			}
			errorStr += L" The file may have been written to partially and may be corrupt."
				L" Attempt patching again, or use the EXE with extreme caution, or restore it from a backup (manually).";
			MessageBoxW(mainWindow, errorStr.c_str(), ERROR_STR, MB_OK);
			somethingWentWrongWhenPatching = true;
		}
	}
	return true;
}

static std::wstring backupFilePath;

bool performExePatching(const std::wstring& szFile, std::wstring& importantRemarks, HWND mainWindow, bool mayCreateBackup, int* newPings) {
    std::wstring fileName = getFileName(szFile);

	// Even if we're not modifying the file right away, we're still opening with the write access to check for the rights
    FILE* file = nullptr;
    if (_wfopen_s(&file, szFile.c_str(), L"r+b") || !file) {
		if (errno) {
			strerror_s(errstr, errno);
			MessageBoxW(mainWindow, (L"Failed to open " + fileName + L" file: " + strToWStr(errstr)).c_str(), ERROR_STR, MB_OK);
		} else {
			MessageBoxW(mainWindow, (L"Failed to open " + fileName + L" file.").c_str(), ERROR_STR, MB_OK);
		}
        if (file) {
            fclose(file);
        }
        return false;
    }

	class CloseFileOnExit {
	public:
		~CloseFileOnExit() {
			if (file) fclose(file);
		}
		FILE* file = NULL;
	} closeFileOnExit;

	closeFileOnExit.file = file;
	
    std::vector<Section> sections = readSections(file);
    if (sections.empty()) {
		MessageBoxW(mainWindow, (L"Error reading " + fileName + L": Failed to read sections.").c_str(), ERROR_STR, MB_OK);
        return false;
    }

	Section* textSection = nullptr;
	Section* rdataSection = nullptr;
	for (Section& section : sections) {
		if (section.name == ".text" ) {
			textSection = &section;
		}
		if (section.name == ".rdata" ) {
			rdataSection = &section;
		}
	}
	if (textSection == nullptr) {
		MessageBoxW(mainWindow, (L"Error reading " + fileName + L": .text section not found.").c_str(), ERROR_STR, MB_OK);
        return false;
	}
	if (rdataSection == nullptr) {
		MessageBoxW(mainWindow, (L"Error reading " + fileName + L": .rdata section not found.").c_str(), ERROR_STR, MB_OK);
        return false;
	}

    std::vector<char> wholeFile;
    if (!readWholeFile(file, wholeFile)) {
        strerror_s(errstr, sizeof errstr, errno);
		MessageBoxW(mainWindow, (L"Error reading file: " + strToWStr(errstr)).c_str(), ERROR_STR, MB_OK);
		return false;
	}
    
    char* wholeFileBegin = wholeFile.data();
	char* textSectionBegin = wholeFileBegin + textSection->rawAddress;
	char* textSectionEnd = wholeFileBegin + textSection->rawAddress + textSection->rawSize;
	char* rdataSectionBegin = wholeFileBegin + rdataSection->rawAddress;
	char* rdataSectionEnd = wholeFileBegin + rdataSection->rawAddress + rdataSection->rawSize;
	
	static bool isFirstRunOutOfARecursiveSeriesOfCalls = true;
	if (mayCreateBackup) {
		
		fclose(file);
		closeFileOnExit.file = NULL;

		backupFilePath = generateUniqueBackupName(szFile);
		dout << "Will use backup file path: " << backupFilePath.c_str() << std::endl;

		if (!CopyFileW(szFile.c_str(), backupFilePath.c_str(), false)) {
			WinError winErr;
			int response = MessageBoxW(mainWindow, (L"Failed to create a backup copy of " + fileName + L" ("
				+ backupFilePath + L"): " + winErr.getMessage() +
				L"Do you want to continue anyway? You won't be able to revert the file to the original. Press OK to agree.\n").c_str(),
				L"Warning", MB_OKCANCEL);
			if (response != IDOK) {
				return false;
			}
		} else {
			dout << "Backup copy created successfully.\n";
			importantRemarks += fileName + L" has been backed up to " + backupFilePath + L"\n";
		}
		isFirstRunOutOfARecursiveSeriesOfCalls = false;
		return performExePatching(szFile, importantRemarks, mainWindow, false, newPings);
	} else if (isFirstRunOutOfARecursiveSeriesOfCalls) {
		importantRemarks += L"No backup copy is created.\n";
		backupFilePath.clear();
	} else {
		isFirstRunOutOfARecursiveSeriesOfCalls = true;
	}
	
	char* func1Ptr;
	if (!findFunction1(mainWindow,
			wholeFileBegin,
			textSectionBegin,
			textSectionEnd,
			rdataSectionBegin,
			rdataSectionEnd,
			sections,
			&func1Ptr)) {
		return false;
	}
	
	bool changedSomething = false;
	if (!overwriteFunction(mainWindow, func1Ptr, textSectionEnd, newPings, L"function 1", file, wholeFileBegin, &changedSomething)) {
		return false;
	}
	
	char* func2Ptr;
	if (!findFunction2(mainWindow,
			wholeFileBegin,
			textSectionBegin,
			textSectionEnd,
			rdataSectionBegin,
			rdataSectionEnd,
			sections,
			&func2Ptr)) {
		return false;
	}
	
	if (!overwriteFunction(mainWindow, func2Ptr, textSectionEnd, newPings, L"function 2", file, wholeFileBegin, &changedSomething)) {
		return false;
	}
	
	if (!changedSomething) {
		importantRemarks += L"The file is already patched exactly as needed. Patching wasn't done.\n";
		dout << "Already patched\n";
		if (!backupFilePath.empty()) {
			std::wstring messageStr = L"The file is already patched exactly as needed. Patching wasn't done.\n"
				L" Do you wish to delete the backup copy that was created - '";
			messageStr += backupFilePath;
			messageStr += L"', as it is not needed anymore?";
			int response = MessageBoxW(mainWindow, messageStr.c_str(), L"Message", MB_YESNO);
			if (response == IDYES) {
				dout << "Deleting backup ";
				dout << backupFilePath.c_str();
				dout << L"\n";
				if (!DeleteFileW(backupFilePath.c_str())) {
					WinError winErr;
					messageStr = L"Failed to delete backup copy '";
					messageStr += backupFilePath;
					messageStr += L"': ";
					messageStr += winErr.getMessage();
					MessageBoxW(mainWindow, messageStr.c_str(), ERROR_STR, MB_OK);
					importantRemarks += messageStr;
					importantRemarks += L'\n';
				} else {
					importantRemarks += L"The backup copy '";
					importantRemarks += backupFilePath;
					importantRemarks += L"' has been deleted.\n";
				}
			}
		}
	} else {
		importantRemarks += L"Patched GuiltyGearXrd.exe.\n";
		if (somethingWentWrongWhenPatching) {
			dout << "Patched with errors\n";
		} else {
	    	dout << "Patched successfully\n";
		}
	}
	
    fclose(file);
	closeFileOnExit.file = NULL;
	return true;
}
