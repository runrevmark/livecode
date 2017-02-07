/*                                                                     -*-c++-*-
Copyright (C) 2017 LiveCode Ltd.

This file is part of LiveCode.

LiveCode is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.

LiveCode is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with LiveCode.  If not see <http://www.gnu.org/licenses/>.  */

#include "system-private.h"

#include <foundation-auto.h>

/* ---------------------------------------------------------------- */

#if defined(__WINDOWS__)
#define MCSLIBRARY_HAVE_HMODULE
#endif

#if defined(__MAC__)
#define MCSLIBRARY_HAVE_CFBUNDLEREF
#endif

#if defined(__MAC__) || defined(__IOS__) || defined(__ANDROID__) || defined(__LINUX__)
#define MCSLIBRARY_HAVE_DLHANDLE
#endif

/* ---------------------------------------------------------------- */

/* The singly-linked list of registered static libraries. */
//static MCSLibraryStaticInfo *s_static_libraries = nullptr;

/* The MCSLibraryRef custom typeinfo. */
MCTypeInfoRef kMCSLibraryTypeInfo = nullptr;

/* The MCSLibraryCouldNotLoadError error. */
MCErrorRef kMCSLibraryCouldNotLoadError = nullptr;

/* ================================================================
 * Types
 * ================================================================ */

class __MCSLibraryImpl
{
public:
    /* Unload the library wrapped by the object. */
    virtual ~__MCSLibraryImpl(void) {}
    
    /* Get the (unique) pointer representing the wrapped library. */
    virtual const void *Pointer(void) const = 0;
    
    /* Copy the full path to the wrapped library. */
    virtual bool CopyNativePath(MCStringRef& r_path) const = 0;
    
    /* Resolve the given symbol in the wrapped library. */
    virtual void *LookupSymbol(MCStringRef p_symbol) const = 0;
};

static inline __MCSLibraryImpl&
__MCSLibraryGetImpl(MCSLibraryRef p_library)
{
    return *(__MCSLibraryImpl *)MCValueGetExtraBytesPtr(p_library);
}

static inline __MCSLibraryImpl&
__MCSLibraryGetImpl(MCValueRef p_value)
{
    MCAssert(MCValueGetTypeInfo(p_value) == kMCSLibraryTypeInfo);
    
    return *(__MCSLibraryImpl *)MCValueGetExtraBytesPtr(p_value);
}

static void
__MCSLibraryDestroy(MCValueRef p_value)
{
    __MCSLibraryGetImpl(p_value).~__MCSLibraryImpl();
}

static bool
__MCSLibraryCopy(MCValueRef p_value,
                 bool p_release,
                 MCValueRef& r_copied_value)
{
    if (p_release)
        r_copied_value = p_value;
    else
        r_copied_value = MCValueRetain(p_value);
    return true;
}

static bool
__MCSLibraryEqual(MCValueRef p_left,
                  MCValueRef p_right)
{
    __MCSLibraryImpl& t_left_impl = __MCSLibraryGetImpl(p_left);
    __MCSLibraryImpl& t_right_impl = __MCSLibraryGetImpl(p_right);
    
    return t_left_impl.Pointer() == t_right_impl.Pointer();
}

static hash_t
__MCSLibraryHash(MCValueRef p_value)
{
    return MCHashPointer(__MCSLibraryGetImpl(p_value).Pointer());
}

static bool
__MCSLibraryDescribe(MCValueRef p_value,
                     MCStringRef& r_description)
{
    return false;
}

static MCValueCustomCallbacks
__kMCSLibraryCallbacks =
{
    false,
    __MCSLibraryDestroy,
    __MCSLibraryCopy,
    __MCSLibraryEqual,
    __MCSLibraryHash,
    __MCSLibraryDescribe,
    nullptr,
    nullptr,
};

bool
__MCSLibraryInitialize(void)
{
    if (!MCNamedCustomTypeInfoCreate(MCNAME("livecode.system.Library"),
                                     kMCNullTypeInfo,
                                     &__kMCSLibraryCallbacks,
                                     kMCSLibraryTypeInfo))
    {
        return false;
    }
    
    return true;
}

void
__MCSLibraryFinalize(void)
{
    MCValueRelease(kMCSLibraryTypeInfo);
}

template<typename Impl, typename HandleType>
static inline bool
__MCSLibraryCreateRef(HandleType p_handle,
                      MCSLibraryRef& r_library)
{
    MCSLibraryRef t_library;
    if (!MCValueCreateCustom(kMCSLibraryTypeInfo,
                             sizeof(Impl),
                             t_library))
    {
        return false;
    }
    
    new (MCValueGetExtraBytesPtr(t_library)) Impl(p_handle);
    
    r_library = t_library;
    
    return true;
}

