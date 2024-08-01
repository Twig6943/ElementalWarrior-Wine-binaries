
#ifndef __WINE_DXCORE_PRIVATE_H
#define __WINE_DXCORE_PRIVATE_H

#include "wine/debug.h"

// #include <assert.h>

#define COBJMACROS
#include "winbase.h"
#include "objbase.h"

#include "dxgi1_6.h"
#ifdef DXCORE_INIT_GUID
#include "initguid.h"
#endif

#include "dxcore.h"
#include "dxcore_interface.h"

// IDXCoreAdapterFactory
struct dxcore_factory {
    IDXCoreAdapterFactory IDXCoreAdapterFactory_iface;
    LONG refcount;
};

HRESULT dxcore_factory_create(REFIID iid, void** factory);

// IDXCoreAdapterList
struct dxcore_adapter_list {
    IDXCoreAdapterList IDXCoreAdapterList_iface;
    LONG refcount;
    struct dxcore_factory *factory;
    LONG len;
    IDXGIFactory *dxgi_factory;
    struct dxcore_adapter **adapters;
};


// IDXCoreAdapter
struct dxcore_adapter {
    IDXCoreAdapter IDXCoreAdapter_iface;
    LONG refcount;
    struct dxcore_factory *factory;
    IDXGIAdapter *adapter;
};

HRESULT dxcore_adapter_create(
    struct dxcore_factory *factory,
    IDXGIAdapter *adapter,
    struct dxcore_adapter **ppv
);

#endif
