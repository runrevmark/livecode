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

#include "prefix.h"

#include "core.h"
#include "globdefs.h"
#include "objdefs.h"
#include "parsedef.h"
#include "filedefs.h"

#include "execpt.h"
#include "express.h"
#include "statemnt.h"
#include "internal.h"
#include "scriptpt.h"
#include "util.h"
#include "globals.h"

#include "dialect.h"

////////////////////////////////////////////////////////////////////////////////

static MCDialectRef s_dialect = nil;

static void trim_mcstring(MCString& str)
{
	const char *s;
	uint4 l;
	str.get(s, l);
	
	while(l > 0 && isspace(*s))
		s++, l--;
	
	while(l > 0 && isspace(s[l - 1]))
		l--;
	
	str.set(s, l);
}

class MCInternalDialectConfigure: public MCStatement
{
public:
	MCInternalDialectConfigure(void)
	{
		m_syntax = nil;
	}
	
	~MCInternalDialectConfigure(void)
	{
		delete m_syntax;
	}
	
	Parse_stat parse(MCScriptPoint& sp)
	{
		initpoint(sp);
		if (sp . parseexp(False, True, &m_syntax) != PS_NORMAL)
			return PS_ERROR;
		return PS_NORMAL;
	}
	
	Exec_stat exec(MCExecPoint& ep)
	{
		if (m_syntax -> eval(ep) != ES_NORMAL)
			return ES_ERROR;
		
		MCAutoPointer<MCString> t_lines;
		uint2 t_line_count;
		t_lines = nil;
		t_line_count = 0;
		MCU_break_string(ep . getsvalue(), &t_lines, t_line_count, False);
		
		MCDialectGrammarRef t_grammar;
		if (!MCDialectGrammarBegin(t_grammar))
			return ES_ERROR;
		
		uindex_t t_line_index;
		for(t_line_index = 0; t_line_index < t_line_count; t_line_index++)
		{
			MCString t_rule_name, t_rule_pattern;
			(*t_lines)[t_line_index] . split(':', t_rule_name, t_rule_pattern);
			
			trim_mcstring(t_rule_name);
			trim_mcstring(t_rule_pattern);
			
			MCAutoPointer<char> t_rule_name_cstring, t_rule_pattern_cstring;
			t_rule_name_cstring = t_rule_name . clone();
			t_rule_pattern_cstring = t_rule_pattern . clone();
			
			MCDialectGrammarDefinePhrase(t_grammar, *t_rule_name_cstring, *t_rule_pattern_cstring, t_line_index);
		}
		
		MCDialectRef t_dialect;
		if (MCDialectGrammarEnd(t_grammar, t_dialect))
		{
			if (s_dialect != nil)
				MCDialectRelease(s_dialect);
			s_dialect = t_dialect;
			
			MCresult -> clear();
		}
		else
		{
			MCresult -> sets("oh dear");
		}
		
		return ES_NORMAL;
	}
	
private:
	MCExpression *m_syntax;
};

static void ep_print_callback(void *ctxt, const char *p_format, ...)
{
	va_list t_args;
	va_start(t_args, p_format);
	((MCExecPoint *)ctxt) -> appendstringfv(p_format, t_args);
	va_end(t_args);
}

class MCInternalDialectPrint: public MCStatement
{
public:
	MCInternalDialectPrint(void)
	{
	}
	
	~MCInternalDialectPrint(void)
	{
	}
	
	Parse_stat parse(MCScriptPoint& sp)
	{
		getit(sp, m_it);
		return PS_NORMAL;
	}
	
	Exec_stat exec(MCExecPoint& ep)
	{
		m_it -> clear();
		ep . clear();
		
		if (s_dialect != nil)
		{
			MCDialectPrint(s_dialect, ep_print_callback, &ep);
			m_it -> set(ep);
		}
		
		return ES_NORMAL;
	}
	
private:
	MCVarref *m_it;
};

#if 0
static bool dialect_parse_process_null(void *p_context, uindex_t p_line, uindex_t p_column, void*& r_value)
{
	return false;
}

static bool dialect_parse_process_token(void *p_context, uindex_t p_line, uindex_t p_column, MCDialectTokenType p_token_type, MCNameRef p_token, void*& r_value)
{
	return false;
}

static bool dialect_parse_process_action(void *p_context, uindex_t p_line, uindex_t p_column, void *p_action, void **p_values, uindex_t p_value_count, void*& r_value)
{
	return false;
}

static bool dialect_parse_process_error(void *p_context, uindex_t p_line, uindex_t p_column)
{
	return false;
}

class MCInternalDialectParse: public MCStatement
{
public:
	MCInternalDialectParse(void)
	{
		m_script = nil;
	}
	
	~MCInternalDialectParse(void)
	{
		delete m_script;
	}
	
	Parse_stat parse(MCScriptPoint& sp)
	{
		initpoint(sp);
		if (sp . parseexp(False, True, &m_script) != PS_NORMAL)
			return PS_ERROR;
		return PS_NORMAL;
	}
	
	Exec_stat exec(MCExecPoint& ep)
	{
		if (m_script -> eval(ep) != ES_NORMAL)
			return ES_ERROR;
		
		if (s_dialect == nil)
		{
			MCresult -> sets("no dialect");
			return ES_NORMAL;
		}
		
		MCAutoPointer<char> t_script;
		t_script = ep . getsvalue() . clone();
		
		MCDialectParseCallbacks t_callbacks =
		{
			dialect_parse_process_null,
			dialect_parse_process_token,
			dialect_parse_process_action,
			dialect_parse_process_error
		};
		
		void *t_result;
		MCDialectParse(s_dialect, *t_script, t_callbacks, &ep, t_result);
		
		return ES_NORMAL;
	}
	
private:
	MCExpression *m_script;
};

#endif

////////////////////////////////////////////////////////////////////////////////

// Small helper template
template<class T> inline MCStatement *class_factory(void) { return new T; }

// The internal verb table used by the '_internal' command
MCInternalVerbInfo MCinternalverbs[] =
{
	{ "dialect", "configure", class_factory<MCInternalDialectConfigure> },
	{ "dialect", "print", class_factory<MCInternalDialectPrint> },
	//{ "dialect", "parse", class_factory<MCInternalDialectParse> },
	{ nil, nil, nil },
};

////////////////////////////////////////////////////////////////////////////////
