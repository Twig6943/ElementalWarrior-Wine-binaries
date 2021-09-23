/*
 * Wayland data device (clipboard and DnD) handling (DLL code)
 *
 * Copyright (c) 2020 Alexandros Frantzis for Collabora Ltd
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

#include "ntstatus.h"
#define WIN32_NO_STATUS

#include "waylanddrv_dll.h"

#define COBJMACROS
#include "objidl.h"
#include "shlobj.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(clipboard);

static IDataObjectVtbl dataOfferDataObjectVtbl;

static LRESULT CALLBACK clipboard_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    struct waylanddrv_unix_clipboard_message_params params;

    switch (msg)
    {
    case WM_NCCREATE:
    case WM_CLIPBOARDUPDATE:
    case WM_RENDERFORMAT:
    case WM_DESTROYCLIPBOARD:
        params.hwnd = hwnd;
        params.msg = msg;
        params.wparam = wp;
        params.lparam = lp;
        return WAYLANDDRV_UNIX_CALL(clipboard_message, &params);
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

NTSTATUS WINAPI waylanddrv_client_create_clipboard_window(void *arg, ULONG size)
{
    static const WCHAR clipboard_classname[] = {
        '_','_','w','i','n','e','_','c','l','i','p','b','o','a','r','d',
        '_','m','a','n','a','g','e','r',0
    };
    WNDCLASSW class;
    HWND clipboard_hwnd;

    memset(&class, 0, sizeof(class));
    class.lpfnWndProc = clipboard_wndproc;
    class.lpszClassName = clipboard_classname;

    if (!RegisterClassW(&class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        ERR("could not register clipboard window class err %lu\n", GetLastError());
        return 0;
    }

    if (!(clipboard_hwnd = CreateWindowW(clipboard_classname, NULL, 0, 0, 0, 0, 0,
                                         HWND_MESSAGE, 0, 0, NULL)))
    {
        ERR("failed to create clipboard window err %lu\n", GetLastError());
        return 0;
    }

    if (!AddClipboardFormatListener(clipboard_hwnd))
        ERR("failed to set clipboard listener %lu\n", GetLastError());

    TRACE("clipboard_hwnd=%p\n", clipboard_hwnd);
    return HandleToUlong(clipboard_hwnd);
}

/**********************************************************************
 *          IDropTarget discovery
 *
 * Based on functions in dlls/ole32/ole2.c
 */

static HANDLE get_drop_target_local_handle(HWND hwnd)
{
    static const WCHAR prop_marshalleddrop_target[] =
        {'W','i','n','e','M','a','r','s','h','a','l','l','e','d',
         'D','r','o','p','T','a','r','g','e','t',0};
    HANDLE handle;
    HANDLE local_handle = 0;

    handle = GetPropW(hwnd, prop_marshalleddrop_target);
    if (handle)
    {
        DWORD pid;
        HANDLE process;

        GetWindowThreadProcessId(hwnd, &pid);
        process = OpenProcess(PROCESS_DUP_HANDLE, FALSE, pid);
        if (process)
        {
            DuplicateHandle(process, handle, GetCurrentProcess(), &local_handle,
                            0, FALSE, DUPLICATE_SAME_ACCESS);
            CloseHandle(process);
        }
    }
    return local_handle;
}

static HRESULT create_stream_from_map(HANDLE map, IStream **stream)
{
    HRESULT hr = E_OUTOFMEMORY;
    HGLOBAL hmem;
    void *data;
    MEMORY_BASIC_INFORMATION info;

    data = MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0);
    if(!data) return hr;

    VirtualQuery(data, &info, sizeof(info));

    hmem = GlobalAlloc(GMEM_MOVEABLE, info.RegionSize);
    if(hmem)
    {
        memcpy(GlobalLock(hmem), data, info.RegionSize);
        GlobalUnlock(hmem);
        hr = CreateStreamOnHGlobal(hmem, TRUE, stream);
    }
    UnmapViewOfFile(data);
    return hr;
}

static IDropTarget* get_drop_target_pointer(HWND hwnd)
{
    IDropTarget *drop_target = NULL;
    HANDLE map;
    IStream *stream;

    map = get_drop_target_local_handle(hwnd);
    if(!map) return NULL;

    if(SUCCEEDED(create_stream_from_map(map, &stream)))
    {
        CoUnmarshalInterface(stream, &IID_IDropTarget, (void**)&drop_target);
        IStream_Release(stream);
    }
    CloseHandle(map);
    return drop_target;
}

