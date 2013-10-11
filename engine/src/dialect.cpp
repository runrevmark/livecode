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

struct MCDialect
{
	uint32_t references;
	
	MCDialectStateRef *rules;
	uindex_t rule_count;
};

////////////////////////////////////////////////////////////////////////////////

static void MCDialectDestroy(MCDialectRef self)
{
	if (self == nil)
		return;
	
	for(uindex_t i = 0; i < self -> rule_count; i++)
		MCDialectStateRelease(self -> rules[i]);
	
	MCMemoryDeleteArray(self -> rules);
	MCMemoryDelete(self);
}

////////////////////////////////////////////////////////////////////////////////

bool MCDialectCreate(MCDialectStateRef *p_rules, uindex_t p_rule_count, MCDialectRef& r_dialect)
{
	bool t_success;
	t_success = true;
	
	MCDialectRef self;
	if (t_success)
		t_success = MCMemoryNew(self);
	
	if (t_success)
		t_success = MCMemoryNewArray(p_rule_count, self -> rules);
	
	if (t_success)
	{
		for(uindex_t i = 0; i < p_rule_count; i++)
			self -> rules[self -> rule_count++] = MCDialectStateRetain(p_rules[i]);
		
		self -> references = 1;
		
		r_dialect = self;
	}
	else
		MCDialectDestroy(self);
	
	return t_success;
}

MCDialectRef MCDialectRetain(MCDialectRef self)
{
	self -> references += 1;
	return self;
}

void MCDialectRelease(MCDialectRef self)
{
	self -> references -= 1;
	if (self -> references != 0)
		return;
	
	MCDialectDestroy(self);
}

void MCDialectPrint(MCDialectRef self, MCDialectPrintCallback p_callback, void *p_context)
{
	for(uindex_t i = 0; i < self -> rule_count; i++)
	{
		p_callback(p_context, "%d\n\t: ", i);
		MCDialectStatePrint(self -> rules[i], p_callback, p_context);
		p_callback(p_context, "\n");
	}
}

////////////////////////////////////////////////////////////////////////////////

#if 0
typedef struct MCDialectState *MCDialectStateRef;

static bool MCDialectStateCreateEpsilon(MCDialectStateRef& r_state);
static bool MCDialectStateCreateSeparator(MCDialectStateRef& r_state);
static bool MCDialectStateCreateAlternation(MCDialectStateRef left, MCDialectStateRef right, MCDialectStateRef& r_state);
static bool MCDialectStateCreateConcatenation(MCDialectStateRef left, MCDialectStateRef right, MCDialectStateRef& r_state);
static bool MCDialectStateCreateRepetition(MCDialectStateRef pattern, MCDialectStateRef separator, MCDialectStateRef& r_state);
static bool MCDialectStateCreateReference(MCNameRef name, MCDialectStateRef& r_state);
static bool MCDialectStateCreateIdentifier(MCNameRef name, bool is_marked, MCDialectStateRef& r_state);
static bool MCDialectStateCreateMatch(void *p_action, MCDialectStateRef& r_state);

static bool MCDialectStateFlatten(MCDialectStateRef self);

static void MCDialectStateRelease(MCDialectStateRef self);
static MCDialectStateRef MCDialectStateRetain(MCDialectStateRef self);

static void MCDialectStatePrint(MCDialectStateRef self, MCDialectPrintCallback callback, void *context);

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

enum MCDialectSyntaxError
{
	kMCDialectSyntaxErrorNone,
	
	kMCDialectSyntaxErrorOutOfMemory,
	
	kMCDialectSyntaxErrorRightBracketExpected,
	kMCDialectSyntaxErrorRightParanthesisExpected,
	kMCDialectSyntaxErrorRightBraceExpected,
	kMCDialectSyntaxErrorColonExpected,
	kMCDialectSyntaxErrorEndExpected,
	kMCDialectSyntaxErrorNormalIdentifierExpected,
	kMCDialectSyntaxErrorRightAngledBracketExpected,
	kMCDialectSyntaxErrorUnterminatedIdentifier,
};

static const char *s_dialect_syntax_error_strings[] =
{
	"",
	
	"out of memory",
	
	"right bracket expected",
	"right paranthesis expected",
	"right brace expected",
	"colon expected",
	"end of rule expected",
	"identifier expected",
	"right angled bracket expected",
	"unterminated identifier",
};

