#include "core/FileDialog.h"
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shobjidl.h>
#endif

std::string FileDialog::OpenFile(const char *filter) {
#ifdef _WIN32
  OPENFILENAMEA ofn;
  CHAR szFile[260] = {0};

  ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
  ofn.lStructSize = sizeof(OPENFILENAMEA);
  ofn.hwndOwner = NULL;
  ofn.lpstrFile = szFile;
  ofn.nMaxFile = sizeof(szFile);
  ofn.lpstrFilter = filter;
  ofn.nFilterIndex = 1;
  ofn.lpstrFileTitle = NULL;
  ofn.nMaxFileTitle = 0;
  ofn.lpstrInitialDir = NULL;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

  if (GetOpenFileNameA(&ofn) == TRUE) {
    return std::string(ofn.lpstrFile);
  }
#endif
  return "";
}

std::string FileDialog::SaveFile(const char *filter, const char *defaultName) {
#ifdef _WIN32
  OPENFILENAMEA ofn;
  CHAR szFile[260] = {0};
  if (defaultName && strlen(defaultName) < sizeof(szFile)) {
    strncpy(szFile, defaultName, sizeof(szFile) - 1);
  }

  ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
  ofn.lStructSize = sizeof(OPENFILENAMEA);
  ofn.hwndOwner = NULL;
  ofn.lpstrFile = szFile;
  ofn.nMaxFile = sizeof(szFile);
  ofn.lpstrFilter = filter;
  ofn.nFilterIndex = 1;
  ofn.lpstrFileTitle = NULL;
  ofn.nMaxFileTitle = 0;
  ofn.lpstrInitialDir = NULL;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

  if (GetSaveFileNameA(&ofn) == TRUE) {
    return std::string(ofn.lpstrFile);
  }
#endif
  return "";
}

std::string FileDialog::OpenDirectory(const char *title) {
#ifdef _WIN32
  std::string result = "";
  IFileOpenDialog *pfd = NULL;

  // CoInitialize might be needed if not already called, but raylib usually
  // initializes COM
  HRESULT hr =
      CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  bool coInit = SUCCEEDED(hr);

  hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
                        IID_PPV_ARGS(&pfd));
  if (SUCCEEDED(hr)) {
    DWORD dwOptions;
    if (SUCCEEDED(pfd->GetOptions(&dwOptions))) {
      pfd->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    }

    // Convert title to wide string
    int titleLen = MultiByteToWideChar(CP_UTF8, 0, title, -1, NULL, 0);
    if (titleLen > 0) {
      std::vector<wchar_t> wTitle(titleLen);
      MultiByteToWideChar(CP_UTF8, 0, title, -1, wTitle.data(), titleLen);
      pfd->SetTitle(wTitle.data());
    }

    if (SUCCEEDED(pfd->Show(NULL))) {
      IShellItem *psi = NULL;
      if (SUCCEEDED(pfd->GetResult(&psi))) {
        PWSTR pszPath = NULL;
        if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath))) {
          int len =
              WideCharToMultiByte(CP_UTF8, 0, pszPath, -1, NULL, 0, NULL, NULL);
          if (len > 0) {
            std::vector<char> pathBuf(len);
            WideCharToMultiByte(CP_UTF8, 0, pszPath, -1, pathBuf.data(), len,
                                NULL, NULL);
            result = std::string(pathBuf.data());
          }
          CoTaskMemFree(pszPath);
        }
        psi->Release();
      }
    }
    pfd->Release();
  }

  if (coInit) {
    CoUninitialize();
  }

  return result;
#else
  return "";
#endif
}