/* ================================================================
 * Errors
 * ================================================================ */

/* Thrown if a library on the given path is not found. */
static bool
__MCSLibraryThrowNotFoundWithPathError(MCStringRef p_path)
{
    return false;
}

/* Thrown if a library at the given address is not found. */
static bool
__MCSLibraryThrowNotFoundWithAddressError(void *p_address)
{
    return false;
}

/* Thrown if the attempt to load the library on the given path failed. */
static bool
__MCSLibraryThrowCreateWithNativePathFailedError(MCStringRef p_native_path)
{
    return false;
}

/* Thrown if the attempt to load the library at the given address failed. */
static bool
__MCSLibraryThrowCreateWithAddressFailedError(void *p_address)
{
    return false;
}

/* Thrown if the attempt to compute a library's path failed */
static bool
__MCSLibraryThrowResolvePathFailedError(void)
{
    return false;
}

/* ================================================================
 * MCSLibraryStaticInfo& (static) binding
 * ================================================================ */

#define __MCSLIBRARY_HAS_STATIC_IMPL
class __MCSStaticLibraryImpl: public __MCSLibraryImpl
{
public:
    __MCSStaticLibraryImpl(const MCSLibraryStaticInfo& p_handle)
        : m_handle(p_handle)
    {
    }
    
    ~__MCSStaticLibraryImpl(void)
    {
    }
    
    const void *Pointer(void) const
    {
        return static_cast<const void *>(&m_handle);
    }
    
    bool CopyNativePath(MCStringRef& r_path) const
    {
        return false;
    }
    
    void *LookupSymbol(MCStringRef p_symbol) const
    {
        return nullptr;
    }
    
    static bool CreateWithNativePath(MCStringRef p_native_path,
                                     MCSLibraryRef& r_library)
    {
        r_library = nullptr;
        return true;
    }
    
private:
    const MCSLibraryStaticInfo& m_handle;
};

/* ================================================================
 * HMODULE (Windows) binding
 * ================================================================ */

#if defined(__WINDOWS__)

#include <windows.h>

/* TODO: Make work with arbitrary length paths */
static bool Win32_GetModuleFileName(HMODULE p_module,
                                    MCStringRef& r_native_path)
{
    
    wchar_t t_native_path[MAX_PATH];
    
    // Make sure the last char in the buffer is NUL so that we can detect
    // failure on Windows XP.
    t_native_path[MAX_PATH - 1] = '\0'
    
    // On Windows XP, the returned path will be truncated if the buffer is
    // too small and a NUL byte *will not* be added.
    DWORD t_native_path_size =
    GetModuleFileName(m_handle,
                      t_native_path,
                      sizeof(t_native_path) / sizeof(t_native_path[0]));
    
    // The error case on Windows XP.
    if (t_native_path[MAX_PATH - 1] != '\0')
    {
        return false;
    }
    
    // Make sure the string is NUL terminated, we already know MAX_PATH-1
    // is NUL, on non-Windows XP char t_native_path_size will be NUL, on
    // WindowsXP t_native_path_size will be the length of the filename
    // without NUL.
    t_native_path[t_native_path_size] = '\0';
    
    return MCStringCreateWithWString(t_native_path,
                                     r_path);
}

#define __MCSLIBRARY_HAS_HMODULE_IMPL
class __MCSHMODULELibraryImpl: public __MCSLibraryImpl
{
public:
    __MCSHMODULELibraryImpl(HMODULE p_handle)
        : m_handle(p_handle)
    {
    }
    
    ~__MCSHMODULELibraryImpl(void)
    {
        FreeLibrary(m_handle);
    }
    
    const void *Pointer(void) const
    {
        return static_cast<void *>(m_handle);
    }
    
    bool CopyNativePath(MCStringRef& r_path) const
    {
        if (!Win32_GetModuleFileName(m_module,
                                     r_path))
        {
            /* TODO: Throw error */
            return false;
        }
    }
    
    void *LookupSymbol(MCStringRef p_symbol) const
    {
        MCStringRefAsCString t_cstring_symbol;
        if (!t_cstring_symbol.Lock(p_symbol))
        {
            /* MASKS_OOM */
            return nullptr;
        }
        
        return GetProcAddress(m_handle,
                              *t_cstring_symbol);
    }
    
    //////////
    
