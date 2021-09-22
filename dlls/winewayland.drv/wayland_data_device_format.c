/*
 * Wayland data device format handling
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

#if 0
#pragma makedep unix
#endif

#include "config.h"

#include "ntstatus.h"
#define WIN32_NO_STATUS

#include "waylanddrv.h"

#include "wine/debug.h"

#include "shlobj.h"
#include "winternl.h"
#include "winnls.h"

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

WINE_DEFAULT_DEBUG_CHANNEL(clipboard);

static void write_all(int fd, const void *buf, size_t count)
{
    size_t nwritten = 0;

    while (nwritten < count)
    {
        ssize_t ret = write(fd, (const char*)buf + nwritten, count - nwritten);
        if (ret == -1 && errno != EINTR)
        {
            WARN("Failed to write all data, had %zu bytes, wrote %zu bytes (errno: %d)\n",
                 count, nwritten, errno);
            break;
        }
        else if (ret > 0)
        {
            nwritten += ret;
        }
    }
}

#define NLS_SECTION_CODEPAGE 11

static BOOL get_cp_tableinfo(ULONG cp, CPTABLEINFO *cptable)
{
    USHORT *ptr;
    SIZE_T nls_size;

    if (!NtGetNlsSectionPtr(NLS_SECTION_CODEPAGE, cp, NULL, (void **)&ptr, &nls_size))
    {
        RtlInitCodePageTable(ptr, cptable);
        return TRUE;
    }

    return FALSE;
}

static void *import_text_as_unicode(struct wayland_data_device_format *format,
                                    const void *data, size_t data_size, size_t *ret_size)
{
    DWORD wsize;
    void *ret;

    if (format->extra == CP_UTF8)
    {
        RtlUTF8ToUnicodeN(NULL, 0, &wsize, data, data_size);
        if (!(ret = malloc(wsize + sizeof(WCHAR)))) return NULL;
        RtlUTF8ToUnicodeN(ret, wsize, &wsize, data, data_size);
    }
    else
    {
        CPTABLEINFO cptable;
        /* In the worst case, each byte of the input text data corresponds
         * to a single character, which may need up to two WCHAR for UTF-16
         * encoding. */
        wsize = data_size * sizeof(WCHAR) * 2;
        if (!get_cp_tableinfo(format->extra, &cptable)) return NULL;
        if (!(ret = malloc(wsize + sizeof(WCHAR)))) return NULL;
        RtlCustomCPToUnicodeN(&cptable, ret, wsize, &wsize, data, data_size);
    }
    ((WCHAR *)ret)[wsize / sizeof(WCHAR)] = 0;

    if (ret_size) *ret_size = wsize + sizeof(WCHAR);

    return ret;
}

static void export_text(struct wayland_data_device_format *format, int fd, void *data, size_t size)
{
    DWORD byte_count;
    char *bytes;

    /* Wayland apps expect strings to not be zero-terminated, so avoid
     * zero-terminating the resulting converted string. */
    if (((WCHAR *)data)[size / sizeof(WCHAR) - 1] == 0) size -= sizeof(WCHAR);

    if (format->extra == CP_UTF8)
    {
        RtlUnicodeToUTF8N(NULL, 0, &byte_count, data, size);
        if (!(bytes = malloc(byte_count))) return;
        RtlUnicodeToUTF8N(bytes, byte_count, &byte_count, data, size);
    }
    else
    {
        CPTABLEINFO cptable;
        if (!get_cp_tableinfo(format->extra, &cptable)) return;
        byte_count = size / sizeof(WCHAR) * cptable.MaximumCharacterSize;
        if (!(bytes = malloc(byte_count))) return;
        RtlUnicodeToCustomCPN(&cptable, bytes, byte_count, &byte_count, data, size);
    }

    write_all(fd, bytes, byte_count);

    free(bytes);
}

static void *import_data(struct wayland_data_device_format *format,
                         const void *data, size_t data_size, size_t *ret_size)
{
    void *ret;

    ret = malloc(data_size);
    if (ret)
    {
        memcpy(ret, data, data_size);
        if (ret_size) *ret_size = data_size;
    }

    return ret;
}

static void export_data(struct wayland_data_device_format *format, int fd, void *data, size_t size)
{
    write_all(fd, data, size);
}

