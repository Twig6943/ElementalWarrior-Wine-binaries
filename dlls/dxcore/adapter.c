#include "private.h"

WINE_DEFAULT_DEBUG_CHANNEL(dxcore);


// -- IDXCoreAdapter --
static inline struct dxcore_adapter *impl_from_IDXCoreAdapter(IDXCoreAdapter *iface)
{
    return CONTAINING_RECORD(iface, struct dxcore_adapter, IDXCoreAdapter_iface);
}


HRESULT dxcore_adapter_pInstanceLuid(struct dxcore_adapter *this, size_t lenBuffer, void *buffer) {
    HRESULT hr;
    DXGI_ADAPTER_DESC desc;

    if (lenBuffer < sizeof(LUID))
        return E_INVALIDARG;

    hr = IDXGIAdapter_GetDesc(this->adapter, &desc);
    if (FAILED(hr))
        return hr;

    memcpy(buffer, (void*)&desc.AdapterLuid, sizeof(&desc.AdapterLuid));
    return S_OK;
}
HRESULT dxcore_adapter_pDriverVersion(struct dxcore_adapter *this, size_t lenBuffer, void *buffer) {
    HRESULT hr;
    LARGE_INTEGER res;
    if (lenBuffer < sizeof(uint64_t))
        return E_INVALIDARG;

    hr = IDXGIAdapter_CheckInterfaceSupport(this->adapter, &IID_IDXGIDevice, &res);
    TRACE("Driver version; h %ld, l %ld, q %lld.\n", res.HighPart, res.LowPart, res.QuadPart);
    memcpy(buffer, &res, sizeof(res));
    return hr;
}
HRESULT dxcore_adapter_pDriverDescription(struct dxcore_adapter *this, size_t lenBuffer, void *buffer) {
    HRESULT hr;
    size_t len;
    DXGI_ADAPTER_DESC desc;

    hr = IDXGIAdapter_GetDesc(this->adapter, &desc);
    if (FAILED(hr))
        return hr;

    len = wcslen(desc.Description);

    if (lenBuffer < len+1)
        return E_INVALIDARG;

    wcstombs(buffer, desc.Description, sizeof(desc.Description));
    TRACE("Driver Description: '%s'\n", (char *)buffer);
    return S_OK;
}
HRESULT dxcore_adapter_pHardwareID(struct dxcore_adapter *this, size_t lenBuffer, void *buffer) {
    HRESULT hr;
    DXGI_ADAPTER_DESC desc;
    DXCoreHardwareID result;

    if (lenBuffer < sizeof(result))
        return E_INVALIDARG;

    if (FAILED(hr = IDXGIAdapter_GetDesc(this->adapter, &desc)))
        return hr;

    result.vendorID = desc.VendorId;
    result.deviceID = desc.DeviceId;
    result.subSysID = desc.SubSysId;
    result.revision = desc.Revision;

    memcpy(buffer, &result, sizeof(result));
    return S_OK;
}
// ...
HRESULT dxcore_adapter_pIsHardware(struct dxcore_adapter *this, size_t lenBuffer, void *buffer) {
    char result = TRUE;
    if (lenBuffer < sizeof(result))
        return E_INVALIDARG;

    memcpy(buffer, &result, sizeof(result));
    return S_OK;
}


HRESULT (*dxcore_adapter_properties[])(struct dxcore_adapter *, size_t, void*) = {
    dxcore_adapter_pInstanceLuid,
    dxcore_adapter_pDriverVersion,
    dxcore_adapter_pDriverDescription,
    dxcore_adapter_pHardwareID,
    NULL, // dxcore_adapter_pKmdModelVersion,
    NULL, // dxcore_adapter_pComputePreemptionGranularity,
    NULL, // dxcore_adapter_pGraphicsPreemptionGranularity,
    NULL, // dxcore_adapter_pDedicatedAdapterMemory,
    NULL, // dxcore_adapter_pDedicatedSystemMemory,
    NULL, // dxcore_adapter_pSharedSystemMemory,
    NULL, // dxcore_adapter_pAcgCompatible,
    dxcore_adapter_pIsHardware,
    NULL, // dxcore_adapter_pIsIntegrated,
    NULL, // dxcore_adapter_pIsDetachable,
    NULL, // dxcore_adapter_pHardwareIDParts,
    NULL, // dxcore_adapter_pPhysicalAdapterCount,
    NULL, // dxcore_adapter_pAdapterEngineCount,
    NULL // dxcore_adapter_pAdapterEngineName
};