static bool MCDialectSyntaxParse(const char*& x_syntax, void *action, MCNameRef& r_scope, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error);

////////////////////////////////////////////////////////////////////////////////

struct MCDialectRule
{
	MCNameRef name;
	MCDialectStateRef pattern;
};

struct MCDialect
{
	bool has_error;
	char *error_string;
	uindex_t error_offset;
	
	MCDialectRule *rules;
	uindex_t rule_count;
};

void MCDialectCreate(MCDialectRef& r_dialect)
{
	if (!MCMemoryNew(r_dialect))
		r_dialect = nil;
}

void MCDialectDestroy(MCDialectRef self)
{
	if (self == nil)
		return;
	
	MCCStringFree(self -> error_string);
	MCMemoryDelete(self);
}

bool MCDialectHasError(MCDialectRef self)
{
	return self == nil || self -> has_error;
}

const char *MCDialectGetErrorString(MCDialectRef self)
{
	return self -> error_string;
}

uindex_t MCDialectGetErrorOffset(MCDialectRef self)
{
	return self -> error_offset;
}

void MCDialectDefine(MCDialectRef self, const char *p_syntax, void *p_action)
{
	MCDialectSyntaxError t_error;
	MCNewAutoNameRef t_rule_name;
	MCAutoDialectStateRef t_pattern;
	const char *t_syntax_ptr;
	t_syntax_ptr = p_syntax;
	if (!MCDialectSyntaxParse(t_syntax_ptr, p_action, &t_rule_name, &t_pattern, t_error))
	{
		MCCStringClone(s_dialect_syntax_error_strings[t_error], self -> error_string);
		self -> error_offset = t_syntax_ptr - p_syntax;
		self -> has_error = true;
		return;
	}
	
	// Search for a suitable scope.
	MCDialectRule *t_rule;
	t_rule = nil;
	for(uindex_t i = 0; i < self -> rule_count; i++)
		if (MCNameIsEqualTo(*t_rule_name, self -> rules[i] . name, kMCCompareExact))
		{
			t_rule = &self -> rules[i];
			break;
		}
	
	// If there was no rule with the given name, create one.
	if (t_rule == nil)
	{
		/* UNCHECKED */ MCMemoryResizeArray(self -> rule_count + 1, self -> rules, self -> rule_count);
		
		t_rule = &self -> rules[self -> rule_count - 1];
		MCNameClone(*t_rule_name, t_rule -> name);
	}
	
	// Add the pattern to the rule.
	if (t_rule -> pattern == nil)
		t_rule -> pattern = t_pattern . Take();
	else
		/* UNCHECKED */ MCDialectStateCreateAlternation(t_rule -> pattern, *t_pattern, t_rule -> pattern);
}

void MCDialectOptimize(MCDialectRef self)
{
	for(uindex_t i = 0; i < self -> rule_count; i++)
		 if (!MCDialectStateFlatten(self -> rules[i] . pattern))
			return;
}

#if 0
struct MCDialectParseInfo
{
	const MCDialectParseCallbacks *callbacks;
	void *context;
	
	void *actions;
	uindex_t action_count;
}

struct MCDialectParsePoint
{
	const char *script;
	uindex_t line;
	uindex_t column;
	uindex_t action_index;
};

static bool MCDialectParseScope(MCDialectRef self, const MCDialectParseInfo& p_info, const MCDialectParsePoint& p_point, uindex_t p_scope_index)
{
	MCDialectScope *t_scope;
	t_scope = &self -> scopes[p_scope_index];

	for(uindex_t i = 0; i < t_scope -> rule_count; i++)
	{
		MCDialectParsePoint t_new_point;
		t_new_point = p_point;
		
		MCDialectParseRule(self, p_info, t_new_point, t_scope -> rules[i]);
	}
}
#endif

bool MCDialectParse(MCDialectRef self, const char *p_script, const MCDialectParseCallbacks& p_callbacks, void *p_context, void*& r_result)
{
#if 0
	MCDialectParseInfo t_info;
	t_info . callbacks = p_callbacks;
	t_info . context = p_context;
	
	MCDialectParsePoint t_point;
	t_point . script = p_script;
	t_point . line = 1;
	t_point . column = 1;
	t_point . action_index = 0;
	return MCDialectParseScope(self, t_info, t_point, 0, r_result);
#endif
}
						   
