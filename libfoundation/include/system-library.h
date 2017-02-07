/*
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

#if !defined(__MCS_SYSTEM_H_INSIDE__)
#	error "Only <foundation-system.h> can be included directly"
#endif

/* ================================================================
 * Loadable library handling
 * ================================================================ */

/* The MCSLibrary library provides an API for handling loadable libraries in a
 * standardized fashion.
 *
 * It unifies the various ways loadable libraries can be accessed on all the
 * different platforms we support. In addition, mainly to support iOS, it
 * provides a mechanism for loadable modules to be statically linked to the main
 * executable by registration at startup. */

/* ================================================================
 * Types
 * ================================================================ */

/* An opaque custom value ref type representing a loaded library. */
typedef struct __MCSLibrary *MCSLibraryRef;

/* The binding to the internal MCTypeInfoRef for the MCSLibraryRef type */
MC_DLLEXPORT MCTypeInfoRef
MCSLibraryTypeInfo(void) ATTRIBUTE_PURE;

/* ================================================================
 * Construction and querying
 * ================================================================ */

/* Create an MCSLibraryRef object by loading the library from the specified
 * path. The path is expected to be in native path format. */
MC_DLLEXPORT bool
MCSLibraryCreateWithPath(MCStringRef p_path,
                         MCSLibraryRef& r_library);

/* Create an MCSLibraryRef object by referencing the library currently loaded
 * at the specified address. */
MC_DLLEXPORT bool
MCSLibraryCreateWithAddress(void *p_address,
                            MCSLibraryRef& r_library);

/* Copy the full native path to the specified library. If the library is of
 * static type, this returns the path of the loadable object which the library
 * is linked into. */
MC_DLLEXPORT bool
MCSLibraryCopyPath(MCSLibraryRef p_library,
                   MCStringRef& r_path);

/* Lookup the symbol in the specified library. */
MC_DLLEXPORT void *
MCSLibraryLookupSymbol(MCSLibraryRef p_library,
                       MCStringRef p_symbol);

/* ================================================================
 * Static binding
 * ================================================================ */

/* The record which each statically linked library should register using a
 * constructor. */
struct MCSLibraryStaticInfo
{
    MCSLibraryStaticInfo *__next;
    const char *name;
    struct {
        const char *symbol;
        void *address;
    } *exports;
};

MC_DLLEXPORT void
MCSLibraryRegisterStatic(const MCSLibraryStaticInfo& p_info);

#ifdef __MCS_INTERNAL_API__
bool
__MCSLibraryInitialize(void);
void
__MCSLibraryFinalize(void);
#endif

/* ================================================================
 * C++ API
 * ================================================================ */

#ifdef __cplusplus

typedef MCAutoValueRefBase<MCSLibraryRef> MCSAutoLibraryRef;

#endif
