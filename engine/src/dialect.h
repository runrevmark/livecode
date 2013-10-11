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

///////////////////////////////////////////////////////////////////////////////

typedef struct MCDialect *MCDialectRef;

#if 0
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

MCDialectRef MCDialectRetain(MCDialectRef dialect);
void MCDialectRelease(MCDialectRef dialect);

typedef void (*MCDialectPrintCallback)(void *context, const char *format, ...);
void MCDialectPrint(MCDialectRef dialect, MCDialectPrintCallback callback, void *context);

///////////////////////////////////////////////////////////////////////////////

// A DialectBuilder is a transient object abstracting the construction of a
// dialect.
//
// A dialect consists of a collection of rules, each of which is choice of
// phrases. Each phrase maps to a named action.
//
// Phrases are specified using a simple BNF-style format:
//   - alternation
//   - concatenation
//   - repetition (both with delimiter and without)
//   - descent (matches a named rule)
//   - token (matches a named token)
//   - constant (matches the empty sequence of tokens and pushes a constant)
//   - epsilon (matches the empty sequence of tokens)
//
// A dialect itself is abstracted from action and lexical structure via the
// use of (unsigned integer) names for actions and tokens. The lexical analysis
// phase provides a stream of named tokens to the dialect when parsing and
// the dialect provides a stream of named actions to the subsequent phases.
//
// Building a dialect requires an appropriate sequence of nested calls to
// the builder:
//   Begin
//     BeginPhrase
//       ... phrase building calls ...
//     EndPhrase
//     ... further phrases ...
//   End
//
// Building a phrase requires an appropriately nested sequence of calls to
// the pattern construction methods. For example, to build the pattern:
//
//   put [ unicode ] IDENTIFIER into IDENTIFIER
//
// The sequence would be:
//
//   BeginConcatenation
//     MatchToken('put')
//     BeginAlternation
//       MatchToken('unicode')
//       MatchEpsilon
//     EndAlternation
//     MatchToken('IDENTIFIER')
//     MatchToken('into')
//     MatchToken('IDENTIFIER')
//   EndConcatenation
//
// A pattern can also match a named rule. This causes the rule with the
// specified name to be descended to for matching.
//
// A token match can be 'marked' meaning that the value of the token is
// pushed on the operand stack for the currently matching phrase.
//
// The builder methods return no error state, instead if an error occurs
// the builder is put into an error state which can be checked for by
// HasError.
//
// If an error occurs during construction, End will return a non-'None' error
// code and will not build a dialect.
//
// Whether or not an error occurs, calling End will destroy the builder.
//
// The build can be cancelled at any time by using the Cancel method. This
// is the same as End except that no attempt is made to build the dialect.

// The dialect builder data type.
typedef struct MCDialectBuilder *MCDialectBuilderRef;

// 
enum MCDialectBuilderError
{
	kMCDialectBuilderErrorNone,
	kMCDialectBuilderErrorOutOfMemory,
	kMCDialectBuilderErrorNoPhraseInProgress,
	kMCDialectBuilderErrorPhraseAlreadyInProgress,
	kMCDialectBuilderErrorInsufficentStateArity,
	kMCDialectBuilderErrorPatternNotFinished,
	kMCDialectBuilderErrorUnbalancedPattern,
	kMCDialectBuilderErrorMismatchedPattern,
	kMCDialectBuilderErrorNotAllRulesDefined
};

// Begin building a dialect, this creates the builder object.
void MCDialectBuilderBegin(MCDialectBuilderRef& r_builder);

// Finish building a dialect. If an error has occured, or occured during final
// construction, r_error returns the error. Otherwise, r_error will be None and
// r_dialect will contain the new dialect. In either case the builder is
// destroyed.
void MCDialectBuilderEnd(MCDialectBuilderRef builder, MCDialectBuilderError& r_error, MCDialectRef& r_dialect);

// HasError returns true if the dialect builder is in the error state.
bool MCDialectBuilderHasError(MCDialectBuilderRef builder);

// Cancel building a dialect. This destroys the dialect builder.
void MCDialectBuilderCancel(MCDialectBuilderRef builder);