void MCDialectPrint(MCDialectRef self, MCDialectPrintCallback p_callback, void *p_context)
{
	for(uindex_t i = 0; i < self -> rule_count; i++)
	{
		p_callback(p_context, "%s\n\t: ", MCNameGetCString(self -> rules[i] . name));
		MCDialectStatePrint(self -> rules[i] . pattern, p_callback, p_context);
		p_callback(p_context, "\n");
	}
}

////////////////////////////////////////////////////////////////////////////////

enum MCDialectStateType
{
	kMCDialectStateTypeNone,
	
	kMCDialectStateTypeEpsilon,
	kMCDialectStateTypeSeparator,
	kMCDialectStateTypeAlternation,
	kMCDialectStateTypeConcatenation,
	kMCDialectStateTypeRepetition,
	kMCDialectStateTypeReference,
	kMCDialectStateTypeIdentifier,
	kMCDialectStateTypeMatch
};

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
			MCNameRef name;
		} reference;
		struct
		{
			MCNameRef name;
			bool is_marked;
		} identifier;
		struct
		{
			void *action;
		} match;
	};
};

static bool MCDialectStateCreate(MCDialectStateType p_type, MCDialectStateRef& r_state)
{
	MCDialectStateRef self;
	if (!MCMemoryNew(self))
		return false;
	
	self -> references = 1;
	self -> type = p_type;
	
	r_state = self;
	
	return true;
}

static bool MCDialectStateCreateBinary(MCDialectStateType p_type, MCDialectStateRef p_left, MCDialectStateRef p_right, MCDialectStateRef& r_state)
{
	MCAutoDialectStateRef t_state;
	if (!MCDialectStateCreate(p_type, &t_state))
		return false;
	
	if (!MCMemoryResizeArray(2, t_state -> alternation . children, t_state -> alternation . child_count))
		return false;
	
	t_state -> alternation . children[0] = MCDialectStateRetain(p_left);
	t_state -> alternation . children[1] = MCDialectStateRetain(p_right);
	
	r_state = t_state . Take();
	
	return true;
}

static void MCDialectStateRelease(MCDialectStateRef self)
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
			
		case kMCDialectStateTypeReference:
			MCNameDelete(self -> reference . name);
			break;
			
		case kMCDialectStateTypeIdentifier:
			MCNameDelete(self -> identifier . name);
			break;
			
		default:
			break;
	}
	
	MCMemoryDelete(self);
}

static MCDialectStateRef MCDialectStateRetain(MCDialectStateRef self)
{
	if (self == nil)
		return nil;
	
	self -> references += 1;
	
	return self;
}

static bool MCDialectStateCreateEpsilon(MCDialectStateRef& r_state)
{
	return MCDialectStateCreate(kMCDialectStateTypeEpsilon, r_state);
}

static bool MCDialectStateCreateSeparator(MCDialectStateRef& r_state)
{
	return MCDialectStateCreate(kMCDialectStateTypeSeparator, r_state);
}

static bool MCDialectStateCreateAlternation(MCDialectStateRef p_left, MCDialectStateRef p_right, MCDialectStateRef& r_state)
{
	return MCDialectStateCreateBinary(kMCDialectStateTypeAlternation, p_left, p_right, r_state);
}

static bool MCDialectStateCreateConcatenation(MCDialectStateRef p_left, MCDialectStateRef p_right, MCDialectStateRef& r_state)
{
	return MCDialectStateCreateBinary(kMCDialectStateTypeConcatenation, p_left, p_right, r_state);
}

static bool MCDialectStateCreateRepetition(MCDialectStateRef p_pattern, MCDialectStateRef p_separator, MCDialectStateRef& r_state)
{
	MCDialectStateRef self;
	if (!MCDialectStateCreate(kMCDialectStateTypeRepetition, self))
		return false;
	
	self -> repetition . pattern = MCDialectStateRetain(p_pattern);
	self -> repetition . separator = MCDialectStateRetain(p_separator);
	
	r_state = self;
	
	return true;
}

static bool MCDialectStateCreateReference(MCNameRef p_name, MCDialectStateRef& r_state)
{
	MCDialectStateRef self;
	if (!MCDialectStateCreate(kMCDialectStateTypeReference, self))
		return false;
	
	MCNameClone(p_name, self -> reference . name);
	
	r_state = self;
	
	return true;
}