size_t dxcore_adapter_sInstanceLuid(struct dxcore_adapter *adapter) {
    return sizeof(LUID);
}
size_t dxcore_adapter_sDriverVersion(struct dxcore_adapter *adapter) {
    return sizeof(uint64_t);
}
size_t dxcore_adapter_sDriverDescription(struct dxcore_adapter *adapter) {
    DXGI_ADAPTER_DESC desc;
    if (FAILED(IDXGIAdapter_GetDesc(adapter->adapter, &desc)))
        return 0;
    return wcslen(desc.Description)+1;
}
size_t dxcore_adapter_sHardwareID(struct dxcore_adapter *adapter) {
    return sizeof(DXCoreHardwareID);
}
size_t dxcore_adapter_sIsHardware(struct dxcore_adapter *adapter) {
    return 1;
}

size_t (*dxcore_adapter_property_sizes[])(struct dxcore_adapter *) = {
    dxcore_adapter_sInstanceLuid,
    dxcore_adapter_sDriverVersion,
    dxcore_adapter_sDriverDescription,
    dxcore_adapter_sHardwareID,
    NULL, // dxcore_adapter_pKmdModelVersion,
    NULL, // dxcore_adapter_pComputePreemptionGranularity,
    NULL, // dxcore_adapter_pGraphicsPreemptionGranularity,
    NULL, // dxcore_adapter_pDedicatedAdapterMemory,
    NULL, // dxcore_adapter_pDedicatedSystemMemory,
    NULL, // dxcore_adapter_pSharedSystemMemory,
    NULL, // dxcore_adapter_pAcgCompatible,
    dxcore_adapter_sIsHardware,
    NULL, // dxcore_adapter_pIsIntegrated,
    NULL, // dxcore_adapter_pIsDetachable,
    NULL, // dxcore_adapter_pHardwareIDParts,
    NULL, // dxcore_adapter_pPhysicalAdapterCount,
    NULL, // dxcore_adapter_pAdapterEngineCount,
    NULL // dxcore_adapter_pAdapterEngineName
};


ULONG STDMETHODCALLTYPE dxcore_adapter_AddRef(IDXCoreAdapter *iface) {
    LONG count;
    struct dxcore_adapter *this = impl_from_IDXCoreAdapter(iface);

    count = InterlockedIncrement(&this->refcount);
    return count;
}


ULONG STDMETHODCALLTYPE dxcore_adapter_Release(IDXCoreAdapter *iface) {
    LONG count;
    struct dxcore_adapter *this = impl_from_IDXCoreAdapter(iface);

    count = InterlockedDecrement(&this->refcount);
    if (!count) {
        IUnknown_Release(&this->factory->IDXCoreAdapterFactory_iface);
        free(this);
    }
    return count;
}


HRESULT STDMETHODCALLTYPE dxcore_adapter_QueryInterface(IDXCoreAdapter *iface, REFIID riid, void **ppv) {
    struct dxcore_adapter *this = impl_from_IDXCoreAdapter(iface);

    *ppv = NULL;
    TRACE("riid %s, factory %p.\n", wine_dbgstr_guid(riid), ppv);


    if (IsEqualIID(riid, &IID_IUnknown)
        ||IsEqualIID(riid, &IID_IDXCoreAdapter))
    {
        *ppv = iface;
        iface->lpVtbl->AddRef(iface);
        return S_OK;
    }

    /*
    -- AFFINITY SPECIFIC --
    d3d12_main.c/wined3d_get_adapter somehow gets passed this adapter object. To avoid having to rewrite d3d12
    (which still allow users to inject other d3d implementations), we just return our inner IDXGIAdapter here.
    */
    return IUnknown_QueryInterface(this->adapter, riid, ppv);
}


HRESULT STDMETHODCALLTYPE dxcore_adapter_GetFactory(IDXCoreAdapter *iface, REFIID riid, void **ppv) {
    struct dxcore_adapter *this = impl_from_IDXCoreAdapter(iface);
    IDXCoreAdapterFactory *factory = &this->factory->IDXCoreAdapterFactory_iface;
    return IUnknown_QueryInterface(factory, riid, ppv);
}


BOOL STDMETHODCALLTYPE dxcore_adapter_IsPropertySupported(
    IDXCoreAdapter *this,
    DXCoreAdapterProperty property
) {
    TRACE("property %d\n", property);
    return !((property >= (sizeof(dxcore_adapter_properties)/sizeof(void*)))
            || (dxcore_adapter_properties[property] == NULL));
}


