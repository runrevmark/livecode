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

enum MCDialectTokenType
{
	kMCDialectTokenTypeNone,
	kMCDialectTokenTypeIdentifier,
	kMCDialectTokenTypeInteger,
	kMCDialectTokenTypeReal,
	kMCDialectTokenTypeString,
};

struct MCDialectParseCallbacks
{
	bool (*process_null)(void *ctxt, uindex_t line, uindex_t column, void*& r_value);
	bool (*process_token)(void *ctxt, uindex_t line, uindex_t column, MCDialectTokenType token_type, MCNameRef token, void*& r_value);
	bool (*process_action)(void *ctxt, uindex_t line, uindex_t column, void *action_id, void **values, uindex_t value_count, void*& r_value);
	bool (*process_error)(void *ctxt, uindex_t line, uindex_t column);
};

void MCDialectCreate(MCDialectRef& r_dialect);
void MCDialectDestroy(MCDialectRef dialect);

bool MCDialectHasError(MCDialectRef dialect);
const char *MCDialectGetErrorString(MCDialectRef dialect);
uindex_t MCDialectGetErrorOffset(MCDialectRef dialect);

void MCDialectDefine(MCDialectRef dialect, const char *syntax, void *action);
void MCDialectOptimize(MCDialectRef dialect);

bool MCDialectParse(MCDialectRef dialect, const char *script, const MCDialectParseCallbacks& callbacks, void *context, void*& r_result);

typedef void (*MCDialectPrintCallback)(void *context, const char *format, ...);
void MCDialectPrint(MCDialectRef dialect, MCDialectPrintCallback callback, void *context);

#endif