static bool MCDialectStateCreateIdentifier(MCNameRef p_name, bool p_is_marked, MCDialectStateRef& r_state)
{
	MCDialectStateRef self;
	if (!MCDialectStateCreate(kMCDialectStateTypeIdentifier, self))
		return false;
	
	MCNameClone(p_name, self -> identifier . name);
	self -> identifier . is_marked = p_is_marked;
	
	r_state = self;
	
	return true;
}

static bool MCDialectStateCreateMatch(void *p_action, MCDialectStateRef& r_state)
{
	MCDialectStateRef self;
	if (!MCDialectStateCreate(kMCDialectStateTypeMatch, self))
		return false;
	
	self -> match . action = p_action;
	
	r_state = self;
	
	return true;
}

static bool MCDialectStateFlatten(MCDialectStateRef self)
{
	switch(self -> type)
	{
		case kMCDialectStateTypeAlternation:
		{
			uindex_t t_new_child_count;
			t_new_child_count = 0;
			for(uindex_t i = 0; i < self -> alternation . child_count; i++)
			{
				if (!MCDialectStateFlatten(self -> alternation . children[i]))
					return false;
				if (self -> alternation . children[i] -> type == kMCDialectStateTypeAlternation)
					t_new_child_count += self -> alternation . children[i] -> alternation . child_count;
				else
					t_new_child_count += 1;
			}
			
			MCDialectStateRef *t_new_children;
			if (!MCMemoryNewArray(t_new_child_count, t_new_children))
				return false;
			
			uindex_t t_new_child_index;
			t_new_child_index = 0;
			for(uindex_t i = 0; i < self -> alternation . child_count; i++)
			{
				if (self -> alternation . children[i] -> type == kMCDialectStateTypeAlternation)
				{
					for(uindex_t j = 0; j < self -> alternation . children[i] -> alternation . child_count; j++)
						t_new_children[t_new_child_index++] = MCDialectStateRetain(self -> alternation . children[i] -> alternation . children[j]);
					MCDialectStateRelease(self -> alternation . children[i]);
				}
				else
					t_new_children[t_new_child_index++] = self -> alternation . children[i];
			}

			MCMemoryDeleteArray(self -> alternation . children);
			self -> alternation . children = t_new_children;
			self -> alternation . child_count = t_new_child_count;
		}
		break;
		
		case kMCDialectStateTypeConcatenation:
			for(uindex_t i = 0; i < self -> concatenation . child_count; i++)
				if (!MCDialectStateFlatten(self -> concatenation . children[i]))
					return false;
			break;
			
		case kMCDialectStateTypeRepetition:
			MCDialectStateFlatten(self -> repetition . pattern);
			if (self -> repetition . separator != nil)
				MCDialectStateFlatten(self -> repetition . separator);
			break;
			
		default:
			break;
	}
	
	return true;
}

static void MCDialectStatePrint(MCDialectStateRef self, MCDialectPrintCallback p_callback, void *p_context)
{
	switch(self -> type)
	{
		case kMCDialectStateTypeEpsilon:
			p_callback(p_context, "e");
			break;
			
		case kMCDialectStateTypeSeparator:
			p_callback(p_context, ";");
			break;
			
		case kMCDialectStateTypeAlternation:
			for(uindex_t i = 0; i < self -> alternation . child_count; i++)
			{
				p_callback(p_context, i == 0 ? "( " : " | ");
				MCDialectStatePrint(self -> alternation . children[i], p_callback, p_context);
			}
			p_callback(p_context, " )");
			break;
			
		case kMCDialectStateTypeConcatenation:
			for(uindex_t i = 0; i < self -> concatenation . child_count; i++)
			{
				p_callback(p_context, i == 0 ? "" : " ");
				MCDialectStatePrint(self -> concatenation . children[i], p_callback, p_context);
			}
			break;
			
		case kMCDialectStateTypeRepetition:
			p_callback(p_context, "{ ");
			MCDialectStatePrint(self -> repetition . pattern, p_callback, p_context);
			if (self -> repetition . separator != nil)
			{
				p_callback(p_context, " , ");
				MCDialectStatePrint(self -> repetition . separator, p_callback, p_context);
			}
			p_callback(p_context, " }");
			break;
				
		case kMCDialectStateTypeReference:
			p_callback(p_context, "<%s>", MCNameGetCString(self -> reference . name));
			break;
			
		case kMCDialectStateTypeIdentifier:
			p_callback(p_context, "%s'%s'", self -> identifier . is_marked ? "@" : "", MCNameGetCString(self -> identifier . name));
			break;
			
		case kMCDialectStateTypeMatch:
			p_callback(p_context, "{%p}", self -> match . action);
			break;
			
		default:
			p_callback(p_context, "?");
			break;
	}
}

