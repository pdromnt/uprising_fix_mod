#include <fstream>
#include <iostream>
#include <string>
#include <windows.h>

// Globals
HMODULE g_hDsound = NULL;
std::string g_currentLflPath = "";
std::string g_baseDir = "";

// Function pointer types
typedef HRESULT(WINAPI *DirectSoundCreate_t)(LPGUID, void **, void *);
DirectSoundCreate_t RealDirectSoundCreate = NULL;

typedef HANDLE(WINAPI *CreateFileA_t)(LPCSTR, DWORD, DWORD,
                                      LPSECURITY_ATTRIBUTES, DWORD, DWORD,
                                      HANDLE);
CreateFileA_t RealCreateFileA = NULL;

// Logging helper
void Log(const std::string &msg) {

  // Uncomment the lines below to enable logging
  //
  // std::ofstream outfile("uprising_hook.log", std::ios_base::app);
  // if (outfile.is_open()) {
  //   outfile << msg << std::endl;
  // }
}

// Trim helper
std::string Trim(const std::string &str) {
  size_t first = str.find_first_not_of(" \t\r\n");
  if (std::string::npos == first)
    return str;
  size_t last = str.find_last_not_of(" \t\r\n");
  return str.substr(first, (last - first + 1));
}

// Case insensitive ends_with
bool EndsWith(const std::string &str, const std::string &suffix) {
  if (str.length() < suffix.length())
    return false;
  std::string s = str.substr(str.length() - suffix.length());
  return _stricmp(s.c_str(), suffix.c_str()) == 0;
}

// Parse LFL to get sky
std::string GetSkyFromLfl(const std::string &lflPath) {
  std::ifstream infile(lflPath);
  if (!infile.is_open()) {
    Log("Failed to open LFL file: " + lflPath);
    return "";
  }

  std::string line;
  while (std::getline(infile, line)) {
    if (line.find("POLY_SKY:") != std::string::npos) {
      size_t pos = line.find(":");
      if (pos != std::string::npos && pos + 1 < line.length()) {
        std::string val = Trim(line.substr(pos + 1));
        return val;
      }
    }
  }
  return "";
}

HANDLE WINAPI MyCreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess,
                            DWORD dwShareMode,
                            LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                            DWORD dwCreationDisposition,
                            DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
  if (lpFileName) {
    std::string fname_str = lpFileName;

    // Watch for level load
    // Game loads .slk or .cam from GRIDS folder
    if (EndsWith(fname_str, ".slk") || EndsWith(fname_str, ".cam")) {
      Log("Level file detected: " + fname_str);

      // Assume fname_str is like "GRIDS\anshar.slk" or just "anshar.slk"
      // We want to construct the matching .lfl path.
      std::string lflPromise =
          fname_str.substr(0, fname_str.find_last_of('.')) + ".lfl";

      g_currentLflPath = lflPromise;
      Log("Set current LFL target: " + g_currentLflPath);
    }

    // Watch for sky load
    if (EndsWith(fname_str, "tempsky.tga")) {
      Log("Sky load detected: " + fname_str);
      if (!g_currentLflPath.empty()) {
        std::string skyName = GetSkyFromLfl(g_currentLflPath);
        if (!skyName.empty()) {
          Log("Found sky override in LFL: " + skyName);

          // Files are in GRIDS\skies
          // If original request was "GRIDS\skies\tempsky.tga", we want
          // "GRIDS\skies\skydakka.bmp", for example.

          std::string directory = "";
          size_t lastSlash = fname_str.find_last_of("\\/");
          if (lastSlash != std::string::npos) {
            directory = fname_str.substr(0, lastSlash + 1);
          }

          if (directory.empty()) {
            directory = "GRIDS\\skies\\";
          }
          std::string newPath = directory + "tga\\" + skyName + ".tga";
          Log("Redirecting to: " + newPath);
          return RealCreateFileA(newPath.c_str(), dwDesiredAccess, dwShareMode,
                                 lpSecurityAttributes, dwCreationDisposition,
                                 dwFlagsAndAttributes, hTemplateFile);
        } else {
          Log("No POLY_SKY found in " + g_currentLflPath);
        }
      } else {
        Log("No current level LFL known, loading default.");
      }
    }
  }

  return RealCreateFileA(lpFileName, dwDesiredAccess, dwShareMode,
                         lpSecurityAttributes, dwCreationDisposition,
                         dwFlagsAndAttributes, hTemplateFile);
}

