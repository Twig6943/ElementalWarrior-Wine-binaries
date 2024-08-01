#include "private.h"

WINE_DEFAULT_DEBUG_CHANNEL(dxcore);


// -- IDXCoreAdapterList --
static inline struct dxcore_adapter_list *impl_from_IDXCoreAdapterList(IDXCoreAdapterList *iface)
{
    return CONTAINING_RECORD(iface, struct dxcore_adapter_list, IDXCoreAdapterList_iface);
}


static ULONG STDMETHODCALLTYPE dxcore_adapter_list_AddRef(IDXCoreAdapterList *_this) {
    ULONG refcount;
    struct dxcore_adapter_list *this = impl_from_IDXCoreAdapterList(_this);

    refcount = InterlockedIncrement(&this->refcount);
    return refcount;
}


static ULONG STDMETHODCALLTYPE dxcore_adapter_list_Release(IDXCoreAdapterList *_this) {
    struct dxcore_adapter_list *this = impl_from_IDXCoreAdapterList(_this);

    ULONG refcount = InterlockedDecrement(&this->refcount);
    if (!refcount) {
        IUnknown_Release(&this->factory->IDXCoreAdapterFactory_iface);
        free(this);
    }
    return refcount;
}


static HRESULT STDMETHODCALLTYPE dxcore_adapter_list_QueryInterface(
    IDXCoreAdapterList *this,
    REFIID iid, void **factory
) {
    *factory = NULL;
    TRACE("this %p, riid, %s, factory, %p\n", this, wine_dbgstr_guid(iid), factory);
    if (IsEqualIID(iid, &IID_IDXCoreAdapterList)
        ||IsEqualIID(iid, &IID_IUnknown))
    {
        *factory = this;
        this->lpVtbl->AddRef(this);
        return S_OK;
    }
    return E_NOINTERFACE;
}


static uint32_t STDMETHODCALLTYPE dxcore_adapter_list_GetAdapterCount(IDXCoreAdapterList *iface) {
    UINT count;
    struct dxcore_adapter_list *this = impl_from_IDXCoreAdapterList(iface);

    count = this->len;
    TRACE("adapter count %d\n", count);
    return count;
}


static HRESULT STDMETHODCALLTYPE dxcore_adapter_list_GetAdapter(IDXCoreAdapterList *iface, uint32_t index, REFIID riid, void **ppv) {
    struct dxcore_adapter_list *this = impl_from_IDXCoreAdapterList(iface);

    *ppv = NULL;
    TRACE("index %d, riid %s, ppv %p\n", index, wine_dbgstr_guid(riid), ppv);

    if (index >= this->len)
        return E_INVALIDARG;

    return IUnknown_QueryInterface(&this->adapters[index]->IDXCoreAdapter_iface, riid, ppv);
}


static BOOL STDMETHODCALLTYPE dxcore_adapter_list_IsStale(IDXCoreAdapterList *this) {
    FIXME("stub\n");
    return FALSE;
}


static HRESULT STDMETHODCALLTYPE dxcore_adapter_list_Sort(
    IDXCoreAdapterList *this,
    uint32_t numPreferences, const DXCoreAdapterPreference *preferences
) {
    FIXME("numPreferences %d, preferences %p, stub\n", numPreferences, preferences);
    return E_NOINTERFACE;
}


static HRESULT STDMETHODCALLTYPE dxcore_adapter_list_GetFactory(IDXCoreAdapterList *iface, REFIID riid, void **ppv) {
    struct dxcore_adapter_list *this = impl_from_IDXCoreAdapterList(iface);

    return IUnknown_QueryInterface(
        &this->factory->IDXCoreAdapterFactory_iface,
        riid, ppv
    );
}


static BOOL STDMETHODCALLTYPE dxcore_adapter_list_IsAdapterPreferenceSupported(
    IDXCoreAdapterList *this, DXCoreAdapterPreference preference
) {
    FIXME("preference %d, stub\n", preference);
    return FALSE;
}


