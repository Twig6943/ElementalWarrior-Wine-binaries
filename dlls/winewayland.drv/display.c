/*
 * WAYLAND display device functions
 *
 * Copyright 2019 Zhiyi Zhang for CodeWeavers
 * Copyright 2020 Alexandros Frantzis for Collabora Ltd
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

#if 0
#pragma makedep unix
#endif

#include "config.h"

#include "waylanddrv.h"

#include "wine/debug.h"

#include "ntuser.h"

WINE_DEFAULT_DEBUG_CHANNEL(waylanddrv);

#define NEXT_DEVMODEW(mode) ((DEVMODEW *)((char *)((mode) + 1) + (mode)->dmDriverExtra))

static BOOL force_display_devices_refresh;

static void wayland_refresh_display_devices(void)
{
    UINT32 num_path, num_mode;
    force_display_devices_refresh = TRUE;
    /* Trigger refresh in win32u */
    NtUserGetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &num_path, &num_mode);
}

static void wayland_resize_desktop_window(void)
{
    BOOL wayland_initialized = wayland_process_acquire()->initialized;
    wayland_process_release();

    /* During process wayland initialization we will get our initial output
     * information and init the display devices. There is no need to resize the
     * desktop in this case, since this is the initial display state.
     * Additionally, initialization may occur in a context that has acquired
     * the internal Wine user32 lock, and sending messages would lead to an
     * internal user32 lock error. */
    if (wayland_initialized)
        send_message(NtUserGetDesktopWindow(), WM_DISPLAYCHANGE, 0, 0);
}

/* Initialize registry display settings when new display devices are added */
static void wayland_init_registry_display_settings(void)
{
    DEVMODEW dm = {.dmSize = sizeof(dm)};
    DISPLAY_DEVICEW dd = {sizeof(dd)};
    UNICODE_STRING device_name;
    DWORD i = 0;
    int ret;

    while (!NtUserEnumDisplayDevices(NULL, i++, &dd, 0))
    {
        RtlInitUnicodeString(&device_name, dd.DeviceName);

        /* Skip if the device already has registry display settings */
        if (NtUserEnumDisplaySettings(&device_name, ENUM_REGISTRY_SETTINGS, &dm, 0))
            continue;

        if (!NtUserEnumDisplaySettings(&device_name, ENUM_CURRENT_SETTINGS, &dm, 0))
        {
            ERR("Failed to query current display settings for %s.\n", wine_dbgstr_w(dd.DeviceName));
            continue;
        }

        TRACE("Device %s current display mode %ux%u %ubits %uHz at %d,%d.\n",
              wine_dbgstr_w(dd.DeviceName), (UINT)dm.dmPelsWidth, (UINT)dm.dmPelsHeight,
              (UINT)dm.dmBitsPerPel, (UINT)dm.dmDisplayFrequency, (int)dm.dmPosition.x,
              (int)dm.dmPosition.y);

        ret = NtUserChangeDisplaySettings(&device_name, &dm, NULL,
                                          CDS_GLOBAL | CDS_NORESET | CDS_UPDATEREGISTRY, NULL);
        if (ret != DISP_CHANGE_SUCCESSFUL)
        {
            ERR("Failed to save registry display settings for %s, returned %d.\n",
                wine_dbgstr_w(dd.DeviceName), ret);
        }
    }
}

void wayland_init_display_devices()
{
    wayland_refresh_display_devices();
    wayland_notify_wine_monitor_change();
    wayland_init_registry_display_settings();
    wayland_resize_desktop_window();
}

static void wayland_add_device_gpu(const struct gdi_device_manager *device_manager,
                                   void *param)
{
    static const WCHAR wayland_gpuW[] = {'W','a','y','l','a','n','d','G','P','U',0};
    struct gdi_gpu gpu = {0};
    lstrcpyW(gpu.name, wayland_gpuW);

    /* TODO: Fill in gpu information from vulkan. */

    TRACE("id=0x%s name=%s\n",
          wine_dbgstr_longlong(gpu.id), wine_dbgstr_w(gpu.name));

    device_manager->add_gpu(&gpu, param);
}

static void wayland_add_device_adapter(const struct gdi_device_manager *device_manager,
                                       void *param, INT output_id)
{
    struct gdi_adapter adapter;
    adapter.id = output_id;
    adapter.state_flags = DISPLAY_DEVICE_ATTACHED_TO_DESKTOP;
    if (output_id == 0)
        adapter.state_flags |= DISPLAY_DEVICE_PRIMARY_DEVICE;

    TRACE("id=0x%s state_flags=0x%x\n",
          wine_dbgstr_longlong(adapter.id), (UINT)adapter.state_flags);

    device_manager->add_adapter(&adapter, param);
}