////////////////////////////////////////////////////////////////////////////////

// alt_expr:
//   { concat_expr , '|' }
// concat_expr:
//   { factor_expr }
// factor_expr
//   : '[' alt_expr ']'
//   | '(' alt_expr ')'
//   | '{' alt_expr ',' alt_expr '}'
//   | id
//   | quot-id
//   | angle-id
//   | '@' id
//   | ';'

enum MCDialectSyntaxTokenType
{
	kMCDialectSyntaxTokenTypeNone,
	
	kMCDialectSyntaxTokenTypeIdentifier,
	kMCDialectSyntaxTokenTypeQuotedIdentifier,
	kMCDialectSyntaxTokenTypeAngledIdentifier,
	kMCDialectSyntaxTokenTypeColon,
	kMCDialectSyntaxTokenTypeComma,
	kMCDialectSyntaxTokenTypeSemicolon,
	kMCDialectSyntaxTokenTypeAt,
	kMCDialectSyntaxTokenTypeBar,
	kMCDialectSyntaxTokenTypeLeftBracket,
	kMCDialectSyntaxTokenTypeRightBracket,
	kMCDialectSyntaxTokenTypeLeftBrace,
	kMCDialectSyntaxTokenTypeRightBrace,
	kMCDialectSyntaxTokenTypeLeftParanthesis,
	kMCDialectSyntaxTokenTypeRightParanthesis,
	kMCDialectSyntaxTokenTypeEnd,
	
	kMCDialectSyntaxTokenTypeError,
};

static bool MCDialectSyntaxParseOptional(const char*& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error);
static bool MCDialectSyntaxParseGroup(const char*& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error);
static bool MCDialectSyntaxParseRepetition(const char*& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error);
static bool MCDialectSyntaxParseMarked(const char*& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error);
static bool MCDialectSyntaxParseSeparator(const char*& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error);
static bool MCDialectSyntaxParseFactor(const char*& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error);
static bool MCDialectSyntaxParseConcatenation(const char *& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error);
static bool MCDialectSyntaxParseAlternation(const char *& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error);

static void MCDialectSyntaxSkipWhitespace(const char*& x_syntax)
{
	while(*x_syntax != '\0')
	{
		if (!isspace(*x_syntax))
			break;
		x_syntax += 1;
	}
}

static bool MCDialectSyntaxMatchCharToken(const char*& x_syntax, char p_char, MCDialectSyntaxError p_error, MCDialectSyntaxError& r_error)
{
	MCDialectSyntaxSkipWhitespace(x_syntax);
	if (*x_syntax == p_char)
	{
		x_syntax += 1;
		return true;
	}
		
	r_error = p_error;
	
	return true;
}

static bool MCDialectSyntaxSkipCharToken(const char*& x_syntax, char p_char, bool& r_skipped, MCDialectSyntaxError& r_error)
{
	MCDialectSyntaxSkipWhitespace(x_syntax);
	if (*x_syntax == p_char)
	{
		x_syntax += 1;
		r_skipped = true;
	}
	else
		r_skipped = false;
		
	return true;
}

static bool MCDialectSyntaxMatchRightBracket(const char *& x_syntax, MCDialectSyntaxError& r_error)
{
	return MCDialectSyntaxMatchCharToken(x_syntax, ']', kMCDialectSyntaxErrorRightBracketExpected, r_error);
}

static bool MCDialectSyntaxMatchRightParanthesis(const char *& x_syntax, MCDialectSyntaxError& r_error)
{
	return MCDialectSyntaxMatchCharToken(x_syntax, ')', kMCDialectSyntaxErrorRightParanthesisExpected, r_error);
}

static bool MCDialectSyntaxMatchRightBrace(const char *& x_syntax, MCDialectSyntaxError& r_error)
{
	return MCDialectSyntaxMatchCharToken(x_syntax, '}', kMCDialectSyntaxErrorRightBraceExpected, r_error);
}

static bool MCDialectSyntaxMatchColon(const char *& x_syntax, MCDialectSyntaxError& r_error)
{
	return MCDialectSyntaxMatchCharToken(x_syntax, ':', kMCDialectSyntaxErrorColonExpected, r_error);
}