static const IDXCoreAdapterListVtbl dxcore_adapter_list_vtbl = {
    dxcore_adapter_list_QueryInterface,
    dxcore_adapter_list_AddRef,
    dxcore_adapter_list_Release,
    dxcore_adapter_list_GetAdapter,
    dxcore_adapter_list_GetAdapterCount,
    dxcore_adapter_list_IsStale,
    dxcore_adapter_list_GetFactory,
    dxcore_adapter_list_Sort,
    dxcore_adapter_list_IsAdapterPreferenceSupported
};


static HRESULT dxcore_adapter_list_init(
    struct dxcore_adapter_list* this,
    IDXGIFactory *dxgi_factory,
    const GUID* filter
) {
    IDXGIAdapter *adapter;
    HRESULT hr = 0;

    this->len = 0;

    // note: we're technically ignoring the filterAttributes
    // feel free to add them if you know how

    // count the adapters
    while (!hr) {
        hr = IDXGIFactory_EnumAdapters(
            dxgi_factory, this->len, &adapter);
        if (hr)
            break;
        IUnknown_Release(adapter);
        this->len++;
    }
    if (!this->len)
        return S_OK;

    if (!(this->adapters = calloc(this->len, sizeof(struct dxcore_adapter*))))
        return E_OUTOFMEMORY;

    for (int i=0; i<this->len; i++) {
        hr = IDXGIFactory_EnumAdapters(
            dxgi_factory, i, &adapter);
        if (hr)
            break;
        hr = dxcore_adapter_create(this->factory, adapter, &this->adapters[i]);
        IUnknown_Release(adapter);
        if (FAILED(hr))
            break;
    }
    return hr;
}


static HRESULT dxcore_adapter_list_create(
    struct dxcore_factory *factory,
    uint32_t numAttributes, const GUID* filterAttributes,
    REFIID riid, void **ppv
) {
    HRESULT result;
    IDXCoreAdapterList *iface;
    struct dxcore_adapter_list *this;
    IDXGIFactory *dxgi_factory;

    if (!numAttributes && filterAttributes)
        return E_INVALIDARG;

    if (FAILED(result = CreateDXGIFactory1(&IID_IDXGIFactory, (void**)&dxgi_factory)))
        return result;

    if (!(this = calloc(1, sizeof(*this))))
        return E_OUTOFMEMORY;

    this->refcount = 1;
    this->dxgi_factory = dxgi_factory;
    IUnknown_AddRef(&factory->IDXCoreAdapterFactory_iface);
    this->factory = factory;
    iface = &this->IDXCoreAdapterList_iface;
    iface->lpVtbl = &dxcore_adapter_list_vtbl;

    if (FAILED(result = dxcore_adapter_list_init(this, dxgi_factory, filterAttributes))) {
        IUnknown_Release(iface);
        return result;
    }

    result = IUnknown_QueryInterface(iface, riid, ppv);
    IUnknown_Release(iface);
    return result;
}


// -- IDXCoreAdapterFactory --
static struct dxcore_factory *dxcore_factory_singleton = NULL;


static inline struct dxcore_factory *impl_from_IDXCoreAdapterFactory(IDXCoreAdapterFactory *iface)
{
    return CONTAINING_RECORD(iface, struct dxcore_factory, IDXCoreAdapterFactory_iface);
}


static ULONG STDMETHODCALLTYPE dxcore_factory_AddRef(IDXCoreAdapterFactory *iface) {
    struct dxcore_factory *this = impl_from_IDXCoreAdapterFactory(iface);
    return InterlockedIncrement(&this->refcount);
}


static ULONG STDMETHODCALLTYPE dxcore_factory_Release(IDXCoreAdapterFactory *iface) {
    struct dxcore_factory *this = impl_from_IDXCoreAdapterFactory(iface);
    ULONG refcount = InterlockedDecrement(&this->refcount);

    if (!refcount) {
        dxcore_factory_singleton = NULL;
        free(this);
    }
    return refcount;
}


