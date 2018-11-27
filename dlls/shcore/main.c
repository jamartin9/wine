/*
 * Copyright 2016 Sebastian Lackner
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

#include "config.h"
#include <stdarg.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "initguid.h"
#include "ocidl.h"
#include "shellscalingapi.h"

#include "wine/debug.h"
#include "wine/heap.h"
#include "wine/unicode.h"

WINE_DEFAULT_DEBUG_CHANNEL(shcore);

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void *reserved)
{
    TRACE("(%p, %u, %p)\n", instance, reason, reserved);

    switch (reason)
    {
        case DLL_WINE_PREATTACH:
            return FALSE;  /* prefer native version */
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(instance);
            break;
    }

    return TRUE;
}

HRESULT WINAPI GetProcessDpiAwareness(HANDLE process, PROCESS_DPI_AWARENESS *value)
{
    if (GetProcessDpiAwarenessInternal( process, (DPI_AWARENESS *)value )) return S_OK;
    return HRESULT_FROM_WIN32( GetLastError() );
}

HRESULT WINAPI SetProcessDpiAwareness(PROCESS_DPI_AWARENESS value)
{
    if (SetProcessDpiAwarenessInternal( value )) return S_OK;
    return HRESULT_FROM_WIN32( GetLastError() );
}

HRESULT WINAPI GetDpiForMonitor(HMONITOR monitor, MONITOR_DPI_TYPE type, UINT *x, UINT *y)
{
    if (GetDpiForMonitorInternal( monitor, type, x, y )) return S_OK;
    return HRESULT_FROM_WIN32( GetLastError() );
}

HRESULT WINAPI _IStream_Read(IStream *stream, void *dest, ULONG size)
{
    ULONG read;
    HRESULT hr;

    TRACE("(%p, %p, %u)\n", stream, dest, size);

    hr = IStream_Read(stream, dest, size, &read);
    if (SUCCEEDED(hr) && read != size)
        hr = E_FAIL;
    return hr;
}

HRESULT WINAPI IStream_Reset(IStream *stream)
{
    static const LARGE_INTEGER zero;

    TRACE("(%p)\n", stream);

    return IStream_Seek(stream, zero, 0, NULL);
}

HRESULT WINAPI IStream_Size(IStream *stream, ULARGE_INTEGER *size)
{
    STATSTG statstg;
    HRESULT hr;

    TRACE("(%p, %p)\n", stream, size);

    memset(&statstg, 0, sizeof(statstg));

    hr = IStream_Stat(stream, &statstg, STATFLAG_NONAME);

    if (SUCCEEDED(hr) && size)
        *size = statstg.cbSize;
    return hr;
}

HRESULT WINAPI _IStream_Write(IStream *stream, const void *src, ULONG size)
{
    ULONG written;
    HRESULT hr;

    TRACE("(%p, %p, %u)\n", stream, src, size);

    hr = IStream_Write(stream, src, size, &written);
    if (SUCCEEDED(hr) && written != size)
        hr = E_FAIL;

    return hr;
}

void WINAPI IUnknown_AtomicRelease(IUnknown **obj)
{
    TRACE("(%p)\n", obj);

    if (!obj || !*obj)
        return;

    IUnknown_Release(*obj);
    *obj = NULL;
}

HRESULT WINAPI IUnknown_GetSite(IUnknown *unk, REFIID iid, void **site)
{
    IObjectWithSite *obj = NULL;
    HRESULT hr = E_INVALIDARG;

    TRACE("(%p, %s, %p)\n", unk, debugstr_guid(iid), site);

    if (unk && iid && site)
    {
        hr = IUnknown_QueryInterface(unk, &IID_IObjectWithSite, (void **)&obj);
        if (SUCCEEDED(hr) && obj)
        {
            hr = IObjectWithSite_GetSite(obj, iid, site);
            IObjectWithSite_Release(obj);
        }
    }

    return hr;
}

HRESULT WINAPI IUnknown_QueryService(IUnknown *obj, REFGUID sid, REFIID iid, void **out)
{
    IServiceProvider *provider = NULL;
    HRESULT hr;

    if (!out)
        return E_FAIL;

    *out = NULL;

    if (!obj)
        return E_FAIL;

    hr = IUnknown_QueryInterface(obj, &IID_IServiceProvider, (void **)&provider);
    if (hr == S_OK && provider)
    {
        TRACE("Using provider %p.\n", provider);

        hr = IServiceProvider_QueryService(provider, sid, iid, out);

        TRACE("Provider %p returned %p.\n", provider, *out);

        IServiceProvider_Release(provider);
    }

    return hr;
}

