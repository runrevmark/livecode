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

#include "dialect.h"

////////////////////////////////////////////////////////////////////////////////

template<typename T> class MCAutoDialectBaseRef
{
public:
	MCAutoDialectBaseRef(void)
	{
		m_ref = nil;
	}
	
	~MCAutoDialectBaseRef(void)
	{
		MCDialectObjectRelease(m_ref);
	}
	
	T operator * (void)
	{
		return m_ref;
	}
	
	T& operator & (void)
	{
		assert(m_ref == nil);
		return m_ref;
	}
	
	void Give(T p_ref)
	{
		assert(m_ref == nil);
		m_ref = p_ref;
	}
	
	T Take(void)
	{
		T t_ref;
		t_ref = m_ref;
		m_ref = nil;
		return t_ref;
	}
	
private:
	T m_ref;
};

////////////////////////////////////////////////////////////////////////////////

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

struct MCDialectObject
{
	uindex_t references;
};

struct MCDialectState: public MCDialectObject
{
};

struct MCDialectIdentifier: public MCDialectObject
{
};

typedef struct MCDialectState *MCDialectStateRef;
typedef struct MCDialectIdentifier *MCDialectIdentifierRef;

typedef MCAutoDialectBaseRef<MCDialectStateRef> MCAutoDialectStateRef;
typedef MCAutoDialectBaseRef<MCDialectIdentifierRef> MCAutoDialectIdentifierRef;

static void MCDialectObjectRelease(MCDialectObject *self);
static void MCDialectObjectRetain(MCDialectObject *self);

////////////////////////////////////////////////////////////////////////////////

static void MCDialectObjectRelease(MCDialectObject *self)
{
}

static void MCDialectObjectRetain(MCDialectObject *self)
{
}

////////////////////////////////////////////////////////////////////////////////

static bool MCDialectIdentifierCreate(const char *p_id, uindex_t p_length, MCDialectIdentifierRef& r_id)
{
	return false;
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
		if (!isspace(*x_syntax))
			break;
}

static bool MCDialectSyntaxMatchCharToken(const char*& x_syntax, char p_char, MCDialectSyntaxError p_error, MCDialectSyntaxError& r_error)
{
	MCDialectSyntaxSkipWhitespace(x_syntax);
	if (*x_syntax == p_char)
		return true;
		
	r_error = p_error;
	
	return true;
}

static bool MCDialectSyntaxSkipCharToken(const char*& x_syntax, char p_char, bool& r_skipped, MCDialectSyntaxError& r_error)
{
	MCDialectSyntaxSkipWhitespace(x_syntax);
	if (*x_syntax == p_char)
		r_skipped = true;
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
	return MCDialectSyntaxSkipCharToken(x_syntax, ']', r_skipped, r_error);
}

static bool MCDialectSyntaxSkipLeftParanthesis(const char*& x_syntax, bool& r_skipped, MCDialectSyntaxError& r_error)
{
	return MCDialectSyntaxSkipCharToken(x_syntax, ')', r_skipped, r_error);
}

static bool MCDialectSyntaxSkipLeftBrace(const char*& x_syntax, bool& r_skipped, MCDialectSyntaxError& r_error)
{
	return MCDialectSyntaxSkipCharToken(x_syntax, '}', r_skipped, r_error);
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

static bool MCDialectSyntaxMatchIdentifier(const char *& x_syntax, MCDialectIdentifierRef& r_id, MCDialectSyntaxError& r_error)
{
	MCDialectSyntaxSkipWhitespace(x_syntax);
	
	const char *t_id_start;
	t_id_start = x_syntax;
	while(*x_syntax != '\0')
		if (!isalpha(*x_syntax))
			break;
			
	if (t_id_start == x_syntax)
	{
		r_error = kMCDialectSyntaxErrorNormalIdentifierExpected;
		return false;
	}
			
	if (!MCDialectIdentifierCreate(t_id_start, x_syntax - t_id_start, r_id))
	{
		r_error = kMCDialectSyntaxErrorOutOfMemory;
		return false;
	}
	
	return true;
}

static bool MCDialectSyntaxSkipAnyIdentifier(const char*& x_syntax, MCDialectSyntaxTokenType& r_type, MCDialectIdentifierRef& r_id, MCDialectSyntaxError& r_error)
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
	
	while(*x_syntax != '\0')
	{
		if (t_type == kMCDialectSyntaxTokenTypeQuotedIdentifier)
		{
			if (*x_syntax != '\'')
				break;
		}
		else if (!isalpha(*x_syntax))
			break;
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
		t_id_start += 1;
		t_id_end -= 1;
	}
	else if (t_type == kMCDialectSyntaxTokenTypeQuotedIdentifier)
	{
		if (*x_syntax == '\0')
		{
			r_error = kMCDialectSyntaxErrorUnterminatedIdentifier;
			return false;
		}
		t_id_start += 1;
		t_id_end -= 1;
	}
	
	if (t_id_start == t_id_end)
	{
		r_error = kMCDialectSyntaxErrorNormalIdentifierExpected;
		return false;
	}
			
	if (!MCDialectIdentifierCreate(t_id_start, t_id_end - t_id_start, r_id))
	{
		r_error = kMCDialectSyntaxErrorOutOfMemory;
		return false;
	}
	
	return true;
}