static bool MCDialectSyntaxMatchEnd(const char *& x_syntax, MCDialectSyntaxError& r_error)
{
	return MCDialectSyntaxMatchCharToken(x_syntax, '\0', kMCDialectSyntaxErrorEndExpected, r_error);
}

static bool MCDialectSyntaxSkipComma(const char*& x_syntax, bool& r_skipped, MCDialectSyntaxError& r_error)
{
	return MCDialectSyntaxSkipCharToken(x_syntax, ',', r_skipped, r_error);
}

static bool MCDialectSyntaxSkipLeftBracket(const char*& x_syntax, bool& r_skipped, MCDialectSyntaxError& r_error)
{
	return MCDialectSyntaxSkipCharToken(x_syntax, '[', r_skipped, r_error);
}

static bool MCDialectSyntaxSkipLeftParanthesis(const char*& x_syntax, bool& r_skipped, MCDialectSyntaxError& r_error)
{
	return MCDialectSyntaxSkipCharToken(x_syntax, '(', r_skipped, r_error);
}

static bool MCDialectSyntaxSkipLeftBrace(const char*& x_syntax, bool& r_skipped, MCDialectSyntaxError& r_error)
{
	return MCDialectSyntaxSkipCharToken(x_syntax, '{', r_skipped, r_error);
}

static bool MCDialectSyntaxSkipAt(const char*& x_syntax, bool& r_skipped, MCDialectSyntaxError& r_error)
{
	return MCDialectSyntaxSkipCharToken(x_syntax, '@', r_skipped, r_error);
}

static bool MCDialectSyntaxSkipSemicolon(const char*& x_syntax, bool& r_skipped, MCDialectSyntaxError& r_error)
{
	return MCDialectSyntaxSkipCharToken(x_syntax, ';', r_skipped, r_error);
}

static bool MCDialectSyntaxSkipBar(const char*& x_syntax, bool& r_skipped, MCDialectSyntaxError& r_error)
{
	return MCDialectSyntaxSkipCharToken(x_syntax, '|', r_skipped, r_error);
}

static bool MCDialectSyntaxMatchIdentifier(const char *& x_syntax, MCNameRef& r_id, MCDialectSyntaxError& r_error)
{
	MCDialectSyntaxSkipWhitespace(x_syntax);
	
	const char *t_id_start;
	t_id_start = x_syntax;
	while(*x_syntax != '\0')
	{
		if (!isalpha(*x_syntax))
			break;
		x_syntax += 1;
	}
		
	if (t_id_start == x_syntax)
	{
		r_error = kMCDialectSyntaxErrorNormalIdentifierExpected;
		return false;
	}
			
	if (!MCNameCreateWithChars(t_id_start, x_syntax - t_id_start, r_id))
	{
		r_error = kMCDialectSyntaxErrorOutOfMemory;
		return false;
	}
	
	return true;
}

static bool MCDialectSyntaxSkipAnyIdentifier(const char*& x_syntax, MCDialectSyntaxTokenType& r_type, MCNameRef& r_id, MCDialectSyntaxError& r_error)
{
	MCDialectSyntaxSkipWhitespace(x_syntax);
	
	const char *t_id_start;
	t_id_start = x_syntax;
	
	MCDialectSyntaxTokenType t_type;
	if (*x_syntax == '<')
	{
		t_type = kMCDialectSyntaxTokenTypeAngledIdentifier;
		x_syntax += 1;
	}
	else if (*x_syntax == '\'')
	{
		t_type = kMCDialectSyntaxTokenTypeQuotedIdentifier;
		x_syntax += 1;
	}
	else
		t_type = kMCDialectSyntaxTokenTypeIdentifier;

	
	while(*x_syntax != '\0')
	{
		if (t_type == kMCDialectSyntaxTokenTypeQuotedIdentifier)
		{
			if (*x_syntax == '\'')
				break;
		}
		else if (!isalpha(*x_syntax))
			break;
		
		x_syntax += 1;
	}
	
	const char *t_id_end;
	t_id_end = x_syntax;
			
	if (t_type == kMCDialectSyntaxTokenTypeAngledIdentifier)
	{
		if (*x_syntax != '>')
		{
			r_error = kMCDialectSyntaxErrorRightAngledBracketExpected;
			return false;
		}
		
		x_syntax += 1;
		
		t_id_start += 1;
	}
	else if (t_type == kMCDialectSyntaxTokenTypeQuotedIdentifier)
	{
		if (*x_syntax == '\0')
		{
			r_error = kMCDialectSyntaxErrorUnterminatedIdentifier;
			return false;
		}
		
		x_syntax += 1;
		
		t_id_start += 1;
	}
	
	if (t_id_start == t_id_end)
	{
		r_error = kMCDialectSyntaxErrorNormalIdentifierExpected;
		return false;
	}
			
	if (!MCNameCreateWithChars(t_id_start, t_id_end - t_id_start, r_id))
	{
		r_error = kMCDialectSyntaxErrorOutOfMemory;
		return false;
	}
	
	r_type = t_type;
	
	return true;
}