void WINAPI IUnknown_Set(IUnknown **dest, IUnknown *src)
{
    TRACE("(%p, %p)\n", dest, src);

    IUnknown_AtomicRelease(dest);

    if (src)
    {
        IUnknown_AddRef(src);
        *dest = src;
    }
}

HRESULT WINAPI IUnknown_SetSite(IUnknown *obj, IUnknown *site)
{
    IInternetSecurityManager *sec_manager;
    IObjectWithSite *objwithsite;
    HRESULT hr;

    if (!obj)
        return E_FAIL;

    hr = IUnknown_QueryInterface(obj, &IID_IObjectWithSite, (void **)&objwithsite);
    TRACE("ObjectWithSite %p, hr %#x.\n", objwithsite, hr);
    if (SUCCEEDED(hr))
    {
        hr = IObjectWithSite_SetSite(objwithsite, site);
        TRACE("SetSite() hr %#x.\n", hr);
        IObjectWithSite_Release(objwithsite);
    }
    else
    {
        hr = IUnknown_QueryInterface(obj, &IID_IInternetSecurityManager, (void **)&sec_manager);
        TRACE("InternetSecurityManager %p, hr %#x.\n", sec_manager, hr);
        if (FAILED(hr))
            return hr;

        hr = IInternetSecurityManager_SetSecuritySite(sec_manager, (IInternetSecurityMgrSite *)site);
        TRACE("SetSecuritySite() hr %#x.\n", hr);
        IInternetSecurityManager_Release(sec_manager);
    }

    return hr;
}

HRESULT WINAPI SetCurrentProcessExplicitAppUserModelID(const WCHAR *appid)
{
    FIXME("%s: stub\n", debugstr_w(appid));
    return E_NOTIMPL;
}

HRESULT WINAPI GetCurrentProcessExplicitAppUserModelID(const WCHAR **appid)
{
    FIXME("%p: stub\n", appid);
    *appid = NULL;
    return E_NOTIMPL;
}

/*************************************************************************
 * CommandLineToArgvW            [SHCORE.@]
 *
 * We must interpret the quotes in the command line to rebuild the argv
 * array correctly:
 * - arguments are separated by spaces or tabs
 * - quotes serve as optional argument delimiters
 *   '"a b"'   -> 'a b'
 * - escaped quotes must be converted back to '"'
 *   '\"'      -> '"'
 * - consecutive backslashes preceding a quote see their number halved with
 *   the remainder escaping the quote:
 *   2n   backslashes + quote -> n backslashes + quote as an argument delimiter
 *   2n+1 backslashes + quote -> n backslashes + literal quote
 * - backslashes that are not followed by a quote are copied literally:
 *   'a\b'     -> 'a\b'
 *   'a\\b'    -> 'a\\b'
 * - in quoted strings, consecutive quotes see their number divided by three
 *   with the remainder modulo 3 deciding whether to close the string or not.
 *   Note that the opening quote must be counted in the consecutive quotes,
 *   that's the (1+) below:
 *   (1+) 3n   quotes -> n quotes
 *   (1+) 3n+1 quotes -> n quotes plus closes the quoted string
 *   (1+) 3n+2 quotes -> n+1 quotes plus closes the quoted string
 * - in unquoted strings, the first quote opens the quoted string and the
 *   remaining consecutive quotes follow the above rule.
 */