static bool MCDialectSyntaxWillMatchConcatEnd(const char*& x_syntax, bool& r_will_match, MCDialectSyntaxError& r_error)
{
	MCDialectSyntaxSkipWhitespace(x_syntax);
	
	if (*x_syntax == '\0' ||
		*x_syntax == ')' ||
		*x_syntax == '}' ||
		*x_syntax == ']' ||
		*x_syntax == '|')
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
	
	if (!MCDialectStateAppendEpsilon(*t_state))
	{
		r_error = kMCDialectSyntaxErrorOutOfMemory;
		return false;
	}
		
	r_state = t_state . Take();
	
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
			
	// Build repetition node
	
	return true;
}

static bool MCDialectSyntaxParseMarked(const char*& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error)
{
	MCAutoDialectIdentifierRef t_identifier;
	if (!MCDialectSyntaxMatchIdentifier(x_syntax, &t_identifier, r_error))
		return false;
	
	// Build @ node
	
	return true;
}

static bool MCDialectSyntaxParseSeparator(const char*& x_syntax, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error)
{
	// Build separator node
	
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
	MCAutoDialectIdentifierRef t_id;
	if (!MCDialectSyntaxSkipAnyIdentifier(x_syntax, t_id_type, &t_id, r_error))
		return false;
	if (t_id_type != kMCDialectSyntaxTokenTypeNone)
	{
		// Build id node
		
		return true;
	}
	
	// Build epsilon node
	
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
			// Build concatenation node
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
		if (!MCDialectSyntaxParseConcatenation(x_syntax, &t_state, r_error))
			return false;
		
		if (*t_state == nil)
			t_state . Give(t_new_state . Take());
		else
		{
			// Build alternation node
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

static bool MCDialectSyntaxParse(const char*& x_syntax, MCDialectIdentifierRef& r_scope, MCDialectStateRef& r_state, MCDialectSyntaxError& r_error)
{
	MCAutoDialectIdentifierRef t_scope;
	if (!MCDialectSyntaxMatchIdentifier(x_syntax, &t_scope, r_error))
		return false;
	
	if (!MCDialectSyntaxMatchColon(x_syntax, r_error))
		return false;
	
	MCAutoDialectStateRef t_state;
	if (!MCDialectSyntaxParseAlternation(x_syntax, &t_state, r_error))
		return false;
	
	if (!MCDialectSyntaxMatchEnd(x_syntax, r_error))
		return false;
	
	r_scope = t_scope . Take();
	r_state = t_state . Take();
	
	return true;
}

void MCDialectCreate(MCDialectRef& r_dialect)
{
}

void MCDialectDestroy(MCDialectRef self)
{
}

bool MCDialectIsValid(MCDialectRef self)
{
	return false;
}

void MCDialectAddRule(MCDialectRef self, const char *p_syntax, uindex_t p_action_id)
{
	MCDialectSyntaxError t_error;
	MCAutoDialectIdentifierRef t_scope;
	MCAutoDialectStateRef t_state;
	const char *t_syntax_ptr;
	t_syntax_ptr = p_syntax;
	if (!MCDialectSyntaxParse(t_syntax_ptr, &t_scope, &t_state, t_error))
	{
		// Generate error
		return;
	}
	
	// Add rule
}

////////////////////////////////////////////////////////////////////////////////
