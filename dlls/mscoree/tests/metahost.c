/*
 * Copyright 2010 Vincent Povirk
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#define COBJMACROS

#include <stdarg.h>

#include "windef.h"
#include "ole2.h"

#include "initguid.h"
#include "metahost.h"
#include "wine/test.h"

static HMODULE hmscoree;

static HRESULT (WINAPI *pCLRCreateInstance)(REFCLSID clsid, REFIID riid, LPVOID *ppInterface);

static ICLRMetaHost *metahost;

BOOL init_pointers(void)
{
    HRESULT hr = E_FAIL;

    hmscoree = LoadLibraryA("mscoree.dll");

    if (hmscoree)
        pCLRCreateInstance = (void *)GetProcAddress(hmscoree, "CLRCreateInstance");

    if (pCLRCreateInstance)
        hr = pCLRCreateInstance(&CLSID_CLRMetaHost, &IID_ICLRMetaHost, (void**)&metahost);

    if (FAILED(hr))
    {
        win_skip(".NET 4 is not installed\n");
        FreeLibrary(hmscoree);
        return FALSE;
    }

    return TRUE;
}

void cleanup(void)
{
    ICLRMetaHost_Release(metahost);

    FreeLibrary(hmscoree);
}

void test_enumruntimes(void)
{
    IEnumUnknown *runtime_enum;
    IUnknown *unk;
    DWORD count;
    ICLRRuntimeInfo *runtime_info;
    HRESULT hr;
    WCHAR buf[MAX_PATH];

    hr = ICLRMetaHost_EnumerateInstalledRuntimes(metahost, &runtime_enum);
    todo_wine ok(hr == S_OK, "EnumerateInstalledRuntimes returned %x\n", hr);
    if (FAILED(hr)) return;

    while ((hr = IEnumUnknown_Next(runtime_enum, 1, &unk, &count)) == S_OK)
    {
        hr = IUnknown_QueryInterface(unk, &IID_ICLRRuntimeInfo, (void**)&runtime_info);
        ok(hr == S_OK, "QueryInterface returned %x\n", hr);

        count = 1;
        hr = ICLRRuntimeInfo_GetVersionString(runtime_info, buf, &count);
        ok(hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER), "GetVersionString returned %x\n", hr);
        ok(count > 1, "GetVersionString returned count %u\n", count);

        count = 0xdeadbeef;
        hr = ICLRRuntimeInfo_GetVersionString(runtime_info, NULL, &count);
        ok(hr == S_OK, "GetVersionString returned %x\n", hr);
        ok(count > 1 && count != 0xdeadbeef, "GetVersionString returned count %u\n", count);

        count = MAX_PATH;
        hr = ICLRRuntimeInfo_GetVersionString(runtime_info, buf, &count);
        ok(hr == S_OK, "GetVersionString returned %x\n", hr);
        ok(count > 1, "GetVersionString returned count %u\n", count);

        trace("runtime found: %s\n", wine_dbgstr_w(buf));

        ICLRRuntimeInfo_Release(runtime_info);

        IUnknown_Release(unk);
    }

    ok(hr == S_FALSE, "IEnumUnknown_Next returned %x\n", hr);

    IEnumUnknown_Release(runtime_enum);
}

START_TEST(metahost)
{
    if (!init_pointers())
        return;

    test_enumruntimes();

    cleanup();
}