static HRESULT STDMETHODCALLTYPE dxcore_factory_QueryInterface(
    IDXCoreAdapterFactory *this,
    REFIID iid, void **factory
) {
    *factory = NULL;
    TRACE("this %p, riid, %s, factory, %p\n", this, wine_dbgstr_guid(iid), factory);
    if (IsEqualIID(iid, &IID_IDXCoreAdapterFactory)
            || IsEqualIID(iid, &IID_IUnknown))
    {
        *factory = this;
        this->lpVtbl->AddRef(this);
        return S_OK;
    }
    return E_NOINTERFACE;
}


static HRESULT STDMETHODCALLTYPE dxcore_factory_CreateAdapterList(
    IDXCoreAdapterFactory *iface,
    uint32_t num_attributes, const GUID *filter_attributes,
    REFIID riid, void **ppv
) {
    struct dxcore_factory *this = impl_from_IDXCoreAdapterFactory(iface);

    TRACE("num_attributes %d, filter_attributes %p, riid %s, ppv %p\n",
        num_attributes, filter_attributes, wine_dbgstr_guid(riid), ppv);

    return dxcore_adapter_list_create(this, num_attributes, filter_attributes, riid, ppv);
}

static HRESULT STDMETHODCALLTYPE dxcore_factory_GetAdapterByLuid(
    IDXCoreAdapterFactory *this,
    REFLUID adapter_luid,
    REFIID riid, void **ppv
) {
    FIXME("this %p, adapter_luid %p, riid %s, ppv %p, stub!\n",
        this, adapter_luid, wine_dbgstr_guid(riid), ppv);
    return E_INVALIDARG;
}


static BOOL STDMETHODCALLTYPE dxcore_factory_IsNotificationTypeSupported(
    IDXCoreAdapterFactory *this,
    DXCoreNotificationType type
) {
    FIXME("type %i\n", type);
    return (type < 2);
}


static HRESULT STDMETHODCALLTYPE dxcore_factory_RegisterEventNotification(
    IDXCoreAdapterFactory *this,
    IUnknown *dxcore_object, DXCoreNotificationType type,
    PFN_DXCORE_NOTIFICATION_CALLBACK callback, void *callback_context,
    uint32_t *event_cookie
) {
    FIXME("dxcore_object %p, type %d, callback %p, callback_context %p, event_cookie %p\n",
        dxcore_object, type, callback, callback_context, event_cookie);
    if (type < 2)
        return S_OK;
    return DXGI_ERROR_INVALID_CALL;
}


static HRESULT STDMETHODCALLTYPE dxcore_factory_UnregisterEventNotification(
    IDXCoreAdapterFactory *this,
    uint32_t event_cookie
) {
    FIXME("event_cookie %d\n", event_cookie);
    return S_OK;
}


static const struct IDXCoreAdapterFactoryVtbl dxcore_factory_vtbl = {
    dxcore_factory_QueryInterface,
    dxcore_factory_AddRef,
    dxcore_factory_Release,
    dxcore_factory_CreateAdapterList,
    dxcore_factory_GetAdapterByLuid,
    dxcore_factory_IsNotificationTypeSupported,
    dxcore_factory_RegisterEventNotification,
    dxcore_factory_UnregisterEventNotification,
};


static HRESULT dxcore_factory_init(struct dxcore_factory *this) {
    dxcore_factory_singleton = this;
    this->IDXCoreAdapterFactory_iface.lpVtbl = &dxcore_factory_vtbl;
    this->refcount = 1;

    return S_OK;
}


HRESULT dxcore_factory_create(REFIID riid, void **ppv) {
    struct dxcore_factory *this;
    HRESULT res;

    *ppv = NULL;

    if (dxcore_factory_singleton != NULL) {
        TRACE("Reusing factory %p\n", dxcore_factory_singleton);
        return IUnknown_QueryInterface(
            &dxcore_factory_singleton->IDXCoreAdapterFactory_iface, riid, ppv
        );
    }

    if (!(this = calloc(1, sizeof(*this))))
        return E_OUTOFMEMORY;

    if (FAILED(res = dxcore_factory_init(this))) {
        WARN("Failed to initialize factory, hr %#lx.\n", res);
        free(this);
        return res;
    }

    TRACE("Created factory %p.\n", this);
    *ppv = this;
    return S_OK;
}
