#pragma once

namespace T4 { namespace engine {
	WEAK symbol<void(scriptInstance_t inst, VariableValue* value)>RemoveRefToValue{ "RemoveRefToValue" };
	WEAK symbol<void(scriptInstance_t inst, OpcodeVM op, int offset, int callType)>EmitOpcode{ "EmitOpcode" };
	WEAK symbol<int(scriptInstance_t inst, unsigned int name, sval_u sourcePos, int create, scr_block_s* block)>Scr_FindLocalVarIndex{ "Scr_FindLocalVarIndex" };
	WEAK symbol<void(scriptInstance_t inst, scr_block_s* block)>EmitCreateLocalVars{ "EmitCreateLocalVars" };
	WEAK symbol<void(scr_block_s** childBlocks, int childCount, scr_block_s* block)>Scr_InitFromChildBlocks{ "Scr_InitFromChildBlocks" };
	WEAK symbol<void(scr_block_s** childBlocks, int childCount, scr_block_s* block)>Scr_MergeChildBlocks{ "Scr_MergeChildBlocks" };
	WEAK symbol<void(scriptInstance_t inst, VariableCompileValue* constValue)>EmitValue{ "EmitValue" };
	WEAK symbol<void(unsigned int name, sval_u sourcePos, scr_block_s* block)>Scr_RegisterLocalVar{ "Scr_RegisterLocalVar" };
	WEAK symbol<void(scriptInstance_t inst, sval_u sourcePos)>EmitCastFieldObject{ "EmitCastFieldObject" };
	WEAK symbol<void(scriptInstance_t inst, sval_u expr, scr_block_s* block)>EmitVariableExpression{ "EmitVariableExpression" };
	WEAK symbol<int(scriptInstance_t inst, sval_u exprlist, scr_block_s* block)>EmitExpressionList{ "EmitExpressionList" };
	WEAK symbol<int(scriptInstance_t inst, int func)>AddFunction{ "AddFunction" };
	WEAK symbol<void(scriptInstance_t inst, sval_u func, int param_count, int bMethod, sval_u nameSourcePos)>EmitPostScriptFunction{ "EmitPostScriptFunction" };
	WEAK symbol<void(scriptInstance_t inst, sval_u expr, sval_u func_name, sval_u params, sval_u methodSourcePos, int bStatement, scr_block_s* block)>EmitMethod{ "EmitMethod" };
	WEAK symbol<void(scriptInstance_t inst, unsigned int posId, unsigned int name, unsigned int sourcePos)>CheckThreadPosition{ "CheckThreadPosition" };
	WEAK symbol<bool(scriptInstance_t inst, sval_u exprlist, sval_u sourcePos, VariableCompileValue* constValue)>EvalPrimitiveExpressionList{ "EvalPrimitiveExpressionList" };
	WEAK symbol<bool(scriptInstance_t inst, sval_u expr, VariableCompileValue* constValue)>EvalPrimitiveExpression{ "EvalPrimitiveExpression" };
	WEAK symbol<bool(scriptInstance_t inst, sval_u expr, VariableCompileValue* constValue, scr_block_s* block)>EmitOrEvalPrimitiveExpression{ "EmitOrEvalPrimitiveExpression" };
	WEAK symbol<bool(scriptInstance_t inst, sval_u expr1, sval_u expr2, sval_u opcode, sval_u sourcePos, VariableCompileValue* constValue)>EvalBinaryOperatorExpression{ "EvalBinaryOperatorExpression" };
	WEAK symbol<bool(scriptInstance_t inst, sval_u expr, VariableCompileValue* constValue, scr_block_s* block)>EmitOrEvalExpression{ "EmitOrEvalExpression" };
	WEAK symbol<void(scriptInstance_t inst, sval_u expr, scr_block_s* block)>EmitExpression{ "EmitExpression" };
	WEAK symbol<void(sval_u expr, scr_block_s* block)>Scr_CalcLocalVarsArrayVariableRef{ "Scr_CalcLocalVarsArrayVariableRef" };
	WEAK symbol<void(scriptInstance_t inst, sval_u expr, sval_u sourcePos, scr_block_s* block)>EmitPrimitiveExpressionFieldObject{ "EmitPrimitiveExpressionFieldObject" };
	WEAK symbol<void(scriptInstance_t inst, sval_u stmt, scr_block_s* block, sval_u* ifStatBlock)>Scr_CalcLocalVarsIfStatement{ "Scr_CalcLocalVarsIfStatement" };
	WEAK symbol<void(scriptInstance_t inst, sval_u expr, sval_u stmt1, sval_u stmt2, sval_u sourcePos, sval_u elseSourcePos, int lastStatement, unsigned int endSourcePos, scr_block_s* block, sval_u* ifStatBlock, sval_u* elseStatBlock)>EmitIfElseStatement{ "EmitIfElseStatement" };
	WEAK symbol<void(scriptInstance_t inst, sval_u stmt1, sval_u stmt2, scr_block_s* block, sval_u* ifStatBlock, sval_u* elseStatBlock)>Scr_CalcLocalVarsIfElseStatement{ "Scr_CalcLocalVarsIfElseStatement" };
	WEAK symbol<void(scriptInstance_t inst, sval_u expr, sval_u stmt, sval_u sourcePos, sval_u whileSourcePos, scr_block_s* block, sval_u* whileStatBlock)>EmitWhileStatement{ "EmitWhileStatement" };
	WEAK symbol<void(scriptInstance_t inst, sval_u expr, sval_u stmt, scr_block_s* block, sval_u* whileStatBlock)>Scr_CalcLocalVarsWhileStatement{ "Scr_CalcLocalVarsWhileStatement" };
	WEAK symbol<void(scriptInstance_t inst, sval_u stmt1, sval_u expr, sval_u stmt2, sval_u stmt, sval_u sourcePos, sval_u forSourcePos, scr_block_s* block, sval_u* forStatBlock, sval_u* forStatPostBlock)>EmitForStatement{ "EmitForStatement" };
	WEAK symbol<void(scriptInstance_t inst, sval_u stmt1, sval_u expr, sval_u stmt2, sval_u stmt, scr_block_s* block, sval_u* forStatBlock, sval_u* forStatPostBlock)>Scr_CalcLocalVarsForStatement{ "Scr_CalcLocalVarsForStatement" };
	WEAK symbol<int(const void* elem1, const void* elem2)>CompareCaseInfo{ "CompareCaseInfo" };
	WEAK symbol<void(scriptInstance_t inst, sval_u val, int lastStatement, unsigned int endSourcePos, scr_block_s* block)>EmitSwitchStatementList{ "EmitSwitchStatementList" };
	WEAK symbol<void(scriptInstance_t inst, sval_u stmtlist, scr_block_s* block)>Scr_CalcLocalVarsSwitchStatement{ "Scr_CalcLocalVarsSwitchStatement" };
	WEAK symbol<void(scriptInstance_t inst, sval_u expr, sval_u stmtlist, sval_u sourcePos, int lastStatement, unsigned int endSourcePos, scr_block_s* block)>EmitSwitchStatement{ "EmitSwitchStatement" };
	WEAK symbol<void(scriptInstance_t inst, sval_u val, int lastStatement, unsigned int endSourcePos, scr_block_s* block)>EmitStatement{ "EmitStatement" };
	WEAK symbol<void(scriptInstance_t inst, sval_u val, scr_block_s* block)>Scr_CalcLocalVarsStatement{ "Scr_CalcLocalVarsStatement" };
	WEAK symbol<void(scriptInstance_t inst, sval_u val, int lastStatement, unsigned int endSourcePos, scr_block_s* block)>EmitStatementList{ "EmitStatementList" };
	WEAK symbol<void(scriptInstance_t inst, sval_u val, scr_block_s* block, sval_u* devStatBlock)>Scr_CalcLocalVarsDeveloperStatementList{ "Scr_CalcLocalVarsDeveloperStatementList" };
	WEAK symbol<void(scriptInstance_t inst, sval_u val, sval_u sourcePos, scr_block_s* block, sval_u* devStatBlock)>EmitDeveloperStatementList{ "EmitDeveloperStatementList" };
	WEAK symbol<void(scriptInstance_t inst, sval_u val)>EmitThreadList{ "EmitThreadList" };
	WEAK symbol<void(scriptInstance_t inst, sval_u expr, scr_block_s* block)>EmitVariableExpressionRef{ "EmitVariableExpressionRef" };

