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

// This file describes an abstract representation of the module format.
// It is based around an interface which can be used either to implement a
// reader, or to implement a writer.

typedef unsigned int MCScriptSize;
typedef unsigned int MCScriptIndex;
typedef unsigned int MCScriptRegister;
typedef signed int MCScriptOffset;

typedef bool MCScriptBool;
typedef int MCScriptInt;
typedef unsigned int MCScriptUInt;
typedef float MCScriptFloat;
typedef double MCScriptDouble;
typedef unsigned char MCScriptByte;
typedef unsigned short MCScriptChar;

struct MCScriptString
{
    MCScriptSize length;
    MCScriptChar *chars;
};

struct MCScriptData
{
    MCScriptSize length;
    MCScriptByte *bytes;
};

struct MCScriptVersion
{
};

struct MCScriptName
{
    MCScriptName *base;
    MCScriptString *name;
};

struct MCScriptRegisterList
{
    MCScriptSize length;
    MCScriptRegister *entries;
};

enum MCScriptImportKind
{
    kMCScriptImportKindType,
    kMCScriptImportKindConstant,
    kMCScriptImportKindVariable,
    kMCScriptImportKindHandler,
    kMCScriptImportKindWidget,
};

enum MCScriptParameterMode
{
    kMCScriptParameterModeIn,
    kMCScriptParameterModeOut,
    kMCScriptParameterModeInOut,
};

struct MCScriptModuleFormat
{
    // The BeginModule / EndModule pair bracket a single module definition.
    virtual bool BeginModule(const MCScriptName *name) = 0;
    virtual bool EndModule(void) = 0;
    
    // The BeginImport / EndImport pair bracket a set of imports from a single module.
    virtual bool BeginImport(const MCScriptName *module_name, const MCScriptVersion *module_version) = 0;
    virtual bool EndImport(void) = 0;
    
    // The Import method describes an import from the currently importing module.
    virtual bool Import(MCScriptImportKind kind, const MCScriptName *name) = 0;
    
    // The DeclareHandler method defines <index> to be a handler. The actual
    // definition will follow in order. This is to allow mutually recursive handlers
    // to be defined, whilst still allowing a module to be created in memory
    // serially.
    virtual bool DeclareHandler(MCScriptIndex index, MCScriptIndex signature) = 0;
    
    virtual bool BeginHandlerType(MCScriptIndex result_type) = 0;
    virtual bool EndHandlerType(void) = 0;
    
    virtual bool DefineParameter(MCScriptParameterMode mode, MCScriptIndex type) = 0;
    
    virtual bool DefineOptionalType(MCScriptIndex base_type) = 0;
    
    virtual bool DefineForeignType(MCScriptIndex base_type, const MCScriptString *binding) = 0;
    
    virtual bool DefineAliasType(MCScriptIndex base_type) = 0;
    
    virtual bool BeginWidget(MCScriptIndex base_widget) = 0;
    virtual bool EndWidget(void) = 0;
    
    virtual bool BeginConstant(MCScriptIndex type) = 0;
    virtual bool EndConstant(void) = 0;
    
    virtual bool DefineVariable(MCScriptIndex type) = 0;
    
    virtual bool BeginHandler(MCScriptIndex signature) = 0;
    virtual bool EndHandler(void) = 0;
    
    virtual bool DefineForeignHandler(MCScriptIndex signature, const MCScriptString *binding) = 0;
    
    virtual bool Export(MCScriptIndex index, const MCScriptName *name) = 0;
    virtual bool ExportWithVersion(MCScriptIndex index, const MCScriptName *name, const MCScriptVersion *version) = 0;
    
    /////
    
    virtual bool ExecJump(MCScriptRegister cond_reg, MCScriptOffset inst_delta) = 0;
    virtual bool ExecJumpIfFalse(MCScriptRegister cond_reg, MCScriptOffset inst_delta) = 0;
    virtual bool ExecJumpIfTrue(MCScriptRegister cond_reg, MCScriptOffset inst_delta) = 0;
    
    virtual bool ExecAssignNull(MCScriptRegister dst_reg) = 0;
    virtual bool ExecAssignBool(MCScriptRegister dst_reg, MCScriptBool value) = 0;
    virtual bool ExecAssignInt(MCScriptRegister dst_reg, MCScriptInt value) = 0;
    virtual bool ExecAssignUInt(MCScriptRegister dst_reg, MCScriptUInt value) = 0;
    virtual bool ExecAssignFloat(MCScriptRegister dst_reg, MCScriptFloat value) = 0;
    virtual bool ExecAssignDouble(MCScriptRegister dst_reg, MCScriptDouble value) = 0;
    virtual bool ExecAssignString(MCScriptRegister dst_reg, const MCScriptString *value) = 0;
    virtual bool ExecAssignData(MCScriptRegister dst_reg, const MCScriptData *value) = 0;
    
    virtual bool ExecCopy(MCScriptRegister dst_reg, MCScriptRegister src_reg) = 0;
    virtual bool ExecMove(MCScriptRegister dst_reg, MCScriptRegister src_reg) = 0;
    
    virtual bool ExecReturn(MCScriptRegister val_reg) = 0;
    virtual bool ExecReturnNothing(void) = 0;
    
    virtual bool ExecInvoke(MCScriptIndex index, MCScriptRegister result_reg, const MCScriptRegisterList *args) = 0;
    virtual bool ExecInvokeIndirect(MCScriptRegister handler_reg, MCScriptRegister result_reg, const MCScriptRegisterList *args) = 0;
    
    virtual bool ExecFetch(MCScriptIndex index, MCScriptRegister dst_reg) = 0;
    virtual bool ExecStore(MCScriptIndex index, MCScriptRegister src_reg) = 0;
};

/////

enum MCScriptMetaKind
{
    kMCScriptMetaKindModule,
    kMCScriptMetaKindType,
    kMCScriptMetaKindHandler,
    kMCScriptMetaKindVariable,
    kMCScriptMetaKindConstant,
    kMCScriptMetaKindWidget,
};

struct MCScriptMetaInfo
{
    MCScriptMetaKind kind;
    MCScriptMetaInfo *owner;
};

struct MCScriptTypeInfo: public MCScriptMetaInfo
{
    
};

struct MCScriptModuleInfo: public MCScriptMetaInfo
{
    MCNameRef name;
    uint32_t slot_count;
    MCScriptTypeInfo *slot_types;
    
    
};

struct MCScriptConstantInfo: public MCScriptMetaInfo
{
};

struct MCScriptVariableInfo: public MCScriptMetaInfo
{
    uint32_t slot_index;
};

struct MCScriptHandlerInfo: public MCScriptMetaInfo
{
    MCNameRef name;
    MCScriptTypeInfo *signature;
    uint32_t variable_count;
    MCScriptTypeInfo *variable_types;
    uint32_t temporary_count;
};