/* Adapted from winex11.drv/clipboard.c */
static char *decode_uri(const char *uri, size_t uri_length)
{
    char *decoded = malloc(uri_length + 1);
    size_t uri_i = 0;
    size_t decoded_i = 0;

    if (decoded == NULL)
        goto err;

    while (uri_i < uri_length)
    {
        if (uri[uri_i] == '%')
        {
            unsigned long number;
            char buffer[3];

            if (uri_i + 1 == uri_length || uri_i + 2 == uri_length)
                goto err;

            buffer[0] = uri[uri_i + 1];
            buffer[1] = uri[uri_i + 2];
            buffer[2] = '\0';
            errno = 0;
            number = strtoul(buffer, NULL, 16);
            if (errno != 0) goto err;
            decoded[decoded_i] = number;

            uri_i += 3;
            decoded_i++;
        }
        else
        {
            decoded[decoded_i++] = uri[uri_i++];
        }
    }

    decoded[decoded_i] = '\0';

    return decoded;

err:
    free(decoded);
    return NULL;
}

/* based on wine_get_dos_file_name */
static WCHAR *get_dos_file_name(const char *path)
{
    ULONG len = strlen(path) + 9; /* \??\unix prefix */
    WCHAR *ret;

    if (!(ret = malloc(len * sizeof(WCHAR)))) return NULL;
    if (wine_unix_to_nt_file_name(path, ret, &len))
    {
        free(ret);
        return NULL;
    }

    if (ret[5] == ':')
    {
        /* get rid of the \??\ prefix */
        memmove(ret, ret + 4, (len - 4) * sizeof(WCHAR));
    }
    else
    {
        ret[1] = '\\';
    }
    return ret;
}

/* Adapted from winex11.drv/clipboard.c */
static WCHAR* decoded_uri_to_dos(const char *uri)
{
    WCHAR *ret = NULL;

    if (strncmp(uri, "file:/", 6))
        return NULL;

    if (uri[6] == '/')
    {
        if (uri[7] == '/')
        {
            /* file:///path/to/file (nautilus, thunar) */
            ret = get_dos_file_name(&uri[7]);
        }
        else if (uri[7])
        {
            /* file://hostname/path/to/file (X file drag spec) */
            char hostname[256];
            char *path = strchr(&uri[7], '/');
            if (path)
            {
                *path = '\0';
                if (strcmp(&uri[7], "localhost") == 0)
                {
                    *path = '/';
                    ret = get_dos_file_name(path);
                }
                else if (gethostname(hostname, sizeof(hostname)) == 0)
                {
                    if (strcmp(hostname, &uri[7]) == 0)
                    {
                        *path = '/';
                        ret = get_dos_file_name(path);
                    }
                }
            }
        }
    }
    else if (uri[6])
    {
        /* file:/path/to/file (konqueror) */
        ret = get_dos_file_name(&uri[5]);
    }

    return ret;
}

static void *import_uri_list(struct wayland_data_device_format *format,
                             const void *data, size_t data_size, size_t *ret_size)
{
    DROPFILES *drop_files = NULL;
    size_t drop_size;
    const char *data_end = (const char *) data + data_size;
    const char *line_start = data;
    const char *line_end;
    WCHAR **path;
    struct wl_array paths;
    size_t total_chars = 0;
    WCHAR *dst;

    TRACE("data=%p size=%lu\n", data, (unsigned long)data_size);

    wl_array_init(&paths);

    while (line_start < data_end)
    {
        /* RFC 2483 requires CRLF for text/uri-list line termination, but
         * some applications send LF. Accept both line terminators. */
        line_end = strchr(line_start, '\n');
        if (line_end == NULL)
        {
            WARN("URI list line doesn't end in (\\r)\\n\n");
            break;
        }

        if (line_end > line_start && line_end[-1] == '\r') line_end--;

        if (line_start[0] != '#')
        {
            char *decoded_uri = decode_uri(line_start, line_end - line_start);
            TRACE("decoded_uri=%s\n", decoded_uri);
            path = wl_array_add(&paths, sizeof *path);
            if (!path) goto out;
            *path = decoded_uri_to_dos(decoded_uri);
            total_chars += lstrlenW(*path) + 1;
            free(decoded_uri);
        }

        line_start = line_end + (*line_end == '\r' ? 2 : 1);
    }

    /* DROPFILES points to an array of consecutive null terminated WCHAR strings,
     * followed by a final 0 WCHAR to denote the end of the array. We place that
     * array just after the DROPFILE struct itself. */
    drop_size = sizeof(DROPFILES) + (total_chars + 1) * sizeof(WCHAR);
    if (!(drop_files = malloc(drop_size)))
        goto out;

    drop_files->pFiles = sizeof(*drop_files);
    drop_files->pt.x = 0;
    drop_files->pt.y = 0;
    drop_files->fNC = FALSE;
    drop_files->fWide = TRUE;

    dst = (WCHAR *)(drop_files + 1);
    wl_array_for_each(path, &paths)
    {
        lstrcpyW(dst, *path);
        dst += lstrlenW(*path) + 1;
    }
    *dst = 0;

    if (ret_size) *ret_size = drop_size;

out:
    wl_array_for_each(path, &paths)
        free(*path);

    wl_array_release(&paths);

    return drop_files;
}