	// WaW sub_67EB90 — usercall(inst@eax, stringVal@edx) -> void ; no stack
	inline void Scr_CompileRemoveRefToString(scriptInstance_t inst, unsigned int stringVal)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_CompileRemoveRefToString"));
		__asm
		{
			mov   eax, inst
			mov   edx, stringVal
			call  fn
		}
	}

	// WaW sub_67EBB0 — usercall(inst@ecx, stringVal@eax) -> void ; no stack
	inline void EmitCanonicalString(scriptInstance_t inst, unsigned int stringVal)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitCanonicalString"));
		__asm
		{
			mov   ecx, inst
			mov   eax, stringVal
			call  fn
		}
	}

	// WaW sub_67EC30 — usercall(stringValue@eax, inst@ecx, user@stack0) -> void ; caller-cleans
	inline void CompileTransferRefToString(unsigned int stringValue, scriptInstance_t inst, unsigned int user)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("CompileTransferRefToString"));
		__asm
		{
			push  user
			mov   eax, stringValue
			mov   ecx, inst
			call  fn
			add   esp, 4
		}
	}

	// WaW sub_67F0C0 — usercall(inst@eax) -> void ; no stack
	inline void EmitEnd(scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitEnd"));
		__asm
		{
			mov   eax, inst
			call  fn
		}
	}

	// WaW sub_67F1A0 — usercall(inst@eax) -> void ; no stack
	inline void EmitReturn(scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitReturn"));
		__asm
		{
			mov   eax, inst
			call  fn
		}
	}

	// WaW sub_67F290 — usercall(inst@eax, codepos@stack0) -> void ; caller-cleans
	inline void EmitCodepos(scriptInstance_t inst, int codepos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitCodepos"));
		__asm
		{
			push  codepos
			mov   eax, inst
			call  fn
			add   esp, 4
		}
	}

	// WaW sub_67F2C0 — usercall(inst@eax, value(word)@stack0) -> void ; caller-cleans
	inline void EmitShort(scriptInstance_t inst, int value)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitShort"));
		__asm
		{
			push  value
			mov   eax, inst
			call  fn
			add   esp, 4
		}
	}

	// WaW sub_67F2F0 — usercall(inst@eax, value(byte)@stack0) -> void ; caller-cleans
	inline void EmitByte(scriptInstance_t inst, int value)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitByte"));
		__asm
		{
			push  value
			mov   eax, inst
			call  fn
			add   esp, 4
		}
	}

	// WaW sub_67F320 — usercall(inst@eax, value@stack0, sourcePos@stack1) -> void ; caller-cleans
	inline void EmitGetInteger(scriptInstance_t inst, int value, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitGetInteger"));
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			push  value
			mov   eax, inst
			call  fn
			add   esp, 8
		}
	}

	// WaW sub_67F470 — usercall(inst@eax, value(float)@stack0, sourcePos@stack1) -> void ; caller-cleans
	inline void EmitGetFloat(scriptInstance_t inst, float value, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitGetFloat"));
		int vb = *reinterpret_cast<int*>(&value);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			push  vb
			mov   eax, inst
			call  fn
			add   esp, 8
		}
	}

	// WaW sub_67F4C0 — usercall(inst@eax, sourcePos@stack0) -> void ; caller-cleans
	inline void EmitAnimTree(scriptInstance_t inst, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitAnimTree"));
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			mov   eax, inst
			call  fn
			add   esp, 4
		}
	}

	// WaW sub_67F880 — usercall(inst@eax, outerBlock@ecx, block@stack0) -> void ; caller-cleans
	inline void EmitRemoveLocalVars(scriptInstance_t inst, scr_block_s* outerBlock, scr_block_s* block)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitRemoveLocalVars"));
		__asm
		{
			push  block
			mov   eax, inst
			mov   ecx, outerBlock
			call  fn
			add   esp, 4
		}
	}
	
	// WaW sub_67F9A0 — usercall(block@ecx, inst@edi, lastStatement@stack0, endSourcePos@stack1) -> void ; caller-cleans
	inline void EmitNOP2(scr_block_s* block, scriptInstance_t inst, int lastStatement, unsigned int endSourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitNOP2"));
		__asm
		{
			push  endSourcePos
			push  lastStatement
			mov   ecx, block
			mov   edi, inst
			call  fn
			add   esp, 8
		}
	}
	
	// WaW sub_67FAA0 — usercall(block@edi, childBlocks@stack0, childCount@stack1) -> void ; caller-cleans
	inline void Scr_AppendChildBlocks(scr_block_s* block, scr_block_s** childBlocks, int childCount)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_AppendChildBlocks"));
		__asm
		{
			push  childCount
			push  childBlocks
			mov   edi, block
			call  fn
			add   esp, 8
		}
	}
	
	// WaW sub_67FC60 — usercall(to@esi, from@stack0) -> void ; caller-cleans
	inline void Scr_TransferBlock(scr_block_s* to, scr_block_s* from)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_TransferBlock"));
		__asm
		{
			push  from
			mov   esi, to
			call  fn
			add   esp, 4
		}
	}
	
	// WaW sub_67FD50 — usercall(block@eax, inst@esi, expr@stack0, sourcePos@stack1) -> void ; caller-cleans
	inline void EmitSafeSetVariableField(scr_block_s* block, scriptInstance_t inst, sval_u expr, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitSafeSetVariableField"));
		int ex = *reinterpret_cast<int*>(&expr);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			push  ex
			mov   eax, block
			mov   esi, inst
			call  fn
			add   esp, 8
		}
	}
	
	// WaW sub_67FDC0 — usercall(block@eax, inst@edi, expr@stack0, sourcePos@stack1) -> void ; caller-cleans
	inline void EmitSafeSetWaittillVariableField(scr_block_s* block, scriptInstance_t inst, sval_u expr, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitSafeSetWaittillVariableField"));
		int ex = *reinterpret_cast<int*>(&expr);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			push  ex
			mov   eax, block
			mov   edi, inst
			call  fn
			add   esp, 8
		}
	}
	
	// WaW sub_67FEE0 — usercall(value@edi, inst@esi, sourcePos@stack0) -> void ; caller-cleans
	inline void EmitGetString(unsigned int value, scriptInstance_t inst, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitGetString"));
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			mov   edi, value
			mov   esi, inst
			call  fn
			add   esp, 4
		}
	}
	
	// WaW sub_67FF30 — usercall(value@edi, inst@esi, sourcePos@stack0) -> void ; caller-cleans
	inline void EmitGetIString(unsigned int value, scriptInstance_t inst, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitGetIString"));
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			mov   edi, value
			mov   esi, inst
			call  fn
			add   esp, 4
		}
	}
	
	// WaW sub_67FF80 — usercall(value@eax, inst@stack0, sourcePos@stack1) -> void ; caller-cleans
	inline void EmitGetVector(const float* value, scriptInstance_t inst, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitGetVector"));
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			push  inst
			mov   eax, value
			call  fn
			add   esp, 8
		}
	}
	
	// WaW sub_680120 — usercall(inst@eax, constValue@esi) -> void ; no stack args
	inline void Scr_PushValue(scriptInstance_t inst, VariableCompileValue* constValue)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_PushValue"));
		__asm
		{
			mov   eax, inst
			mov   esi, constValue
			call  fn
		}
	}
	
	// WaW sub_680180 — usercall(inst@edi, sourcePos@stack0) -> void ; caller-cleans
	inline void EmitCastBool(scriptInstance_t inst, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitCastBool"));
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			mov   edi, inst
			call  fn
			add   esp, 4
		}
	}
	
	// WaW sub_680270 — usercall(inst@edi, sourcePos@stack0) -> void ; caller-cleans
	inline void EmitBoolNot(scriptInstance_t inst, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitBoolNot"));
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			mov   edi, inst
			call  fn
			add   esp, 4
		}
	}
	
	// WaW sub_680360 — usercall(inst@edi, sourcePos@stack0) -> void ; caller-cleans
	inline void EmitBoolComplement(scriptInstance_t inst, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitBoolComplement"));
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			mov   edi, inst
			call  fn
			add   esp, 4
		}
	}
	
	// WaW sub_680450 — usercall(block@eax, inst@edi, expr@stack0, sourcePos@stack1) -> void ; caller-cleans
	inline void EmitSize(scr_block_s* block, scriptInstance_t inst, sval_u expr, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitSize"));
		int ex = *reinterpret_cast<int*>(&expr);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			push  ex
			mov   eax, block
			mov   edi, inst
			call  fn
			add   esp, 8
		}
	}
	
	// WaW sub_680560 — usercall(inst@edi, sourcePos@stack0) -> void ; caller-cleans
	inline void EmitSelf(scriptInstance_t inst, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitSelf"));
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			mov   edi, inst
			call  fn
			add   esp, 4
		}
	}
	
	// WaW sub_680660 — usercall(inst@edi, sourcePos@stack0) -> void ; caller-cleans
	inline void EmitLevel(scriptInstance_t inst, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitLevel"));
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			mov   edi, inst
			call  fn
			add   esp, 4
		}
	}
	
	// WaW sub_680760 — usercall(inst@edi, sourcePos@stack0) -> void ; caller-cleans
	inline void EmitGame(scriptInstance_t inst, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitGame"));
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			mov   edi, inst
			call  fn
			add   esp, 4
		}
	}
	
	// WaW sub_680860 — usercall(inst@edi, sourcePos@stack0) -> void ; caller-cleans
	inline void EmitAnim(scriptInstance_t inst, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitAnim"));
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			mov   edi, inst
			call  fn
			add   esp, 4
		}
	}
	
	// WaW sub_680960 — usercall(inst@edi, sourcePos@stack0) -> void ; caller-cleans
	inline void EmitSelfObject(scriptInstance_t inst, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitSelfObject"));
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			mov   edi, inst
			call  fn
			add   esp, 4
		}
	}
	
	// WaW sub_680A50 — usercall(inst@edi, sourcePos@stack0) -> void ; caller-cleans
	inline void EmitLevelObject(scriptInstance_t inst, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitLevelObject"));
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			mov   edi, inst
			call  fn
			add   esp, 4
		}
	}
	
	// WaW sub_680B40 — usercall(inst@edi, sourcePos@stack0) -> void ; caller-cleans
	inline void EmitAnimObject(scriptInstance_t inst, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitAnimObject"));
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			mov   edi, inst
			call  fn
			add   esp, 4
		}
	}
	
	// WaW sub_680C30 — usercall(block@eax, inst@esi, expr@stack0, sourcePos@stack1) -> void ; caller-cleans
	inline void EmitLocalVariable(scr_block_s* block, scriptInstance_t inst, sval_u expr, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitLocalVariable"));
		int ex = *reinterpret_cast<int*>(&expr);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			push  ex
			mov   eax, block
			mov   esi, inst
			call  fn
			add   esp, 8
		}
	}
	
	// WaW sub_680CA0 — usercall(block@eax, inst@esi, expr@stack0, sourcePos@stack1) -> void ; caller-cleans
	inline void EmitLocalVariableRef(scr_block_s* block, scriptInstance_t inst, sval_u expr, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitLocalVariableRef"));
		int ex = *reinterpret_cast<int*>(&expr);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			push  ex
			mov   eax, block
			mov   esi, inst
			call  fn
			add   esp, 8
		}
	}
	
	// WaW sub_680D60 — usercall(inst@edi, sourcePos@stack0) -> void ; caller-cleans
	inline void EmitGameRef(scriptInstance_t inst, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitGameRef"));
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			mov   edi, inst
			call  fn
			add   esp, 4
		}
	}
	
	// WaW sub_680E50 — usercall(inst@edi, sourcePos@stack0, indexSourcePos@stack1) -> void ; caller-cleans
	inline void EmitClearArray(scriptInstance_t inst, sval_u sourcePos, sval_u indexSourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitClearArray"));
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		int isp = *reinterpret_cast<int*>(&indexSourcePos);
		__asm
		{
			push  isp
			push  srcpos
			mov   edi, inst
			call  fn
			add   esp, 8
		}
	}
	
	// WaW sub_680F50 — usercall(inst@edi, sourcePos@stack0) -> void ; caller-cleans
	inline void EmitEmptyArray(scriptInstance_t inst, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitEmptyArray"));
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			mov   edi, inst
			call  fn
			add   esp, 4
		}
	}
	
	// WaW sub_681050 — usercall(inst@eax, anim@stack0, sourcePos@stack1) -> void ; caller-cleans
	inline void EmitAnimation(scriptInstance_t inst, sval_u anim, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitAnimation"));
		int an = *reinterpret_cast<int*>(&anim);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			push  an
			mov   eax, inst
			call  fn
			add   esp, 8
		}
	}
	
	// WaW sub_6811C0 — usercall(block@eax, inst@esi, expr@stack0, field@stack1, sourcePos@stack2) -> void ; caller-cleans
	inline void EmitFieldVariable(scr_block_s* block, scriptInstance_t inst, sval_u expr, sval_u field, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitFieldVariable"));
		int ex = *reinterpret_cast<int*>(&expr);
		int fd = *reinterpret_cast<int*>(&field);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			push  fd
			push  ex
			mov   eax, block
			mov   esi, inst
			call  fn
			add   esp, 0Ch
		}
	}
	
	// WaW sub_681200 — usercall(block@eax, inst@esi, expr@stack0, field@stack1, sourcePos@stack2, rhsSourcePos@stack3) -> void ; caller-cleans
	inline void EmitClearFieldVariable(scr_block_s* block, scriptInstance_t inst, sval_u expr, sval_u field, sval_u sourcePos, sval_u rhsSourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitClearFieldVariable"));
		int ex = *reinterpret_cast<int*>(&expr);
		int fd = *reinterpret_cast<int*>(&field);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		int rhssrc = *reinterpret_cast<int*>(&rhsSourcePos);
		__asm
		{
			push  rhssrc
			push  srcpos
			push  fd
			push  ex
			mov   eax, block
			mov   esi, inst
			call  fn
			add   esp, 10h
		}
	}
	
	// WaW sub_681310 — usercall(inst@edi, expr@stack0, sourcePos@stack1) -> void ; caller-cleans
	inline void EmitObject(scriptInstance_t inst, sval_u expr, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitObject"));
		int ex = *reinterpret_cast<int*>(&expr);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			push  ex
			mov   edi, inst
			call  fn
			add   esp, 8
		}
	}
	
	// WaW sub_681630 — usercall(inst@eax) -> void ; no stack args
	inline void EmitDecTop(scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitDecTop"));
		__asm
		{
			mov   eax, inst
			call  fn
		}
	}
	
	// WaW sub_681840 — usercall(block@edi, inst@esi, expr@stack0, index@stack1, sourcePos@stack2, indexSourcePos@stack3) -> void ; caller-cleans
	inline void EmitArrayVariable(scr_block_s* block, scriptInstance_t inst, sval_u expr, sval_u index, sval_u sourcePos, sval_u indexSourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitArrayVariable"));
		int ex = *reinterpret_cast<int*>(&expr);
		int ix = *reinterpret_cast<int*>(&index);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		int idxsrc = *reinterpret_cast<int*>(&indexSourcePos);
		__asm
		{
			push  idxsrc
			push  srcpos
			push  ix
			push  ex
			mov   edi, block
			mov   esi, inst
			call  fn
			add   esp, 10h
		}
	}
	
	// WaW sub_6818C0 — usercall(block@eax, inst@esi, expr@stack0, index@stack1, sourcePos@stack2, indexSourcePos@stack3) -> void ; caller-cleans
	inline void EmitArrayVariableRef(scr_block_s* block, scriptInstance_t inst, sval_u expr, sval_u index, sval_u sourcePos, sval_u indexSourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitArrayVariableRef"));
		int ex = *reinterpret_cast<int*>(&expr);
		int ix = *reinterpret_cast<int*>(&index);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		int idxsrc = *reinterpret_cast<int*>(&indexSourcePos);
		__asm
		{
			push  idxsrc
			push  srcpos
			push  ix
			push  ex
			mov   eax, block
			mov   esi, inst
			call  fn
			add   esp, 10h
		}
	}
	
	// WaW sub_681930 — usercall(block@eax, inst@ecx, expr@stack0, index@stack1, sourcePos@stack2, indexSourcePos@stack3) -> void ; caller-cleans
	inline void EmitClearArrayVariable(scr_block_s* block, scriptInstance_t inst, sval_u expr, sval_u index, sval_u sourcePos, sval_u indexSourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitClearArrayVariable"));
		int ex = *reinterpret_cast<int*>(&expr);
		int ix = *reinterpret_cast<int*>(&index);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		int idxsrc = *reinterpret_cast<int*>(&indexSourcePos);
		__asm
		{
			push  idxsrc
			push  srcpos
			push  ix
			push  ex
			mov   eax, block
			mov   ecx, inst
			call  fn
			add   esp, 10h
		}
	}
	
	// WaW sub_681B30 — usercall(inst@edi, exprlist@stack0) -> void ; caller-cleans
	inline void AddExpressionListOpcodePos(scriptInstance_t inst, sval_u exprlist)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("AddExpressionListOpcodePos"));
		int el = *reinterpret_cast<int*>(&exprlist);
		__asm
		{
			push  el
			mov   edi, inst
			call  fn
			add   esp, 4
		}
	}
	
	// WaW sub_681B70 — usercall(inst@eax, filename@stack0, sourcePos@stack1, include@stack2, filePosId@stack3, fileCountId@stack4) -> void ; caller-cleans
	inline void AddFilePrecache(scriptInstance_t inst, unsigned int filename, unsigned int sourcePos, int include, unsigned int* filePosId, unsigned int* fileCountId)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("AddFilePrecache"));
		int incl = include;
		__asm
		{
			push  fileCountId
			push  filePosId
			push  incl
			push  sourcePos
			push  filename
			mov   eax, inst
			call  fn
			add   esp, 14h
		}
	}
	
	// WaW sub_681C30 — usercall(inst@eax, func@stack0, sourcePos@stack1) -> void ; caller-cleans
	inline void EmitFunction(scriptInstance_t inst, sval_u func, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitFunction"));
		int fc = *reinterpret_cast<int*>(&func);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			push  fc
			mov   eax, inst
			call  fn
			add   esp, 8
		}
	}
	
	// WaW sub_681F30 — usercall(inst@edi, func@stack0, sourcePos@stack1) -> void ; caller-cleans
	inline void EmitGetFunction(scriptInstance_t inst, sval_u func, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitGetFunction"));
		int fc = *reinterpret_cast<int*>(&func);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			push  fc
			mov   edi, inst
			call  fn
			add   esp, 8
		}
	}
	
	// WaW sub_6822F0 — usercall(block@eax, inst@edi, expr@stack0, param_count@stack1, bMethod@stack2, nameSourcePos@stack3, sourcePos@stack4) -> void ; caller-cleans
	inline void EmitPostScriptFunctionPointer(scr_block_s* block, scriptInstance_t inst, sval_u expr, int param_count, int bMethod, sval_u nameSourcePos, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitPostScriptFunctionPointer"));
		int ex = *reinterpret_cast<int*>(&expr);
		int nsrc = *reinterpret_cast<int*>(&nameSourcePos);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			push  nsrc
			push  bMethod
			push  param_count
			push  ex
			mov   eax, block
			mov   edi, inst
			call  fn
			add   esp, 14h
		}
	}
	
	// WaW sub_682500 — usercall(inst@edi, func@stack0, param_count@stack1, bMethod@stack2, sourcePos@stack3) -> void ; caller-cleans
	inline void EmitPostScriptThread(scriptInstance_t inst, sval_u func, int param_count, int bMethod, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitPostScriptThread"));
		int fc = *reinterpret_cast<int*>(&func);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			push  bMethod
			push  param_count
			push  fc
			mov   edi, inst
			call  fn
			add   esp, 10h
		}
	}
	
	// WaW sub_682730 — usercall(block@eax, inst@edi, expr@stack0, param_count@stack1, bMethod@stack2, sourcePos@stack3) -> void ; caller-cleans
	inline void EmitPostScriptThreadPointer(scr_block_s* block, scriptInstance_t inst, sval_u expr, int param_count, int bMethod, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitPostScriptThreadPointer"));
		int ex = *reinterpret_cast<int*>(&expr);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			push  bMethod
			push  param_count
			push  ex
			mov   eax, block
			mov   edi, inst
			call  fn
			add   esp, 10h
		}
	}
	
	// WaW sub_682950 — usercall(inst@eax, bMethod@edx, param_count@esi, func_name@stack0, nameSourcePos@stack1, block@stack2) -> void ; caller-cleans
	inline void EmitPostScriptFunctionCall(scriptInstance_t inst, int bMethod, int param_count, sval_u func_name, sval_u nameSourcePos, scr_block_s* block)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitPostScriptFunctionCall"));
		int fname = *reinterpret_cast<int*>(&func_name);
		int nsrc = *reinterpret_cast<int*>(&nameSourcePos);
		__asm
		{
			push  block
			push  nsrc
			push  fname
			mov   eax, inst
			mov   edx, bMethod
			mov   esi, param_count
			call  fn
			add   esp, 0Ch
		}
	}
	
	// WaW sub_6829A0 — usercall(inst@eax, isMethod@edx, param_count@esi, func_name@stack0, sourcePos@stack1, nameSourcePos@stack2, block@stack3) -> void ; caller-cleans
	inline void EmitPostScriptThreadCall(scriptInstance_t inst, int isMethod, int param_count, sval_u func_name, sval_u sourcePos, sval_u nameSourcePos, scr_block_s* block)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitPostScriptThreadCall"));
		int fname = *reinterpret_cast<int*>(&func_name);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		int nsrc = *reinterpret_cast<int*>(&nameSourcePos);
		__asm
		{
			push  block
			push  nsrc
			push  srcpos
			push  fname
			mov   eax, inst
			mov   edx, isMethod
			mov   esi, param_count
			call  fn
			add   esp, 10h
		}
	}
	
	// WaW sub_6829F0 — usercall(inst@eax) -> void ; no stack args
	inline void EmitPreFunctionCall(scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitPreFunctionCall"));
		__asm
		{
			mov   eax, inst
			call  fn
		}
	}
	
	// WaW sub_682AE0 — usercall(inst@eax, bMethod@edx, func_name@stack0, param_count@stack1, block@stack2) -> void ; caller-cleans
	inline void EmitPostFunctionCall(scriptInstance_t inst, int bMethod, sval_u func_name, int param_count, scr_block_s* block)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitPostFunctionCall"));
		int fname = *reinterpret_cast<int*>(&func_name);
		__asm
		{
			push  block
			push  param_count
			push  fname
			mov   eax, inst
			mov   edx, bMethod
			call  fn
			add   esp, 0Ch
		}
	}
	
	// WaW sub_682B30 — usercall(inst@eax, type_@edi, savedPos@stack0) -> void ; caller-cleans
	inline void Scr_BeginDevScript(scriptInstance_t inst, int* type_, char** savedPos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_BeginDevScript"));
		__asm
		{
			push  savedPos
			mov   eax, inst
			mov   edi, type_
			call  fn
			add   esp, 4
		}
	}

	// WaW sub_682BA0 — usercall(inst@eax, savedPos@edx) -> void ; no stack args
	inline void Scr_EndDevScript(scriptInstance_t inst, char** savedPos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_EndDevScript"));
		__asm
		{
			mov   eax, inst
			mov   edx, savedPos
			call  fn
		}
	}
	
	// WaW sub_682BD0 — usercall(inst@eax, param_count@stack0, sourcePos@stack1) -> void ; caller-cleans
	inline void EmitCallBuiltinOpcode(scriptInstance_t inst, int param_count, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitCallBuiltinOpcode"));
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			push  param_count
			mov   eax, inst
			call  fn
			add   esp, 8
		}
	}
	
	// WaW sub_682C40 — usercall(inst@eax, param_count@stack0, sourcePos@stack1) -> void ; caller-cleans
	inline void EmitCallBuiltinMethodOpcode(scriptInstance_t inst, int param_count, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitCallBuiltinMethodOpcode"));
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			push  param_count
			mov   eax, inst
			call  fn
			add   esp, 8
		}
	}
	
	// WaW sub_682CA0 — usercall(inst@eax, func_name@stack0, params@stack1, bStatement@stack2, block@stack3) -> void ; caller-cleans
	inline void EmitCall(scriptInstance_t inst, sval_u func_name, sval_u params, int bStatement, scr_block_s* block)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitCall"));
		int fname = *reinterpret_cast<int*>(&func_name);
		int prms = *reinterpret_cast<int*>(&params);
		__asm
		{
			push  block
			push  bStatement
			push  prms
			push  fname
			mov   eax, inst
			call  fn
			add   esp, 10h
		}
	}
	
	// WaW sub_683250 — usercall(inst@ecx, threadCountId@eax, pos@stack0, allowFarCall@stack1) -> void ; caller-cleans
	inline void LinkThread(scriptInstance_t inst, unsigned int threadCountId, VariableValue* pos, int allowFarCall)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("LinkThread"));
		__asm
		{
			push  allowFarCall
			push  pos
			mov   ecx, inst
			mov   eax, threadCountId
			call  fn
			add   esp, 8
		}
	}
	
	// WaW sub_6833F0 — usercall(inst@eax, filePosId@stack0, fileCountId@stack1) -> void ; caller-cleans
	inline void LinkFile(scriptInstance_t inst, unsigned int filePosId, unsigned int fileCountId)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("LinkFile"));
		__asm
		{
			push  fileCountId
			push  filePosId
			mov   eax, inst
			call  fn
			add   esp, 8
		}
	}
	
	// WaW sub_683640 — usercall(inst@eax, block@esi, expr@stack0, bStatement@stack1) -> void ; caller-cleans
	inline void EmitCallExpression(scriptInstance_t inst, scr_block_s* block, sval_u expr, int bStatement)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitCallExpression"));
		int ex = *reinterpret_cast<int*>(&expr);
		__asm
		{
			push  bStatement
			push  ex
			mov   eax, inst
			mov   esi, block
			call  fn
			add   esp, 8
		}
	}
	
	// WaW sub_683690 — usercall(block@ecx, inst@edi, expr@stack0) -> void ; caller-cleans
	inline void EmitCallExpressionFieldObject(scr_block_s* block, scriptInstance_t inst, sval_u expr)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitCallExpressionFieldObject"));
		int ex = *reinterpret_cast<int*>(&expr);
		__asm
		{
			push  ex
			mov   ecx, block
			mov   edi, inst
			call  fn
			add   esp, 4
		}
	}
	
	// WaW sub_6836F0 — usercall(inst@eax, constValue@stack0, value@stack1) -> void ; caller-cleans
	inline void Scr_CreateVector(scriptInstance_t inst, VariableCompileValue* constValue, VariableValue* value)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_CreateVector"));
		__asm
		{
			push  value
			push  constValue
			mov   eax, inst
			call  fn
			add   esp, 8
		}
	}
	
	// WaW sub_6838D0 — usercall(inst@eax, exprlist@stack0, sourcePos@stack1, constValue@stack2, block@stack3) -> bool@al ; caller-cleans
	inline bool EmitOrEvalPrimitiveExpressionList(scriptInstance_t inst, sval_u exprlist, sval_u sourcePos, VariableCompileValue* constValue, scr_block_s* a5)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitOrEvalPrimitiveExpressionList"));
		int exl = *reinterpret_cast<int*>(&exprlist);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		bool result;
		__asm
		{
			push  a5
			push  constValue
			push  srcpos
			push  exl
			mov   eax, inst
			call  fn
			add   esp, 10h
			mov   result, al
		}
		return result;
	}
	
	// WaW sub_683AF0 — usercall(inst@edx, exprlist@stack0, sourcePos@stack1, block@stack2) -> void ; caller-cleans
	inline void EmitExpressionListFieldObject(scriptInstance_t inst, sval_u exprlist, sval_u sourcePos, scr_block_s* block)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitExpressionListFieldObject"));
		int exl = *reinterpret_cast<int*>(&exprlist);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  block
			push  srcpos
			push  exl
			mov   edx, inst
			call  fn
			add   esp, 0Ch
		}
	}
	
	// WaW sub_683F00 — usercall(inst@eax, expr1@stack0, expr2@stack1, expr1sourcePos@stack2, expr2sourcePos@stack3, block@stack4) -> void ; caller-cleans
	inline void EmitBoolOrExpression(scriptInstance_t inst, sval_u expr1, sval_u expr2, sval_u expr1sourcePos, sval_u expr2sourcePos, scr_block_s* block)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitBoolOrExpression"));
		int e1 = *reinterpret_cast<int*>(&expr1);
		int e2 = *reinterpret_cast<int*>(&expr2);
		int e1sp = *reinterpret_cast<int*>(&expr1sourcePos);
		int e2sp = *reinterpret_cast<int*>(&expr2sourcePos);
		__asm
		{
			push  block
			push  e2sp
			push  e1sp
			push  e2
			push  e1
			mov   eax, inst
			call  fn
			add   esp, 14h
		}
	}
	
	// WaW sub_684090 — usercall(inst@eax, expr1@stack0, expr2@stack1, expr1sourcePos@stack2, expr2sourcePos@stack3, block@stack4) -> void ; caller-cleans
	inline void EmitBoolAndExpression(scriptInstance_t inst, sval_u expr1, sval_u expr2, sval_u expr1sourcePos, sval_u expr2sourcePos, scr_block_s* a6)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitBoolAndExpression"));
		int e1 = *reinterpret_cast<int*>(&expr1);
		int e2 = *reinterpret_cast<int*>(&expr2);
		int e1sp = *reinterpret_cast<int*>(&expr1sourcePos);
		int e2sp = *reinterpret_cast<int*>(&expr2sourcePos);
		__asm
		{
			push  a6
			push  e2sp
			push  e1sp
			push  e2
			push  e1
			mov   eax, inst
			call  fn
			add   esp, 14h
		}
	}
	
	// WaW sub_684350 — usercall(inst@edi, expr1@stack0, expr2@stack1, opcode@stack2, sourcePos@stack3, constValue@stack4, block@stack5) -> bool@al ; caller-cleans
	inline bool EmitOrEvalBinaryOperatorExpression(scriptInstance_t inst, sval_u expr1, sval_u expr2, sval_u opcode, sval_u sourcePos, VariableCompileValue* constValue, scr_block_s* a8)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitOrEvalBinaryOperatorExpression"));
		int e1 = *reinterpret_cast<int*>(&expr1);
		int e2 = *reinterpret_cast<int*>(&expr2);
		int opc = *reinterpret_cast<int*>(&opcode);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		bool result;
		__asm
		{
			push  a8
			push  constValue
			push  srcpos
			push  opc
			push  e2
			push  e1
			mov   edi, inst
			call  fn
			add   esp, 18h
			mov   result, al
		}
		return result;
	}
	
	// WaW sub_684460 — usercall(block@edi, inst@esi, lhs@stack0, rhs@stack1, opcode@stack2, sourcePos@stack3) -> void ; caller-cleans
	inline void EmitBinaryEqualsOperatorExpression(scr_block_s* block, scriptInstance_t inst, sval_u lhs, sval_u rhs, sval_u opcode, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitBinaryEqualsOperatorExpression"));
		int lhsv = *reinterpret_cast<int*>(&lhs);
		int rhsv = *reinterpret_cast<int*>(&rhs);
		int opc = *reinterpret_cast<int*>(&opcode);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			push  opc
			push  rhsv
			push  lhsv
			mov   edi, block
			mov   esi, inst
			call  fn
			add   esp, 10h
		}
	}
	
	// WaW sub_684500 — usercall(block@edx, expr@stack0) -> void ; caller-cleans
	inline void Scr_CalcLocalVarsVariableExpressionRef(scr_block_s* block, sval_u expr)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_CalcLocalVarsVariableExpressionRef"));
		int ex = *reinterpret_cast<int*>(&expr);
		__asm
		{
			push  ex
			mov   edx, block
			call  fn
			add   esp, 4
		}
	}
	
	// WaW sub_684540 — usercall(constValue@edx, inst@esi, expr@stack0) -> bool@al ; caller-cleans
	inline bool EvalExpression(VariableCompileValue* constValue, scriptInstance_t inst, sval_u expr)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EvalExpression"));
		int ex = *reinterpret_cast<int*>(&expr);
		bool result;
		__asm
		{
			push  ex
			mov   edx, constValue
			mov   esi, inst
			call  fn
			add   esp, 4
			mov   result, al
		}
		return result;
	}
	
	// WaW sub_684900 — usercall(inst@eax, expr@stack0, sourcePos@stack1, block@stack2) -> void ; caller-cleans
	inline void EmitArrayPrimitiveExpressionRef(scriptInstance_t inst, sval_u expr, sval_u sourcePos, scr_block_s* block)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitArrayPrimitiveExpressionRef"));
		int ex = *reinterpret_cast<int*>(&expr);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  block
			push  srcpos
			push  ex
			mov   eax, inst
			call  fn
			add   esp, 0Ch
		}
	}
	
	// WaW sub_684AC0 — usercall(inst@eax) -> void ; no stack args
	inline void ConnectBreakStatements(scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("ConnectBreakStatements"));
		__asm
		{
			mov   eax, inst
			call  fn
		}
	}
	
	// WaW sub_684B00 — usercall(inst@eax) -> void ; no stack args
	inline void ConnectContinueStatements(scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("ConnectContinueStatements"));
		__asm
		{
			mov   eax, inst
			call  fn
		}
	}
	
	// WaW sub_684B40 — usercall(block@eax, inst@ecx, expr@stack0, rhsSourcePos@stack1) -> bool@al ; caller-cleans
	inline bool EmitClearVariableExpression(scr_block_s* block, scriptInstance_t inst, sval_u expr, sval_u rhsSourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitClearVariableExpression"));
		int ex = *reinterpret_cast<int*>(&expr);
		int rhssrc = *reinterpret_cast<int*>(&rhsSourcePos);
		bool result;
		__asm
		{
			push  rhssrc
			push  ex
			mov   eax, block
			mov   ecx, inst
			call  fn
			add   esp, 8
			mov   result, al
		}
		return result;
	}
	
	// WaW sub_684C40 — usercall(inst@esi, lhs@stack0, rhs@stack1, sourcePos@stack2, rhsSourcePos@stack3, block@stack4) -> void ; caller-cleans
	inline void EmitAssignmentStatement(scriptInstance_t inst, sval_u lhs, sval_u rhs, sval_u sourcePos, sval_u rhsSourcePos, scr_block_s* block)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitAssignmentStatement"));
		int lhsv = *reinterpret_cast<int*>(&lhs);
		int rhsv = *reinterpret_cast<int*>(&rhs);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		int rhssrc = *reinterpret_cast<int*>(&rhsSourcePos);
		__asm
		{
			push  block
			push  rhssrc
			push  srcpos
			push  rhsv
			push  lhsv
			mov   esi, inst
			call  fn
			add   esp, 14h
		}
	}
	
	// WaW sub_684CD0 — usercall(inst@eax, block@esi, expr@stack0) -> void ; caller-cleans
	inline void EmitCallExpressionStatement(scriptInstance_t inst, scr_block_s* block, sval_u expr)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitCallExpressionStatement"));
		int ex = *reinterpret_cast<int*>(&expr);
		__asm
		{
			push  ex
			mov   eax, inst
			mov   esi, block
			call  fn
			add   esp, 4
		}
	}
	
	// WaW sub_684D20 — usercall(block@eax, inst@esi, expr@stack0, sourcePos@stack1) -> void ; caller-cleans
	inline void EmitReturnStatement(scr_block_s* block, scriptInstance_t inst, sval_u expr, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitReturnStatement"));
		int ex = *reinterpret_cast<int*>(&expr);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			push  ex
			mov   eax, block
			mov   esi, inst
			call  fn
			add   esp, 8
		}
	}
	
	// WaW sub_684D80 — usercall(block@eax, inst@edi, expr@stack0, sourcePos@stack1, waitSourcePos@stack2) -> void ; caller-cleans
	inline void EmitWaitStatement(scr_block_s* block, scriptInstance_t inst, sval_u expr, sval_u sourcePos, sval_u waitSourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitWaitStatement"));
		int ex = *reinterpret_cast<int*>(&expr);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		int waitsrc = *reinterpret_cast<int*>(&waitSourcePos);
		__asm
		{
			push  waitsrc
			push  srcpos
			push  ex
			mov   eax, block
			mov   edi, inst
			call  fn
			add   esp, 0Ch
		}
	}
	
	// WaW sub_684EB0 — usercall(inst@edi, sourcePos@stack0) -> void ; caller-cleans
	inline void EmitWaittillFrameEnd(scriptInstance_t inst, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitWaittillFrameEnd"));
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			mov   edi, inst
			call  fn
			add   esp, 4
		}
	}
	
	// WaW sub_684FB0 — usercall(inst@edi, expr@stack0, stmt@stack1, sourcePos@stack2, lastStatement@stack3, endSourcePos@stack4, block@stack5, ifStatBlock@stack6) -> void ; caller-cleans
	inline void EmitIfStatement(scriptInstance_t inst, sval_u expr, sval_u stmt, sval_u sourcePos, int lastStatement, unsigned int endSourcePos, scr_block_s* block, sval_u* ifStatBlock)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitIfStatement"));
		int ex = *reinterpret_cast<int*>(&expr);
		int stmtv = *reinterpret_cast<int*>(&stmt);   // not 'st' — collides with FPU register ST
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  ifStatBlock
			push  block
			push  endSourcePos
			push  lastStatement
			push  srcpos
			push  stmtv
			push  ex
			mov   edi, inst
			call  fn
			add   esp, 1Ch
		}
	}
	
	// WaW sub_6856A0 — usercall(inst@eax, block@edi) -> void ; no stack args
	inline void Scr_AddBreakBlock(scriptInstance_t inst, scr_block_s* block)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_AddBreakBlock"));
		__asm
		{
			mov   eax, inst
			mov   edi, block
			call  fn
		}
	}
	
	// WaW sub_685700 — usercall(inst@eax, block@edi) -> void ; no stack args
	inline void Scr_AddContinueBlock(scriptInstance_t inst, scr_block_s* block)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_AddContinueBlock"));
		__asm
		{
			mov   eax, inst
			mov   edi, block
			call  fn
		}
	}
	
	// WaW sub_686570 — usercall(block@eax, inst@edi, expr@stack0, sourcePos@stack1) -> void ; caller-cleans
	inline void EmitIncStatement(scr_block_s* block, scriptInstance_t inst, sval_u expr, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitIncStatement"));
		int ex = *reinterpret_cast<int*>(&expr);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			push  ex
			mov   eax, block
			mov   edi, inst
			call  fn
			add   esp, 8
		}
	}
	
	// WaW sub_686690 — usercall(block@eax, inst@edi, expr@stack0, sourcePos@stack1) -> void ; caller-cleans
	inline void EmitDecStatement(scr_block_s* block, scriptInstance_t inst, sval_u expr, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitDecStatement"));
		int ex = *reinterpret_cast<int*>(&expr);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			push  ex
			mov   eax, block
			mov   edi, inst
			call  fn
			add   esp, 8
		}
	}
	
	// WaW sub_6867B0 — usercall(node@eax, block@esi) -> void ; no stack args
	inline void Scr_CalcLocalVarsFormalParameterListInternal(sval_u* node, scr_block_s* block)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_CalcLocalVarsFormalParameterListInternal"));
		__asm
		{
			mov   eax, node
			mov   esi, block
			call  fn
		}
	}
	
	// WaW sub_686810 — usercall(inst@eax, obj@stack0, exprlist@stack1, sourcePos@stack2, waitSourcePos@stack3, block@stack4) -> void ; caller-cleans
	inline void EmitWaittillStatement(scriptInstance_t inst, sval_u obj, sval_u exprlist, sval_u sourcePos, sval_u waitSourcePos, scr_block_s* block)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitWaittillStatement"));
		int ob = *reinterpret_cast<int*>(&obj);
		int exl = *reinterpret_cast<int*>(&exprlist);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		int waitsrc = *reinterpret_cast<int*>(&waitSourcePos);
		__asm
		{
			push  block
			push  waitsrc
			push  srcpos
			push  exl
			push  ob
			mov   eax, inst
			call  fn
			add   esp, 14h
		}
	}
	
	// WaW sub_686A60 — usercall(inst@edi, obj@stack0, exprlist@stack1, sourcePos@stack2, waitSourcePos@stack3, block@stack4) -> void ; caller-cleans
	inline void EmitWaittillmatchStatement(scriptInstance_t inst, sval_u obj, sval_u exprlist, sval_u sourcePos, sval_u waitSourcePos, scr_block_s* block)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitWaittillmatchStatement"));
		int ob = *reinterpret_cast<int*>(&obj);
		int exl = *reinterpret_cast<int*>(&exprlist);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		int waitsrc = *reinterpret_cast<int*>(&waitSourcePos);
		__asm
		{
			push  block
			push  waitsrc
			push  srcpos
			push  exl
			push  ob
			mov   edi, inst
			call  fn
			add   esp, 14h
		}
	}
	
	// WaW sub_686D30 — usercall(inst@edi, obj@stack0, exprlist@stack1, sourcePos@stack2, notifySourcePos@stack3, block@stack4) -> void ; caller-cleans
	inline void EmitNotifyStatement(scriptInstance_t inst, sval_u obj, sval_u exprlist, sval_u sourcePos, sval_u notifySourcePos, scr_block_s* block)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitNotifyStatement"));
		int ob = *reinterpret_cast<int*>(&obj);
		int exl = *reinterpret_cast<int*>(&exprlist);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		int notifysrc = *reinterpret_cast<int*>(&notifySourcePos);
		__asm
		{
			push  block
			push  notifysrc
			push  srcpos
			push  exl
			push  ob
			mov   edi, inst
			call  fn
			add   esp, 14h
		}
	}
	
	// WaW sub_686F90 — usercall(block@eax, inst@edi, obj@stack0, expr@stack1, sourcePos@stack2, exprSourcePos@stack3) -> void ; caller-cleans
	inline void EmitEndOnStatement(scr_block_s* block, scriptInstance_t inst, sval_u obj, sval_u expr, sval_u sourcePos, sval_u exprSourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitEndOnStatement"));
		int ob = *reinterpret_cast<int*>(&obj);
		int ex = *reinterpret_cast<int*>(&expr);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		int exprsrc = *reinterpret_cast<int*>(&exprSourcePos);
		__asm
		{
			push  exprsrc
			push  srcpos
			push  ex
			push  ob
			mov   eax, block
			mov   edi, inst
			call  fn
			add   esp, 10h
		}
	}
	
	// WaW sub_687100 — usercall(inst@edi, expr@stack0, sourcePos@stack1) -> void ; caller-cleans
	inline void EmitCaseStatement(scriptInstance_t inst, sval_u expr, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitCaseStatement"));
		int ex = *reinterpret_cast<int*>(&expr);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			push  ex
			mov   edi, inst
			call  fn
			add   esp, 8
		}
	}
	
	// WaW sub_687990 — usercall(inst@eax, name@stack0, sourcePos@stack1) -> void ; caller-cleans
	inline void EmitCaseStatementInfo(scriptInstance_t inst, unsigned int name, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitCaseStatementInfo"));
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			push  name
			mov   eax, inst
			call  fn
			add   esp, 8
		}
	}
	
	// WaW sub_6879F0 — usercall(block@eax, inst@stack0, sourcePos@stack1) -> void ; caller-cleans
	inline void EmitBreakStatement(scr_block_s* block, scriptInstance_t inst, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitBreakStatement"));
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			push  inst
			mov   eax, block
			call  fn
			add   esp, 8
		}
	}
	
	// WaW sub_687B90 — usercall(block@eax, inst@stack0, sourcePos@stack1) -> void ; caller-cleans
	inline void EmitContinueStatement(scr_block_s* block, scriptInstance_t inst, sval_u sourcePos)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitContinueStatement"));
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  srcpos
			push  inst
			mov   eax, block
			call  fn
			add   esp, 8
		}
	}
	
	// WaW sub_687D30 — usercall(inst@eax, profileName@stack0, sourcePos@stack1, op@stack2) -> void ; caller-cleans
	inline void EmitProfStatement(scriptInstance_t inst, sval_u profileName, sval_u sourcePos, OpcodeVM op)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitProfStatement"));
		int pname = *reinterpret_cast<int*>(&profileName);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  op
			push  srcpos
			push  pname
			mov   eax, inst
			call  fn
			add   esp, 0Ch
		}
	}
	
	// WaW sub_6884F0 — usercall(block@edi, inst@stack0, val@stack1) -> void ; caller-cleans
	inline void Scr_CalcLocalVarsStatementList(scr_block_s* block, scriptInstance_t inst, sval_u val)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_CalcLocalVarsStatementList"));
		int vl = *reinterpret_cast<int*>(&val);
		__asm
		{
			push  vl
			push  inst
			mov   edi, block
			call  fn
			add   esp, 8
		}
	}
	
	// WaW sub_6886A0 — usercall(inst@eax, exprlist@stack0, sourcePos@stack1, block@stack2) -> void ; caller-cleans
	inline void EmitFormalParameterList(scriptInstance_t inst, sval_u exprlist, sval_u sourcePos, scr_block_s* block)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitFormalParameterList"));
		int exl = *reinterpret_cast<int*>(&exprlist);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		__asm
		{
			push  block
			push  srcpos
			push  exl
			mov   eax, inst
			call  fn
			add   esp, 0Ch
		}
	}
	
	// WaW sub_6887C0 — usercall(inst@eax, val@stack0) -> void ; caller-cleans
	inline void SpecifyThread(scriptInstance_t inst, sval_u val)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("SpecifyThread"));
		int vl = *reinterpret_cast<int*>(&val);
		__asm
		{
			push  vl
			mov   eax, inst
			call  fn
			add   esp, 4
		}
	}

	// WaW sub_6888D0 — usercall(inst@esi, val@stack0, sourcePos@stack1, endSourcePos@stack2, block@stack3) -> void ; caller-cleans
	inline void EmitThreadInternal(scriptInstance_t inst, sval_u val, sval_u sourcePos, sval_u endSourcePos, scr_block_s* block)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitThreadInternal"));
		int vl = *reinterpret_cast<int*>(&val);
		int srcpos = *reinterpret_cast<int*>(&sourcePos);
		int endsrc = *reinterpret_cast<int*>(&endSourcePos);
		__asm
		{
			push  block
			push  endsrc
			push  srcpos
			push  vl
			mov   esi, inst
			call  fn
			add   esp, 10h
		}
	}
	
	// WaW sub_688990 — usercall(stmttblock@eax, inst@stack0, exprlist@stack1, stmtlist@stack2) -> void ; caller-cleans
	inline void Scr_CalcLocalVarsThread(sval_u* stmttblock, scriptInstance_t inst, sval_u exprlist, sval_u stmtlist)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_CalcLocalVarsThread"));
		int exl = *reinterpret_cast<int*>(&exprlist);
		int stl = *reinterpret_cast<int*>(&stmtlist);
		__asm
		{
			push  stl
			push  exl
			push  inst
			mov   eax, stmttblock
			call  fn
			add   esp, 0Ch
		}
	}
	
	// WaW sub_688A00 — usercall(type_@ecx, inst@esi) -> void ; no stack args
	inline void InitThread(int type_, scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("InitThread"));
		__asm
		{
			mov   ecx, type_
			mov   esi, inst
			call  fn
		}
	}
	
	// WaW sub_688A70 — usercall(inst@eax, val@stack0, stmttblock@stack1) -> void ; caller-cleans
	inline void EmitNormalThread(scriptInstance_t inst, sval_u val, sval_u* stmttblock)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitNormalThread"));
		int vl = *reinterpret_cast<int*>(&val);
		__asm
		{
			push  stmttblock
			push  vl
			mov   eax, inst
			call  fn
			add   esp, 8
		}
	}
	
	// WaW sub_688B00 — usercall(inst@eax, val@stack0, stmttblock@stack1) -> void ; caller-cleans
	inline void EmitDeveloperThread(scriptInstance_t inst, sval_u val, sval_u* stmttblock)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitDeveloperThread"));
		int vl = *reinterpret_cast<int*>(&val);
		__asm
		{
			push  stmttblock
			push  vl
			mov   eax, inst
			call  fn
			add   esp, 8
		}
	}
	
	// WaW sub_688C40 — usercall(inst@eax, val@stack0) -> void ; caller-cleans
	inline void EmitThread(scriptInstance_t inst, sval_u val)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitThread"));
		int vl = *reinterpret_cast<int*>(&val);
		__asm
		{
			push  vl
			mov   eax, inst
			call  fn
			add   esp, 4
		}
	}
	
	// WaW sub_688DD0 — usercall(inst@eax, val@stack0) -> void ; caller-cleans
	inline void EmitInclude(scriptInstance_t inst, sval_u val)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("EmitInclude"));
		int vl = *reinterpret_cast<int*>(&val);
		__asm
		{
			push  vl
			mov   eax, inst
			call  fn
			add   esp, 4
		}
	}
	
	// WaW sub_688E70 — usercall(inst@eax, val@stack0, filePosId@stack1, fileCountId@stack2, scriptId@stack3, entries@stack4, entriesCount@stack5) -> void ; caller-cleans
	inline void ScriptCompile(scriptInstance_t inst, sval_u val, unsigned int filePosId, unsigned int fileCountId, unsigned int scriptId, PrecacheEntry* entries, int entriesCount)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("ScriptCompile"));
		int vl = *reinterpret_cast<int*>(&val);
		__asm
		{
			push  entriesCount
			push  entries
			push  scriptId
			push  fileCountId
			push  filePosId
			push  vl
			mov   eax, inst
			call  fn
			add   esp, 18h
		}
	}

	// Unknown Adress
	void EmitFloat(scriptInstance_t inst, float value);
	void EmitCanonicalStringConst(scriptInstance_t inst, unsigned int stringValue);
	int Scr_FindLocalVar(scr_block_s* block, int startIndex, unsigned int name);
	void Scr_CheckLocalVarsCount(int localVarsCount);
	void EmitGetUndefined(scriptInstance_t inst, sval_u sourcePos);
	void EmitPrimitiveExpression(scriptInstance_t inst, sval_u expr, scr_block_s* block);
	void Scr_EmitAnimation(scriptInstance_t inst, char* pos, unsigned int animName, unsigned int sourcePos);
	void EmitEvalArray(scriptInstance_t inst, sval_u sourcePos, sval_u indexSourcePos);
	void EmitEvalArrayRef(scriptInstance_t inst, sval_u sourcePos, sval_u indexSourcePos);
	unsigned int Scr_GetBuiltin(scriptInstance_t inst, sval_u func_name);
	int Scr_GetUncacheType(int type);
	int Scr_GetCacheType(int type);
	BuiltinFunction Scr_GetFunction(const char** pName, int* type);
	BuiltinFunction GetFunction(scriptInstance_t inst, const char** pName, int* type);
	BuiltinMethod GetMethod(scriptInstance_t inst, const char** pName, int* type);
	unsigned int GetVariableName(scriptInstance_t inst, unsigned int id);
	int GetExpressionCount(sval_u exprlist);
	sval_u* GetSingleParameter(sval_u exprlist);
	void EmitExpressionFieldObject(scriptInstance_t inst, sval_u expr, sval_u sourcePos, scr_block_s* block);
	void EvalInteger(int value, sval_u sourcePos, VariableCompileValue* constValue);
	void EvalFloat(float value, sval_u sourcePos, VariableCompileValue* constValue);
	void EvalString(unsigned int value, sval_u sourcePos, VariableCompileValue* constValue);
	void EvalIString(unsigned int value, sval_u sourcePos, VariableCompileValue* constValue);
	void EvalUndefined(sval_u sourcePos, VariableCompileValue* constValue);
	void Scr_PopValue(scriptInstance_t inst);
	void EmitSetVariableField(scriptInstance_t inst, sval_u sourcePos);
	void EmitFieldVariableRef(scriptInstance_t inst, sval_u expr, sval_u field, sval_u sourcePos, scr_block_s* block);
	void Scr_CalcLocalVarsArrayPrimitiveExpressionRef(sval_u expr, scr_block_s* block);
	BOOL IsUndefinedPrimitiveExpression(sval_u expr);
	bool IsUndefinedExpression(sval_u expr);
	void Scr_CopyBlock(scr_block_s* from, scr_block_s** to);
	void Scr_CheckMaxSwitchCases(int count);
	void Scr_CalcLocalVarsSafeSetVariableField(sval_u expr, sval_u sourcePos, scr_block_s* block);
	void EmitFormalWaittillParameterListRefInternal(scriptInstance_t inst, sval_u* node, scr_block_s* block);
	void EmitDefaultStatement(scriptInstance_t inst, sval_u sourcePos);
	char Scr_IsLastStatement(scriptInstance_t inst, sval_u* node);
	void EmitEndStatement(scriptInstance_t inst, sval_u sourcePos, scr_block_s* block);
	void EmitProfBeginStatement(scriptInstance_t inst, sval_u profileName, sval_u sourcePos);
	void EmitProfEndStatement(scriptInstance_t inst, sval_u profileName, sval_u sourcePos);
	void Scr_CalcLocalVarsIncStatement(sval_u expr, scr_block_s *block);
	void Scr_CalcLocalVarsWaittillStatement(sval_u exprlist, scr_block_s* block);
	void EmitFormalParameterListInternal(scriptInstance_t inst, sval_u* node, scr_block_s* block);
	unsigned int SpecifyThreadPosition(scriptInstance_t inst, unsigned int posId, unsigned int name, unsigned int sourcePos, int type);
	void Scr_CalcLocalVarsFormalParameterList(sval_u exprlist, scr_block_s* block);
	void SetThreadPosition(scriptInstance_t inst, unsigned int posId);
	void EmitIncludeList(scriptInstance_t inst, sval_u val);
} } // namespace T4::engine