HRESULT STDMETHODCALLTYPE dxcore_adapter_GetProperty(
    IDXCoreAdapter *iface,
    DXCoreAdapterProperty property, size_t buffer, void *propertyData
) {
    struct dxcore_adapter *this = impl_from_IDXCoreAdapter(iface);

    TRACE("property %d, buffer %lld, propertyData %p\n",
        property, (long long)buffer, propertyData);

    if ((property >= (sizeof(dxcore_adapter_properties)/sizeof(void*)))
            || (dxcore_adapter_properties[property] == NULL))
        return DXGI_ERROR_UNSUPPORTED;

    return dxcore_adapter_properties[property](this, buffer, propertyData);
}


HRESULT STDMETHODCALLTYPE dxcore_adapter_GetPropertySize(
    IDXCoreAdapter *iface,
    DXCoreAdapterProperty property, size_t *bufferSize
) {
    struct dxcore_adapter *this = impl_from_IDXCoreAdapter(iface);
    TRACE("property %d, bufferSize %p\n", property, bufferSize);

    if ((property >= (sizeof(dxcore_adapter_properties)/sizeof(void*)))
            || (dxcore_adapter_properties[property] == NULL))
        return DXGI_ERROR_UNSUPPORTED;

    *bufferSize = dxcore_adapter_property_sizes[property](this);
    return S_OK;
}


BOOL STDMETHODCALLTYPE dxcore_adapter_IsAttributeSupported(
    IDXCoreAdapter *this,
    REFGUID attributeGUID
) {
    FIXME("attributeGUID %s stub!\n", wine_dbgstr_guid(attributeGUID));
    return TRUE;
}


BOOL STDMETHODCALLTYPE dxcore_adapter_IsQueryStateSupported(
    IDXCoreAdapter *this,
    DXCoreAdapterState state
) {
    FIXME("state %d stub!\n", state);
    return FALSE;
}


BOOL STDMETHODCALLTYPE dxcore_adapter_IsSetStateSupported(
    IDXCoreAdapter *this,
    DXCoreAdapterState state
) {
    FIXME("state %d, stub!\n", state);
    return FALSE;
}


BOOL STDMETHODCALLTYPE dxcore_adapter_IsValid(
    IDXCoreAdapter *this
) {
    FIXME("assuming valid; stub!\n");
    return TRUE;
}


HRESULT STDMETHODCALLTYPE dxcore_adapter_QueryState(
    IDXCoreAdapter *this,
    DXCoreAdapterState state,
    size_t inputStateDetailsSize,
    void const *inputStateDetails,
    size_t outputBufferSize,
    void * outputBuffer
) {
    FIXME("state %d, inputStateDetailSize %lld, inputStateDetails %p, stub!\n",
        state, (long long)inputStateDetailsSize, inputStateDetails);
    return DXGI_ERROR_UNSUPPORTED;
}

HRESULT STDMETHODCALLTYPE dxcore_adapter_SetState(
    IDXCoreAdapter *this,
    DXCoreAdapterState state,
    size_t inputStateDetailsSize,
    void const *inputStateDetails,
    size_t inputDataSize,
    void const *inputData
) {
    FIXME("state %d, inputStateDetailsSize %lld, inputStateDetails %p, stub!\n",
        state, (long long)inputStateDetailsSize, inputStateDetails);
    return DXGI_ERROR_UNSUPPORTED;
}


static IDXCoreAdapterVtbl dxcore_adapter_vtbl = {
    dxcore_adapter_QueryInterface,
    dxcore_adapter_AddRef,
    dxcore_adapter_Release,
    dxcore_adapter_IsValid,
    dxcore_adapter_IsAttributeSupported,
    dxcore_adapter_IsPropertySupported,
    dxcore_adapter_GetProperty,
    dxcore_adapter_GetPropertySize,
    dxcore_adapter_IsQueryStateSupported,
    dxcore_adapter_QueryState,
    dxcore_adapter_IsSetStateSupported,
    dxcore_adapter_SetState,
    dxcore_adapter_GetFactory
};


HRESULT dxcore_adapter_create(
    struct dxcore_factory *factory,
    IDXGIAdapter *adapter,
    struct dxcore_adapter **result
) {
    struct dxcore_adapter *this;

    if (!(this = calloc(1, sizeof(*this))))
        return E_OUTOFMEMORY;

    this->refcount = 1;
    this->IDXCoreAdapter_iface.lpVtbl = &dxcore_adapter_vtbl;
    this->factory = factory;
    this->adapter = adapter;
    IUnknown_AddRef(&factory->IDXCoreAdapterFactory_iface);
    IUnknown_AddRef(adapter);

    *result = this;
    return S_OK;
}