static IDropTarget *drop_target_from_window_point(HWND hwnd, POINT point)
{
    HWND child;
    IDropTarget *drop_target;
    HWND orig_hwnd = hwnd;
    POINT orig_point = point;

    /* Find the deepest child window. */
    ScreenToClient(hwnd, &point);
    while ((child = ChildWindowFromPointEx(hwnd, point, CWP_SKIPDISABLED | CWP_SKIPINVISIBLE)) &&
            child != hwnd)
    {
        MapWindowPoints(hwnd, child, &point, 1);
        hwnd = child;
    }

    /* Ascend the children hierarchy until we find one that accepts drops. */
    do
    {
        drop_target = get_drop_target_pointer(hwnd);
    } while (drop_target == NULL && (hwnd = GetParent(hwnd)) != NULL);

    TRACE("hwnd=%p point=(%ld,%ld) => dnd_hwnd=%p drop_target=%p\n",
          orig_hwnd, orig_point.x, orig_point.y, hwnd, drop_target);
    return drop_target;
}

static NTSTATUS WINAPI waylanddrv_client_dnd_enter(void *params, ULONG size)
{
    struct waylanddrv_client_dnd_params *p = params;
    IDropTarget *drop_target;
    DWORD drop_effect = p->drop_effect;
    IDataObject *data_object = UIntToPtr(p->data_object);
    HRESULT hr;

    /* If unixlib is 64 bits and PE is 32 bits, this will write a 32 bit
     * pointer value to the bottom of 64 bit pointer variable, which works out
     * fine due to little-endianness and the fact that lpVtbl has been zero
     * initialized. */
    data_object->lpVtbl = &dataOfferDataObjectVtbl;

    drop_target = drop_target_from_window_point(ULongToHandle(p->hwnd), p->point);
    if (!drop_target)
        return STATUS_UNSUCCESSFUL;

    hr = IDropTarget_DragEnter(drop_target, data_object, MK_LBUTTON,
                               *(POINTL*)&p->point, &drop_effect);
    IDropTarget_Release(drop_target);
    if (FAILED(hr))
        return STATUS_UNSUCCESSFUL;

    return STATUS_SUCCESS;
}

static NTSTATUS WINAPI waylanddrv_client_dnd_leave(void *params, ULONG size)
{
    struct waylanddrv_client_dnd_params *p = params;
    IDropTarget *drop_target;
    HRESULT hr;

    drop_target = drop_target_from_window_point(ULongToHandle(p->hwnd), p->point);
    if (!drop_target)
        return STATUS_UNSUCCESSFUL;

    hr = IDropTarget_DragLeave(drop_target);
    IDropTarget_Release(drop_target);
    if (FAILED(hr))
        return STATUS_UNSUCCESSFUL;

    return STATUS_SUCCESS;
}

static NTSTATUS WINAPI waylanddrv_client_dnd_motion(void *params, ULONG size)
{
    struct waylanddrv_client_dnd_params *p = params;
    IDropTarget *drop_target;
    DWORD drop_effect = p->drop_effect;
    HRESULT hr;

    drop_target = drop_target_from_window_point(ULongToHandle(p->hwnd), p->point);
    if (!drop_target)
        return STATUS_UNSUCCESSFUL;

    hr = IDropTarget_DragOver(drop_target, MK_LBUTTON, *(POINTL*)&p->point,
                              &drop_effect);
    IDropTarget_Release(drop_target);
    if (FAILED(hr))
        return STATUS_UNSUCCESSFUL;

    return STATUS_SUCCESS;
}

NTSTATUS WINAPI waylanddrv_client_dnd(void *params, ULONG size)
{
    struct waylanddrv_client_dnd_params *p = params;

    switch (p->event) {
    case CLIENT_DND_EVENT_ENTER:
        return waylanddrv_client_dnd_enter(params, size);
    case CLIENT_DND_EVENT_LEAVE:
        return waylanddrv_client_dnd_leave(params, size);
    case CLIENT_DND_EVENT_MOTION:
        return waylanddrv_client_dnd_motion(params, size);
    }

    return STATUS_UNSUCCESSFUL;
}

