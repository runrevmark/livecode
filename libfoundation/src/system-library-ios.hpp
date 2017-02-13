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
 * iOS Simulator Handle Class
 * ================================================================ */

#if TARGET_IPHONE_SIMULATOR

#include "system-library-mac.hpp"

class __MCSLibraryHandleIOSSimulator: public __MCSLibraryHandleMac
{
};

typedef class __MCSLibraryHandleIOSSimulator __MCSLibraryHandle;

#endif

/* ================================================================
 * iOS Device Handle Class
 * ================================================================ */

#if TARGET_OS_IPHONE

#include "system-library-static.hpp"

class __MCSLibraryHandleIOSDevice: public __MCSLibraryHandleStatic
{
};

MC_DLLEXPORT_DEF void
MCSLibraryRegisterStatic(MCSLibraryStaticInfo& p_info)
{
    __MCSLibraryHandleIOSDevice::Register(p_info);
}

typedef class __MCSLibraryHandleIOSDevice __MCSLibraryHandle;

#endif
