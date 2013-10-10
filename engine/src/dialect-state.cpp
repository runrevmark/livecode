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

#include "dialect.h"
#include "dialect-internal.h"

////////////////////////////////////////////////////////////////////////////////

struct MCDialectState
{
	uint32_t references;
	MCDialectStateType type;
	union
	{
		struct
		{
			MCDialectStateRef *children;
			uindex_t child_count;
		} alternation, concatenation;
		struct
		{
			MCDialectStateRef pattern;
			MCDialectStateRef separator;
		} repetition;
		struct
		{
			uindex_t name;
		} rule;
		struct
		{
			uindex_t name;
			bool is_marked;
		} token;
		struct
		{
			uindex_t name;
		} match;
	};
};

////////////////////////////////////////////////////////////////////////////////

bool MCDialectStateCreate(MCDialectStateType p_type, MCDialectStateRef& r_state)
{
	MCDialectStateRef self;
	if (!MCMemoryNew(self))
		return false;
	
	self -> references = 1;
	self -> type = p_type;
	
	r_state = self;
	
	return true;
}

bool MCDialectStateCreateEpsilon(MCDialectStateRef& r_state)
{
	return MCDialectStateCreate(kMCDialectStateTypeEpsilon, r_state);
}

bool MCDialectStateCreateRule(uindex_t p_name, MCDialectStateRef& r_state)
{
	MCDialectStateRef self;
	if (!MCDialectStateCreate(kMCDialectStateTypeRule, self))
		return false;
	
	self -> rule . name = p_name;
	
	r_state = self;
	
	return true;
}

bool MCDialectStateCreateToken(uindex_t p_name, bool p_is_marked, MCDialectStateRef& r_state)
{
	MCDialectStateRef self;
	if (!MCDialectStateCreate(kMCDialectStateTypeToken, self))
		return false;
	
	self -> token . name = p_name;
	self -> token . is_marked = p_is_marked;
	
	r_state = self;
	
	return true;
}

bool MCDialectStateCreateMatch(uindex_t p_name, MCDialectStateRef& r_state)
{
	MCDialectStateRef self;
	if (!MCDialectStateCreate(kMCDialectStateTypeMatch, self))
		return false;
	
	self -> match . name = p_name;
	
	r_state = self;
	
	return true;
}

////////////////////////////////////////////////////////////////////////////////

MCDialectStateRef MCDialectStateRetain(MCDialectStateRef self)
{
	if (self == nil)
		return nil;
	
	self -> references += 1;
	
	return self;
}

void MCDialectStateRelease(MCDialectStateRef self)
{
	if (self == nil)
		return;
	
	self -> references -= 1;
	if (self -> references != 0)
		return;
	
	switch(self -> type)
	{
		case kMCDialectStateTypeAlternation:
		case kMCDialectStateTypeConcatenation:
			for(uindex_t i = 0; i < self -> alternation . child_count; i++)
				MCDialectStateRelease(self -> alternation . children[i]);
			MCMemoryDeleteArray(self -> alternation . children);
			break;
			
		case kMCDialectStateTypeRepetition:
			MCDialectStateRelease(self -> repetition . pattern);
			MCDialectStateRelease(self -> repetition . separator);
			break;

		default:
			break;
	}
	
	MCMemoryDelete(self);
}

////////////////////////////////////////////////////////////////////////////////

MCDialectStateType MCDialectStateGetType(MCDialectStateRef self)
{
	return self -> type;
}

uindex_t MCDialectStateGetArity(MCDialectStateRef self)
{
	switch(self -> type)
	{
		case kMCDialectStateTypeAlternation:
		case kMCDialectStateTypeConcatenation:
			return self -> alternation . child_count;
			
		case kMCDialectStateTypeRepetition:
			return (self -> repetition . pattern != nil ? 1 : 0) +
					(self -> repetition . separator != nil ? 1 : 0);
			
		default:
			break;
	}
	
	return 0;
}

bool MCDialectStateIsAlternation(MCDialectStateRef self)
{
	return self -> type == kMCDialectStateTypeAlternation;
}

bool MCDialectStateIsConcatenation(MCDialectStateRef self)
{
	return self -> type == kMCDialectStateTypeConcatenation;
}

bool MCDialectStateIsRepetition(MCDialectStateRef self)
{
	return self -> type == kMCDialectStateTypeRepetition;
}

////////////////////////////////////////////////////////////////////////////////

bool MCDialectStateAppend(MCDialectStateRef self, MCDialectStateRef p_new_state)
{
	switch(self -> type)
	{
		case kMCDialectStateTypeAlternation:
		case kMCDialectStateTypeConcatenation:
			if (!MCMemoryResizeArray(self -> alternation . child_count + 1, self -> alternation . children, self -> alternation . child_count))
				return false;
			self -> alternation . children[self -> alternation . child_count - 1] = MCDialectStateRetain(p_new_state);
			break;
			
		case kMCDialectStateTypeRepetition:
			if (self -> repetition . pattern == nil)
				self -> repetition . pattern = MCDialectStateRetain(p_new_state);
			else if (self -> repetition . separator == nil)
				self -> repetition . separator = MCDialectStateRetain(p_new_state);
			else
				assert(false);
			break;
			
		default:
			assert(false);
			break;
	}
	
	return true;
}

////////////////////////////////////////////////////////////////////////////////
