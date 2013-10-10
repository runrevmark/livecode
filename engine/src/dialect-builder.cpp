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

struct MCDialectBuilderRule
{
	// A rule is named by an integer. The name is used to reference the
	// rule from a 'MatchRule' state, and to allow multiple phrases to
	// be attached to one rule.
	uindex_t name;
	MCDialectStateRef pattern;
};

struct MCDialectBuilder
{
	// The current error state of the builder. If this is not 'None' then
	// operations on the builder have no effect.
	MCDialectBuilderError error;
	
	// The current list of rules under construction. A rule is an alternation
	// of phrases, where a phrase is a pattern ending with a 'match' state.
	MCDialectBuilderRule *rules;
	uindex_t rule_count;
	
	// The current stack of states for the phrase under construction. When
	// a phrase is finished, the stack should consist of one element - the
	// root state of the phrase's pattern. If a phrase is being built then
	// there will be at least one state on the stack which is the root
	// concatenation state.
	MCDialectStateRef *states;
	uindex_t state_count;
	
	// The index of the rule (in rules) currently being processed.
	uindex_t current_rule_name;
	
	// The name of the action that should be associated with the phrase
	// currently under construction when it's finished.
	uindex_t current_action_name;
	
};

////////////////////////////////////////////////////////////////////////////////

// Sets the dialect builder to an error state with the given error.
static bool MCDialectBuilderThrow(MCDialectBuilderRef self, MCDialectBuilderError error);

// The 'Check' methods all check their predicate returning true if it is
// satisfied. If it is not satisfied an appropriate error is flagged in the
// builder.

// Check that there is a phrase in progress.
static bool MCDialectBuilderCheckPhraseInProgress(MCDialectBuilderRef builder);
// Check that there is no phrase in progress.
static bool MCDialectBuilderCheckNoPhraseInProgress(MCDialectBuilderRef builder);
// Check that a state can be appended to the top of the stack.
static bool MCDialectBuilderCheckCanAppendState(MCDialectBuilderRef builder);

// Ensure that a rule with given name exists, and return a pointer to it.
static bool MCDialectBuilderEnsureRule(MCDialectBuilderRef builder, uindex_t rule_name, MCDialectBuilderRule*& r_rule);

// Create a new state.
static bool MCDialectBuilderNewState(MCDialectBuilderRef builder, MCDialectStateRef& r_state, MCDialectStateType state_type, ...);
static bool MCDialectBuilderNewStateV(MCDialectBuilderRef builder, MCDialectStateRef& r_state, MCDialectStateType state_type, va_list args);

// Append the given state to the target state.
static bool MCDialectBuilderAppendStateToState(MCDialectBuilderRef builder, MCDialectStateRef target_state, MCDialectStateRef state);
// Append a new state to the target state.
static bool MCDialectBuilderAppendNewStateToState(MCDialectBuilderRef builder, MCDialectStateRef target_state, MCDialectStateType state_type, ...);
static bool MCDialectBuilderAppendNewStateToStateV(MCDialectBuilderRef builder, MCDialectStateRef target_state, MCDialectStateType state_type, va_list args);

// Push the given state onto the builder statck.
static bool MCDialectBuilderPushState(MCDialectBuilderRef builder, MCDialectStateRef state);
// Push a new state onto the builder stack.
static bool MCDialectBuilderPushNewState(MCDialectBuilderRef builder, MCDialectStateType state_type, ...);
static bool MCDialectBuilderPushNewStateV(MCDialectBuilderRef builder, MCDialectStateType state_type, va_list args);
// Append the given state to the top state on the builder stack.
static bool MCDialectBuilderAppendState(MCDialectBuilderRef builder, MCDialectStateRef state);
// Append a new state to the top state on the builder stack.
static bool MCDialectBuilderAppendNewState(MCDialectBuilderRef builder, MCDialectStateType state_type, ...);
static bool MCDialectBuilderAppendNewStateV(MCDialectBuilderRef builder, MCDialectStateType state_type, va_list args);

