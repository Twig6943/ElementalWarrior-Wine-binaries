#include "opencl_private.h"
#include "opencl_types.h"
#include "unixlib.h"
#include "extensions.h"

#define COBJMACROS
#include "objbase.h"
#include "initguid.h"
#include "dxgi1_6.h"


WINE_DEFAULT_DEBUG_CHANNEL(opencl);


cl_int compare_dxgi_cl_device(IUnknown* d3d_adapter, cl_device_id cl_device, cl_bool *result) {
    cl_int err;
    IDXGIAdapter *this;
    DXGI_ADAPTER_DESC info;
    WCHAR *cl_name;
    struct clGetDeviceInfo_params params;

    // note: it is possible to compare the UUID if the device supports cl_khr_device_uuid
    // and I would like to do that, but I also want built in DXVK support :)
    // also SLI isn't really supported anymore so probs not relevant
    if (FAILED(IUnknown_QueryInterface(d3d_adapter, &IID_IDXGIAdapter, (void **)&this))) {
        ERR("Not a IDXGIAdapter %p.\n", d3d_adapter);
        return CL_DEVICE_NOT_FOUND;
    }

    if (FAILED(IDXGIAdapter_GetDesc(this, &info))) {
        ERR("Could not get adapter info %p.\n", this);
        IUnknown_Release(this);
        return CL_DEVICE_NOT_FOUND;
    }
    IUnknown_Release(this);


    params.device = cl_device;
    params.param_name = CL_DEVICE_NAME;
    params.param_value = NULL;
    params.param_value_size = 0;
    params.param_value_size_ret = &params.param_value_size;
    if ((err = OPENCL_CALL( clGetDeviceInfo, &params ))) {
        ERR("Error getting device name of %p; %d.\n", cl_device, err);
        return err;
    }

    if (!(params.param_value = calloc(params.param_value_size+1, 1)))
        err = CL_OUT_OF_HOST_MEMORY;
    if (!(cl_name = calloc(params.param_value_size+1, sizeof(WCHAR))))
        err = CL_OUT_OF_HOST_MEMORY;

    if (err) {
        if (params.param_value) free(params.param_value);
        if (cl_name) free(params.param_value);
        return err;
    }

    if ((err = OPENCL_CALL( clGetDeviceInfo, &params ))) {
        ERR("Error getting device name of %p; %d.\n", cl_device, err);
        return err;
    }

    mbstowcs(cl_name, params.param_value, params.param_value_size);
    free(params.param_value);

    TRACE("cl_name %s, from %lld, d3d %s.\n", wine_dbgstr_w(cl_name), params.param_value_size, wine_dbgstr_w(info.Description));
    *result = (wcscmp(cl_name, info.Description) == 0);

    free(cl_name);

    return CL_SUCCESS;
}


cl_int WINAPI dxgi_adapter_to_opencl(
    IUnknown *adapter,
    cl_platform_id platform,
    cl_uint d3d_device_set,
    cl_uint num_entries,
    cl_device_id *devices,
    cl_uint *num_devices
) {
    struct clGetDeviceIDs_params ids_params;

    int i;
    cl_int err;
    cl_uint p_num_devices;
    cl_bool same_device;


    ids_params.platform = platform;
    ids_params.device_type = CL_DEVICE_TYPE_ALL;
    ids_params.num_devices = &ids_params.num_entries;
    ids_params.num_entries = 0;
    ids_params.devices = NULL;

    if ((err = OPENCL_CALL( clGetDeviceIDs , &ids_params )))
        return err;

    if (!(ids_params.devices = calloc(ids_params.num_entries, sizeof(cl_device_id))))
        return CL_OUT_OF_HOST_MEMORY;

    if ((err = OPENCL_CALL( clGetDeviceIDs , &ids_params ))) {
        free(ids_params.devices);
        return err;
    }

    p_num_devices = 0;

    // compare devices
    for (i=0; i<ids_params.num_entries; i++) {
        err = compare_dxgi_cl_device(adapter, ids_params.devices[i], &same_device);
        if (err)
            break;

        if (!same_device)
            continue;

        if (devices) {
            if (num_entries > p_num_devices) {
                devices[p_num_devices] = ids_params.devices[i];
            } else {
                err = CL_INVALID_VALUE;
                break;
            }
        }
        p_num_devices++;
    }

    free(ids_params.devices);

    if ((!err) && num_devices)
        *num_devices = p_num_devices;

    return err;
}


cl_int WINAPI clGetDeviceIDsFromD3D10KHR(
    cl_platform_id platform,
    cl_uint d3d_device_source,
    void* d3d_object,
    cl_uint d3d_device_set,
    cl_uint num_entries,
    cl_device_id* devices,
    cl_uint* num_devices
) {
    TRACE("platform %p, device_source %d, d3d_object %p, d3d_device_set %u, num_entries %u, devices %p, num_devices %p semi-stub!\n",
        platform, d3d_device_source, d3d_object, d3d_device_set, num_entries, devices, num_devices);

    return dxgi_adapter_to_opencl((IUnknown*)d3d_object, platform,
            d3d_device_set, num_entries, devices, num_devices);
}


void* cl_khr_d3d10_sharing_get_function(const char* name) {
    // NOTE: other functions in this extension seem non-trivial to implement,
    // involve lots of deep knowledge of D3D and OpenCL (which I do not have),
    // and may involve a heavy rewrite of this whole OpenCL wrapper.
    if (!strcmp(name, "clGetDeviceIDsFromD3D10KHR"))
        return clGetDeviceIDsFromD3D10KHR;
    return NULL;
}