static CPTABLEINFO *get_ansi_cp(void)
{
    USHORT utf8_hdr[2] = { 0, CP_UTF8 };
    static CPTABLEINFO cp;
    if (!cp.CodePage)
    {
        if (NtCurrentTeb()->Peb->AnsiCodePageData)
            RtlInitCodePageTable(NtCurrentTeb()->Peb->AnsiCodePageData, &cp);
        else
            RtlInitCodePageTable(utf8_hdr, &cp);
    }
    return &cp;
}

/* Helper functions to implement export_hdrop, adapted from winex11.drv */

static BOOL get_nt_pathname(const WCHAR *name, UNICODE_STRING *nt_name)
{
    static const WCHAR ntprefixW[] = {'\\','?','?','\\'};
    static const WCHAR uncprefixW[] = {'U','N','C','\\'};
    size_t len = lstrlenW(name);
    WCHAR *ptr;

    nt_name->MaximumLength = (len + 8) * sizeof(WCHAR);
    if (!(ptr = malloc(nt_name->MaximumLength))) return FALSE;
    nt_name->Buffer = ptr;

    memcpy(ptr, ntprefixW, sizeof(ntprefixW));
    ptr += ARRAYSIZE(ntprefixW);
    if (name[0] == '\\' && name[1] == '\\')
    {
        if ((name[2] == '.' || name[2] == '?') && name[3] == '\\')
        {
            name += 4;
            len -= 4;
        }
        else
        {
            memcpy(ptr, uncprefixW, sizeof(uncprefixW));
            ptr += ARRAYSIZE(uncprefixW);
            name += 2;
            len -= 2;
        }
    }
    memcpy(ptr, name, (len + 1) * sizeof(WCHAR));
    ptr += len;
    nt_name->Length = (ptr - nt_name->Buffer) * sizeof(WCHAR);
    return TRUE;
}

static char *get_unix_file_name(const WCHAR *dosW)
{
    UNICODE_STRING nt_name;
    OBJECT_ATTRIBUTES attr;
    NTSTATUS status;
    ULONG size = 256;
    char *buffer;

    if (!get_nt_pathname(dosW, &nt_name)) return NULL;
    InitializeObjectAttributes(&attr, &nt_name, 0, 0, NULL);
    for (;;)
    {
        if (!(buffer = malloc(size)))
        {
            free(nt_name.Buffer);
            return NULL;
        }
        status = wine_nt_to_unix_file_name(&attr, buffer, &size, FILE_OPEN_IF);
        if (status != STATUS_BUFFER_TOO_SMALL) break;
        free(buffer);
    }
    free(nt_name.Buffer);
    if (status)
    {
        free(buffer);
        return NULL;
    }
    return buffer;
}