static void wayland_add_device_monitor(const struct gdi_device_manager *device_manager,
                                       void *param, struct wayland_output *output)
{

    struct gdi_monitor monitor = {0};

    SetRect(&monitor.rc_monitor, output->x, output->y,
            output->x + output->current_wine_mode->width,
            output->y + output->current_wine_mode->height);

    /* We don't have a direct way to get the work area in Wayland. */
    monitor.rc_work = monitor.rc_monitor;

    monitor.state_flags = DISPLAY_DEVICE_ATTACHED | DISPLAY_DEVICE_ACTIVE;

    TRACE("name=%s rc_monitor=rc_work=%s state_flags=0x%x\n",
          output->name, wine_dbgstr_rect(&monitor.rc_monitor),
          (UINT)monitor.state_flags);

    device_manager->add_monitor(&monitor, param);
}

static void populate_devmode(struct wayland_output_mode *output_mode, DEVMODEW *mode)
{
    mode->dmFields = DM_DISPLAYORIENTATION | DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT |
                     DM_DISPLAYFLAGS | DM_DISPLAYFREQUENCY;
    mode->dmDisplayOrientation = DMDO_DEFAULT;
    mode->dmDisplayFlags = 0;
    mode->dmBitsPerPel = output_mode->bpp;
    mode->dmPelsWidth = output_mode->width;
    mode->dmPelsHeight = output_mode->height;
    mode->dmDisplayFrequency = output_mode->refresh / 1000;
}

static void wayland_add_device_modes(const struct gdi_device_manager *device_manager,
                                     void *param, struct wayland_output *output)
{

    struct wayland_output_mode *output_mode;

    wl_list_for_each(output_mode, &output->mode_list, link)
    {
        DEVMODEW mode;
        populate_devmode(output_mode, &mode);
        device_manager->add_mode(&mode, param);
    }
}

static void wayland_add_device_output(const struct gdi_device_manager *device_manager,
                                      void *param, struct wayland_output *output,
                                      INT output_id)
{
    char buf[16];
    int len;

    /* TODO: Detect and support multiple monitors per adapter (i.e., mirroring). */
    wayland_add_device_adapter(device_manager, param, output_id);
    wayland_add_device_monitor(device_manager, param, output);
    wayland_add_device_modes(device_manager, param, output);

    /* Set the wine name in wayland_output so that we can look it up. */
    len = snprintf(buf, sizeof(buf), "\\\\.\\DISPLAY%u", output_id + 1);
    if (len > sizeof(buf)) len = sizeof(buf);
    ascii_to_unicode_z(output->wine_name, ARRAY_SIZE(output->wine_name), buf, len);
    TRACE("name=%s wine_name=%s\n",
          output->name, wine_dbgstr_w(output->wine_name));
}

static struct wayland_output *wayland_get_primary_output(struct wayland *wayland)
{
    struct wayland_output *output;

    wl_list_for_each(output, &wayland->output_list, link)
    {
        if (output->current_wine_mode && output->x == 0 && output->y == 0)
            return output;
    }

    return NULL;
}

/***********************************************************************
 *      UpdateDisplayDevices (WAYLAND.@)
 */
BOOL WAYLAND_UpdateDisplayDevices(const struct gdi_device_manager *device_manager,
                                  BOOL force, void *param)
{
    struct wayland *wayland;
    struct wayland_output *output, *primary;
    INT output_id = 0;

    if (!force && !force_display_devices_refresh) return TRUE;

    TRACE("force=%d force_refresh=%d\n", force, force_display_devices_refresh);

    force_display_devices_refresh = FALSE;

    wayland = wayland_process_acquire();

    wayland_add_device_gpu(device_manager, param);

    /* Get the primary output (i.e., positioned at 0,0) and add it with id 0. */
    primary = wayland_get_primary_output(wayland);
    if (primary)
    {
        wayland_add_device_output(device_manager, param, primary, output_id);
        output_id++;
    }

    wl_list_for_each(output, &wayland->output_list, link)
    {
        if (!output->current_wine_mode || output == primary) continue;
        wayland_add_device_output(device_manager, param, output, output_id);
        output_id++;
    }

    wayland_process_release();

    return TRUE;
}

static BOOL wayland_get_current_devmode(struct wayland *wayland, LPCWSTR name, DEVMODEW *mode)
{
    struct wayland_output *output;

    output = wayland_output_get_by_wine_name(wayland, name);
    if (!output || !output->current_wine_mode)
        return FALSE;

    populate_devmode(output->current_wine_mode, mode);

    mode->dmFields |= DM_POSITION;
    mode->dmPosition.x = output->x;
    mode->dmPosition.y = output->y;

    return TRUE;
}

