// osspecific.cpp - Implementation of the platform-specific classes
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2006, 2008, 2010 Oliver Schneider (assarbad.net)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// Author(s): - bseifert -> http://windirstat.info/contact/bernhard/
//            - assarbad -> http://windirstat.info/contact/oliver/
//

#include "stdafx.h"
#include "osspecific.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#ifdef UNICODE
#define TSPEC "W"
#else
#define TSPEC "A"
#endif

#define PROCNAME(name) #name
#define TPROCNAME(name) #name TSPEC

#define CHECK(name) if(!m_##name.IsSupported()) return false

CDllModule dllKernel32(nameKernel32);
CDllModule dllShell32(nameShell32);
CDllModule dllPsApi(namePsApi);

// Required to use the system image lists
// - http://www.catch22.net/tuts/sysimg
// - http://msdn.microsoft.com/en-us/library/bb776418(VS.85).aspx
BOOL FileIconInit(__in  BOOL fRestoreCache)
{
    typedef BOOL (WINAPI * TFNFileIconInit)(BOOL);
    static HMODULE hShell32 = NULL;
    static TFNFileIconInit pfnFileIconInit = 0;
    if(!hShell32)
    {
        hShell32 = ::LoadLibrary(TEXT("shell32.dll"));
    }
    if(hShell32 && !pfnFileIconInit)
    {
        pfnFileIconInit = reinterpret_cast<TFNFileIconInit>(::GetProcAddress(hShell32, ((LPCSTR)660)));
    }
    if(pfnFileIconInit)
    {
        return pfnFileIconInit(fRestoreCache);
    }
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////

CRecycleBinApi::CRecycleBinApi()
    : m_hDll(dllShell32.Handle())
    , m_SHEmptyRecycleBin(m_hDll, TPROCNAME(SHEmptyRecycleBin))
    , m_SHQueryRecycleBin(m_hDll, TPROCNAME(SHQueryRecycleBin))
{
}

CRecycleBinApi::~CRecycleBinApi()
{
}

bool CRecycleBinApi::IsSupported()
{
    CHECK(SHEmptyRecycleBin);
    CHECK(SHQueryRecycleBin);

    return true;
}

HRESULT CRecycleBinApi::SHEmptyRecycleBin(HWND hwnd, LPCTSTR pszRootPath, DWORD dwFlags)
{
    ASSERT(m_SHEmptyRecycleBin.IsSupported());
    return m_SHEmptyRecycleBin.pfnFct(hwnd, pszRootPath, dwFlags);
}

HRESULT CRecycleBinApi::SHQueryRecycleBin(LPCTSTR pszRootPath, LPSHQUERYRBINFO pSHQueryRBInfo)
{
    ASSERT(m_SHQueryRecycleBin.IsSupported());
    return m_SHQueryRecycleBin.pfnFct(pszRootPath, pSHQueryRBInfo);
}

/////////////////////////////////////////////////////////////////////////////

CPsapi::CPsapi()
    : m_hDll(dllPsApi.Handle())
    , m_GetProcessMemoryInfo(m_hDll, PROCNAME(GetProcessMemoryInfo))
{
}

CPsapi::~CPsapi()
{
}

bool CPsapi::IsSupported()
{
    CHECK(GetProcessMemoryInfo);

    return true;
}

BOOL CPsapi::GetProcessMemoryInfo(HANDLE Process, PPROCESS_MEMORY_COUNTERS ppsmemCounters, DWORD cb)
{
    ASSERT(m_GetProcessMemoryInfo.IsSupported());
    return m_GetProcessMemoryInfo.pfnFct(Process, ppsmemCounters, cb);
}

/////////////////////////////////////////////////////////////////////////////

CGetDiskFreeSpaceApi::CGetDiskFreeSpaceApi()
    : m_hDll(dllKernel32.Handle())
    , m_GetDiskFreeSpace(m_hDll, TPROCNAME(GetDiskFreeSpace))
    , m_GetDiskFreeSpaceEx(m_hDll, TPROCNAME(GetDiskFreeSpaceEx))
{
}

CGetDiskFreeSpaceApi::~CGetDiskFreeSpaceApi()
{
}

bool CGetDiskFreeSpaceApi::IsSupported()
{
    // Either of the functions exists definitely, even on Windows 95 w/o OSR2
    return m_GetDiskFreeSpace.IsSupported() | m_GetDiskFreeSpaceEx.IsSupported();
}

void CGetDiskFreeSpaceApi::GetDiskFreeSpace(LPCTSTR pszRootPath, ULONGLONG& total, ULONGLONG& unused)
{
    ULARGE_INTEGER u64available = {0};
    ULARGE_INTEGER u64total = {0};
    ULARGE_INTEGER u64free = {0};

    if(m_GetDiskFreeSpaceEx.IsSupported())
    {
        // On NT 4.0, the 2nd Parameter to this function must NOT be NULL.
        BOOL b = m_GetDiskFreeSpaceEx.pfnFct(pszRootPath, &u64available, &u64total, &u64free);
        if(!b)
        {
            TRACE(_T("GetDiskFreeSpaceEx(%s) failed.\n"), pszRootPath);
        }
    }
    else /*if(m_GetDiskFreeSpace.IsSupported())*/ // compatibility mode ...
    {
        // Actually this code should only be called on versions not supporting
        // GetDiskFreeSpaceEx(). By chance these versions are those which do
        // not support partition sizes larger than 2 GB anyway :)
        DWORD dwSectorsPerCluster = 0, dwBytesPerSector = 0;
        BOOL b = m_GetDiskFreeSpace.pfnFct(pszRootPath, &dwSectorsPerCluster, &dwBytesPerSector, &u64free.LowPart, &u64total.LowPart);
        if(!b)
        {
            TRACE(_T("GetDiskFreeSpace(%s) failed.\n"), pszRootPath);
        }

        // Now we need to do some arithmetics to get the sizes in byte ...
        u64free.QuadPart *= dwSectorsPerCluster * dwBytesPerSector;
        u64total.QuadPart *= dwSectorsPerCluster * dwBytesPerSector;
    }

    // Will fail, when more than 2^63 Bytes free ... (signed vs. unsigned type)
    total = u64total.QuadPart;
    unused = u64free.QuadPart;

    // Race condition ...
    ASSERT(unused <= total);
}
