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

struct MCDialectGrammarNamedEntity
{
	MCNameRef name;
	uindex_t index;
};

struct MCDialectGrammarLiteral
{
	uindex_t index;
	MCNameRef literal;
};

enum MCDialectGrammarConstantType
{
	kMCDialectGrammarConstantTypeNone,
	kMCDialectGrammarConstantTypeBoolean,
	kMCDialectGrammarConstantTypeInteger,
	kMCDialectGrammarConstantTypeReal,
	kMCDialectGrammarConstantTypeString,
};

struct MCDialectGrammarConstant
{
	uindex_t index;
	MCNameRef constant;
};
	
struct MCDialectGrammar
{
	bool has_error;
	
	MCDialectBuilderRef builder;

	MCDialectGrammarNamedEntity *named_constants;
	uindex_t named_constant_count;
	
	MCDialectGrammarNamedEntity *named_tokens;
	uindex_t named_token_count;
	
	//MCDialectGrammarNamedEntity *named_rules;
	//uindex_t named_rule_count;
	
	MCNameRef *rule_names;
	uindex_t rule_count;
	
	MCDialectGrammarLiteral *literals;
	uindex_t literal_count;
	
	MCDialectGrammarConstant *constants;
	uindex_t constant_count;
	
	bool has_parse_error;
	const char *pattern_ptr;
};

////////////////////////////////////////////////////////////////////////////////

static void MCDialectGrammarNewName(MCDialectGrammarRef self, const char *p_chars, uindex_t p_char_count, MCNameRef& r_name);

static void MCDialectGrammarEnsureRule(MCDialectGrammarRef self, MCNameRef rule_name, uindex_t& r_rule_index);
static void MCDialectGrammarEnsureLiteral(MCDialectGrammarRef self, MCNameRef literal_name, uindex_t& r_literal_index);
static bool MCDialectGrammarLookupToken(MCDialectGrammarRef self, MCNameRef p_token_name, uindex_t& r_token_index);

static void MCDialectGrammarParsePattern(MCDialectGrammarRef self);

////////////////////////////////////////////////////////////////////////////////

bool MCDialectGrammarBegin(MCDialectGrammarRef& r_grammar)
{
	MCDialectGrammarRef self;
	if (!MCMemoryNew(self))
		return false;
	
	MCDialectBuilderBegin(self -> builder);
	
	r_grammar = self;
	
	return true;
}

bool MCDialectGrammarEnd(MCDialectGrammarRef self, MCDialectRef& r_dialect)
{
	bool t_success;
	if (!MCDialectGrammarHasError(self))
	{
		MCDialectBuilderError t_error;
		MCDialectBuilderEnd(self -> builder, t_error, r_dialect);
		t_success = t_error == kMCDialectBuilderErrorNone;
	}
	else
	{
		MCDialectBuilderCancel(self -> builder);
		t_success = false;
	}
	
	MCMemoryDelete(self);
	
	return t_success;
}

bool MCDialectGrammarHasError(MCDialectGrammarRef self)
{
	return self -> has_error;
}

void MCDialectGrammarDefineConstant(MCDialectGrammarRef grammar, const char *constant, uindex_t constant_index)
{
}

void MCDialectGrammarDefineToken(MCDialectGrammarRef grammar, const char *token, uindex_t token_index)
{
}

void MCDialectGrammarDefinePhrase(MCDialectGrammarRef self, const char *p_rule, const char *p_pattern, uindex_t p_action_index)
{
	// Ensure we have a rule with the given name.
	MCNewAutoNameRef t_rule_name;
	uindex_t t_rule_index;
	MCDialectGrammarNewName(self, p_rule, strlen(p_rule), &t_rule_name);
	MCDialectGrammarEnsureRule(self, *t_rule_name, t_rule_index);
	
	// Begin building the phrase.
	MCDialectBuilderBeginPhrase(self -> builder, t_rule_index, p_action_index);
	
	// Now parse the phrase.
	self -> pattern_ptr = p_pattern;
	MCDialectGrammarParsePattern(self);
	
	// And end building the phrase.
	MCDialectBuilderEndPhrase(self -> builder);
	
	if (self -> has_parse_error)
	{
		self -> has_error = true;
		self -> has_parse_error = false;
	}
}

////////////////////////////////////////////////////////////////////////////////

static void MCDialectGrammarThrowOutOfMemory(MCDialectGrammarRef self)
{
	self -> has_error = true;
}

////////////////////////////////////////////////////////////////////////////////

