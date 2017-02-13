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

#include <windows.h>

/* ================================================================
 * Posix Handle Class
 * ================================================================ */

class __MCSLibraryHandleWin32
{
public:
    __MCSLibraryHandleWin32(void)
        : m_handle(nullptr)
    {
    }
    
    ~__MCSLibraryHandleWin32(void)
    {
        if (m_handle == nullptr)
            return;
        FreeLibrary(m_handle);
    }
    
    bool IsDefined(void) const
    {
        return m_handle != nullptr;
    }
    
    bool IsEqualTo(const __MCSLibraryHandlePosix& p_other) const
    {
        return m_handle == p_other.m_handle;
    }
    
    bool Hash(void) const
    {
        return MCHashPointer(reinterpret_cast<void *>(m_handle));
    }
    
    bool CreateWithNativePath(MCStringRef p_native_path)
    {
        MCAutoStringRefAsWString t_wstring_path;
        if (!t_wstring_path.Lock(p_path))
        {
            return false;
        }
        
        HMODULE t_hmodule =
                LoadLibraryExW(*t_wstring_path,
                               NULL,
                               LOAD_WITH_ALTERED_SEARCH_PATH);
        
        if (t_hmodule == NULL)
        {
            /* TODO: Use GetLastError() */
            return __MCSLibraryThrowCreteWithNativePathFailed(p_native_path);
        }
        
        m_handle = t_hmodule;
        
        return true;
    }
    
    bool CreateWithAddress(void *p_address)
    {
        HMODULE t_handle = nullptr;
        if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                               (LPCTSTR)p_address,
                               &t_handle))
        {
            /* TODO: Use GetLastError() */
            return __MCSLibraryThrowCreateWithAddressFailed(p_address);
        }
        
        m_handle = t_handle;
        
        return true;
    }
    
    bool CopyNativePath(MCStringRef& r_native_path) const
    {
        for(unsigned int i = 4; i < 1024; i++)
        {
            size_t t_native_path_capacity = MAX_PATH * i;
            wchar_t t_native_path[MAX_PATH * i + 1];
            
            // Make sure the last char in the buffer is NUL so that we can detect
            // failure on Windows XP.
            t_native_path[t_native_path_capacity - 1] = '\0'
            
            // On Windows XP, the returned path will be truncated if the buffer is
            // too small and a NUL byte *will not* be added.
            DWORD t_native_path_size =
                    GetModuleFileName(m_handle,
                                      t_native_path,
                              t_native_path_capacity);
            
            DWORD t_error_code =
                    GetLastError();
            
            // A too small buffer is indicated by a 0 return value and:
            //   on XP: the buffer being filled without terminating NUL
            //   others: GetLastError() returning ERROR_INSUFFICIENT_BUFFER
            if (t_native_path_size == 0)
            {
                if (t_error_code == ERROR_SUCCESS && t_native_path[t_native_path_capacity - 1] != '\0') ||
                    t_error_code == ERROR_INSUFFICIENT_BUFFER)
                {
                    continue;
                }
            }
            
            // If we get an error other than insufficient buffer, then we return
            // it.
            if (t_error_code != ERROR_SUCCESS)
            {
                /* TODO: Use last error */
                return __MCSLibraryThrowResolvePathFailed();
            }
            
            // Make sure the string is NUL terminated, we already know MAX_PATH-1
            // is NUL, on non-Windows XP char t_native_path_size will be NUL, on
            // WindowsXP t_native_path_size will be the length of the filename
            // without NUL.
            t_native_path[t_native_path_size] = '\0';
            
            // We have success.
            return MCStringCreateWithWString(t_native_path,
                                             r_path);
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
    
protected:
    HMODULE m_handle;
};

typedef class __MCSLibraryHandleWin32 __MCSLibraryHandle;