WCHAR** WINAPI CommandLineToArgvW(const WCHAR *cmdline, int *numargs)
{
    int qcount, bcount;
    const WCHAR *s;
    WCHAR **argv;
    DWORD argc;
    WCHAR *d;

    if (!numargs)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    if (*cmdline == 0)
    {
        /* Return the path to the executable */
        DWORD len, deslen = MAX_PATH, size;

        size = sizeof(WCHAR *) * 2 + deslen * sizeof(WCHAR);
        for (;;)
        {
            if (!(argv = LocalAlloc(LMEM_FIXED, size))) return NULL;
            len = GetModuleFileNameW(0, (WCHAR *)(argv + 2), deslen);
            if (!len)
            {
                LocalFree(argv);
                return NULL;
            }
            if (len < deslen) break;
            deslen *= 2;
            size = sizeof(WCHAR *) * 2 + deslen * sizeof(WCHAR);
            LocalFree(argv);
        }
        argv[0] = (WCHAR *)(argv + 2);
        argv[1] = NULL;
        *numargs = 1;

        return argv;
    }

    /* --- First count the arguments */
    argc = 1;
    s = cmdline;
    /* The first argument, the executable path, follows special rules */
    if (*s == '"')
    {
        /* The executable path ends at the next quote, no matter what */
        s++;
        while (*s)
            if (*s++ == '"')
                break;
    }
    else
    {
        /* The executable path ends at the next space, no matter what */
        while (*s && *s != ' ' && *s != '\t')
            s++;
    }
    /* skip to the first argument, if any */
    while (*s == ' ' || *s == '\t')
        s++;
    if (*s)
        argc++;

    /* Analyze the remaining arguments */
    qcount = bcount = 0;
    while (*s)
    {
        if ((*s == ' ' || *s == '\t') && qcount == 0)
        {
            /* skip to the next argument and count it if any */
            while (*s == ' ' || *s == '\t')
                s++;
            if (*s)
                argc++;
            bcount = 0;
        }
        else if (*s == '\\')
        {
            /* '\', count them */
            bcount++;
            s++;
        }
        else if (*s == '"')
        {
            /* '"' */
            if ((bcount & 1) == 0)
                qcount++; /* unescaped '"' */
            s++;
            bcount = 0;
            /* consecutive quotes, see comment in copying code below */
            while (*s == '"')
            {
                qcount++;
                s++;
            }
            qcount = qcount % 3;
            if (qcount == 2)
                qcount = 0;
        }
        else
        {
            /* a regular character */
            bcount = 0;
            s++;
        }
    }

    /* Allocate in a single lump, the string array, and the strings that go
     * with it. This way the caller can make a single LocalFree() call to free
     * both, as per MSDN.
     */
    argv = LocalAlloc(LMEM_FIXED, (argc + 1) * sizeof(WCHAR *) + (strlenW(cmdline) + 1) * sizeof(WCHAR));
    if (!argv)
        return NULL;

    /* --- Then split and copy the arguments */
    argv[0] = d = strcpyW((WCHAR *)(argv + argc + 1), cmdline);
    argc = 1;
    /* The first argument, the executable path, follows special rules */
    if (*d == '"')
    {
        /* The executable path ends at the next quote, no matter what */
        s = d + 1;
        while (*s)
        {
            if (*s == '"')
            {
                s++;
                break;
            }
            *d++ = *s++;
        }
    }
    else
    {
        /* The executable path ends at the next space, no matter what */
        while (*d && *d != ' ' && *d != '\t')
            d++;
        s = d;
        if (*s)
            s++;
    }
    /* close the executable path */
    *d++ = 0;
    /* skip to the first argument and initialize it if any */
    while (*s == ' ' || *s == '\t')
        s++;
    if (!*s)
    {
        /* There are no parameters so we are all done */
        argv[argc] = NULL;
        *numargs = argc;
        return argv;
    }

    /* Split and copy the remaining arguments */
    argv[argc++] = d;
    qcount = bcount = 0;
    while (*s)
    {
        if ((*s == ' ' || *s == '\t') && qcount == 0)
        {
            /* close the argument */
            *d++ = 0;
            bcount = 0;

            /* skip to the next one and initialize it if any */
            do {
                s++;
            } while (*s == ' ' || *s == '\t');
            if (*s)
                argv[argc++] = d;
        }
        else if (*s=='\\')
        {
            *d++ = *s++;
            bcount++;
        }
        else if (*s == '"')
        {
            if ((bcount & 1) == 0)
            {
                /* Preceded by an even number of '\', this is half that
                 * number of '\', plus a quote which we erase.
                 */
                d -= bcount / 2;
                qcount++;
            }
            else
            {
                /* Preceded by an odd number of '\', this is half that
                 * number of '\' followed by a '"'
                 */
                d = d - bcount / 2 - 1;
                *d++ = '"';
            }
            s++;
            bcount = 0;
            /* Now count the number of consecutive quotes. Note that qcount
             * already takes into account the opening quote if any, as well as
             * the quote that lead us here.
             */
            while (*s == '"')
            {
                if (++qcount == 3)
                {
                    *d++ = '"';
                    qcount = 0;
                }
                s++;
            }
            if (qcount == 2)
                qcount = 0;
        }
        else
        {
            /* a regular character */
            *d++ = *s++;
            bcount = 0;
        }
    }
    *d = '\0';
    argv[argc] = NULL;
    *numargs = argc;

    return argv;
}

struct shstream
{
    IStream IStream_iface;
    LONG refcount;

    union
    {
        struct
        {
            BYTE *buffer;
            DWORD length;
            DWORD position;
        } mem;
    } u;
};