/* Export text/uri-list to CF_HDROP, adapted from winex11.drv */
static void export_hdrop(struct wayland_data_device_format *format, int fd,
                         void *data, size_t size)
{
    char *textUriList = NULL;
    UINT textUriListSize = 32;
    UINT next = 0;
    const WCHAR *ptr;
    WCHAR *unicode_data = NULL;
    DROPFILES *drop_files = data;

    if (!drop_files->fWide)
    {
        char *files = (char *)data + drop_files->pFiles;
        CPTABLEINFO *cp = get_ansi_cp();
        DWORD len = 0;

        while (files[len]) len += strlen(files + len) + 1;
        len++;

        if (!(ptr = unicode_data = malloc(len * sizeof(WCHAR)))) goto out;

        if (cp->CodePage == CP_UTF8)
            RtlUTF8ToUnicodeN(unicode_data, len * sizeof(WCHAR), &len, files, len);
        else
            RtlCustomCPToUnicodeN(cp, unicode_data, len * sizeof(WCHAR), &len, files, len);
    }
    else ptr = (const WCHAR *)((char *)data + drop_files->pFiles);

    if (!(textUriList = malloc(textUriListSize))) goto out;

    while (*ptr)
    {
        char *unixFilename = NULL;
        UINT uriSize;
        UINT u;

        unixFilename = get_unix_file_name(ptr);
        if (unixFilename == NULL) goto out;
        ptr += lstrlenW(ptr) + 1;

        uriSize = 8 + /* file:/// */
                  3 * (lstrlenA(unixFilename) - 1) + /* "%xy" per char except first '/' */
                  2; /* \r\n */
        if ((next + uriSize) > textUriListSize)
        {
            UINT biggerSize = max(2 * textUriListSize, next + uriSize);
            void *bigger = realloc(textUriList, biggerSize);
            if (bigger)
            {
                textUriList = bigger;
                textUriListSize = biggerSize;
            }
            else
            {
                free(unixFilename);
                goto out;
            }
        }
        lstrcpyA(&textUriList[next], "file:///");
        next += 8;
        /* URL encode everything - unnecessary, but easier/lighter than
         * linking in shlwapi, and can't hurt */
        for (u = 1; unixFilename[u]; u++)
        {
            static const char hex_table[] = "0123456789abcdef";
            textUriList[next++] = '%';
            textUriList[next++] = hex_table[unixFilename[u] >> 4];
            textUriList[next++] = hex_table[unixFilename[u] & 0xf];
        }
        textUriList[next++] = '\r';
        textUriList[next++] = '\n';
        free(unixFilename);
    }

    write_all(fd, textUriList, next);

out:
    free(unicode_data);
    free(textUriList);
}

#define CP_ASCII 20127

static const WCHAR rich_text_formatW[] = {'R','i','c','h',' ','T','e','x','t',' ','F','o','r','m','a','t',0};

/* Order is important. When selecting a mime-type for a clipboard format we
 * will choose the first entry that matches the specified clipboard format. */
static struct wayland_data_device_format supported_formats[] =
{
    {"text/plain;charset=utf-8", CF_UNICODETEXT, NULL, import_text_as_unicode, export_text, CP_UTF8},
    {"text/plain;charset=us-ascii", CF_UNICODETEXT, NULL, import_text_as_unicode, export_text, CP_ASCII},
    {"text/plain", CF_UNICODETEXT, NULL, import_text_as_unicode, export_text, CP_ASCII},
    {"text/rtf", 0, rich_text_formatW, import_data, export_data, 0},
    {"text/richtext", 0, rich_text_formatW, import_data, export_data, 0},
    {"text/uri-list", CF_HDROP, NULL, import_uri_list, export_hdrop, 0},
    {NULL, 0, NULL, NULL, NULL, 0},
};

static ATOM register_clipboard_format(const WCHAR *name)
{
    ATOM atom;
    if (NtAddAtom(name, lstrlenW(name) * sizeof(WCHAR), &atom)) return 0;
    return atom;
}

void wayland_data_device_init_formats(void)
{
    struct wayland_data_device_format *format = supported_formats;

    while (format->mime_type)
    {
        if (format->clipboard_format == 0)
            format->clipboard_format = register_clipboard_format(format->register_name);
        format++;
    }
}

struct wayland_data_device_format *wayland_data_device_format_for_mime_type(const char *mime)
{
    struct wayland_data_device_format *format = supported_formats;

    while (format->mime_type)
    {
        if (!strcmp(mime, format->mime_type))
            return format;
        format++;
    }

    return NULL;
}

static BOOL string_array_contains(struct wl_array *array, const char *str)
{
    char **p;

    wl_array_for_each(p, array)
        if (!strcmp(*p, str)) return TRUE;

    return FALSE;
}

struct wayland_data_device_format *wayland_data_device_format_for_clipboard_format(UINT clipboard_format,
                                                                                   struct wl_array *mimes)
{
    struct wayland_data_device_format *format = supported_formats;

    while (format->mime_type)
    {
        if (format->clipboard_format == clipboard_format &&
            (!mimes || string_array_contains(mimes, format->mime_type)))
        {
             return format;
        }
        format++;
    }

    return NULL;
}