// Pop the root state from the builder stack. An error is flagged if the root
// state is not top.
static bool MCDialectBuilderPopRootState(MCDialectBuilderRef builder, MCDialectStateRef& r_state);
// Pop the top non-root state from the builder stack. An error is flagged if
// the top state is the root state.
static bool MCDialectBuilderPopState(MCDialectBuilderRef builder, MCDialectStateRef& r_state);
// Pop the top non-root state from the builder stack and ensure it is of the
// appropriate type.
static bool MCDialectBuilderPopStateOfType(MCDialectBuilderRef builder, MCDialectStateType state_type, MCDialectStateRef& r_state);

// Begin a nested constuct of the given type.
static bool MCDialectBuilderBeginNest(MCDialectBuilderRef self, MCDialectStateType p_state_type);
// Begin a nested constuct of the given type.
static bool MCDialectBuilderContinueNest(MCDialectBuilderRef self, MCDialectStateType p_state_type);
// Begin a nested constuct of the given type.
static bool MCDialectBuilderEndNest(MCDialectBuilderRef self, MCDialectStateType p_state_type);

////////////////////////////////////////////////////////////////////////////////

void MCDialectBuilderBegin(MCDialectBuilderRef& r_builder)
{
	MCDialectBuilderRef self;
	
	// Allocate a new builder record. If this fails, we return a nil pointer
	// which is treated as a builder with error field 'OutOfMemory'.
	if (!MCMemoryNew(self))
	{
		r_builder = nil;
		return;
	}
	
	// Initialize the members of the record.
	self -> error = kMCDialectBuilderErrorNone;
	
	r_builder = self;
}

void MCDialectBuilderEnd(MCDialectBuilderRef self, MCDialectBuilderError& r_error, MCDialectRef& r_dialect)
{
	// If builder is nil, then we must have been out of memory on creation.
	if (self == nil)
	{
		r_error = kMCDialectBuilderErrorOutOfMemory;
		return;
	}
	
	// TODO: Build the dialect!!
}

bool MCDialectBuilderHasError(MCDialectBuilderRef self)
{
	return self -> error != kMCDialectBuilderErrorNone;
}

void MCDialectBuilderCancel(MCDialectBuilderRef self)
{
	// TODO: Cancel the build.
}

void MCDialectBuilderBeginPhrase(MCDialectBuilderRef self, uindex_t p_rule_name, uindex_t p_action_name)
{
	// If we are in an error state, then do nothing.
	if (MCDialectBuilderHasError(self))
		return;
	
	// If we already have a phrase in progress, it's an error.
	if (!MCDialectBuilderCheckNoPhraseInProgress(self))
		return;
	
	// A phrase begines with an implicit concatenation as it terminates in a
	// match state.
	if (!MCDialectBuilderPushNewState(self, kMCDialectStateTypeConcatenation, 0))
		return;
	
	// Set the rule and action names appropriately.
	self -> current_rule_name = p_rule_name;
	self -> current_action_name = p_action_name;
}

void MCDialectBuilderEndPhrase(MCDialectBuilderRef self)
{
	// If we are in an error state, then do nothing.
	if (MCDialectBuilderHasError(self))
		return;
	
	// If we are not in a phrase, it's an error.
	if (!MCDialectBuilderCheckPhraseInProgress(self))
		return;
	
	// Pop the root state.
	MCAutoDialectStateRef t_root_state;
	if (!MCDialectBuilderPopRootState(self, &t_root_state))
		return;

	// Append a match state.
	if (!MCDialectBuilderAppendNewStateToState(self, *t_root_state, kMCDialectStateTypeMatch, self -> current_action_name))
		return;
	
	// Now find the rule the phrase is being added to.
	MCDialectBuilderRule *t_rule;
	if (!MCDialectBuilderEnsureRule(self, self -> current_rule_name, t_rule))
		return;
	
	if (!MCDialectBuilderAppendStateToState(self, t_rule -> pattern, *t_root_state))
		return;
}

void MCDialectBuilderBeginAlternation(MCDialectBuilderRef self)
{
	if (!MCDialectBuilderBeginNest(self, kMCDialectStateTypeAlternation))
		return;
}