    static bool CreateWithNativePath(MCStringRef p_path,
                                     MCSLibraryRef& r_library)
    {
        MCAutoStringRefAsWString t_wstring_path;
        if (!t_wstring_path.Lock(p_path))
        {
            return false;
        }
        
        /* REVIEW: LOAD_WITH_ALTERED_SEARCH_PATH */
        HMODULE t_hmodule =
                LoadLibraryExW(*t_wstring_path,
                               NULL,
                               LOAD_WITH_ALTERED_SEARCH_PATH);
        
        if (t_hmodule == NULL)
        {
            return __MCSLibraryThrowCouldNotLoadLibrary(p_path,
                                                        GetLastError());
        }
        
        if (!__MCSLibraryCreateRef<__MCSHMODULELibraryImpl>(t_hmodule,
                                                            r_library))
        {
            FreeLibrary(t_hmodule);
            return false;
        }
        
        return true;
    }
    
    static bool CreateWithAddress(void *p_address,
                                  MCSLibraryRef& r_library)
    {
        r_library = nullptr;
        return true;
    }
    
private:
    HMODULE m_handle;
};

#endif

/* ================================================================
 * DlHandle (POSIX) binding
 * ================================================================ */

#if !defined(__WINDOWS__)

#include <dlfcn.h>

#if defined(__MAC__) || defined(__IOS__)

#include <mach-o/dyld.h>

static bool
__MCSDlHandleGetImageName(void *p_dl_handle,
                          MCStringRef& r_image_name_string)
{
    // Iterate through all images currently in memory.
    for(int32_t i = _dyld_image_count(); i >= 0; i--)
    {
        // Fetch the image name
        const char *t_image_name =
        _dyld_get_image_name(i);
        
        // Now dlopen the image with RTLD_NOLOAD so that we only get a handle
        // if it is already loaded and (critically) means it is unaffected by
        // RTLD_GLOBAL (the default mode).
        void *t_image_dl_handle =
        dlopen(t_image_name,
               RTLD_NOLOAD);
        
        // If the handle matches the one we are looking for, we are done.
        if (t_image_dl_handle == p_dl_handle)
        {
            return MCStringCreateWithSysString(t_image_name,
                                               r_image_name_string);
        }
        
        // If we got a handle, then we must release it.
        if (t_image_dl_handle != nullptr)
        {
            dlclose(t_image_dl_handle);
        }
    }
    
    return __MCSLibraryThrowResolvePathFailedError();
}

#else

#error implement __MCSDlHandleGetImageName

/* TODO: Make work with arbitrary length paths */

#endif

#define __MCSLIBRARY_HAS_DLHANDLE_IMPL
class __MCSDlHandleLibraryImpl: public __MCSLibraryImpl
{
public:
    __MCSDlHandleLibraryImpl(void *p_handle)
        : m_handle(p_handle)
    {
    }
    
    ~__MCSDlHandleLibraryImpl(void)
    {
        dlclose(m_handle);
    }
    
    const void *Pointer(void) const
    {
        return m_handle;
    }
    
    bool CopyNativePath(MCStringRef& r_path) const
    {
        return __MCSDlHandleGetImageName(m_handle,
                                         r_path);
    }
    
    void *LookupSymbol(MCStringRef p_symbol) const
    {
        MCAutoStringRefAsCString t_cstring_symbol;
        if (!t_cstring_symbol.Lock(p_symbol))
        {
            /* MASKS OOM */
            return nullptr;
        }
        
        void *t_address =
                dlsym(m_handle,
                      *t_cstring_symbol);
        
        return t_address;
    }
    
    static bool CreateWithNativePath(MCStringRef p_native_path,
                                     MCSLibraryRef& r_library)
    {
        MCAutoStringRefAsSysString t_sys_path;
        if (!t_sys_path.Lock(p_native_path))
        {
            return false;
        }
        
        void *t_dl_handle =
                dlopen(*t_sys_path,
                       RTLD_LAZY);
        if (t_dl_handle == nullptr)
        {
            /* TODO: Use dlerror */
            return __MCSLibraryThrowCreateWithNativePathFailedError(p_native_path);
        }
        
        if (!__MCSLibraryCreateRef<__MCSDlHandleLibraryImpl>(t_dl_handle,
                                                             r_library))
        {
            dlclose(t_dl_handle);
            return false;
        }
        
        return true;
    }
    
    static bool CreateWithAddress(void *p_address,
                                  MCSLibraryRef& r_library)
    {
        void *t_dl_handle = nullptr;
        Dl_info t_addr_info;
        if (dladdr(p_address,
                   &t_addr_info) != 0)
        {
            t_dl_handle = dlopen(t_addr_info.dli_fname,
                                 RTLD_LAZY);
        }
        
        if (t_dl_handle == nullptr)
        {
            /* TODO: Use dlerror */
            return __MCSLibraryThrowCreateWithAddressFailedError(p_address);
        }
        
        if (!__MCSLibraryCreateRef<__MCSDlHandleLibraryImpl>(t_dl_handle,
                                                             r_library))
        {
            dlclose(t_dl_handle);
            return false;
        }
        
        return true;
    }
    
    
private:
    void *m_handle;
};