static bool MCDialectSyntaxWillMatchConcatEnd(const char*& x_syntax, bool& r_will_match, MCDialectSyntaxError& r_error)
{
	MCDialectSyntaxSkipWhitespace(x_syntax);
	
	if (*x_syntax == '\0' ||
		*x_syntax == ')' ||
		*x_syntax == '}' ||
		*x_syntax == ']' ||
		*x_syntax == '|' ||
		*x_syntax == ',' )
		r_will_match = true;
	else
		r_will_match = false;
		
	return true;
}

static bool MCDialectSyntaxParseOptional(const char*& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error)
{
	MCAutoDialectStateRef t_state;
	if (!MCDialectSyntaxParseAlternation(x_syntax, &t_state, r_error))
		return false;
		
	if (!MCDialectSyntaxMatchRightBracket(x_syntax, r_error))
		return false;
	
	MCAutoDialectStateRef t_epsilon;
	if (!MCDialectStateCreateEpsilon(&t_epsilon) ||
		!MCDialectStateCreateAlternation(*t_state, *t_epsilon, r_state))
	{
		r_error = kMCDialectSyntaxErrorOutOfMemory;
		return false;
	}
	
	return true;
}

static bool MCDialectSyntaxParseGroup(const char*& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error)
{
	MCAutoDialectStateRef t_state;
	if (!MCDialectSyntaxParseAlternation(x_syntax, &t_state, r_error))
		return false;
		
	if (!MCDialectSyntaxMatchRightParanthesis(x_syntax, r_error))
		return false;
		
	r_state = t_state . Take();
	
	return true;
}

static bool MCDialectSyntaxParseRepetition(const char*& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error)
{
	MCAutoDialectStateRef t_pattern;
	if (!MCDialectSyntaxParseAlternation(x_syntax, &t_pattern, r_error))
		return false;
	
	MCAutoDialectStateRef t_separator;
	bool t_skipped;
	if (!MCDialectSyntaxSkipComma(x_syntax, t_skipped, r_error))
		if (!MCDialectSyntaxParseAlternation(x_syntax, &t_separator, r_error))
			return false;
	
	if (!MCDialectSyntaxMatchRightBrace(x_syntax, r_error))
		return false;
	
	if (!MCDialectStateCreateRepetition(*t_pattern, *t_separator, r_state))
	{
		r_error = kMCDialectSyntaxErrorOutOfMemory;
		return false;
	}
	
	return true;
}

static bool MCDialectSyntaxParseMarked(const char*& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error)
{
	MCNewAutoNameRef t_identifier;
	if (!MCDialectSyntaxMatchIdentifier(x_syntax, &t_identifier, r_error))
		return false;
	
	if (!MCDialectStateCreateIdentifier(*t_identifier, true, r_state))
	{
		r_error = kMCDialectSyntaxErrorOutOfMemory;
		return false;
	}
	
	return true;
}

static bool MCDialectSyntaxParseSeparator(const char*& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error)
{
	if (!MCDialectStateCreateSeparator(r_state))
	{
		r_error = kMCDialectSyntaxErrorOutOfMemory;
		return false;
	}
	
	return true;
}