void MCDialectBuilderEndAlternation(MCDialectBuilderRef self)
{
	if (!MCDialectBuilderEndNest(self, kMCDialectStateTypeAlternation))
		return;
}

void MCDialectBuilderBeginConcatenation(MCDialectBuilderRef self)
{
	if (!MCDialectBuilderBeginNest(self, kMCDialectStateTypeConcatenation))
		return;
}

void MCDialectBuilderEndConcatenation(MCDialectBuilderRef self)
{
	if (!MCDialectBuilderEndNest(self, kMCDialectStateTypeConcatenation))
		return;
}

void MCDialectBuilderBeginRepetition(MCDialectBuilderRef self)
{
	// Begin a repetition nest.
	if (!MCDialectBuilderBeginNest(self, kMCDialectStateTypeRepetition))
		return;
}

void MCDialectBuilderContinueRepetition(MCDialectBuilderRef self)
{
	// Continue the repetition nest.
	if (!MCDialectBuilderContinueNest(self, kMCDialectStateTypeRepetition))
		return;
}

void MCDialectBuilderEndRepetition(MCDialectBuilderRef self)
{
	if (!MCDialectBuilderEndNest(self, kMCDialectStateTypeRepetition))
		return;
}

void MCDialectBuilderMatchEpsilon(MCDialectBuilderRef self)
{
	// If we are in an error state, then do nothing.
	if (MCDialectBuilderHasError(self))
		return;
	
	// If we are not in a phrase, it's an error.
	if (!MCDialectBuilderCheckPhraseInProgress(self))
		return;
	
	if (!MCDialectBuilderCheckCanAppendState(self))
		return;
	
	if (!MCDialectBuilderAppendNewState(self, kMCDialectStateTypeEpsilon))
		return;
}

void MCDialectBuilderMatchRule(MCDialectBuilderRef self, index_t p_rule_name)
{
	// If we are in an error state, then do nothing.
	if (MCDialectBuilderHasError(self))
		return;
	
	// If we are not in a phrase, it's an error.
	if (!MCDialectBuilderCheckPhraseInProgress(self))
		return;
	
	if (!MCDialectBuilderCheckCanAppendState(self))
		return;
	
	if (!MCDialectBuilderAppendNewState(self, kMCDialectStateTypeRule, p_rule_name))
		return;
}

void MCDialectBuilderMatchToken(MCDialectBuilderRef self, index_t p_token_name, bool p_marked)
{
	// If we are in an error state, then do nothing.
	if (MCDialectBuilderHasError(self))
		return;
	
	// If we are not in a phrase, it's an error.
	if (!MCDialectBuilderCheckPhraseInProgress(self))
		return;
	
	if (!MCDialectBuilderCheckCanAppendState(self))
		return;
	
	if (!MCDialectBuilderAppendNewState(self, kMCDialectStateTypeToken, p_token_name, p_marked))
		return;
}

////////////////////////////////////////////////////////////////////////////////

static bool MCDialectBuilderThrow(MCDialectBuilderRef self, MCDialectBuilderError p_error)
{
	assert(!MCDialectBuilderHasError(self));
	
	self -> error = p_error;
	
	return false;
}

////////////////////////////////////////////////////////////////////////////////

static bool MCDialectBuilderCheckPhraseInProgress(MCDialectBuilderRef self)
{
	assert(!MCDialectBuilderHasError(self));
	
	if (self -> state_count == 0)
		return MCDialectBuilderThrow(self, kMCDialectBuilderErrorNoPhraseInProgress);
	
	return true;
}

static bool MCDialectBuilderCheckNoPhraseInProgress(MCDialectBuilderRef self)
{
	assert(!MCDialectBuilderHasError(self));
	
	if (self -> state_count != 0)
		return MCDialectBuilderThrow(self, kMCDialectBuilderErrorPhraseAlreadyInProgress);
	
	return true;
}