#endif

/* ================================================================
 * CFBundleRef (Mac) binding
 * ================================================================ */

#if defined(__MAC__)

#define __MCSLIBRARY_HAS_CFBUNDLEREF_IMPL
class __MCSCFBundleRefLibraryImpl: public __MCSLibraryImpl
{
public:
    __MCSCFBundleRefLibraryImpl(CFBundleRef p_handle)
        : m_handle(p_handle)
    {
    }
    
    ~__MCSCFBundleRefLibraryImpl(void)
    {
        CFRelease(m_handle);
    }
    
    const void *Pointer(void) const
    {
        return static_cast<const void *>(m_handle);
    }
    
    bool CopyNativePath(MCStringRef& r_path) const
    {
        return false;
    }
    
    void *LookupSymbol(MCStringRef p_symbol) const
    {
        return nullptr;
    }
    
    static bool CreateWithNativePath(MCStringRef p_native_path,
                                     MCSLibraryRef& r_library)
    {
        r_library = nullptr;
        return true;
    }
    
private:
    CFBundleRef m_handle;
};
#endif

/* ================================================================
 * Public API
 * ================================================================ */

MC_DLLEXPORT_DEF bool
MCSLibraryCreateWithPath(MCStringRef p_path,
                         MCSLibraryRef& r_library)
{
    MCAutoStringRef t_native_path;
    if (!__MCSFilePathToNative(p_path,
                               &t_native_path))
    {
        return false;
    }
    
    MCSAutoLibraryRef t_new_library;
    
#if defined(__MCSLIBRARY_HAS_STATIC_IMPL)
    if (!t_new_library.IsSet() &&
        !__MCSStaticLibraryImpl::CreateWithNativePath(*t_native_path,
                                                      &t_new_library))
    {
        return false;
    }
#endif
    
#if defined(__MCSLIBRARY_HAS_HMODULE_IMPL)
    if (!t_new_library.IsSet() &&
        !__MCSHMODULELibraryImpl::CreateWithNativePath(*t_native_path,
                                                       &t_new_library))
    {
        return false;
    }
#endif
    
#if defined(__MCSLIBRARY_HAS_DLHANDLE_IMPL)
    if (!t_new_library.IsSet() &&
        !__MCSDlHandleLibraryImpl::CreateWithNativePath(*t_native_path,
                                                        &t_new_library))
    {
        return false;
    }
#endif
    
#if defined(__MCSLIBRARY_HAS_CFBUNDLEREF_IMPL)
    if (!t_new_library.IsSet() &&
        !__MCSCFBundleRefLibraryImpl::CreateWithNativePath(*t_native_path,
                                                           &t_new_library))
    {
        return false;
    }
#endif
    
    if (!t_new_library.IsSet())
    {
        return __MCSLibraryThrowNotFoundWithPathError(p_path);
    }
    
    r_library = t_new_library.Take();
    
    return true;
}

MC_DLLEXPORT_DEF bool
MCSLibraryCreateWithAddress(void *p_address,
                            MCSLibraryRef& r_library)
{
    MCSAutoLibraryRef t_new_library;

#if defined(__MCSLIBRARY_HAS_HMODULE_IMPL)
    if (!t_new_library.IsSet() &&
        !__MCSHMODULELibraryImpl::CreateWithAddress(p_address,
                                                    &t_new_library))
    {
        return false;
    }
#endif
    
#if defined(__MCSLIBRARY_HAS_DLHANDLE_IMPL)
    if (!t_new_library.IsSet() &&
        !__MCSDlHandleLibraryImpl::CreateWithAddress(p_address,
                                                     &t_new_library))
    {
        return false;
    }
#endif
    
    if (!t_new_library.IsSet())
    {
        return __MCSLibraryThrowNotFoundWithAddressError(p_address);
    }
    
    r_library = t_new_library.Take();
    
    return true;
}

MC_DLLEXPORT_DEF bool
MCSLibraryCopyPath(MCSLibraryRef p_library,
                   MCStringRef& r_path)
{
    __MCSLibraryImpl& t_impl =
            __MCSLibraryGetImpl(p_library);
    
    MCAutoStringRef t_native_path;
    if (!t_impl.CopyNativePath(&t_native_path))
    {
        return false;
    }
    
    return __MCSFilePathFromNative(*t_native_path,
                                   r_path);
}

MC_DLLEXPORT_DEF void *
MCSLibraryLookupSymbol(MCSLibraryRef p_library,
                       MCStringRef p_symbol)
{
    __MCSLibraryImpl& t_impl =
            __MCSLibraryGetImpl(p_library);
    
    return t_impl.LookupSymbol(p_symbol);
}