/*********************************************************
 * Implementation of IDataObject for wayland data offers *
 *********************************************************/

static HRESULT WINAPI dataOfferDataObject_QueryInterface(IDataObject *data_object,
                                                         REFIID riid, void **object)
{
    TRACE("(%p, %s, %p)\n", data_object, debugstr_guid(riid), object);
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDataObject))
    {
        *object = data_object;
        IDataObject_AddRef(data_object);
        return S_OK;
    }
    *object = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI dataOfferDataObject_AddRef(IDataObject *data_object)
{
    TRACE("(%p)\n", data_object);
    /* Each data object is owned by the data_offer which contains it, and will
     * be freed when the data_offer is destroyed, so we don't care about proper
     * reference tracking. */
    return 2;
}

static ULONG WINAPI dataOfferDataObject_Release(IDataObject *data_object)
{
    TRACE("(%p)\n", data_object);
    /* Each data object is owned by the data_offer which contains it, and will
     * be freed when, so we don't care about proper reference tracking. */
    return 1;
}

static HRESULT WINAPI dataOfferDataObject_GetData(IDataObject *data_object,
                                                  FORMATETC *format_etc,
                                                  STGMEDIUM *medium)
{
    HRESULT hr;
    struct waylanddrv_unix_data_offer_import_format_params params;
    void *data;

    TRACE("(%p, %p, %p)\n", data_object, format_etc, medium);

    hr = IDataObject_QueryGetData(data_object, format_etc);
    if (!SUCCEEDED(hr))
        return hr;

    params.data_offer = PtrToUint(data_object);
    params.format = format_etc->cfFormat;
    params.data = 0;
    params.size = 0;

    if (WAYLANDDRV_UNIX_CALL(data_offer_import_format, &params) != 0 || !params.data)
        return E_UNEXPECTED;

    data = UIntToPtr(params.data);

    medium->hGlobal = GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT, params.size);
    if (medium->hGlobal == NULL)
        return E_OUTOFMEMORY;
    memcpy(GlobalLock(medium->hGlobal), data, params.size);
    GlobalUnlock(medium->hGlobal);

    medium->tymed = TYMED_HGLOBAL;
    medium->pUnkForRelease = 0;

    VirtualFree(data, params.size, MEM_RELEASE);

    return S_OK;
}

static HRESULT WINAPI dataOfferDataObject_GetDataHere(IDataObject *data_object,
                                                      FORMATETC *format_etc,
                                                      STGMEDIUM *medium)
{
    FIXME("(%p, %p, %p): stub\n", data_object, format_etc, medium);
    return DATA_E_FORMATETC;
}

static HRESULT WINAPI dataOfferDataObject_QueryGetData(IDataObject *data_object,
                                                       FORMATETC *format_etc)
{
    struct waylanddrv_unix_data_offer_accept_format_params params;

    TRACE("(%p, %p={.tymed=0x%lx, .dwAspect=%ld, .cfFormat=%d}\n",
          data_object, format_etc, format_etc->tymed, format_etc->dwAspect,
          format_etc->cfFormat);

    if (format_etc->tymed && !(format_etc->tymed & TYMED_HGLOBAL))
    {
        FIXME("only HGLOBAL medium types supported right now\n");
        return DV_E_TYMED;
    }

    params.data_offer = PtrToUint(data_object);
    params.format = format_etc->cfFormat;

    if (WAYLANDDRV_UNIX_CALL(data_offer_accept_format, &params) == 0)
        return S_OK;

    TRACE("didn't find offer for clipboard format %u\n", format_etc->cfFormat);
    return DV_E_FORMATETC;
}