static inline struct shstream *impl_from_IStream(IStream *iface)
{
    return CONTAINING_RECORD(iface, struct shstream, IStream_iface);
}

static HRESULT WINAPI shstream_QueryInterface(IStream *iface, REFIID riid, void **out)
{
    struct shstream *stream = impl_from_IStream(iface);

    TRACE("(%p)->(%s, %p)\n", stream, debugstr_guid(riid), out);

    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IStream))
    {
        *out = iface;
        IStream_AddRef(iface);
        return S_OK;
    }

    *out = NULL;
    WARN("Unsupported interface %s.\n", debugstr_guid(riid));
    return E_NOINTERFACE;
}

static ULONG WINAPI shstream_AddRef(IStream *iface)
{
    struct shstream *stream = impl_from_IStream(iface);
    ULONG refcount = InterlockedIncrement(&stream->refcount);

    TRACE("(%p)->(%u)\n", stream, refcount);

    return refcount;
}

static ULONG WINAPI memstream_Release(IStream *iface)
{
    struct shstream *stream = impl_from_IStream(iface);
    ULONG refcount = InterlockedDecrement(&stream->refcount);

    TRACE("(%p)->(%u)\n", stream, refcount);

    if (!refcount)
    {
        heap_free(stream->u.mem.buffer);
        heap_free(stream);
    }

    return refcount;
}

static HRESULT WINAPI memstream_Read(IStream *iface, void *buff, ULONG buff_size, ULONG *read_len)
{
    struct shstream *stream = impl_from_IStream(iface);
    DWORD length;

    TRACE("(%p)->(%p, %u, %p)\n", stream, buff, buff_size, read_len);

    if (stream->u.mem.position >= stream->u.mem.length)
        length = 0;
    else
        length = stream->u.mem.length - stream->u.mem.position;

    length = buff_size > length ? length : buff_size;
    if (length != 0) /* not at end of buffer and we want to read something */
    {
        memmove(buff, stream->u.mem.buffer + stream->u.mem.position, length);
        stream->u.mem.position += length; /* adjust pointer */
    }

    if (read_len)
        *read_len = length;

    return S_OK;
}

static HRESULT WINAPI memstream_Write(IStream *iface, const void *buff, ULONG buff_size, ULONG *written)
{
    struct shstream *stream = impl_from_IStream(iface);
    DWORD length = stream->u.mem.position + buff_size;

    TRACE("(%p)->(%p, %u, %p)\n", stream, buff, buff_size, written);

    if (length < stream->u.mem.position) /* overflow */
        return STG_E_INSUFFICIENTMEMORY;

    if (length > stream->u.mem.length)
    {
        BYTE *buffer = HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, stream->u.mem.buffer, length);
        if (!buffer)
            return STG_E_INSUFFICIENTMEMORY;

        stream->u.mem.length = length;
        stream->u.mem.buffer = buffer;
    }
    memmove(stream->u.mem.buffer + stream->u.mem.position, buff, buff_size);
    stream->u.mem.position += buff_size; /* adjust pointer */

    if (written)
        *written = buff_size;

    return S_OK;
}

static HRESULT WINAPI memstream_Seek(IStream *iface, LARGE_INTEGER move, DWORD origin, ULARGE_INTEGER*new_pos)
{
    struct shstream *stream = impl_from_IStream(iface);
    LARGE_INTEGER tmp;

    TRACE("(%p)->(%s, %d, %p)\n", stream, wine_dbgstr_longlong(move.QuadPart), origin, new_pos);

    if (origin == STREAM_SEEK_SET)
        tmp = move;
    else if (origin == STREAM_SEEK_CUR)
        tmp.QuadPart = stream->u.mem.position + move.QuadPart;
    else if (origin == STREAM_SEEK_END)
        tmp.QuadPart = stream->u.mem.length + move.QuadPart;
    else
        return STG_E_INVALIDPARAMETER;

    if (tmp.QuadPart < 0)
        return STG_E_INVALIDFUNCTION;

    /* we cut off the high part here */
    stream->u.mem.position = tmp.u.LowPart;

    if (new_pos)
        new_pos->QuadPart = stream->u.mem.position;
    return S_OK;
}

