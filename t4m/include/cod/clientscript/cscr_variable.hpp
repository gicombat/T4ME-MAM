#pragma once

namespace T4
{
	namespace engine
	{
		WEAK symbol<int(const void* info1, const void* info2)>ThreadInfoCompare{ "ThreadInfoCompare" };
		WEAK symbol<void(scriptInstance_t scriptInstance)>Scr_DumpScriptThreads{ "Scr_DumpScriptThreads" };
		WEAK symbol<unsigned int(scriptInstance_t inst, unsigned int parentId, unsigned int name, unsigned int index)>GetNewVariableIndexInternal3{ "GetNewVariableIndexInternal3" };
		WEAK symbol<void(scriptInstance_t inst, unsigned int parentId)>ClearObjectInternal{ "ClearObjectInternal" };
		WEAK symbol<void(scriptInstance_t inst, VariableType type, VariableUnion a3)>RemoveRefToValueInternal{ "RemoveRefToValueInternal" };
		WEAK symbol<void(scriptInstance_t inst, unsigned int parentId, unsigned int newParentId)>CopyArray{ "CopyArray" };
		WEAK symbol<void(scriptInstance_t inst, unsigned int parentId, unsigned int name, VariableValue* value)>SetVariableEntityFieldValue{ "SetVariableEntityFieldValue" };
		WEAK symbol<void(scriptInstance_t inst, VariableValue* a2)>Scr_ClearVector{ "Scr_ClearVector" };
		WEAK symbol<void(scriptInstance_t inst)>Scr_FreeEntityList{ "Scr_FreeEntityList" };
		WEAK symbol<void(scriptInstance_t inst)>Scr_FreeObjects{ "Scr_FreeObjects" };
		WEAK symbol<void(unsigned int arrayId, scriptInstance_t inst)>Scr_AddArrayKeys{ "Scr_AddArrayKeys" };
		WEAK symbol<float(scriptInstance_t inst, unsigned int parentId)>Scr_GetObjectUsage{ "Scr_GetObjectUsage" };
		WEAK symbol<char* (const char* filename)>Scr_GetSourceFile_LoadObj{ "Scr_GetSourceFile_LoadObj" };
		WEAK symbol<char* (const char* filename)>Scr_GetSourceFile_FastFile{ "Scr_GetSourceFile_FastFile" };
		WEAK symbol<void(scriptInstance_t inst, char* Format)>Scr_AddFieldsForFile{ "Scr_AddFieldsForFile" };
		WEAK symbol<void(scriptInstance_t inst, const char* path, const char* extension)>Scr_AddFields_LoadObj{ "Scr_AddFields_LoadObj" };
		WEAK symbol<void(scriptInstance_t inst, const char* path, const char* extension)>Scr_AddFields_FastFile{ "Scr_AddFields_FastFile" };
		WEAK symbol<int(scriptInstance_t inst, unsigned int parentId)>Scr_MakeValuePrimitive{ "Scr_MakeValuePrimitive" };