static HRESULT WINAPI dataOfferDataObject_GetCanonicalFormatEtc(IDataObject *data_object,
                                                                FORMATETC *format_etc,
                                                                FORMATETC *format_etc_out)
{
    FIXME("(%p, %p, %p): stub\n", data_object, format_etc, format_etc_out);
    format_etc_out->ptd = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI dataOfferDataObject_SetData(IDataObject *data_object,
                                                  FORMATETC *format_etc,
                                                  STGMEDIUM *medium, BOOL release)
{
    FIXME("(%p, %p, %p, %s): stub\n", data_object, format_etc,
          medium, release ? "TRUE" : "FALSE");
    return E_NOTIMPL;
}

static BOOL formats_etc_contains_clipboard_format(FORMATETC *formats_etc,
                                                  size_t formats_etc_count,
                                                  UINT clipboard_format)
{
    size_t i;

    for (i = 0; i < formats_etc_count; i++)
        if (formats_etc[i].cfFormat == clipboard_format) return TRUE;

    return FALSE;
}

static HRESULT WINAPI dataOfferDataObject_EnumFormatEtc(IDataObject *data_object,
                                                        DWORD direction,
                                                        IEnumFORMATETC **enum_format_etc)
{
    HRESULT hr;
    FORMATETC *formats_etc;
    size_t formats_etc_count = 0;
    struct waylanddrv_unix_data_offer_enum_formats_params params;

    TRACE("(%p, %lu, %p)\n", data_object, direction, enum_format_etc);

    if (direction != DATADIR_GET)
    {
        FIXME("only the get direction is implemented\n");
        return E_NOTIMPL;
    }

    params.data_offer = PtrToUint(data_object);
    params.formats = NULL;
    params.num_formats = 0;

    WAYLANDDRV_UNIX_CALL(data_offer_enum_formats, &params);
    params.formats = HeapAlloc(GetProcessHeap(), 0, params.num_formats * sizeof(UINT));
    WAYLANDDRV_UNIX_CALL(data_offer_enum_formats, &params);
    if (!params.formats)
        return E_OUTOFMEMORY;

    /* Allocate space for all offered mime types, although we may not use them all */
    formats_etc = HeapAlloc(GetProcessHeap(), 0, params.num_formats * sizeof(FORMATETC));
    if (!formats_etc)
    {
        HeapFree(GetProcessHeap(), 0, params.formats);
        return E_OUTOFMEMORY;
    }

    for (int i = 0; i < params.num_formats; i++)
    {
        if (!formats_etc_contains_clipboard_format(formats_etc, formats_etc_count,
                                                   params.formats[i]))
        {
            FORMATETC *current= &formats_etc[formats_etc_count];

            current->cfFormat = params.formats[i];
            current->ptd = NULL;
            current->dwAspect = DVASPECT_CONTENT;
            current->lindex = -1;
            current->tymed = TYMED_HGLOBAL;

            formats_etc_count += 1;
        }
    }

    hr = SHCreateStdEnumFmtEtc(formats_etc_count, formats_etc, enum_format_etc);
    HeapFree(GetProcessHeap(), 0, params.formats);
    HeapFree(GetProcessHeap(), 0, formats_etc);

    return hr;
}

static HRESULT WINAPI dataOfferDataObject_DAdvise(IDataObject *data_object,
                                                  FORMATETC *format_etc, DWORD advf,
                                                  IAdviseSink *advise_sink,
                                                  DWORD *connection)
{
    FIXME("(%p, %p, %lu, %p, %p): stub\n", data_object, format_etc, advf,
          advise_sink, connection);
    return OLE_E_ADVISENOTSUPPORTED;
}

static HRESULT WINAPI dataOfferDataObject_DUnadvise(IDataObject *data_object,
                                                    DWORD connection)
{
    FIXME("(%p, %lu): stub\n", data_object, connection);
    return OLE_E_ADVISENOTSUPPORTED;
}

static HRESULT WINAPI dataOfferDataObject_EnumDAdvise(IDataObject *data_object,
                                                      IEnumSTATDATA **enum_advise)
{
    FIXME("(%p, %p): stub\n", data_object, enum_advise);
    return OLE_E_ADVISENOTSUPPORTED;
}

static IDataObjectVtbl dataOfferDataObjectVtbl =
{
    dataOfferDataObject_QueryInterface,
    dataOfferDataObject_AddRef,
    dataOfferDataObject_Release,
    dataOfferDataObject_GetData,
    dataOfferDataObject_GetDataHere,
    dataOfferDataObject_QueryGetData,
    dataOfferDataObject_GetCanonicalFormatEtc,
    dataOfferDataObject_SetData,
    dataOfferDataObject_EnumFormatEtc,
    dataOfferDataObject_DAdvise,
    dataOfferDataObject_DUnadvise,
    dataOfferDataObject_EnumDAdvise
};