static bool MCDialectBuilderCheckCanAppendState(MCDialectBuilderRef self)
{
	assert(!MCDialectBuilderHasError(self));
	assert(self -> state_count > 0);
	
	// Get the top state.
	MCDialectStateRef t_top_state;
	t_top_state = self -> states[self -> state_count - 1];
	
	// If the top state is an alternation or concatenation then we are fine.
	if (MCDialectStateIsAlternation(t_top_state) ||
		MCDialectStateIsConcatenation(t_top_state))
		return true;
	
	// If the top state is a repetition and it has arity < 2, then we are fine.
	if (MCDialectStateIsRepetition(t_top_state) &&
		MCDialectStateGetArity(t_top_state) < 2)
		return true;
	
	// Otherwise we can't do an append.
	return MCDialectBuilderThrow(self, kMCDialectBuilderErrorInsufficentStateArity);
}

////////////////////////////////////////////////////////////////////////////////

static bool MCDialectBuilderEnsureRule(MCDialectBuilderRef self, uindex_t p_rule_name, MCDialectBuilderRule*& r_rule)
{
	assert(!MCDialectBuilderHasError(self));
	
	MCDialectBuilderRule *t_rule;
	for(uindex_t i = 0; i < self -> rule_count; i++)
		if (self -> rules[i] . name == p_rule_name)
		{
			t_rule = &self -> rules[i];
			break;
		}
	
	if (t_rule == nil)
	{
		if (!MCMemoryResizeArray(self -> rule_count + 1, self -> rules, self -> rule_count))
			return MCDialectBuilderThrow(self, kMCDialectBuilderErrorOutOfMemory);
		
		t_rule = &self -> rules[self -> rule_count - 1];
		
		if (!MCDialectBuilderNewState(self, t_rule -> pattern, kMCDialectStateTypeAlternation, 0))
			return false;
		
		t_rule -> name = p_rule_name;
	}
	
	r_rule = t_rule;
	
	return true;
}		

////////////////////////////////////////////////////////////////////////////////

static bool MCDialectBuilderNewState(MCDialectBuilderRef self, MCDialectStateRef& r_new_state, MCDialectStateType p_state_type, ...)
{
	bool t_success;
	va_list t_args;
	va_start(t_args, p_state_type);
	t_success = MCDialectBuilderNewStateV(self, r_new_state, p_state_type, t_args);
	va_end(t_args);
	return t_success;
}

static bool MCDialectBuilderNewStateV(MCDialectBuilderRef self, MCDialectStateRef& r_new_state, MCDialectStateType p_state_type, va_list p_args)
{
	assert(!MCDialectBuilderHasError(self));
	
	MCAutoDialectStateRef t_new_state;
	
	switch(p_state_type)
	{
		case kMCDialectStateTypeEpsilon:
			if (!MCDialectStateCreateEpsilon(&t_new_state))
				return MCDialectBuilderThrow(self, kMCDialectBuilderErrorOutOfMemory);
			break;
			
		case kMCDialectStateTypeAlternation:
		case kMCDialectStateTypeConcatenation:
		case kMCDialectStateTypeRepetition:
			if (!MCDialectStateCreate(p_state_type, &t_new_state))
				return MCDialectBuilderThrow(self, kMCDialectBuilderErrorOutOfMemory);
			
			uindex_t t_child_state_count;
			t_child_state_count = va_arg(p_args, uindex_t);
			for(uindex_t i = 0; i < t_child_state_count; i++)
			{
				MCDialectStateRef t_child_state;
				t_child_state = va_arg(p_args, MCDialectStateRef);
				if (!MCDialectBuilderAppendStateToState(self, *t_new_state, t_child_state))
					return false;
			}
			break;
			
		case kMCDialectStateTypeRule:
			if (!MCDialectStateCreateRule(va_arg(p_args, uindex_t), &t_new_state))
				return MCDialectBuilderThrow(self, kMCDialectBuilderErrorOutOfMemory);
			break;
			
		case kMCDialectStateTypeToken:
		{
			uindex_t t_name;
			t_name = va_arg(p_args, uindex_t);
			
			bool t_marked;
			t_marked = va_arg(p_args, int) != 0;
			
			if (!MCDialectStateCreateToken(t_name, t_marked, &t_new_state))
				return MCDialectBuilderThrow(self, kMCDialectBuilderErrorOutOfMemory);
		}
		break;
			
		case kMCDialectStateTypeMatch:
			if (!MCDialectStateCreateMatch(va_arg(p_args, uindex_t), &t_new_state))
				return MCDialectBuilderThrow(self, kMCDialectBuilderErrorOutOfMemory);
			break;
			
		default:
			assert(false);
			break;
	}
	
	r_new_state = t_new_state . Take();
}