static void MCDialectGrammarNewName(MCDialectGrammarRef self, const char *p_chars, uindex_t p_char_count, MCNameRef& r_name)
{
	if (MCNameCreateWithChars(p_chars, p_char_count, r_name))
		return;
	
	MCNameClone(kMCEmptyName, r_name);
	
	MCDialectGrammarThrowOutOfMemory(self);
}

static void MCDialectGrammarEnsureRule(MCDialectGrammarRef self, MCNameRef p_rule_name, uindex_t& r_rule_index)
{
	for(uindex_t i = 0; i < self -> rule_count; i++)
		if (MCNameIsEqualTo(p_rule_name, self -> rule_names[i], kMCCompareExact))
		{
			r_rule_index = i;
			return;
		}
	
	if (MCMemoryResizeArray(self -> rule_count + 1, self -> rule_names, self -> rule_count))
	{
		MCNameClone(p_rule_name, self -> rule_names[self -> rule_count - 1]);
		r_rule_index = self -> rule_count - 1;
		return;
	}
		
	r_rule_index = 0;

	MCDialectGrammarThrowOutOfMemory(self);
}

static void MCDialectGrammarEnsureLiteral(MCDialectGrammarRef self, MCNameRef p_rule_name, uindex_t& r_literal_index)
{
	r_literal_index = 0;
}

static bool MCDialectGrammarLookupToken(MCDialectGrammarRef self, MCNameRef p_token_name, uindex_t& r_token_index)
{
	r_token_index = 0;
	return false;
}

////////////////////////////////////////////////////////////////////////////////

static void MCDialectGrammarThrowParseError(MCDialectGrammarRef self)
{
	self -> has_parse_error = true;
}

static bool MCDialectGrammarHasParseError(MCDialectGrammarRef self)
{
	return self -> has_parse_error;
}

static void MCDialectGrammarSkipWhitespace(MCDialectGrammarRef self)
{
	while(self -> pattern_ptr[0] != '\0')
	{
		if (!isspace(self -> pattern_ptr[0]))
			break;
		self -> pattern_ptr += 1;
	}
}

static bool MCDialectGrammarSkipChar(MCDialectGrammarRef self, char p_char)
{
	MCDialectGrammarSkipWhitespace(self);
	
	if (self -> pattern_ptr[0] == p_char)
	{
		self -> pattern_ptr += 1;
		return true;
	}
	
	return false;
}

static void MCDialectGrammarParseChar(MCDialectGrammarRef self, char p_char)
{
	if (MCDialectGrammarSkipChar(self, p_char))
		return;
	
	MCDialectGrammarThrowParseError(self); // char expected error
}

static bool MCDialectGrammarSkipIdentifier(MCDialectGrammarRef self, MCNameRef& r_id)
{
	MCDialectGrammarSkipWhitespace(self);
	
	if (!isalpha(self -> pattern_ptr[0]))
		return false;

	const char *t_id_start;
	t_id_start = self -> pattern_ptr;
	
	while(self -> pattern_ptr[0] != '\0')
	{
		if (!isalpha(self -> pattern_ptr[0]))
			break;
		self -> pattern_ptr += 1;
	}
	
	MCDialectGrammarNewName(self, t_id_start, self -> pattern_ptr - t_id_start, r_id);
	
	return true;
}

static bool MCDialectGrammarSkipQuotedIdentifier(MCDialectGrammarRef self, MCNameRef& r_id)
{
	MCDialectGrammarSkipWhitespace(self);
	
	if (self -> pattern_ptr[0] != '\'')
		return false;
	
	self -> pattern_ptr += 1;
	
	const char *t_id_start;
	t_id_start = self -> pattern_ptr;
	
	while(self -> pattern_ptr[0] != '\0' && self -> pattern_ptr[0] != '\'')
		self -> pattern_ptr += 1;
	
	if (self -> pattern_ptr[0] != '\'')
	{
		MCDialectGrammarThrowParseError(self); // unterminated quoted id
		return false;
	}
	
	self -> pattern_ptr += 1;
	
	if (self -> pattern_ptr - t_id_start == 0)
	{
		MCDialectGrammarThrowParseError(self); // empty id
		return false;
	}
	
	MCDialectGrammarNewName(self, t_id_start, self -> pattern_ptr - t_id_start - 1, r_id);
	
	return true;
}

