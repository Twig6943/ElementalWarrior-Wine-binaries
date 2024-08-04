#include "opencl_private.h"
#include "opencl_types.h"
#include "unixlib.h"
#include "extensions.h"

#define COBJMACROS
#include "objbase.h"
#include "initguid.h"
#include "wine/wined3d.h"
#include "wine/winedxgi.h"

WINE_DEFAULT_DEBUG_CHANNEL(opencl);


cl_int compare_dxgi_cl_device(IUnknown* d3d_adapter, cl_device_id cl_device, cl_bool *result) {
    cl_int err;
    IWineDXGIAdapter *this;
    DWORD cl_vid;
    struct wine_dxgi_adapter_info dx_info;
    struct clGetDeviceInfo_params params;

    /*
    So it turns out that the ROCM opencl driver sets the name to a weird codename,
    and does not support cl_khr_device_uuid, so we can't use those to confirm the devices are the same.
    There's also some funkiness with clSetEventCallback and/or clFlush.
    */
    if (FAILED(IUnknown_QueryInterface(d3d_adapter, &IID_IWineDXGIAdapter, (void **)&this))) {
        ERR("Not a IDXGIAdapter %p.\n", d3d_adapter);
        return CL_DEVICE_NOT_FOUND;
    }

    if (FAILED(IWineDXGIAdapter_get_adapter_info(this, &dx_info))) {
        ERR("Could not get adapter info %p.\n", this);
        IUnknown_Release(this);
        return CL_DEVICE_NOT_FOUND;
    }
    IUnknown_Release(this);

    params.device = cl_device;
    params.param_name = CL_DEVICE_VENDOR_ID;
    params.param_value = &cl_vid;
    params.param_value_size = sizeof(cl_vid);
    params.param_value_size_ret = NULL;
    if ((err = OPENCL_CALL( clGetDeviceInfo, &params ))) {
        ERR("Error getting device uuid of %p; %d.\n", cl_device, err);
        return err;
    }

    *result = cl_vid == dx_info.vendor_id;
    TRACE("cl_vid %ld, dx_uuid %ld, samedevice %i\n", cl_vid, dx_info.vendor_id, *result);

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
