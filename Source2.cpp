#include <windows.h>
#include <stdio.h>

int main() {
    HMODULE hNTDLL = GetModuleHandleW(L"ntdll.dll");
    if (hNTDLL == NULL) {
        printf("[-] Failed getting handle to module");
        return 1;
    }
    LPCSTR NtFunctionName = "NtAddAtomEx"; // should be 0x69 syscall number :3
    DWORD    SyscallNumber = 0;
    UINT_PTR NtFunctionAddress = 0;

    NtFunctionAddress = (UINT_PTR)GetProcAddress(hNTDLL, NtFunctionName);
    if (NULL == (PVOID)NtFunctionAddress) {
        printf("[-] GetProcAddress failed, Error: %ld", GetLastError());
        return 1;
    }
    printf("Name: %s\nSSN: 0x%0.4x\n(?)SSN: 0x%0.4x", NtFunctionName, ((PBYTE)NtFunctionAddress + 1)[0], ((PBYTE)NtFunctionAddress + 4)[0]);

    return 0;
}