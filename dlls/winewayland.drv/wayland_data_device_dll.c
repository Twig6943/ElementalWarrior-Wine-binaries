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

#include "waylanddrv_dll.h"

#define COBJMACROS
#include "objidl.h"

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
    TRACE("(%p, %p, %p)\n", data_object, format_etc, medium);

    return E_UNEXPECTED;
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

static HRESULT WINAPI dataOfferDataObject_EnumFormatEtc(IDataObject *data_object,
                                                        DWORD direction,
                                                        IEnumFORMATETC **enum_format_etc)
{
    TRACE("(%p, %lu, %p)\n", data_object, direction, enum_format_etc);
    return E_NOTIMPL;
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