// Begin a phrase. The phrase is added to the rule named 'rule_name' and will
// produce the action 'action_name' when matched.
void MCDialectBuilderBeginPhrase(MCDialectBuilderRef builder, uindex_t rule_name, uindex_t action_name);

// End a phrase. This finishes phrase construction.
void MCDialectBuilderEndPhrase(MCDialectBuilderRef builder);

// Start an alternation pattern. This produces a pattern which matches one of
// its children.
void MCDialectBuilderBeginAlternation(MCDialectBuilderRef builder);

// Finish an alternation pattern.
void MCDialectBuilderEndAlternation(MCDialectBuilderRef builder);

// Start a concatenation pattern. This produces a pattern which matches all of
// its children in order, left to right.
void MCDialectBuilderBeginConcatenation(MCDialectBuilderRef builder);

// Finish a concatenation pattern.
void MCDialectBuilderEndConcatenation(MCDialectBuilderRef builder);

// Start a repetition pattern. A repetition pattern has one or two children.
// If the pattern has one child then it matches one or more occurances of the
// child.
// If the pattern has two children then it matches one or more occurances of the
// first child, separated by the second child.
void MCDialectBuilderBeginRepetition(MCDialectBuilderRef builder);

// Finish a repetition pattern.
void MCDialectBuilderEndRepetition(MCDialectBuilderRef builder);

// Match the empty sequence of tokens.
void MCDialectBuilderMatchEpsilon(MCDialectBuilderRef builder);

// Match the named rule.
void MCDialectBuilderMatchRule(MCDialectBuilderRef builder, uindex_t rule_name);

// Match the empty sequence of tokens and push a named constant.
void MCDialectBuilderMatchConstant(MCDialectBuilderRef builder, uindex_t constant_name);

// Match the named token. If marked is true, then in the case of a successful
// match, the value of the token will be pushed on the operand stack.
void MCDialectBuilderMatchToken(MCDialectBuilderRef builder, uindex_t name, bool marked);

// Match the empty sequence of tokens and cause a break (indicates a succesful
// match for the entire rule).
void MCDialectBuilderMatchBreak(MCDialectBuilderRef builder);

////////////////////////////////////////////////////////////////////////////////

// A DialectGrammar is a transient object which simplifies the building of a
// LiveCode-like dialect by the use of standard conventions and pattern syntax.
//
// The pattern syntax is as follows:
//
// pattern
//   : alternation
//
// alternation
//   : { concatenation , '|' }
//
// concatenation
//   : { factor }
//
// factor
//   : '[' pattern ']'
//   | '(' pattern ')'
//   | '{' pattern [ ',' pattern ] '}'
//   | id
//   | quoted-id
//   | angled-id
//   | '@' constant
//   | ';'
//
// constant
//   : true
//   | false
//   | integer
//   | real
//   | string
//   | id
//
// Where the lexical elements are:
//
// id
//   : [a-zA-Z]+
//
// quoted-id
//   : \' id \'
//
// angled-id
//   : \< id \<
//
// An id represents either a literal token or a named token, if one with the given
// name has been defined.
//
// A quoted id represents a literal token consiting of the quoted id itself without
// the quotes.
//
// An angled id represents a descent to the rule named by the id.
//
// Specifying '@' before an id indicates that the token match should be marked. Note
// that if the id is a named token, then they are always marked so it has no effect.
//

typedef struct MCDialectGrammar *MCDialectGrammarRef;

bool MCDialectGrammarBegin(MCDialectGrammarRef& r_grammar);
bool MCDialectGrammarEnd(MCDialectGrammarRef grammar, MCDialectRef& r_dialect);

bool MCDialectGrammarHasError(MCDialectGrammarRef grammar);

void MCDialectGrammarDefineConstant(MCDialectGrammarRef grammar, const char *constant, uindex_t constant_index);
void MCDialectGrammarDefineToken(MCDialectGrammarRef grammar, const char *token, uindex_t token_index);
void MCDialectGrammarDefinePhrase(MCDialectGrammarRef grammar, const char *rule, const char *pattern, uindex_t action_index);

////////////////////////////////////////////////////////////////////////////////

#endif