		// WaW sub_68EF90 — usercall(begin@edi, end@esi, inst@stack0) -> void ; caller-cleans
		inline void Scr_InitVariableRange(unsigned int begin, unsigned int end, scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_InitVariableRange"));
			__asm
			{
				push  inst
				mov   edi, begin
				mov   esi, end
				call  fn
				add   esp, 4
			}
		}
		// WaW sub_68F030 — usercall(inst@eax) -> void ; no stack args
		inline void Scr_InitClassMap(scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_InitClassMap"));
			__asm
			{
				mov   eax, inst
				call  fn
			}
		}
		// WaW sub_68F4A0 — usercall(name@ecx, inst@stack0, parentId@stack1, index@stack2) -> uint@eax ; caller-cleans
		inline unsigned int GetNewVariableIndexInternal2(unsigned int name, scriptInstance_t inst, unsigned int parentId, unsigned int index)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("GetNewVariableIndexInternal2"));
			unsigned int result;
			__asm
			{
				push  index
				push  parentId
				push  inst
				mov   ecx, name
				call  fn
				add   esp, 12
				mov   result, eax
			}
			return result;
		}
		// WaW sub_68F560 — usercall(name@ecx, inst@stack0, parentId@stack1, index@stack2) -> uint@eax ; caller-cleans
		inline unsigned int GetNewVariableIndexReverseInternal2(unsigned int name, scriptInstance_t inst, unsigned int parentId, unsigned int index)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("GetNewVariableIndexReverseInternal2"));
			unsigned int result;
			__asm
			{
				push  index
				push  parentId
				push  inst
				mov   ecx, name
				call  fn
				add   esp, 12
				mov   result, eax
			}
			return result;
		}
		// WaW sub_68F620 — usercall(parentValue@eax, inst@stack0, index@stack1) -> void ; caller-cleans
		inline void MakeVariableExternal(VariableValueInternal* parentValue, scriptInstance_t inst, unsigned int index)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("MakeVariableExternal"));
			__asm
			{
				push  index
				push  inst
				mov   eax, parentValue
				call  fn
				add   esp, 8
			}
		}
		// WaW sub_68F800 — usercall(id@eax, inst@stack0, parentId@stack1) -> void ; caller-cleans
		inline void FreeChildValue(unsigned int id, scriptInstance_t inst, unsigned int parentId)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("FreeChildValue"));
			__asm
			{
				push  parentId
				push  inst
				mov   eax, id
				call  fn
				add   esp, 8
			}
		}
		// WaW sub_68F9E0 — usercall(parentId@edi, inst@stack0) -> void ; caller-cleans
		inline void ClearObject(unsigned int parentId, scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("ClearObject"));
			__asm
			{
				push  inst
				mov   edi, parentId
				call  fn
				add   esp, 4
			}
		}
		// WaW sub_68FA30 — usercall(inst@ecx, threadId@eax) -> void ; no stack args
		inline void Scr_StopThread(scriptInstance_t inst, unsigned int threadId)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_StopThread"));
			__asm
			{
				mov   ecx, inst
				mov   eax, threadId
				call  fn
			}
		}
		// WaW sub_68FAA0 — usercall(inst@eax, threadId@stack0) -> uint@eax ; caller-cleans
		inline unsigned int GetSafeParentLocalId(scriptInstance_t inst, unsigned int threadId)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("GetSafeParentLocalId"));
			unsigned int result;
			__asm
			{
				push  threadId
				mov   eax, inst
				call  fn
				add   esp, 4
				mov   result, eax
			}
			return result;
		}
		// WaW sub_68FAD0 — usercall(threadId@eax, inst@ecx) -> uint@eax ; no stack args
		inline unsigned int GetStartLocalId(unsigned int threadId, scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("GetStartLocalId"));
			unsigned int result;
			__asm
			{
				mov   eax, threadId
				mov   ecx, inst
				call  fn
				mov   result, eax
			}
			return result;
		}
		// WaW sub_68FB10 — usercall(inst@ecx, parentId@eax) -> void ; no stack args
		inline void Scr_KillThread(scriptInstance_t inst, unsigned int parentId)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_KillThread"));
			__asm
			{
				mov   ecx, inst
				mov   eax, parentId
				call  fn
			}
		}
		// WaW sub_68FCE0 — usercall(inst@eax) -> u16@ax ; no stack args
		inline unsigned __int16 AllocVariable(scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("AllocVariable"));
			unsigned __int16 result;
			__asm
			{
				mov   eax, inst
				call  fn
				mov   result, ax
			}
			return result;
		}
		// WaW sub_68FDC0 — usercall(id@eax, inst@edx) -> void ; no stack args
		inline void FreeVariable(unsigned int id, scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("FreeVariable"));
			__asm
			{
				mov   eax, id
				mov   edx, inst
				call  fn
			}
		}
		// WaW sub_68FE20 — usercall(inst@eax) -> uint@eax ; no stack args
		inline unsigned int AllocValue(scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("AllocValue"));
			unsigned int result;
			__asm
			{
				mov   eax, inst
				call  fn
				mov   result, eax
			}
			return result;
		}
		// WaW sub_68FF10 — usercall(classnum@eax, inst@ecx, entnum@stack0, clientnum@stack1) -> void ; caller-cleans
		inline void AllocEntity(unsigned int classnum, scriptInstance_t inst, int entnum, int clientnum)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("AllocEntity"));
			__asm
			{
				push  clientnum
				push  entnum
				mov   eax, classnum
				mov   ecx, inst
				call  fn
				add   esp, 8
			}
		}
		// WaW sub_68FF60 — usercall(inst@eax) -> uint@eax ; no stack args
		inline unsigned int Scr_AllocArray(scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_AllocArray"));
			unsigned int result;
			__asm
			{
				mov   eax, inst
				call  fn
				mov   result, eax
			}
			return result;
		}
		// WaW sub_68FF90 — usercall(inst@eax, parentLocalId@ecx, self@stack0) -> uint@eax ; caller-cleans
		inline unsigned int AllocChildThread(scriptInstance_t inst, unsigned int parentLocalId, unsigned int self)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("AllocChildThread"));
			unsigned int result;
			__asm
			{
				push  self
				mov   eax, inst
				mov   ecx, parentLocalId
				call  fn
				add   esp, 4
				mov   result, eax
			}
			return result;
		}
		// WaW sub_68FFD0 — usercall(id@stack0, inst@eax) -> void ; caller-cleans
		inline void FreeValue(unsigned int id, scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("FreeValue"));
			__asm
			{
				push  id
				mov   eax, inst
				call  fn
				add   esp, 4
			}
		}
		// WaW sub_690040 — usercall(id@eax, inst@ecx) -> void ; no stack args
		inline void RemoveRefToObject(unsigned int id, scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("RemoveRefToObject"));
			__asm
			{
				mov   eax, id
				mov   ecx, inst
				call  fn
			}
		}
		// WaW sub_690100 — usercall(inst@eax) -> float*@eax ; no stack args
		inline float* Scr_AllocVector(scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_AllocVector"));
			float* result;
			__asm
			{
				mov   eax, inst
				call  fn
				mov   result, eax
			}
			return result;
		}
		// WaW sub_690130 — usercall(vectorValue@eax, inst@stack0) -> void ; caller-cleans
		inline void RemoveRefToVector(const float* vectorValue, scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("RemoveRefToVector"));
			__asm
			{
				push  inst
				mov   eax, vectorValue
				call  fn
				add   esp, 4
			}
		}
		// WaW sub_690160 — usercall(inst@eax, type_@ecx, u@stack0) -> void ; caller-cleans
		inline void AddRefToValue(scriptInstance_t inst, VariableType type_, VariableUnion u)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("AddRefToValue"));
			__asm
			{
				push  u
				mov   eax, inst
				mov   ecx, type_
				call  fn
				add   esp, 4
			}
		}
		// WaW sub_690210 — usercall(id@eax, intvalue@ecx, inst@stack0) -> int@eax ; caller-cleans
		inline int FindArrayVariable(unsigned int id, unsigned int intvalue, scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("FindArrayVariable"));
			int result;
			__asm
			{
				push  inst
				mov   eax, id
				mov   ecx, intvalue
				call  fn
				add   esp, 4
				mov   result, eax
			}
			return result;
		}
		// WaW sub_690260 — usercall(unsignedValue@eax, parentId@ecx, inst@stack0) -> uint@eax ; caller-cleans
		inline unsigned int FindVariable(unsigned int unsignedValue, unsigned int parentId, scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("FindVariable"));
			unsigned int result;
			__asm
			{
				push  inst
				mov   eax, unsignedValue
				mov   ecx, parentId
				call  fn
				add   esp, 4
				mov   result, eax
			}
			return result;
		}
		// WaW sub_6902A0 — usercall(unsignedValue@eax, inst@stack0, parentId@stack1) -> uint@eax ; caller-cleans
		inline unsigned int GetArrayVariableIndex(unsigned int unsignedValue, scriptInstance_t inst, unsigned int parentId)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("GetArrayVariableIndex"));
			unsigned int result;
			__asm
			{
				push  parentId
				push  inst
				mov   eax, unsignedValue
				call  fn
				add   esp, 8
				mov   result, eax
			}
			return result;
		}
		// WaW sub_6902F0 — usercall(inst@eax, name@esi, parentId@stack0) -> uint@eax ; caller-cleans
		inline unsigned int Scr_GetVariableFieldIndex(scriptInstance_t inst, unsigned int name, unsigned int parentId)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_GetVariableFieldIndex"));
			unsigned int result;
			__asm
			{
				push  parentId
				mov   eax, inst
				mov   esi, name
				call  fn
				add   esp, 4
				mov   result, eax
			}
			return result;
		}
		// WaW sub_6903B0 — usercall(inst@edi, parentId@stack0, name@stack1) -> VariableValue in edx:eax ; caller-cleans
		inline VariableValue Scr_FindVariableField(scriptInstance_t inst, unsigned int parentId, unsigned int name)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_FindVariableField"));
			VariableValue result;
			__asm
			{
				push  name
				push  parentId
				mov   edi, inst
				call  fn
				add   esp, 8
				lea   ecx, result
				mov[ecx], eax        // union value
				mov[ecx + 4], edx      // type
			}
			return result;
		}
		// WaW sub_690450 — usercall(id@eax, inst@ecx, name@stack0, value@stack1) -> void ; caller-cleans
		inline void ClearVariableField(scriptInstance_t inst, unsigned int id, unsigned int name, VariableValue* value)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("ClearVariableField"));
			__asm
			{
				push  value
				push  name
				mov   eax, id
				mov   ecx, inst
				call  fn
				add   esp, 8
			}
		}
		// WaW sub_690510 — usercall(inst@eax, parentId@stack0, name@stack1) -> uint@eax ; caller-cleans
		inline unsigned int GetVariable(scriptInstance_t inst, unsigned int parentId, unsigned int name)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("GetVariable"));
			unsigned int result;
			__asm
			{
				push  name
				push  parentId
				mov   eax, inst
				call  fn
				add   esp, 8
				mov   result, eax
			}
			return result;
		}
		// WaW sub_690570 — usercall(inst@eax, unsignedValue@ecx, parentId@edi) -> uint@eax ; no stack args
		inline unsigned int GetNewVariable(scriptInstance_t inst, unsigned int unsignedValue, unsigned int parentId)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("GetNewVariable"));
			unsigned int result;
			__asm
			{
				mov   eax, inst
				mov   ecx, unsignedValue
				mov   edi, parentId
				call  fn
				mov   result, eax
			}
			return result;
		}
		// WaW sub_6905B0 — usercall(id@stack0, inst@eax, parentId@stack1) -> uint@eax ; caller-cleans
		inline unsigned int GetObjectVariable(unsigned int id, scriptInstance_t inst, unsigned int parentId)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("GetObjectVariable"));
			unsigned int result;
			__asm
			{
				push  parentId
				push  id
				mov   eax, inst
				call  fn
				add   esp, 8
				mov   result, eax
			}
			return result;
		}
		// WaW sub_690610 — usercall(inst@eax, name@edi, parentId@ecx) -> uint@eax ; no stack args
		inline unsigned int GetNewObjectVariable(scriptInstance_t inst, unsigned int name, unsigned int parentId)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("GetNewObjectVariable"));
			unsigned int result;
			__asm
			{
				mov   eax, inst
				mov   edi, name
				mov   ecx, parentId
				call  fn
				mov   result, eax
			}
			return result;
		}
		// WaW sub_690650 — usercall(name@edi, parentId@ecx, inst@esi) -> void ; no stack args
		inline void RemoveVariable(unsigned int name, unsigned int parentId, scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("RemoveVariable"));
			__asm
			{
				mov   edi, name
				mov   ecx, parentId
				mov   esi, inst
				call  fn
			}
		}
		// WaW sub_6906A0 — usercall(inst@edi, parentId@stack0) -> void ; caller-cleans
		inline void RemoveNextVariable(scriptInstance_t inst, unsigned int parentId)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("RemoveNextVariable"));
			__asm
			{
				push  parentId
				mov   edi, inst
				call  fn
				add   esp, 4
			}
		}
		// WaW sub_690710 — usercall(unsignedValue@edi, parentId@ecx, inst@esi) -> void ; no stack args
		inline void SafeRemoveVariable(unsigned int unsignedValue, unsigned int parentId, scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("SafeRemoveVariable"));
			__asm
			{
				mov   edi, unsignedValue
				mov   ecx, parentId
				mov   esi, inst
				call  fn
			}
		}
		// WaW sub_6908D0 — usercall(inst@eax, value@edi, id@stack0) -> void ; caller-cleans
		inline void SetVariableValue(scriptInstance_t inst, VariableValue* value, unsigned int id)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("SetVariableValue"));
			__asm
			{
				push  id
				mov   eax, inst
				mov   edi, value
				call  fn
				add   esp, 4
			}
		}
		// WaW sub_690A10 — usercall(inst@eax, id@stack0) -> VariableValue in edx:eax ; caller-cleans
		inline VariableValue Scr_EvalVariable(scriptInstance_t inst, unsigned int id)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_EvalVariable"));
			VariableValue result;
			__asm
			{
				push  id
				mov   eax, inst
				call  fn
				add   esp, 4
				lea   ecx, result
				mov[ecx], eax
				mov[ecx + 4], edx
			}
			return result;
		}
		// WaW sub_690A50 — usercall(inst@ecx, id@eax) -> uint@eax ; no stack args
		inline unsigned int Scr_EvalVariableObject(scriptInstance_t inst, int id)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_EvalVariableObject"));
			unsigned int result;
			__asm
			{
				mov   ecx, inst
				mov   eax, id
				call  fn
				mov   result, eax
			}
			return result;
		}
		// WaW sub_690AB0 — usercall(entId@ecx, inst@stack0, name@stack1) -> VariableValue in edx:eax ; caller-cleans
		inline VariableValue Scr_EvalVariableEntityField(unsigned int entId, scriptInstance_t inst, unsigned int name)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_EvalVariableEntityField"));
			VariableValue result;
			__asm
			{
				push  name
				push  inst
				mov   ecx, entId
				call  fn
				add   esp, 8
				lea   ecx, result
				mov[ecx], eax
				mov[ecx + 4], edx
			}
			return result;
		}
		// WaW sub_690BB0 — usercall(inst@eax, id@edx) -> VariableValue in edx:eax ; no stack args
		inline VariableValue Scr_EvalVariableField(scriptInstance_t inst, unsigned int id)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_EvalVariableField"));
			VariableValue result;
			__asm
			{
				mov   eax, inst
				mov   edx, id
				call  fn
				lea   ecx, result
				mov[ecx], eax
				mov[ecx + 4], edx
			}
			return result;
		}
		// WaW sub_690C10 — usercall(inst@eax, value@stack0) -> void ; caller-cleans
		inline void Scr_EvalSizeValue(scriptInstance_t inst, VariableValue* value)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_EvalSizeValue"));
			__asm
			{
				push  value
				mov   eax, inst
				call  fn
				add   esp, 4
			}
		}
		// WaW sub_690CF0 — usercall(inst@eax, id@ecx) -> uint@eax ; no stack args
		inline unsigned int GetObject(scriptInstance_t inst, unsigned int id)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("GetObject"));
			unsigned int result;
			__asm
			{
				mov   eax, inst
				mov   ecx, id
				call  fn
				mov   result, eax
			}
			return result;
		}
		// WaW sub_690D50 — usercall(inst@eax, id@ecx) -> uint@eax ; no stack args
		inline unsigned int GetArray(scriptInstance_t inst, unsigned int id)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("GetArray"));
			unsigned int result;
			__asm
			{
				mov   eax, inst
				mov   ecx, id
				call  fn
				mov   result, eax
			}
			return result;
		}
		// WaW sub_690DB0 — usercall(inst@eax, value@esi) -> void ; no stack args
		inline void Scr_EvalBoolComplement(scriptInstance_t inst, VariableValue* value)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_EvalBoolComplement"));
			__asm
			{
				mov   eax, inst
				mov   esi, value
				call  fn
			}
		}
		// WaW sub_690E00 — usercall(inst@eax, value@esi) -> void ; no stack args
		inline void Scr_CastBool(scriptInstance_t inst, VariableValue* value)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_CastBool"));
			__asm
			{
				mov   eax, inst
				mov   esi, value
				call  fn
			}
		}
		// WaW sub_690E80 — usercall(value@esi, inst@edi) -> char@al ; no stack args
		inline char Scr_CastString(scriptInstance_t inst, VariableValue* value)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_CastString"));
			char result;
			__asm
			{
				mov   esi, value
				mov   edi, inst
				call  fn
				mov   result, al
			}
			return result;
		}
		// WaW sub_690F30 — usercall(value@eax, inst@ecx) -> void ; no stack args
		inline void Scr_CastDebugString(scriptInstance_t inst, VariableValue* value)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_CastDebugString"));
			__asm
			{
				mov   eax, value
				mov   ecx, inst
				call  fn
			}
		}
		// WaW sub_691040 — usercall(inst@eax, value@esi) -> void ; no stack args
		inline void Scr_CastVector(scriptInstance_t inst, VariableValue* value)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_CastVector"));
			__asm
			{
				mov   eax, inst
				mov   esi, value
				call  fn
			}
		}
		// WaW sub_691110 — usercall(value@eax, inst@ecx, a3@stack0) -> VariableUnion@eax ; caller-cleans
		inline VariableUnion Scr_EvalFieldObject(VariableValue* value, scriptInstance_t inst, int a3)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_EvalFieldObject"));
			VariableUnion result;
			__asm
			{
				push  a3
				mov   eax, value
				mov   ecx, inst
				call  fn
				add   esp, 4
				mov   dword ptr result, eax
			}
			return result;
		}
		// WaW sub_6911B0 — usercall(inst@eax, value2@esi, value1@stack0) -> void ; caller-cleans
		inline void Scr_UnmatchingTypesError(scriptInstance_t inst, VariableValue* value2, VariableValue* value1)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_UnmatchingTypesError"));
			__asm
			{
				push  value1
				mov   eax, inst
				mov   esi, value2
				call  fn
				add   esp, 4
			}
		}
		// WaW sub_691270 — usercall(value2@ecx, value1@edi, inst@eax) -> void ; no stack args
		inline void Scr_CastWeakerPair(VariableValue* value2, VariableValue* value1, scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_CastWeakerPair"));
			__asm
			{
				mov   ecx, value2
				mov   edi, value1
				mov   eax, inst
				call  fn
			}
		}
		// WaW sub_691370 — usercall(value2@eax, value1@ecx, inst@stack0) -> void ; caller-cleans
		inline void Scr_CastWeakerStringPair(VariableValue* value2, VariableValue* value1, scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_CastWeakerStringPair"));
			__asm
			{
				push  inst
				mov   eax, value2
				mov   ecx, value1
				call  fn
				add   esp, 4
			}
		}
		// WaW sub_6914E0 — usercall(value1@eax, value2@ecx, inst@stack0) -> void ; caller-cleans
		inline void Scr_EvalOr(VariableValue* value1, VariableValue* value2, scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_EvalOr"));
			__asm
			{
				push  inst
				mov   eax, value1
				mov   ecx, value2
				call  fn
				add   esp, 4
			}
		}
		// WaW sub_691510 — usercall(value1@eax, value2@ecx, inst@stack0) -> void ; caller-cleans
		inline void Scr_EvalExOr(VariableValue* value1, VariableValue* value2, scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_EvalExOr"));
			__asm
			{
				push  inst
				mov   eax, value1
				mov   ecx, value2
				call  fn
				add   esp, 4
			}
		}
		// WaW sub_691540 — usercall(value1@eax, value2@ecx, inst@stack0) -> void ; caller-cleans
		inline void Scr_EvalAnd(VariableValue* value1, VariableValue* value2, scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_EvalAnd"));
			__asm
			{
				push  inst
				mov   eax, value1
				mov   ecx, value2
				call  fn
				add   esp, 4
			}
		}
		// WaW sub_691570 — usercall(value1@eax, inst@stack0, value2@stack1) -> void ; caller-cleans
		inline void Scr_EvalEquality(VariableValue* value1, scriptInstance_t inst, VariableValue* value2)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_EvalEquality"));
			__asm
			{
				push  value2
				push  inst
				mov   eax, value1
				call  fn
				add   esp, 8
			}
		}
		// WaW sub_691760 — usercall(value1@eax, value2@ecx, a3@stack0) -> void ; caller-cleans
		inline void Scr_EvalLess(VariableValue* value1, VariableValue* value2, scriptInstance_t a3)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_EvalLess"));
			__asm
			{
				push  a3
				mov   eax, value1
				mov   ecx, value2
				call  fn
				add   esp, 4
			}
		}
		// WaW sub_6917D0 — usercall(inst@eax, value1@esi, value2@stack0) -> void ; caller-cleans
		inline void Scr_EvalGreaterEqual(scriptInstance_t inst, VariableValue* value1, VariableValue* value2)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_EvalGreaterEqual"));
			__asm
			{
				push  value2
				mov   eax, inst
				mov   esi, value1
				call  fn
				add   esp, 4
			}
		}
		// WaW sub_6917F0 — usercall(value1@eax, value2@ecx, inst@stack0) -> void ; caller-cleans
		inline void Scr_EvalGreater(VariableValue* value1, VariableValue* value2, scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_EvalGreater"));
			__asm
			{
				push  inst
				mov   eax, value1
				mov   ecx, value2
				call  fn
				add   esp, 4
			}
		}
		// WaW sub_691860 — usercall(inst@eax, value1@esi, value2@stack0) -> void ; caller-cleans
		inline void Scr_EvalLessEqual(scriptInstance_t inst, VariableValue* value1, VariableValue* value2)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_EvalLessEqual"));
			__asm
			{
				push  value2
				mov   eax, inst
				mov   esi, value1
				call  fn
				add   esp, 4
			}
		}
		// WaW sub_691880 — usercall(value1@eax, value2@ecx, inst@stack0) -> void ; caller-cleans
		inline void Scr_EvalShiftLeft(VariableValue* value1, VariableValue* value2, scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_EvalShiftLeft"));
			__asm
			{
				push  inst
				mov   eax, value1
				mov   ecx, value2
				call  fn
				add   esp, 4
			}
		}
		// WaW sub_6918B0 — usercall(value1@eax, value2@ecx, inst@stack0) -> void ; caller-cleans
		inline void Scr_EvalShiftRight(VariableValue* value1, VariableValue* value2, scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_EvalShiftRight"));
			__asm
			{
				push  inst
				mov   eax, value1
				mov   ecx, value2
				call  fn
				add   esp, 4
			}
		}
		// WaW sub_6918E0 — usercall(inst@ecx, value1@stack0, value2@stack1) -> void ; caller-cleans
		inline void Scr_EvalPlus(scriptInstance_t inst, VariableValue* value1, VariableValue* value2)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_EvalPlus"));
			__asm
			{
				push  value2
				push  value1
				mov   ecx, inst
				call  fn
				add   esp, 8
			}
		}
		// WaW sub_691B00 — usercall(value1@eax, inst@stack0, value2@stack1) -> void ; caller-cleans
		inline void Scr_EvalMinus(VariableValue* value1, scriptInstance_t inst, VariableValue* value2)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_EvalMinus"));
			__asm
			{
				push  value2
				push  inst
				mov   eax, value1
				call  fn
				add   esp, 8
			}
		}
		// WaW sub_691C20 — usercall(value1@eax, inst@stack0, value2@stack1) -> void ; caller-cleans
		inline void Scr_EvalMultiply(VariableValue* value1, scriptInstance_t inst, VariableValue* value2)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_EvalMultiply"));
			__asm
			{
				push  value2
				push  inst
				mov   eax, value1
				call  fn
				add   esp, 8
			}
		}
		// WaW sub_691D40 — usercall(value1@eax, inst@stack0, value2@stack1) -> void ; caller-cleans
		inline void Scr_EvalDivide(VariableValue* value1, scriptInstance_t inst, VariableValue* value2)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_EvalDivide"));
			__asm
			{
				push  value2
				push  inst
				mov   eax, value1
				call  fn
				add   esp, 8
			}
		}
		// WaW sub_691F00 — usercall(inst@eax, value1@ecx, value2@stack0) -> void ; caller-cleans
		// inst arrives in eax (used as Scr_Error inst in the divide-by-0 path: mov edi,eax; call Scr_Error).
		inline void Scr_EvalMod(scriptInstance_t inst, VariableValue* value1, VariableValue* value2)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_EvalMod"));
			__asm
			{
				push  value2
				mov   eax, inst
				mov   ecx, value1
				call  fn
				add   esp, 4
			}
		}
		// WaW sub_691F50 — usercall(inst@eax, value1@ecx, op@stack0, value2@stack1) -> void ; caller-cleans
		inline void Scr_EvalBinaryOperator(scriptInstance_t inst, VariableValue* value1, OpcodeVM op, VariableValue* value2)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_EvalBinaryOperator"));
			__asm
			{
				push  value2
				push  op
				mov   eax, inst
				mov   ecx, value1
				call  fn
				add   esp, 8
			}
		}
		// WaW sub_692090 — usercall(inst@ecx, resultId@eax, entnum@stack0) -> void ; caller-cleans
		inline void Scr_FreeEntityNum(scriptInstance_t inst, unsigned int resultId, unsigned int entnum)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_FreeEntityNum"));
			__asm
			{
				push  entnum
				mov   ecx, inst
				mov   eax, resultId
				call  fn
				add   esp, 4
			}
		}
		// WaW sub_692260 — usercall(inst@esi, classnum@stack0) -> void ; caller-cleans
		inline void Scr_SetClassMap(scriptInstance_t inst, unsigned int classnum)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_SetClassMap"));
			__asm
			{
				push  classnum
				mov   esi, inst
				call  fn
				add   esp, 4
			}
		}
		// WaW sub_6922E0 — usercall(classnum@eax, inst@edi) -> void ; no stack args
		inline void Scr_RemoveClassMap(unsigned int classnum, scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_RemoveClassMap"));
			__asm
			{
				mov   eax, classnum
				mov   edi, inst
				call  fn
			}
		}
		// WaW sub_692350 — usercall(inst@ecx, classnum@eax, name@stack0, fieldOffset@stack1) -> void ; caller-cleans
		// NB: 'offset' is a MASM operator → param renamed fieldOffset to keep inline asm valid.
		inline void Scr_AddClassField(scriptInstance_t inst, unsigned int classnum, const char* name, unsigned int fieldOffset)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_AddClassField"));
			__asm
			{
				push  fieldOffset
				push  name
				mov   ecx, inst
				mov   eax, classnum
				call  fn
				add   esp, 8
			}
		}
		// WaW sub_692440 — usercall(classNum@eax, inst@edi, name@stack0) -> VariableUnion@eax ; caller-cleans
		inline VariableUnion Scr_GetOffset(const char* name, scriptInstance_t inst, classNum_e classNum)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_GetOffset"));
			VariableUnion result;
			__asm
			{
				push  name
				mov   eax, classNum
				mov   edi, inst
				call  fn
				add   esp, 4
				mov   dword ptr result, eax
			}
			return result;
		}
		// WaW sub_6924C0 — usercall(entClass@eax, entNum@ecx, inst@edi) -> uint@eax ; no stack args
		inline unsigned int FindEntityId(unsigned int entClass, int entNum, scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("FindEntityId"));
			unsigned int result;
			__asm
			{
				mov   eax, entClass
				mov   ecx, entNum
				mov   edi, inst
				call  fn
				mov   result, eax
			}
			return result;
		}
		// WaW sub_692520 — usercall(entNum@eax, inst@stack0, classnum@stack1, clientnum@stack2) -> uint@eax ; caller-cleans
		inline unsigned int Scr_GetEntityId(int entNum, scriptInstance_t inst, classNum_e classnum, int clientnum)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_GetEntityId"));
			unsigned int result;
			__asm
			{
				push  clientnum
				push  classnum
				push  inst
				mov   eax, entNum
				call  fn
				add   esp, 0Ch
				mov   result, eax
			}
			return result;
		}
		// WaW sub_6925B0 — usercall(inst@eax, index@ecx, parentId@stack0) -> uint@eax ; caller-cleans
		inline unsigned int Scr_FindArrayIndex(scriptInstance_t inst, VariableValue* index, unsigned int parentId)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_FindArrayIndex"));
			unsigned int result;
			__asm
			{
				push  parentId
				mov   eax, inst
				mov   ecx, index
				call  fn
				add   esp, 4
				mov   result, eax
			}
			return result;
		}
		// WaW sub_692680 — usercall(inst@ecx, index@stack0, value@eax) -> void ; caller-cleans
		inline void Scr_EvalArray(scriptInstance_t inst, VariableValue* index, VariableValue* value)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_EvalArray"));
			__asm
			{
				push  index
				mov   ecx, inst
				mov   eax, value
				call  fn
				add   esp, 4
			}
		}
		// WaW sub_692850 — usercall(inst@ecx, parentId@eax) -> uint@eax ; no stack args
		inline unsigned int Scr_EvalArrayRef(scriptInstance_t inst, unsigned int parentId)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_EvalArrayRef"));
			unsigned int result;
			__asm
			{
				mov   ecx, inst
				mov   eax, parentId
				call  fn
				mov   result, eax
			}
			return result;
		}
		// WaW sub_692AF0 — usercall(inst@ecx, parentId@eax, value@stack0) -> void ; caller-cleans
		inline void ClearArray(unsigned int parentId, scriptInstance_t inst, VariableValue* value)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("ClearArray"));
			__asm
			{
				push  value
				mov   ecx, inst
				mov   eax, parentId
				call  fn
				add   esp, 4
			}
		}
		// WaW sub_692D70 — usercall(inst@edi, parentId@stack0) -> void ; caller-cleans
		inline void SetEmptyArray(scriptInstance_t inst, unsigned int parentId)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("SetEmptyArray"));
			__asm
			{
				push  parentId
				mov   edi, inst
				call  fn
				add   esp, 4
			}
		}
		// WaW sub_692EC0 — usercall(retstr@eax, inst@ecx, entId@stack0) -> scr_entref_t*@eax ; caller-cleans
		inline scr_entref_t* Scr_GetEntityIdRef(scr_entref_t* retstr, scriptInstance_t inst, unsigned int entId)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_GetEntityIdRef"));
			scr_entref_t* result;
			__asm
			{
				push  entId
				mov   eax, retstr
				mov   ecx, inst
				call  fn
				add   esp, 4
				mov   result, eax
			}
			return result;
		}
		// WaW sub_692F00 — usercall(parentId@eax, newParentId@stack0) -> void ; caller-cleans
		inline void CopyEntity(unsigned int parentId, unsigned int newParentId)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("CopyEntity"));
			__asm
			{
				push  newParentId
				mov   eax, parentId
				call  fn
				add   esp, 4
			}
		}
		// WaW sub_692FC0 — usercall(parentId@edi, inst@ecx) -> float@st0 ; no stack args
		inline float Scr_GetEndonUsage(unsigned int parentId, scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_GetEndonUsage"));
			float result;
			__asm
			{
				mov   edi, parentId
				mov   ecx, inst
				call  fn
				fstp  result
			}
			return result;
		}
		// WaW sub_693130 — usercall(stackBuf@eax, inst@ecx, endonUsage@stack0) -> float@xmm0 ; caller-cleans
		inline float Scr_GetThreadUsage(VariableStackBuffer* stackBuf, scriptInstance_t inst, float* endonUsage)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_GetThreadUsage"));
			float result;
			__asm
			{
				push  endonUsage
				mov   eax, stackBuf
				mov   ecx, inst
				call  fn
				add   esp, 4
				movss result, xmm0
			}
			return result;
		}
		// WaW sub_693250 — usercall(inst@eax, name@stack0, typeOut@stack1) -> uint@eax ; caller-cleans
		// NB: 'type' is a MASM operator → param renamed typeOut to keep inline asm valid.
		inline unsigned int Scr_FindField(scriptInstance_t inst, const char* name, int* typeOut)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_FindField"));
			unsigned int result;
			__asm
			{
				push  typeOut
				push  name
				mov   eax, inst
				call  fn
				add   esp, 8
				mov   result, eax
			}
			return result;
		}
		// WaW sub_693A10 — usercall(inst@eax, bComplete@stack0) -> void ; caller-cleans
		inline void Scr_FreeGameVariable(scriptInstance_t inst, int bComplete)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_FreeGameVariable"));
			__asm
			{
				push  bComplete
				mov   eax, inst
				call  fn
				add   esp, 4
			}
		}
		// WaW sub_693A70 — usercall(parentId@eax, strv@edx) -> bool@al ; no stack args
		inline bool Scr_SLHasLowercaseString(unsigned int parentId, const char* strv)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_SLHasLowercaseString"));
			bool result;
			__asm
			{
				mov   eax, parentId
				mov   edx, strv
				call  fn
				mov   result, al
			}
			return result;
		}

		// Warning Adress unknow for now
		WEAK symbol<unsigned int(scriptInstance_t inst, unsigned int id)>FindObject{ "FindObject" };
		// Warning Adress unknow for now
		WEAK symbol<unsigned int(scriptInstance_t inst, unsigned int id)>FindFirstSibling{ "FindFirstSibling" };
		// Warning Adress unknow for now
		WEAK symbol<unsigned int(scriptInstance_t inst, unsigned int id)>FindNextSibling{ "FindNextSibling" };
		// Warning Adress unknow for now
		WEAK symbol<VariableValue(scriptInstance_t inst, unsigned int name)>Scr_GetArrayIndexValue{ "Scr_GetArrayIndexValue" };
		// Warning Adress unknow for now
		WEAK symbol<float(scriptInstance_t inst, unsigned int type, VariableUnion u)>Scr_GetEntryUsageInternal{ "Scr_GetEntryUsageInternal" };
		// Warning Adress unknow for now
		WEAK symbol<float(scriptInstance_t inst, VariableValueInternal* entryValue)>Scr_GetEntryUsage{ "Scr_GetEntryUsage" };
		// Warning Adress unknow for now
		WEAK symbol<void(scriptInstance_t inst, unsigned int id)>AddRefToObject{ "AddRefToObject" };
		// Warning Adress unknow for now
		WEAK symbol<void(scriptInstance_t inst, unsigned int id)>RemoveRefToEmptyObject{ "RemoveRefToEmptyObject" };
		// Warning Adress unknow for now
		WEAK symbol<void(scriptInstance_t inst, unsigned int parentId)>Scr_ClearThread{ "Scr_ClearThread" };
		// Warning Adress unknow for now
		WEAK symbol<unsigned int(scriptInstance_t inst, unsigned int parentId, unsigned int id)>FindObjectVariable{ "FindObjectVariable" };
		// Warning Adress unknow for now
		WEAK symbol<void(scriptInstance_t inst, unsigned int parentId, unsigned int id)>RemoveObjectVariable{ "RemoveObjectVariable" };
		// Warning Adress unknow for now
		WEAK symbol<VariableValueInternal_u* (scriptInstance_t inst, unsigned int id)>GetVariableValueAddress{ "GetVariableValueAddress" };
		// Warning Adress unknow for now
		WEAK symbol<void(scriptInstance_t inst, unsigned int threadId)>Scr_KillEndonThread{ "Scr_KillEndonThread" };
		// Warning Adress unknow for now
		WEAK symbol<BOOL(scriptInstance_t inst, unsigned int unsignedValue)>IsValidArrayIndex{ "IsValidArrayIndex" };
		// Warning Adress unknow for now
		WEAK symbol<void(scriptInstance_t inst, unsigned int parentId, unsigned int unsignedValue)>RemoveArrayVariable{ "RemoveArrayVariable" };
		// Warning Adress unknow for now
		WEAK symbol<void(scriptInstance_t inst, unsigned int parentId, unsigned int unsignedValue)>SafeRemoveArrayVariable{ "SafeRemoveArrayVariable" };
		// Warning Adress unknow for now
		WEAK symbol<void(scriptInstance_t inst, const float* floatVal)>AddRefToVector{ "AddRefToVector" };
		// Warning Adress unknow for now
		WEAK symbol<unsigned int(scriptInstance_t inst, unsigned int parentId, unsigned int unsignedValue)>FindArrayVariableIndex{ "FindArrayVariableIndex" };
		// Warning Adress unknow for now
		WEAK symbol<unsigned int(scriptInstance_t inst, unsigned int parentId, unsigned int name)>GetVariableIndexInternal{ "GetVariableIndexInternal" };
		// Warning Adress unknow for now
		WEAK symbol<unsigned int(scriptInstance_t inst, unsigned int parentId, unsigned int name)>GetNewVariableIndexInternal{ "GetNewVariableIndexInternal" };
		// Warning Adress unknow for now
		WEAK symbol<unsigned int(scriptInstance_t inst)>AllocObject{ "AllocObject" };
		// Warning Adress unknow for now
		WEAK symbol<VariableType(scriptInstance_t inst, unsigned int id)>GetValueType{ "GetValueType" };
		// Warning Adress unknow for now
		WEAK symbol<VariableType(scriptInstance_t inst, unsigned int id)>GetObjectType{ "GetObjectType" };
		// Warning Adress unknow for now
		WEAK symbol<float* (scriptInstance_t inst, const float* v)>Scr_AllocVector_{ "Scr_AllocVector_" };
		// Warning Adress unknow for now
		WEAK symbol<void(scriptInstance_t inst, VariableValue* value1, VariableValue* value2)>Scr_EvalInequality{ "Scr_EvalInequality" };
		// Warning Adress unknow for now
		WEAK symbol<unsigned int(scriptInstance_t inst, VariableValue* varValue, VariableValueInternal* parentValue)>Scr_EvalArrayRefInternal{ "Scr_EvalArrayRefInternal" };
		// Warning Adress unknow for now
		WEAK symbol<unsigned int(scriptInstance_t inst, unsigned int parentId, unsigned int unsignedValue)>GetNewArrayVariableIndex{ "GetNewArrayVariableIndex" };
		// Warning Adress unknow for now
		WEAK symbol<unsigned int(scriptInstance_t inst, unsigned int parentId, unsigned int unsignedValue)>GetNewArrayVariable{ "GetNewArrayVariable" };
		// Warning Adress unknow for now
		WEAK symbol<unsigned int(scriptInstance_t inst, unsigned int parentId, unsigned int unsignedValue)>GetArrayVariable{ "GetArrayVariable" };
		// Warning Adress unknow for now
		WEAK symbol<unsigned int(scriptInstance_t inst, unsigned int self)>AllocThread{ "AllocThread" };
	}
} // namespace T4::engine
