#include <Windows.h>
#include <TlHelp32.h>
#include <iostream>
#include <vector>

using namespace std;

DWORD GetProcId(const wchar_t* procName) {
	DWORD procId = 0;
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32 procEntry{ sizeof(PROCESSENTRY32) };
	if (Process32First(hSnap, &procEntry)) {
		do {
			if (!_wcsicmp(procEntry.szExeFile, procName)) {
				procId = procEntry.th32ProcessID;
				break;
			}
		} while (Process32Next(hSnap, &procEntry));
	}
	CloseHandle(hSnap);
	return procId;
}

uintptr_t GetModuleBaseAddress(DWORD procId, const wchar_t* modName) {
	uintptr_t modBaseAddr = 0;
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, procId);
	MODULEENTRY32 modEntry{ sizeof(modEntry) };
	if (Module32First(hSnap, &modEntry)) {
		do {
			if (!_wcsicmp(modEntry.szModule, modName)) {
				modBaseAddr = reinterpret_cast<uintptr_t>(modEntry.modBaseAddr);
				break;
			}
		} while (Module32Next(hSnap, &modEntry));
	}
	CloseHandle(hSnap);
	return modBaseAddr;
}

bool PatchEx(HANDLE hProcess, uintptr_t dest, BYTE* src, SIZE_T size) {
	DWORD oldProtect;
	if (VirtualProtectEx(hProcess, (LPVOID)dest, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
		SIZE_T bytesWritten;
		BOOL success = WriteProcessMemory(hProcess, (LPVOID)dest, src, size, &bytesWritten);
		VirtualProtectEx(hProcess, (LPVOID)dest, size, oldProtect, &oldProtect);
		return success && bytesWritten == size;
	}
	return false;
}


void ToggleGodMode(HANDLE hProcess, uintptr_t moduleBase, bool activate) {
	BYTE godmodeOn[10] = { 0xEB, 0x19, 0xC5, 0xFA, 0x5D, 0x80, 0x78, 0x01, 0x00, 0x00 };
	BYTE godmodeOff[10] = { 0x74, 0x19, 0xC5, 0xFA, 0x5D, 0x80, 0x78, 0x01, 0x00, 0x00 };
	uintptr_t godModeOffset = 0x02AB7C22;
	uintptr_t godModeAddr = moduleBase + godModeOffset;

	bool result = activate ?
		PatchEx(hProcess, godModeAddr, godmodeOn, sizeof(godmodeOn)) :
		PatchEx(hProcess, godModeAddr, godmodeOff, sizeof(godmodeOff));

	cout << (result ? "[+] God Mode toggled successfully.\n" : "[-] God Mode toggle failed!\n");
}

void ToggleFreeUpgrade(HANDLE hProcess, uintptr_t moduleBase, bool activate) {
	BYTE upgradeON[3] = { 0x90, 0x90, 0x90 };
	BYTE upgradeOFF[3] = { 0x89, 0x5E, 0x30 };
	uintptr_t upgradeOffset = 0x07F35432;
	uintptr_t upgradeAddr = moduleBase + upgradeOffset;

	bool result = activate ?
		PatchEx(hProcess, upgradeAddr, upgradeON, sizeof(upgradeON)) :
		PatchEx(hProcess, upgradeAddr, upgradeOFF, sizeof(upgradeOFF));

	cout << (result ? "[+] Free Upgrade toggled successfully.\n" : "[-] Free Upgrade toggle failed!\n");
}



struct Hack {
	string name;
	bool enabled;
	void(*ToggleFunc)(HANDLE, uintptr_t, bool);
};

// Display the menu and ASCII art logo
void DisplayMenu(const vector<Hack>& hacks, int selectedIndex) {
	system("cls"); // clear the console screen

	// ASCII Art (KRONOSQC ACS) at the top
	cout << R"( 
 _       _______ _______ _       _______ _______ _______ _______    _______ _______ _______ 
| \    /(  ____ (  ___  ( (    /(  ___  (  ____ (  ___  (  ____ \  (  ___  (  ____ (  ____ \
|  \  / | (    )| (   ) |  \  ( | (   ) | (    \| (   ) | (    \/  | (   ) | (    \| (    \/
|  (_/ /| (____)| |   | |   \ | | |   | | (_____| |   | | |        | (___) | |     | (_____ 
|   _ ( |     __| |   | | (\ \) | |   | (_____  | |   | | |        |  ___  | |     (_____  )
|  ( \ \| (\ (  | |   | | | \   | |   | |     ) | | /\| | |        | (   ) | |           ) |
|  /  \ | ) \ \_| (___) | )  \  | (___) /\____) | (_\ \ | (____/\  | )   ( | (____//\____) |
|_/    \|/   \__(_______|/    )_(_______\_______(____\/_(_______/  |/     \(_______\_______)
                                                                                            
)" << "\n";

	// Instructions for the menu controls
	cout << "Use UP/DOWN arrows to change hack selection\n";
	cout << "LEFT(Disable), RIGHT(Enable), ESC(Exit)\n\n";

	// Display the hack options
	for (size_t i = 0; i < hacks.size(); ++i) {
		cout << ((i == selectedIndex) ? " -> " : "    ")
			<< hacks[i].name << " : "
			<< (hacks[i].enabled ? "[ ON ]" : "[ OFF ]")
			<< "\n";
	}
}

int main() {
	const wchar_t* gameExeName = L"ACShadows_Plus.exe";
	DWORD pid = GetProcId(gameExeName);
	if (pid == 0) {
		cerr << "[-] Assassin's Creed Shadows is not running.\n";
		return 1;
	}

	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	if (!hProcess || hProcess == INVALID_HANDLE_VALUE) {
		cerr << "[-] Unable to open process. Run as administrator.\n";
		return 1;
	}

	uintptr_t moduleBase = GetModuleBaseAddress(pid, gameExeName);
	if (!moduleBase) {
		cerr << "[-] Module base address not found.\n";
		CloseHandle(hProcess);
		return 1;
	}

	vector<Hack> hacks = {
		{ "God Mode", false, ToggleGodMode },
		{ "Free Upgrade", false, ToggleFreeUpgrade }
	};

	int selectedIndex = 0;
	DisplayMenu(hacks, selectedIndex);

	bool running = true;

	while (running) {
		if (GetAsyncKeyState(VK_DOWN) & 0x8000) {
			selectedIndex = (selectedIndex + 1) % hacks.size();
			DisplayMenu(hacks, selectedIndex);
			Sleep(150);
		}

		if (GetAsyncKeyState(VK_UP) & 0x8000) {
			selectedIndex = (selectedIndex - 1 + hacks.size()) % hacks.size();
			DisplayMenu(hacks, selectedIndex);
			Sleep(150);
		}

		if (GetAsyncKeyState(VK_RIGHT) & 0x8000) {
			if (!hacks[selectedIndex].enabled) {
				hacks[selectedIndex].enabled = true;
				hacks[selectedIndex].ToggleFunc(hProcess, moduleBase, true);
				DisplayMenu(hacks, selectedIndex);
				Sleep(150);
			}
		}

		if (GetAsyncKeyState(VK_LEFT) & 0x8000) {
			if (hacks[selectedIndex].enabled) {
				hacks[selectedIndex].enabled = false;
				hacks[selectedIndex].ToggleFunc(hProcess, moduleBase, false);
				DisplayMenu(hacks, selectedIndex);
				Sleep(150);
			}
		}

		if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
			running = false;
			cout << "\n[+] Exiting...\n";
		}

		Sleep(50);
	}

	CloseHandle(hProcess);
	return 0;
}