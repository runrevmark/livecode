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

#include "script.h"
#include "script-private.h"

struct MCScriptFrame
{
    // A pointer to the calling frame, or nil if there is no outer frame.
    MCScriptFrame *caller;
    
    // The environment in which the frame is executing. This can be a module, an
    // instance or a (split) scope.
    MCScriptObjectRef environment;
    
    // The handler being executed in this frame.
    MCScriptHandlerDefinition *handler;
    
    // The pointer to the currently executing instruction.
    byte_t *address;
    
    // The slots for the current handler invocation.
    MCValueRef *slots;
};

// The MCScriptFrameRun function executes the code in the handler in the given
// frame until it returns.
bool MCScriptFrameRun(MCScriptFrame *self, MCValueRef& r_result)
{
    // STATIC-CHECK: The scope of the handler must match the environment chain.
    // STATIC-CHECK: The address must be within the address range of the handler.
    // STATIC-CHECK: The parameter values in the slots must be of the correct type.
    // STATIC-CHECK: All other slot values must be kMCNull.
    
    // The module which the handler in the current frame is in.
    MCScriptModuleRef t_module;
    t_module = __MCScriptEnvironmentGetModule(self -> environment);
    
execute:
    byte_t *t_next_bytecode;
    MCScriptBytecodeOp t_op;
    uindex_t t_arity;
    uindex_t t_arguments[256];
    
    // Decode the bytecode operation at the current address.
    t_next_bytecode = __MCScriptBytecodeDecode(self -> address, t_op, t_arity, t_arguments);
    
    // Perform the appropriate operation.
    switch(t_op)
    {
        case kMCScriptBytecodeOpJump:
        {
            // jump <offset>
            uindex_t t_encoded_offset;
            t_encoded_offset = t_arguments[0];
            
            // The offset field is encoded as a uint, we must decode.
            int t_offset;
            t_offset = __MCScriptBytecodeDecodeOffset(t_encoded_offset);
            
            // STATIC-CHECK: current_offset + offset is within handler
            
            // Compute the next (jumped to) instruction.
            t_next_bytecode = self -> address + t_offset;
        }
        break;
            
        case kMCScriptBytecodeOpJumpIfTrue:
        {
            // jumpiftrue <condition_reg>, <offset>
            uindex_t t_condition_reg, t_encoded_offset;
            t_condition_reg = t_arguments[0];
            t_encoded_offset = t_arguments[1];
            
            // STATIC-CHECK: condition_reg is valid
            
            // The offset field is encoded as a uint, we must decode.
            int t_offset;
            t_offset = __MCScriptBytecodeDecodeOffset(t_encoded_offset);
            
            // STATIC-CHECK: current_offset + offset is within handler
            
            // Fetch the condition value from the register.
            MCValueRef t_value;
            if (!__MCScriptFrameGetRegister(self, t_condition_reg, t_value))
                goto error;
            
            // If the value is not a boolean, it is an error.
            if (MCValueGetTypeCode(t_value) != kMCValueTypeCodeBoolean)
            {
                MCScriptThrowNotABooleanError(t_value);
                goto error;
            }
            
            // If the value is not true, then just advance to the next instruction.
            if (t_value != kMCTrue)
                break;
            
            // Compute the next (jumped to) instruction.
            t_next_bytecode = self -> address + t_offset;
        }
        break;
            
        case kMCScriptBytecodeOpJumpIfFalse:
        {
            // jumpiffalse <condition_reg>, <offset>
            uindex_t t_condition_reg, t_encoded_offset;
            t_condition_reg = t_arguments[0];
            t_encoded_offset = t_arguments[1];
            
            // STATIC-CHECK: condition_reg is valid
            
            int t_offset;
            t_offset = __MCScriptBytecodeDecodeOffset(t_encoded_offset);
            
            // STATIC-CHECK: current_offset + offset is within handler
            
            // Fetch the condition value from the register.
            MCValueRef t_value;
            if (!__MCScriptFrameGetRegister(self, t_condition_reg, t_value))
                goto error;
            
            // If the value is not a boolean, it is an error.
            if (MCValueGetTypeCode(t_value) != kMCValueTypeCodeBoolean)
            {
                MCScriptThrowNotABooleanError(t_value);
                goto error;
            }
            
            // If the value is not false, then just advance to the next instruction.
            if (t_value != kMCFalse)
                break;
            
            t_next_bytecode = self -> address + t_offset;
        }
        break;
            
        case kMCScriptBytecodeOpAssignConstant:
        {
            // assignconst <dst_reg>, <constant_idx>
            uindex_t t_dst_reg, t_constant_idx;
            t_dst_reg = t_arguments[0]
            t_constant_idx = t_arguments[1];
            
            // STATIC-CHECK: dst_reg is valid
            // STATIC-CHECK: constant_idx is valid
            
            // Fetch the constant from the module.
            MCValueRef t_value;
            t_value = __MCScriptModuleGetConstant(t_module, t_constant_idx);
            
            // Set the destination register to its value.
            if (!__MCScriptFrameSetRegister(t_frame, t_dst_reg, t_value))
                goto error;
        }
        break;
            
        case kMCScriptBytecodeOpAssign:
        {
            // assign <dst_reg>, <src_reg>
            uindex_t t_dst_reg, t_src_reg;
            t_dst_reg = t_arguments[0];
            t_src_reg = t_arguments[1];
            
            // STATIC-CHECK: dst_reg is valid
            // STATIC-CHECK: src_reg is valid
            
            // Fetch the value from the source register.
            MCValueRef t_value;
            if (!__MCScriptFrameGetRegister(t_frame, t_src_reg, t_value))
                goto error;
            
            // Set the destination register to its value.
            if (!__MCScriptFrameSetRegister(t_frame, t_dst_reg, t_value))
                goto error;
        }
        break;
            
        case kMCScriptBytecodeOpReturn:
        {
            // return [<reg>]
            
            // The value to return is either the value from the register if present,
            // or otherwise undefined.
            MCValueRef t_return_value;
            if (t_arity != 0)
            {
                uindex_t t_return_reg;
                t_return_reg = t_arguments[0];
                
                // STATIC-CHECK: return_reg is valid
                
                if (!__MCScriptFrameGetRegister(t_frame, t_return_reg, t_return_value))
                    goto error;
            }
            else
                t_return_value = kMCNull;
            
            //// CALLEE CONTEXT
            
            // The first part of the return operation occurs in the context of the
            // callee frame (i.e. self).
            
            // Get the signature of the handler we are returning from.
            MCTypeInfoRef t_signature;
            t_signature = __MCScriptModuleGetType(t_module, self -> handler -> type);
            
            // Check that the return type is correct.
            if (!MCTypeInfoConforms(MCValueGetTypeInfo(t_return_value), MCHandlerTypeInfoGetReturnType(t_signature)))
            {
                MCScriptThrowInvalidValueForResultError(t_module, self -> handler, t_return_value);
                goto error;
            }
            
            // Check that all out variables which are undefined have an optional type.
            for(uindex_t t_param = 0; t_param < MCHandlerTypeInfoGetParameterCount(t_signature); t_param += 1)
                if (MCHandlerTypeInfoGetParameterMode(t_signature, t_param) == kMCHandlerTypeFieldModeOut &&
                    self -> slots[t_param] == kMCNull &&
                    !MCTypeInfoResolvesToOptional(MCHandlerTypeInfoGetParameterType(t_signature, t_param)))
                {
                    MCScriptThrowOutParameterNotDefinedError(t_module, self -> handler, t_param);
                    goto error;
                }
            
            // If there is no caller, then nothing to copy-back - the caller will
            // process the frame's slots appropriately.
            if (self -> caller == nil)
            {
                r_result = MCValueRetain(t_return_value);
                goto done;
            }
            
            //// CALLER CONTEXT
            
            // The remainder of the return operation occurs in the context of the
            // caller frame (i.e. self -> caller).
            
            // Keep the callee frame around.
            MCScriptFrame *t_callee;
            t_callee = self;
            
            // Shift ourselves back to the caller frame.
            self = self -> caller;
            
            // We are transitioning back to the calling frame which might be in a
            // different module so we must recompute that field.
            t_module = __MCScriptEnvironmentGetModule(self -> environment);
            
            // Decode the original invoke so we can get the register mapping.
            t_next_bytecode = __MCScriptBytecodeDecode(self -> address, t_op, t_arity, t_arguments);
            
            // Sort out the registers for result and arguments.
            uindex_t t_caller_return_reg, *t_caller_arg_regs;
            t_caller_return_reg = t_arguments[1];
            t_caller_arg_regs = t_arguments + 2;
            
            // Try to store the result - and make sure we free the callee frame if
            // there's an error.
            if (!__MCScriptFrameSetRegister(t_frame -> caller, t_caller_return_reg, t_return_value))
            {
                __MCScriptFrameDestroy(t_callee);
                goto error;
            }
            
            // Now try to store each argument - and make sure we free the callee frame
            // if there's an error.
            for(uindex_t i = 0; i < MCHandlerTypeInfoGetParameterCount(t_signature); i++)
                if (MCHandlerTypeInfoGetParameterMode(t_signature, i) != kMCHandlerTypeFieldModeIn)
                {
                    if (!__MCScriptFrameSetRegister(t_frame -> caller, t_caller_arg_regs[i], t_callee -> slots[i]))
                    {
                        __MCScriptFrameDestroy(t_callee);
                        goto error;
                    }
                }
            
            // We succeeded, so free the callee frame.
            __MCScriptFrameDestroy(t_callee);
        }
        break;
            
        case kMCScriptBytecodeOpInvoke:
        {
            // invoke <def_index>, <result_reg>, <arg_reg_1>, ..., <arg_reg_n>
            uindex_t t_def_index, t_result_reg, *t_arg_regs, t_arg_count;
            t_def_index = t_arguments[0];
            t_result_reg = t_arguments[1];
            t_arg_regs = &t_arguments[2];
            t_arg_count = t_arity - 2;
            
            // STATIC-CHECK: def_index must resolve to a callable definition.
            // STATIC-CHECK: def_index must resolve to a definition which conforms
            //   to the current environment chain.
            // STATIC-CHECK: result_reg must be valid
            // STATIC-CHECK: arg_reg_n must be valid
            
            // Fetch the appropriate definition and environment - this will resolve a
            // polymorphic invoke so the resulting definition will always be a handler
            // of some kind.
            MCScriptObjectRef t_environment;
            MCScriptDefinition *t_definition;
            if (!__MCScriptFrameResolveDefinition(self, t_module, t_def_index, t_environment, t_definition))
                goto error;
            
            // Foreign handler calls don't require building a new frame - they act
            // directly on the existing frame - so we handle them slightly differently.
            if (t_definition -> kind == kMCScriptDefinitionKindForeignHandler)
            {
                if (!__MCScriptFramePerformForeignInvoke(self, t_environment, t_definition, t_result_reg, t_arg_regs, t_arg_count))
                    goto error;
                break;
            }
            
            // We have an internal invoke to do. These don't recurse, instead the perform
            // mutates the frame and other state we have so we can execute iteratively.
            if (!__MCScriptFramePerformInternalInvoke(self, t_module, t_environment, t_definition, t_result_reg, t_arg_regs, t_arg_count))
                goto error;
        
            // The next bytecode to execute is the address in the frame.
            t_next_bytecode = self -> address;
        }
        break;
            
        case kMCScriptBytecodeOpInvokeIndirect:
        {
            // invoke *<src_reg>, <result_reg>, <arg_reg_1>, ..., <arg_reg_n>
            uindex_t t_src_reg, t_result_reg, *t_arg_regs, t_arg_count;
            t_def_index = t_arguments[0];
            t_result_reg = t_arguments[1];
            t_arg_regs = &t_arguments[2];
            t_arg_count = t_arity - 2;
            
            // STATIC-CHECK: src_reg must be valid
            // STATIC-CHECK: result_reg must be valid
            // STATIC-CHECK: arg_reg_n must be valid
            
            // Fetch the value to attempt to execute.
            MCValueRef t_handler;
            if (!__MCScriptFrameGetRegister(self, t_src_reg))
                goto error;
            
            // If the value is not of handler type then we can do nothing with it.
            if (MCValueGetTypeCode(t_handler) != kMCValueTypeCodeHandler)
            {
                MCScriptThrowNotAHandlerValueError(t_handler);
                goto error;
            }
            
            // If the handler value is not one we made we cannot short-circuit it.
            if (MCHandlerGetCallbacks((MCHandlerRef)t_handler) != &__kMCScriptHandlerCallbacks)
            {
                if (!__MCScriptFramePerformExternalInvoke(self, (MCHandlerRef)t_handler, t_result_reg, t_arg_regs, t_arg_count))
                    goto error;
                break;
            }
            
            // Otherwise unpack its context and perform directly.
            __MCScriptHandlerContext *t_context;
            t_context = (__MCScriptHandlerContext *)MCHandlerGetContext((MCHandlerRef)t_handler);
            
            // If the definition is a foreign handler then deal with that in a different way.
            if (t_context -> definition -> kind == kMCSCriptDefinitionKindForeignHandler)
            {
                if (!__MCScriptFramePerformForeignInvoke(self, t_context -> environment, t_context -> definition, t_result_reg, t_arg_regs, t_arg_count))
                    goto error;
                break;
            }
            
            // Otherwise it is an internal invoke.
            if (!__MCScriptFramePerformInternalInvoke(self, t_module, t_environment, t_definition, t_result_reg, t_arg_regs, t_arg_count))
                goto error;
            
            // The next bytecode to execute is the address in the frame.
            t_next_bytecode = self -> address;
        }
        break;
            
        case kMCScriptBytecodeOpFetch:
        {
            // fetch <dst_reg>, <def_index>
            uindex_t t_dst_reg, t_def_index;
            t_dst_reg = t_arguments[0];
            t_def_index = t_arguments[1];
            
            // STATIC-CHECK: dst_reg must be valid
            // STATIC-CHECK: def_index must resolve to a evaluatable definition.
            // STATIC-CHECK: def_index must resolve to a definition which conforms
            //   to the current environment chain.
            
            // The environment
            MCScriptObjectRef t_environment;
            MCScriptDefinition *t_definition;
            if (!__MCScriptFrameResolveDefinition(self, t_module, t_def_index, t_environment, t_definition))
                goto error;
            
            // If the definition is a foreign handler, then first attempt to prepare it.
            // If that fails, then we must return undefined.
            if (t_definition -> kind == kMCScriptDefinitionKindForeignHandler)
            {
                MCScriptForeignHandlerDefinition *t_foreign_handler;
                t_foreign_handler = static_cast<MCScriptForeignHandlerDefinition *>(t_definition);
                
                // We pass 'false' to throw - this means it will only return an error
                // if binding failed for some reason unrelated to not being able to
                // find the appropriate function.
                if (!__MCScriptFramePrepareForeignInvoke(self, t_environment, t_foreign_handler, false))
                    goto error;
                
                if (t_foreign_handler -> function == nil)
                {
                    if (!__MCScriptFrameSetRegister(self, t_dst_reg, kMCNull))
                        goto error;
                    
                    break;
                }
            }
            
            // If the definition is a normal handler, then we build a handler value.
            if (t_definition -> kind == kMCScriptDefinitionKindHandler ||
                t_definition -> kind == kMCScriptDefinitionKindForeignHandler)
            {
                MCScriptCommonHandlerDefinition *t_handler;
                t_handler = static_cast<MCScriptCommonHandlerDefinition *>(t_definition);
                
                // Fetch the signature of the handler - this will be found in the
                // module of the environment.
                MCTypeInfoRef t_signature;
                t_signature = __MCScriptEnvironmentGetType(t_environment, t_handler -> type);
                
                // Now build a context struct - this is 'moved' into the handler value.
                __MCScriptHandlerContext t_context;
                t_context . environment = MCScriptObjectRetain(t_environment);
                t_context . definition = t_handler;
                
                // Now create the handler value.
                MCHandlerRef t_value;
                if (!MCHandlerCreate(t_signature, &__kMCScriptHandlerCallbacks, &t_context, t_value))
                {
                    MCScriptObjectRelease(t_context . t_environment);
                    goto error;
                }
                
                if (!__MCScriptFrameSetRegisterAndRelease(self, t_dst_reg, t_value))
                {
                    MCValueRelease(t_value);
                    goto error;
                }
                
                break;
            }
            
            // Otherwise the definition must be a variable.
            MCScriptVariableDefinition *t_variable;
            t_variable = static_cast<MCScriptVariableDefinition *>(t_definition);
            
            // Fetch the slot.
            MCValueRef t_value;
            if (!__MCScriptEnvironmentGetSlot(t_environment, t_variable -> slot_index))
                goto error;
            
            // And store it in the register.
            if (!__MCScriptFrameSetRegister(self, t_dst_reg, t_value))
                goto error;
        }
        break;
    }
    
error:
}