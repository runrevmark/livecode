/* Copyright (C) 2003-2013 Runtime Revolution Ltd.

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

#ifndef __DIALECT__
#define __DIALECT__

typedef struct MCDialect *MCDialectRef;

void MCDialectCreate(MCDialectRef& r_dialect);
void MCDialectDestroy(MCDialectRef dialect);

bool MCDialectHasError(MCDialectRef dialect);
const char *MCDialectGetErrorString(MCDialectRef dialect);
uindex_t MCDialectGetErrorOffset(MCDialectRef dialect);

void MCDialectAddRule(MCDialectRef dialect, const char *syntax, uindex_t action_id);

typedef void (*MCDialectPrintCallback)(void *context, const char *format, ...);
void MCDialectPrint(MCDialectRef dialect, MCDialectPrintCallback callback, void *context);

#endif