static bool MCDialectSyntaxParseFactor(const char*& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error)
{
	bool t_skipped;
	
	if (!MCDialectSyntaxSkipLeftBracket(x_syntax, t_skipped, r_error))
		return false;
	if (t_skipped)
		return MCDialectSyntaxParseOptional(x_syntax, r_state, r_error);
	
	if (!MCDialectSyntaxSkipLeftParanthesis(x_syntax, t_skipped, r_error))
		return false;
	if (t_skipped)
		return MCDialectSyntaxParseGroup(x_syntax, r_state, r_error);
	
	if (!MCDialectSyntaxSkipLeftBrace(x_syntax, t_skipped, r_error))
		return false;
	if (t_skipped)
		return MCDialectSyntaxParseRepetition(x_syntax, r_state, r_error);
	
	if (!MCDialectSyntaxSkipAt(x_syntax, t_skipped, r_error))
		return false;
	if (t_skipped)
		return MCDialectSyntaxParseMarked(x_syntax, r_state, r_error);
	
	if (!MCDialectSyntaxSkipSemicolon(x_syntax, t_skipped, r_error))
		return false;
	if (t_skipped)
		return MCDialectSyntaxParseSeparator(x_syntax, r_state, r_error);
	
	MCDialectSyntaxTokenType t_id_type;
	MCNewAutoNameRef t_id;
	if (!MCDialectSyntaxSkipAnyIdentifier(x_syntax, t_id_type, &t_id, r_error))
		return false;
	
	bool t_success;
	switch(t_id_type)
	{
		case kMCDialectSyntaxTokenTypeIdentifier:
		case kMCDialectSyntaxTokenTypeQuotedIdentifier:
			t_success = MCDialectStateCreateIdentifier(*t_id, false, r_state);
			break;
		case kMCDialectSyntaxTokenTypeAngledIdentifier:
			t_success = MCDialectStateCreateReference(*t_id, r_state);
			break;
		default:
			t_success = MCDialectStateCreateEpsilon(r_state);
			break;
	}
	
	if (!t_success)
	{
		r_error = kMCDialectSyntaxErrorOutOfMemory;
		return false;
	}
	
	return true;
}

static bool MCDialectSyntaxParseConcatenation(const char *& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error)
{
	MCAutoDialectStateRef t_state;
	for(;;)
	{
		MCAutoDialectStateRef t_new_state;
		if (!MCDialectSyntaxParseFactor(x_syntax, &t_new_state, r_error))
			return false;
		
		if (*t_state == nil)
			t_state . Give(t_new_state . Take());
		else
		{
			MCAutoDialectStateRef t_concat_state;
			if (!MCDialectStateCreateConcatenation(*t_state, *t_new_state, &t_concat_state))
			{
				r_error = kMCDialectSyntaxErrorOutOfMemory;
				return false;
			}
			
			t_state . Give(t_concat_state . Take());
		}
		
		bool t_will_match;
		if (!MCDialectSyntaxWillMatchConcatEnd(x_syntax, t_will_match, r_error))
			return false;
		if (t_will_match)
			break;
	}
	
	r_state = t_state . Take();
	
	return true;
}

static bool MCDialectSyntaxParseAlternation(const char *& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error)
{
	MCAutoDialectStateRef t_state;
	for(;;)
	{
		MCAutoDialectStateRef t_new_state;
		if (!MCDialectSyntaxParseConcatenation(x_syntax, &t_new_state, r_error))
			return false;
		
		if (*t_state == nil)
			t_state . Give(t_new_state . Take());
		else
		{
			MCAutoDialectStateRef t_alt_state;
			if (!MCDialectStateCreateAlternation(*t_state, *t_new_state, &t_alt_state))
			{
				r_error = kMCDialectSyntaxErrorOutOfMemory;
				return false;
			}
			
			t_state . Give(t_alt_state . Take());
		}
		
		bool t_skipped;
		if (!MCDialectSyntaxSkipBar(x_syntax, t_skipped, r_error))
			return false;
		
		if (!t_skipped)
			break;
	}
	
	r_state = t_state . Take();
	
	return true;
}

static bool MCDialectSyntaxParse(const char*& x_syntax, void *p_action, MCNameRef& r_scope, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error)
{
	MCNewAutoNameRef t_scope;
	if (!MCDialectSyntaxMatchIdentifier(x_syntax, &t_scope, r_error))
		return false;
	
	if (!MCDialectSyntaxMatchColon(x_syntax, r_error))
		return false;
	
	MCAutoDialectStateRef t_state;
	if (!MCDialectSyntaxParseAlternation(x_syntax, &t_state, r_error))
		return false;
	
	if (!MCDialectSyntaxMatchEnd(x_syntax, r_error))
		return false;
		
	MCAutoDialectStateRef t_match_state;
	if (!MCDialectStateCreateMatch(p_action, &t_match_state) ||
		!MCDialectStateCreateConcatenation(*t_state, *t_match_state, r_state))
	{
		r_error = kMCDialectSyntaxErrorOutOfMemory;
		return false;
	}

	r_scope = t_scope . Take();
	
	return true;
}

#endif
				