////////////////////////////////////////////////////////////////////////////////

static bool MCDialectBuilderAppendStateToState(MCDialectBuilderRef self, MCDialectStateRef p_target_state, MCDialectStateRef p_state)
{
	assert(!MCDialectBuilderHasError(self));
	assert(MCDialectStateIsAlternation(p_target_state) ||
		   MCDialectStateIsConcatenation(p_target_state) ||
		   (MCDialectStateIsRepetition(p_target_state) && MCDialectStateGetArity(p_target_state) < 2));
	
	if (!MCDialectStateAppend(p_target_state, p_state))
		return MCDialectBuilderThrow(self, kMCDialectBuilderErrorOutOfMemory);
	
	return true;
}

static bool MCDialectBuilderAppendNewStateToState(MCDialectBuilderRef self, MCDialectStateRef p_target_state, MCDialectStateType p_state_type, ...)
{
	bool t_success;

	va_list t_args;
	va_start(t_args, p_state_type);
	t_success = MCDialectBuilderAppendNewStateToStateV(self, p_target_state, p_state_type, t_args);
	va_end(t_args);
	
	return t_success;
}

static bool MCDialectBuilderAppendNewStateToStateV(MCDialectBuilderRef self, MCDialectStateRef p_target_state, MCDialectStateType p_state_type, va_list p_args)
{
	assert(!MCDialectBuilderHasError(self));
	
	MCAutoDialectStateRef t_new_state;
	if (!MCDialectBuilderNewStateV(self, &t_new_state, p_state_type, p_args))
		return false;
	
	return MCDialectBuilderAppendStateToState(self, p_target_state, *t_new_state);
}

////////////////////////////////////////////////////////////////////////////////

static bool MCDialectBuilderPushState(MCDialectBuilderRef self, MCDialectStateRef p_state)
{
	assert(!MCDialectBuilderHasError(self));
	
	if (!MCMemoryResizeArray(self -> state_count + 1, self -> states, self -> state_count))
		return MCDialectBuilderThrow(self, kMCDialectBuilderErrorOutOfMemory);
	
	self -> states[self -> state_count - 1] = MCDialectStateRetain(p_state);
	
	return true;
}

static bool MCDialectBuilderAppendState(MCDialectBuilderRef self, MCDialectStateRef p_state)
{
	assert(!MCDialectBuilderHasError(self));
	assert(self -> state_count > 0);
	return MCDialectBuilderAppendStateToState(self, self -> states[self -> state_count - 1], p_state);
}

//////////

static bool MCDialectBuilderPushNewState(MCDialectBuilderRef self, MCDialectStateType p_state_type, ...)
{
	bool t_success;
	
	va_list t_args;
	va_start(t_args, p_state_type);
	t_success = MCDialectBuilderPushNewStateV(self, p_state_type, t_args);
	va_end(t_args);
	
	return t_success;
}

static bool MCDialectBuilderPushNewStateV(MCDialectBuilderRef self, MCDialectStateType p_state_type, va_list p_args)
{
	assert(!MCDialectBuilderHasError(self));
	
	MCAutoDialectStateRef t_new_state;
	if (!MCDialectBuilderNewStateV(self, &t_new_state, p_state_type, p_args))
		return false;
	
	return MCDialectBuilderPushState(self, *t_new_state);
}

static bool MCDialectBuilderAppendNewState(MCDialectBuilderRef self, MCDialectStateType p_state_type, ...)
{
	bool t_success;
	
	va_list t_args;
	va_start(t_args, p_state_type);
	t_success = MCDialectBuilderAppendNewStateV(self, p_state_type, t_args);
	va_end(t_args);
	
	return t_success;
}

