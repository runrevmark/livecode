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

/* ================================================================
 * Static Handle Class
 * ================================================================ */

class __MCSLibraryHandleStatic
{
public:
    __MCSLibraryHandleStatic(void)
        : m_handle(nullptr)
    {
    }
    
    ~__MCSLibraryHandleStatic(void)
    {
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
        return MCHashPointer(m_handle);
    }
    
    bool CreateWithNativePath(MCStringRef p_native_path)
    {
        for(MCSLibraryStaticInfo *t_info = s_chain;
            t_info != nullptr;
            t_info = t_info->__next)
        {
            if (MCStringIsEqualToCString(p_native_path,
                                         t_info->name,
                                         kMCStringOptionsCompareCaseless))
            {
                m_handle = t_info;
                return true;
            }
        }
        
        return __MCSLibraryThrowCreateWithNativePathFailed(p_native_path);
    }
    
    bool CreateWithAddress(void *p_address)
    {
        return __MCSLibraryThrowCreateWithAddressFailed(p_address);
    }
    
    bool CopyNativePath(MCStringRef& r_native_path) const
    {
        return MCStringCreateWithCString(m_handle->name,
                                         r_native_path);
    }
    
    void *LookupSymbol(MCStringRef p_symbol) const
    {
        for(size_t i = 0; m_handle->exports[i].symbol != nullptr; i++)
        {
            if (MCStringIsEqualToCString(p_symbol,
                                         m_handle->exports[i].symbol,
                                         kMCStringOptionsCompareExact))
            {
                return m_handle->exports[i].address;
            }
        }
        
        return nullptr;
    }
    
    static void Register(MCSLibraryStaticInfo& p_info)
    {
        // If the __next link is already set, then the library has already been
        // registered.
        if (p_info.__next != nullptr)
        {
            return;
        }
        
        p_info.__next = s_chain;
        s_chain = &p_info;
    }
    
protected:
    static MCSLibraryStaticInfo *s_chain = nullptr;
    const MCSLibraryStaticInfo *m_handle;
};