/***********************************************************************
 *             GetCurrentDisplaySettings  (WAYLAND.@)
 *
 */
BOOL WAYLAND_GetCurrentDisplaySettings(LPCWSTR name, BOOL is_primary, LPDEVMODEW devmode)
{
    struct wayland *wayland = wayland_process_acquire();
    BOOL ret;

    TRACE("(%s,%p) wayland=%p\n", debugstr_w(name), devmode, wayland);

    ret = wayland_get_current_devmode(wayland, name, devmode);

    wayland_process_release();

    if (ret)
    {
        TRACE("=> %d,%d+%ux%u@%u %ubpp\n",
              (int)devmode->dmPosition.x, (int)devmode->dmPosition.y,
              (UINT)devmode->dmPelsWidth, (UINT)devmode->dmPelsHeight,
              (UINT)devmode->dmDisplayFrequency, (UINT)devmode->dmBitsPerPel);
    }
    else
    {
        ERR("Failed to get %s current display settings.\n", wine_dbgstr_w(name));
    }

    return ret;
}

static INT wayland_get_current_bpp(struct wayland *wayland, LPCWSTR name)
{
    struct wayland_output *output;

    output = wayland_output_get_by_wine_name(wayland, name);
    if (!output || !output->current_wine_mode)
        return 0;

    return output->current_wine_mode->bpp;
}

/***********************************************************************
 *             GetDisplayDepth  (WAYLAND.@)
 *
 */
INT WAYLAND_GetDisplayDepth(LPCWSTR name, BOOL is_primary)
{
    struct wayland *wayland = wayland_process_acquire();
    INT bpp;

    TRACE("(%s) wayland=%p\n", debugstr_w(name), wayland);

    bpp = wayland_get_current_bpp(wayland, name);

    wayland_process_release();

    if (bpp > 0)
        TRACE("=> %dbpp\n", bpp);
    else
        ERR("Failed to get %s display depth, returning 32.\n", wine_dbgstr_w(name));

    return bpp > 0 ? bpp : 32;
}

static struct wayland_output_mode *get_matching_output_mode(struct wayland_output *output,
                                                            LPDEVMODEW devmode)
{
    struct wayland_output_mode *output_mode;

    wl_list_for_each(output_mode, &output->mode_list, link)
    {
        if (devmode->dmPelsWidth == output_mode->width &&
            devmode->dmPelsHeight == output_mode->height &&
            output_mode->bpp == devmode->dmBitsPerPel &&
            output_mode->refresh / 1000 == devmode->dmDisplayFrequency)
        {
            return output_mode;
        }
    }

    return NULL;
}

/***********************************************************************
 *		ChangeDisplaySettings  (WAYLAND.@)
 *
 */
LONG WAYLAND_ChangeDisplaySettings(LPDEVMODEW displays, LPCWSTR primary_name,
                                   HWND hwnd, DWORD flags, LPVOID lpvoid)
{
    LONG ret;
    struct wayland *wayland = wayland_process_acquire();
    struct wayland_output *output;
    struct wayland_output_mode *output_mode;
    DEVMODEW *devmode;

    for (devmode = displays; devmode->dmSize; devmode = NEXT_DEVMODEW(devmode))
    {
        TRACE("device=%s devmode=%ux%u@%u %ubpp\n",
              wine_dbgstr_w(devmode->dmDeviceName), (UINT)devmode->dmPelsWidth,
              (UINT)devmode->dmPelsHeight, (UINT)devmode->dmDisplayFrequency,
              (UINT)devmode->dmBitsPerPel);

        output = wayland_output_get_by_wine_name(wayland, devmode->dmDeviceName);
        if (!output)
        {
            ret = DISP_CHANGE_BADPARAM;
            goto out;
        }

        output_mode = get_matching_output_mode(output, devmode);
        if (!output_mode)
        {
            ret = DISP_CHANGE_BADMODE;
            goto out;
        }

        wayland_output_set_wine_mode(output, output_mode);

        TRACE("output=%s (%s) set current wine mode %dx%d wine_scale %f\n",
              output->name, wine_dbgstr_w(output->wine_name),
              output_mode->width, output_mode->height, output->wine_scale);
    }

    /* Release the wayland process instance lock to avoid potential deadlocks
     * while notifying other thread instances below. */
    wayland_process_release();

    wayland_notify_wine_monitor_change();

    return DISP_CHANGE_SUCCESSFUL;

out:
    wayland_process_release();
    return ret;
}