static bool MCDialectBuilderAppendNewStateV(MCDialectBuilderRef self, MCDialectStateType p_state_type, va_list p_args)
{
	assert(!MCDialectBuilderHasError(self));
	
	MCAutoDialectStateRef t_new_state;
	if (!MCDialectBuilderNewStateV(self, &t_new_state, p_state_type, p_args))
		return false;
	
	return MCDialectBuilderAppendState(self, *t_new_state);
}

////////////////////////////////////////////////////////////////////////////////

static bool MCDialectBuilderPopRootState(MCDialectBuilderRef self, MCDialectStateRef& r_state)
{
	assert(!MCDialectBuilderHasError(self));
	
	if (self -> state_count != 1)
		return MCDialectBuilderThrow(self, kMCDialectBuilderErrorPatternNotFinished);
	
	r_state = self -> states[0];
	self -> state_count = 0;
	
	return true;
}

static bool MCDialectBuilderPopState(MCDialectBuilderRef self, MCDialectStateRef& r_state)
{
	assert(!MCDialectBuilderHasError(self));
	
	if (self -> state_count < 2)
		return MCDialectBuilderThrow(self, kMCDialectBuilderErrorUnbalancedPattern);
	
	r_state = self -> states[self -> state_count - 1];
	self -> state_count -= 1;
	
	return true;
}

static bool MCDialectBuilderPopStateOfType(MCDialectBuilderRef self, MCDialectStateType p_state_type, MCDialectStateRef& r_state)
{
	assert(!MCDialectBuilderHasError(self));
	
	MCDialectStateRef t_top_state;
	t_top_state = self -> states[self -> state_count - 1];
	
	if (MCDialectStateGetType(t_top_state) != p_state_type)
		return MCDialectBuilderThrow(self, kMCDialectBuilderErrorMismatchedPattern);
	
	r_state = t_top_state;
	self -> state_count -= 1;
	
	return true;
}

////////////////////////////////////////////////////////////////////////////////

static bool MCDialectBuilderBeginNest(MCDialectBuilderRef self, MCDialectStateType p_state_type)
{
	// If we are in an error state, then do nothing.
	if (MCDialectBuilderHasError(self))
		return false;
	
	// If we are not in a phrase, it's an error.
	if (!MCDialectBuilderCheckPhraseInProgress(self))
		return false;
	
	// Push a new alternation state.
	if (!MCDialectBuilderPushNewState(self, p_state_type, 0))
		return false;
}

static bool MCDialectBuilderContinueNest(MCDialectBuilderRef self, MCDialectStateType p_state_type)
{
	// If we are in an error state, then do nothing.
	if (MCDialectBuilderHasError(self))
		return false;
	
	// If we are not in a phrase, it's an error.
	if (!MCDialectBuilderCheckPhraseInProgress(self))
		return false;
	
	// Pop the top state, ensuring it is of the correct type.
	MCAutoDialectStateRef t_state;
	if (!MCDialectBuilderPopStateOfType(self, p_state_type, &t_state))
		return false;
	
	// Now push the state back on again.
	if (!MCDialectBuilderPushState(self, *t_state))
		return false;
}

static bool MCDialectBuilderEndNest(MCDialectBuilderRef self, MCDialectStateType p_state_type)
{
	// If we are in an error state, then do nothing.
	if (MCDialectBuilderHasError(self))
		return false;
	
	// If we are not in a phrase, it's an error.
	if (!MCDialectBuilderCheckPhraseInProgress(self))
		return false;
	
	// Pop the top state, ensuring it is of the correct type.
	MCAutoDialectStateRef t_state;
	if (!MCDialectBuilderPopStateOfType(self, p_state_type, &t_state))
		return false;
	
	// Append the state to the new top of the stack.
	if (!MCDialectBuilderAppendState(self, *t_state))
		return false;
}

////////////////////////////////////////////////////////////////////////////////