static bool MCDialectGrammarSkipAngledIdentifier(MCDialectGrammarRef self, MCNameRef& r_id)
{
	MCDialectGrammarSkipWhitespace(self);
	
	if (self -> pattern_ptr[0] != '<')
		return false;
	
	self -> pattern_ptr += 1;
	
	const char *t_id_start;
	t_id_start = self -> pattern_ptr;
	
	while(self -> pattern_ptr[0] != '\0')
	{
		if (!isalpha(self -> pattern_ptr[0]))
			break;
		self -> pattern_ptr += 1;
	}
	
	if (self -> pattern_ptr[0] != '>')
	{
		MCDialectGrammarThrowParseError(self); // right angled bracket expected
		return false;
	}
	
	self -> pattern_ptr += 1;
	
	if (self -> pattern_ptr - t_id_start == 0)
	{
		MCDialectGrammarThrowParseError(self); // empty id
		return false;
	}

	MCDialectGrammarNewName(self, t_id_start, self -> pattern_ptr - t_id_start - 1, r_id);
	
	return true;
}

static bool MCDialectGrammarWillMatchCharSet(MCDialectGrammarRef self, const char *p_set)
{
	char t_input;
	t_input = self -> pattern_ptr[0];
	if (t_input == '\0')
		t_input = '\004';
	return strchr(p_set, t_input) != nil;
}

static void MCDialectGrammarParseFactor(MCDialectGrammarRef self)
{
	MCNewAutoNameRef t_id;
	
	if (MCDialectGrammarSkipChar(self, '['))
	{
		MCDialectBuilderBeginAlternation(self -> builder);
		
		MCDialectGrammarParsePattern(self);
		
		MCDialectBuilderMatchEpsilon(self -> builder);
		
		MCDialectBuilderEndAlternation(self -> builder);
		
		MCDialectGrammarParseChar(self, ']');
		
	}
	else if (MCDialectGrammarSkipChar(self, '('))
	{
		MCDialectGrammarParsePattern(self);
		
		MCDialectGrammarParseChar(self, ')');
	}
	else if (MCDialectGrammarSkipChar(self, '{'))
	{
		MCDialectBuilderBeginRepetition(self -> builder);
		
		MCDialectGrammarParsePattern(self);
		if (MCDialectGrammarSkipChar(self, ','))
			MCDialectGrammarParsePattern(self);
		
		MCDialectBuilderEndRepetition(self -> builder);
		
		MCDialectGrammarParseChar(self, '}');
	}
	else if (MCDialectGrammarSkipChar(self, '@'))
	{
		// TODO CONSTANT
	}
	else if (MCDialectGrammarSkipChar(self, ';'))
	{
		MCDialectBuilderMatchBreak(self -> builder);
	}
	else if (MCDialectGrammarSkipIdentifier(self, &t_id))
	{
		uindex_t t_token_index;
		if (MCDialectGrammarLookupToken(self, *t_id, t_token_index))
			MCDialectBuilderMatchToken(self -> builder, t_token_index, true);
		else
		{
			uindex_t t_literal_index;
			MCDialectGrammarEnsureLiteral(self, *t_id, t_literal_index);
			MCDialectBuilderMatchToken(self -> builder, t_literal_index, false);
		}
	}
	else if (MCDialectGrammarSkipQuotedIdentifier(self, &t_id))
	{
		uindex_t t_literal_index;
		MCDialectGrammarEnsureLiteral(self, *t_id, t_literal_index);
		MCDialectBuilderMatchToken(self -> builder, t_literal_index, false);
	}
	else if (MCDialectGrammarSkipAngledIdentifier(self, &t_id))
	{
		uindex_t t_rule_index;
		MCDialectGrammarEnsureRule(self, *t_id, t_rule_index);
		MCDialectBuilderMatchRule(self -> builder, t_rule_index);
	}
	else if (!MCDialectGrammarWillMatchCharSet(self, ")}]|,\004"))
	{
		MCDialectGrammarThrowParseError(self);
	}
}

static void MCDialectGrammarParseConcatenation(MCDialectGrammarRef self)
{
	MCDialectBuilderBeginConcatenation(self -> builder);
	
	while(!MCDialectGrammarHasParseError(self))
	{
		MCDialectGrammarParseFactor(self);
		
		if (MCDialectGrammarWillMatchCharSet(self, ")}]|,\004"))
			break;
	}
	
	MCDialectBuilderEndConcatenation(self -> builder);
}

static void MCDialectGrammarParseAlternation(MCDialectGrammarRef self)
{
	MCDialectBuilderBeginAlternation(self -> builder);
	
	while(!MCDialectGrammarHasParseError(self))
	{
		MCDialectGrammarParseConcatenation(self);
		
		if (!MCDialectGrammarSkipChar(self, '|'))
			break;
	}
	
	MCDialectBuilderEndAlternation(self -> builder);
}

static void MCDialectGrammarParsePattern(MCDialectGrammarRef self)
{
	MCDialectGrammarParseAlternation(self);
}

////////////////////////////////////////////////////////////////////////////////