static HRESULT WINAPI memstream_SetSize(IStream *iface, ULARGE_INTEGER new_size)
{
    struct shstream *stream = impl_from_IStream(iface);
    DWORD length;
    BYTE *buffer;

    TRACE("(%p, %s)\n", stream, wine_dbgstr_longlong(new_size.QuadPart));

    /* we cut off the high part here */
    length = new_size.u.LowPart;
    buffer = HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, stream->u.mem.buffer, length);
    if (!buffer)
        return STG_E_INSUFFICIENTMEMORY;

    stream->u.mem.buffer = buffer;
    stream->u.mem.length = length;

    return S_OK;
}

static HRESULT WINAPI shstream_CopyTo(IStream *iface, IStream *pstm, ULARGE_INTEGER size, ULARGE_INTEGER *read_len, ULARGE_INTEGER *written)
{
    struct shstream *stream = impl_from_IStream(iface);

    TRACE("(%p)\n", stream);

    if (read_len)
        read_len->QuadPart = 0;

    if (written)
        written->QuadPart = 0;

    /* TODO implement */
    return E_NOTIMPL;
}

static HRESULT WINAPI shstream_Commit(IStream *iface, DWORD flags)
{
    struct shstream *stream = impl_from_IStream(iface);

    TRACE("(%p, %#x)\n", stream, flags);

    /* Commit is not supported by this stream */
    return E_NOTIMPL;
}

static HRESULT WINAPI shstream_Revert(IStream *iface)
{
    struct shstream *stream = impl_from_IStream(iface);

    TRACE("(%p)\n", stream);

    /* revert not supported by this stream */
    return E_NOTIMPL;
}

static HRESULT WINAPI shstream_LockRegion(IStream *iface, ULARGE_INTEGER offset, ULARGE_INTEGER size, DWORD lock_type)
{
    struct shstream *stream = impl_from_IStream(iface);

    TRACE("(%p)\n", stream);

    /* lock/unlock not supported by this stream */
    return E_NOTIMPL;
}

static HRESULT WINAPI shstream_UnlockRegion(IStream *iface, ULARGE_INTEGER offset, ULARGE_INTEGER size, DWORD lock_type)
{
    struct shstream *stream = impl_from_IStream(iface);

    TRACE("(%p)\n", stream);

    /* lock/unlock not supported by this stream */
    return E_NOTIMPL;
}

static HRESULT WINAPI memstream_Stat(IStream *iface, STATSTG *statstg, DWORD flags)
{
    struct shstream *stream = impl_from_IStream(iface);

    TRACE("(%p, %p, %#x)\n", stream, statstg, flags);

    memset(statstg, 0, sizeof(*statstg));
    statstg->type = STGTY_STREAM;
    statstg->cbSize.QuadPart = stream->u.mem.length;
    statstg->grfMode = STGM_READWRITE;

    return S_OK;
}

static HRESULT WINAPI shstream_Clone(IStream *iface, IStream **dest)
{
    struct shstream *stream = impl_from_IStream(iface);

    TRACE("(%p, %p)\n", stream, dest);

    *dest = NULL;

    /* clone not supported by this stream */
    return E_NOTIMPL;
}

static const IStreamVtbl shstreamvtbl =
{
    shstream_QueryInterface,
    shstream_AddRef,
    memstream_Release,
    memstream_Read,
    memstream_Write,
    memstream_Seek,
    memstream_SetSize,
    shstream_CopyTo,
    shstream_Commit,
    shstream_Revert,
    shstream_LockRegion,
    shstream_UnlockRegion,
    memstream_Stat,
    shstream_Clone,
};

/*************************************************************************
 * SHCreateMemStream   [SHCORE.@]
 *
 * Create an IStream object on a block of memory.
 *
 * PARAMS
 * data     [I] Memory block to create the IStream object on
 * data_len [I] Length of data block
 *
 * RETURNS
 * Success: A pointer to the IStream object.
 * Failure: NULL, if any parameters are invalid or an error occurs.
 *
 * NOTES
 *  A copy of the memory block is made, it's freed when the stream is released.
 */
IStream * WINAPI SHCreateMemStream(const BYTE *data, UINT data_len)
{
    struct shstream *stream;

    TRACE("(%p, %u)\n", data, data_len);

    if (!data)
        data_len = 0;

    stream = heap_alloc(sizeof(*stream));
    stream->IStream_iface.lpVtbl = &shstreamvtbl;
    stream->refcount = 1;
    stream->u.mem.buffer = heap_alloc(data_len);
    if (!stream->u.mem.buffer)
    {
        heap_free(stream);
        return NULL;
    }
    memcpy(stream->u.mem.buffer, data, data_len);
    stream->u.mem.length = data_len;
    stream->u.mem.position = 0;

    return &stream->IStream_iface;
}