void SetupHook() {
  HMODULE hExe = GetModuleHandle(NULL);
  if (!hExe)
    return;

  PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)hExe;
  PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)((BYTE *)hExe + pDos->e_lfanew);

  PIMAGE_IMPORT_DESCRIPTOR pImport =
      (PIMAGE_IMPORT_DESCRIPTOR)((BYTE *)hExe +
                                 pNt->OptionalHeader
                                     .DataDirectory
                                         [IMAGE_DIRECTORY_ENTRY_IMPORT]
                                     .VirtualAddress);

  while (pImport->Name) {
    char *modName = (char *)((BYTE *)hExe + pImport->Name);
    if (_stricmp(modName, "KERNEL32.dll") == 0) {
      PIMAGE_THUNK_DATA pThunk =
          (PIMAGE_THUNK_DATA)((BYTE *)hExe + pImport->FirstThunk);

      while (pThunk->u1.Function) {
        PROC *ppfn = (PROC *)&pThunk->u1.Function;
        if (*ppfn == (PROC)GetProcAddress(GetModuleHandleA("KERNEL32.dll"),
                                          "CreateFileA")) {
          DWORD oldProtect;
          VirtualProtect(ppfn, sizeof(PROC), PAGE_EXECUTE_READWRITE,
                         &oldProtect);

          RealCreateFileA = (CreateFileA_t)*ppfn;
          *ppfn = (PROC)MyCreateFileA;

          VirtualProtect(ppfn, sizeof(PROC), oldProtect, &oldProtect);
          Log("Hooked CreateFileA");
          return;
        }
        pThunk++;
      }
    }
    pImport++;
  }
  Log("Failed to find CreateFileA in imports");
}

// IAT Patch for WINMM
void PatchWinmm() {
    HMODULE hExe = GetModuleHandle(NULL);
    if (!hExe) return;

    // Load our renamed local WINMM
    // Assuming it's in the same directory as dsound.dll/uprising.exe
    char sysDir[MAX_PATH];
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string exeDir = exePath;
    size_t lastSlash = exeDir.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        exeDir = exeDir.substr(0, lastSlash + 1);
    }
    std::string winmmPath = exeDir + "winmm_game.dll";
    
    HMODULE hLocalWinmm = LoadLibraryA(winmmPath.c_str());
    if (!hLocalWinmm) {
        Log("Failed to load local winmm_game.dll: " + winmmPath);
        return;
    }
    Log("Loaded local winmm_game.dll");

    PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)hExe;
    PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)((BYTE*)hExe + pDos->e_lfanew);
    PIMAGE_IMPORT_DESCRIPTOR pImport = (PIMAGE_IMPORT_DESCRIPTOR)((BYTE*)hExe + pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

    while (pImport->Name) {
        char* modName = (char*)((BYTE*)hExe + pImport->Name);
        if (_stricmp(modName, "WINMM.dll") == 0) {
            Log("Found WINMM.dll import descriptor");
            
            PIMAGE_THUNK_DATA pThunk = (PIMAGE_THUNK_DATA)((BYTE*)hExe + pImport->FirstThunk);
            PIMAGE_THUNK_DATA pOrigThunk = (PIMAGE_THUNK_DATA)((BYTE*)hExe + pImport->OriginalFirstThunk);
            
            if (!pImport->OriginalFirstThunk) {
                // strict fallback, usually not needed for normal EXEs
                pOrigThunk = pThunk; 
            }

            while (pOrigThunk->u1.AddressOfData) {
                FARPROC pNewFunc = NULL;
                
                if (IMAGE_SNAP_BY_ORDINAL(pOrigThunk->u1.Ordinal)) {
                    DWORD ordinal = IMAGE_ORDINAL(pOrigThunk->u1.Ordinal);
                    pNewFunc = GetProcAddress(hLocalWinmm, (LPCSTR)ordinal);
                    // Log "Patching ordinal..."
                } else {
                    PIMAGE_IMPORT_BY_NAME pIBN = (PIMAGE_IMPORT_BY_NAME)((BYTE*)hExe + pOrigThunk->u1.AddressOfData);
                    pNewFunc = GetProcAddress(hLocalWinmm, (char*)pIBN->Name);
                    // Log("Patching " + std::string((char*)pIBN->Name));
                }

                if (pNewFunc) {
                    DWORD oldProtect;
                    VirtualProtect(&pThunk->u1.Function, sizeof(DWORD), PAGE_EXECUTE_READWRITE, &oldProtect);
                    pThunk->u1.Function = (DWORD)pNewFunc;
                    VirtualProtect(&pThunk->u1.Function, sizeof(DWORD), oldProtect, &oldProtect);
                } else {
                    Log("Failed to find function in local winmm");
                }

                pThunk++;
                pOrigThunk++;
            }
        }
        pImport++;
    }
}

extern "C" __declspec(dllexport) HRESULT WINAPI DirectSoundCreate(LPGUID lpGuid, void** ppDS, void* pUnkOuter) {
    if (!RealDirectSoundCreate) {
        return E_FAIL;
    }
    return RealDirectSoundCreate(lpGuid, ppDS, pUnkOuter);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        Log("DLL_PROCESS_ATTACH");
        
        // Load system DSOUND (as before)
        char sysDir[MAX_PATH];
        GetSystemDirectoryA(sysDir, MAX_PATH);
        std::string dsoundPath = sysDir;
        dsoundPath += "\\dsound.dll";
        
        g_hDsound = LoadLibraryA(dsoundPath.c_str());
        if (g_hDsound) {
            RealDirectSoundCreate = (DirectSoundCreate_t)GetProcAddress(g_hDsound, "DirectSoundCreate");
            Log("Loaded real dsound.dll");
        } else {
            Log("Failed to load real dsound.dll");
        }

        // Install Hooks
        SetupHook();     // CreateFileA hook for sky textures
        PatchWinmm();    // WINMM redirection
        break;
    }
  case DLL_PROCESS_DETACH:
    if (g_hDsound)
      FreeLibrary(g_hDsound);
    break;
  }
  return TRUE;
}
