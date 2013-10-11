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

#ifndef __MC_DIALECT_INTERNAL__
#define __MC_DIALECT_INTERNAL__

////////////////////////////////////////////////////////////////////////////////

typedef struct MCDialectState *MCDialectStateRef;

enum MCDialectStateType
{
	kMCDialectStateTypeNone,
	
	kMCDialectStateTypeEpsilon,
	kMCDialectStateTypeAlternation,
	kMCDialectStateTypeConcatenation,
	kMCDialectStateTypeRepetition,
	kMCDialectStateTypeRule,
	kMCDialectStateTypeToken,
	kMCDialectStateTypeMatch,
	kMCDialectStateTypeBreak,
};

bool MCDialectStateCreate(MCDialectStateType type, MCDialectStateRef& r_state);
bool MCDialectStateCreateEpsilon(MCDialectStateRef& r_state);
bool MCDialectStateCreateRule(uindex_t name, MCDialectStateRef& r_state);
bool MCDialectStateCreateToken(uindex_t token, bool is_marked, MCDialectStateRef& r_state);
bool MCDialectStateCreateMatch(uindex_t action, MCDialectStateRef& r_state);
bool MCDialectStateCreateBreak(MCDialectStateRef& r_state);

MCDialectStateRef MCDialectStateRetain(MCDialectStateRef state);
void MCDialectStateRelease(MCDialectStateRef state);

MCDialectStateType MCDialectStateGetType(MCDialectStateRef state);
uindex_t MCDialectStateGetArity(MCDialectStateRef state);

bool MCDialectStateIsAlternation(MCDialectStateRef state);
bool MCDialectStateIsConcatenation(MCDialectStateRef state);
bool MCDialectStateIsRepetition(MCDialectStateRef state);

bool MCDialectStateAppend(MCDialectStateRef state, MCDialectStateRef new_state);

void MCDialectStatePrint(MCDialectStateRef state, MCDialectPrintCallback p_callback, void *p_context);

////////////////////////////////////////////////////////////////////////////////

class MCAutoDialectStateRef
{
public:
	MCAutoDialectStateRef(void)
	{
		m_ref = nil;
	}
	
	~MCAutoDialectStateRef(void)
	{
		MCDialectStateRelease(m_ref);
	}
	
	MCDialectStateRef operator * (void)
	{
		return m_ref;
	}
	
	MCDialectStateRef operator -> (void)
	{
		return m_ref;
	}
	
	MCDialectStateRef& operator & (void)
	{
		assert(m_ref == nil);
		return m_ref;
	}
	
	void Give(MCDialectStateRef p_ref)
	{
		if (m_ref != nil)
			MCDialectStateRelease(m_ref);
		m_ref = p_ref;
	}
	
	MCDialectStateRef Take(void)
	{
		MCDialectStateRef t_ref;
		t_ref = m_ref;
		m_ref = nil;
		return t_ref;
	}
	
private:
	MCDialectStateRef m_ref;
};

////////////////////////////////////////////////////////////////////////////////

bool MCDialectCreate(MCDialectStateRef *rules, uindex_t rule_count, MCDialectRef& r_dialect);

////////////////////////////////////////////////////////////////////////////////

#endif
