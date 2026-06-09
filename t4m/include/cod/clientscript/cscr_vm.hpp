#pragma once

namespace T4 { namespace engine {
	WEAK symbol<void(scriptInstance_t inst)>Scr_VM_Init{ "Scr_VM_Init" };
	WEAK symbol<void(scriptInstance_t inst, unsigned int startLocalId, VariableStackBuffer* stackValue)>VM_UnarchiveStack{ "VM_UnarchiveStack" };
	WEAK symbol<unsigned int(scriptInstance_t inst)>VM_ExecuteInternal{ "VM_ExecuteInternal" };
	WEAK symbol<unsigned int(scriptInstance_t inst, unsigned int localId, const char* pos, unsigned int paramcount)>VM_Execute{ "VM_Execute" };
	WEAK symbol<void(scriptInstance_t inst, int bComplete)>Scr_ShutdownSystem{ "Scr_ShutdownSystem" };
	WEAK symbol<BOOL()>Scr_IsSystemActive{ "Scr_IsSystemActive" };
	WEAK symbol<scr_animtree_t()>Scr_GetAnimTree{ "Scr_GetAnimTree" };
	WEAK symbol<unsigned int()>Scr_GetFunc{ "Scr_GetFunc" };
	WEAK symbol<void(VariableUnion value)>Scr_AddAnim{ "Scr_AddAnim" };
	WEAK symbol<void(scriptInstance_t inst)>Scr_AddArray{ "Scr_AddArray" };

	// WaW sub_693C20 — usercall(inst@eax) -> void ; no stack args
	inline void Scr_Init(scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_Init"));
		__asm
		{
			mov   eax, inst
			call  fn
		}
	}
	// WaW sub_693C90 — usercall(inst@edi) -> void ; no stack args
	inline void Scr_Shutdown(scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_Shutdown"));
		__asm
		{
			mov   edi, inst
			call  fn
		}
	}
	// WaW sub_693CF0 — usercall(inst@eax) -> void ; no stack args
	inline void Scr_ErrorInternal(scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_ErrorInternal"));
		__asm
		{
			mov   eax, inst
			call  fn
		}
	}
	// WaW sub_693DA0 — usercall(inst@edi) -> void ; no stack args
	inline void Scr_ClearOutParams(scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_ClearOutParams"));
		__asm
		{
			mov   edi, inst
			call  fn
		}
	}
	// WaW sub_693DE0 — usercall(inst@edi) -> uint@eax ; no stack args
	inline unsigned int GetDummyObject(scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("GetDummyObject"));
		unsigned int result;
		__asm
		{
			mov   edi, inst
			call  fn
			mov   result, eax
		}
		return result;
	}
	// WaW sub_693E30 — usercall(inst@eax) -> uint@eax ; no stack args
	inline unsigned int GetDummyFieldValue(scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("GetDummyFieldValue"));
		unsigned int result;
		__asm
		{
			mov   eax, inst
			call  fn
			mov   result, eax
		}
		return result;
	}
	// WaW sub_6978C0 — usercall(inst@ecx, notifyListOwnerId@eax, startLocalId@stack0, notifyListId@stack1, notifyNameListId@stack2, stringValue@stack3) -> void ; caller-cleans
	inline void VM_CancelNotifyInternal(scriptInstance_t inst, unsigned int notifyListOwnerId, unsigned int startLocalId, unsigned int notifyListId, unsigned int notifyNameListId, unsigned int stringValue)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("VM_CancelNotifyInternal"));
		__asm
		{
			push  stringValue
			push  notifyNameListId
			push  notifyListId
			push  startLocalId
			mov   ecx, inst
			mov   eax, notifyListOwnerId
			call  fn
			add   esp, 10h
		}
	}
	// WaW sub_697950 — usercall(inst@edi, a2@stack0, a3@stack1) -> void ; caller-cleans
	inline void VM_CancelNotify(scriptInstance_t inst, unsigned int a2, unsigned int a3)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("VM_CancelNotify"));
		__asm
		{
			push  a3
			push  a2
			mov   edi, inst
			call  fn
			add   esp, 8
		}
	}
	// WaW sub_697A00 — usercall(inst@eax) -> VariableStackBuffer*@eax ; no stack args
	inline VariableStackBuffer* VM_ArchiveStack(scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("VM_ArchiveStack"));
		VariableStackBuffer* result;
		__asm
		{
			mov   eax, inst
			call  fn
			mov   result, eax
		}
		return result;
	}
	// WaW sub_697B60 — usercall(inst@eax, a2@edx) -> int@eax ; no stack args
	inline int Scr_AddLocalVars(scriptInstance_t inst, unsigned int a2)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_AddLocalVars"));
		int result;
		__asm
		{
			mov   eax, inst
			mov   edx, a2
			call  fn
			mov   result, eax
		}
		return result;
	}
	// WaW sub_697D80 — usercall(inst@esi, endLocalId@stack0, startLocalId@stack1, name@stack2) -> void ; caller-cleans
	inline void VM_TerminateStack(scriptInstance_t inst, unsigned int endLocalId, unsigned int startLocalId, VariableStackBuffer* name)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("VM_TerminateStack"));
		__asm
		{
			push  name
			push  startLocalId
			push  endLocalId
			mov   esi, inst
			call  fn
			add   esp, 0Ch
		}
	}
	// WaW sub_697F20 — usercall(inst@eax, parentId@stack0, a3@stack1, fromEndon@stack2) -> void ; caller-cleans
	inline void VM_TrimStack(scriptInstance_t inst, unsigned int parentId, VariableStackBuffer* a3, int fromEndon)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("VM_TrimStack"));
		__asm
		{
			push  fromEndon
			push  a3
			push  parentId
			mov   eax, inst
			call  fn
			add   esp, 0Ch
		}
	}
	// WaW sub_698150 — usercall(inst@edx, a2@stack0) -> void ; caller-cleans
	inline void Scr_TerminateRunningThread(scriptInstance_t inst, unsigned int a2)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_TerminateRunningThread"));
		__asm
		{
			push  a2
			mov   edx, inst
			call  fn
			add   esp, 4
		}
	}
	// WaW sub_698200 — usercall(inst@eax, a2@stack0, a3@stack1) -> void ; caller-cleans
	inline void Scr_TerminateWaitThread(scriptInstance_t inst, unsigned int a2, unsigned int a3)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_TerminateWaitThread"));
		__asm
		{
			push  a3
			push  a2
			mov   eax, inst
			call  fn
			add   esp, 8
		}
	}
	// WaW sub_698310 — usercall(inst@ecx, startLocalId@eax) -> void ; no stack args
	inline void Scr_CancelWaittill(scriptInstance_t inst, unsigned int startLocalId)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_CancelWaittill"));
		__asm
		{
			mov   ecx, inst
			mov   eax, startLocalId
			call  fn
		}
	}
	// WaW sub_698400 — usercall(inst@eax, a2@stack0, a3@stack1) -> void ; caller-cleans
	inline void Scr_TerminateWaittillThread(scriptInstance_t inst, unsigned int a2, unsigned int a3)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_TerminateWaittillThread"));
		__asm
		{
			push  a3
			push  a2
			mov   eax, inst
			call  fn
			add   esp, 8
		}
	}
	// WaW sub_698610 — usercall(a2@edi, inst@esi) -> void ; caller-cleans, no stack args
	inline void Scr_TerminateThread(unsigned int a2, scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_TerminateThread"));
		__asm
		{
			mov   edi, a2
			mov   esi, inst
			call  fn
		}
	}
	// WaW sub_698670 — usercall(inst@eax, notifyListOwnerId@stack0, stringValue@stack1, top@stack2) -> void ; caller-cleans
	inline void VM_Notify(scriptInstance_t inst, int notifyListOwnerId, unsigned int stringValue, VariableValue* top)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("VM_Notify"));
		__asm
		{
			push  top
			push  stringValue
			push  notifyListOwnerId
			mov   eax, inst
			call  fn
			add   esp, 0Ch
		}
	}
	// WaW sub_698CC0 — usercall(inst@eax, entNum@stack0, entClass@stack1, notifStr@stack2, numParams@stack3) -> void ; caller-cleans
	inline void Scr_NotifyNum_Internal(scriptInstance_t inst, int entNum, int entClass, unsigned int notifStr, int numParams)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_NotifyNum_Internal"));
		__asm
		{
			push  numParams
			push  notifStr
			push  entClass
			push  entNum
			mov   eax, inst
			call  fn
			add   esp, 10h
		}
	}
	// WaW sub_698DE0 — usercall(notifyListOwnerId@eax, inst@stack0) -> void ; caller-cleans
	inline void Scr_CancelNotifyList(unsigned int notifyListOwnerId, scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_CancelNotifyList"));
		__asm
		{
			push  inst
			mov   eax, notifyListOwnerId
			call  fn
			add   esp, 4
		}
	}
	// WaW sub_698FE0 — usercall(inst@eax, parentId@stack0) -> void ; caller-cleans
	inline void VM_TerminateTime(scriptInstance_t inst, unsigned int parentId)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("VM_TerminateTime"));
		__asm
		{
			push  parentId
			mov   eax, inst
			call  fn
			add   esp, 4
		}
	}
	// WaW sub_6990E0 — usercall(inst@eax, timeId@stack0) -> void ; caller-cleans
	inline void VM_Resume(scriptInstance_t inst, unsigned int timeId)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("VM_Resume"));
		__asm
		{
			push  timeId
			mov   eax, inst
			call  fn
			add   esp, 4
		}
	}
	// WaW sub_699560 — usercall(inst@edi, handle@stack0, paramCount@stack1) -> ushort@ax ; caller-cleans
	inline unsigned short Scr_ExecThread(scriptInstance_t inst, unsigned int handle, unsigned int paramCount)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_ExecThread"));
		unsigned short result;
		__asm
		{
			push  paramCount
			push  handle
			mov   edi, inst
			call  fn
			add   esp, 8
			mov   result, ax
		}
		return result;
	}
	// WaW sub_699640 — usercall(inst@edi, entNum@stack0, handle@stack1, numParams@stack2, clientNum@stack3) -> ushort@ax ; caller-cleans
	inline unsigned short Scr_ExecEntThreadNum(scriptInstance_t inst, int entNum, unsigned int handle, int numParams, unsigned int clientNum)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_ExecEntThreadNum"));
		unsigned short result;
		__asm
		{
			push  clientNum
			push  numParams
			push  handle
			push  entNum
			mov   edi, inst
			call  fn
			add   esp, 10h
			mov   result, ax
		}
		return result;
	}
	// WaW sub_699730 — usercall(inst@edi, handle@stack0) -> void ; caller-cleans
	inline void Scr_AddExecThread(scriptInstance_t inst, unsigned int handle)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_AddExecThread"));
		__asm
		{
			push  handle
			mov   edi, inst
			call  fn
			add   esp, 4
		}
	}
	// WaW sub_6997E0 — usercall(inst@eax) -> void ; caller-cleans, no stack args
	inline void VM_SetTime(scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("VM_SetTime"));
		__asm
		{
			mov   eax, inst
			call  fn
		}
	}
	// WaW sub_699860 — usercall(inst@edi) -> void ; caller-cleans, no stack args
	// The true Scr_InitSystem (allocates per-instance script var slots). Not to be
	// confused with sub_60C5C0 = XAnimInit, which older code mislabeled "Scr_InitSystem".
	inline void Scr_InitSystem(scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_InitSystem"));
		__asm
		{
			mov   edi, inst
			call  fn
		}
	}
	// WaW sub_699C50 — usercall(inst@eax, index@ecx) -> int@eax ; caller-cleans, no stack args
	inline int Scr_GetInt(scriptInstance_t inst, unsigned int index)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_GetInt"));
		int result;
		__asm
		{
			mov   eax, inst
			mov   ecx, index
			call  fn
			mov   result, eax
		}
		return result;
	}
	// WaW sub_699CE0 — usercall(index@eax, anims@ecx) -> scr_anim_s(4 bytes)@eax ; caller-cleans, no stack args
	inline scr_anim_s Scr_GetAnim(unsigned int index, XAnimTree_s* anims)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_GetAnim"));
		scr_anim_s result;
		__asm
		{
			mov   eax, index
			mov   ecx, anims
			call  fn
			mov   dword ptr result, eax
		}
		return result;
	}
	// WaW sub_699E90 — usercall(inst@eax, index@ecx) -> float@xmm0 ; caller-cleans, no stack args
	inline float Scr_GetFloat(scriptInstance_t inst, unsigned int index)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_GetFloat"));
		float result;
		__asm
		{
			mov   eax, inst
			mov   ecx, index
			call  fn
			movss result, xmm0
		}
		return result;
	}
	// WaW sub_699F30 — usercall(inst@eax, index@stack0) -> uint@eax ; caller-cleans
	inline unsigned int Scr_GetConstString(scriptInstance_t inst, unsigned int index)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_GetConstString"));
		unsigned int result;
		__asm
		{
			push  index
			mov   eax, inst
			call  fn
			add   esp, 4
			mov   result, eax
		}
		return result;
	}
	// WaW sub_699FB0 — usercall(inst@ecx, index@stack0) -> uint@eax ; caller-cleans
	inline unsigned int Scr_GetConstLowercaseString(scriptInstance_t inst, unsigned int index)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_GetConstLowercaseString"));
		unsigned int result;
		__asm
		{
			push  index
			mov   ecx, inst
			call  fn
			add   esp, 4
			mov   result, eax
		}
		return result;
	}
	// WaW sub_69A0D0 — usercall(index@eax, inst@esi) -> const char*@eax ; caller-cleans, no stack args
	inline const char* Scr_GetString(unsigned int index, scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_GetString"));
		const char* result;
		__asm
		{
			mov   eax, index
			mov   esi, inst
			call  fn
			mov   result, eax
		}
		return result;
	}
	// WaW sub_69A100 — usercall(inst@eax) -> uint@eax ; caller-cleans, no stack args
	inline unsigned int Scr_GetConstStringIncludeNull(scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_GetConstStringIncludeNull"));
		unsigned int result;
		__asm
		{
			mov   eax, inst
			call  fn
			mov   result, eax
		}
		return result;
	}
	// WaW sub_69A130 — usercall(inst@eax, index@ecx) -> char*@eax ; caller-cleans, no stack args
	inline char* Scr_GetDebugString(scriptInstance_t inst, unsigned int index)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_GetDebugString"));
		char* result;
		__asm
		{
			mov   eax, inst
			mov   ecx, index
			call  fn
			mov   result, eax
		}
		return result;
	}
	// WaW sub_69A1A0 — usercall(index@eax) -> uint@eax ; caller-cleans, no stack args
	inline unsigned int Scr_GetConstIString(unsigned int index)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_GetConstIString"));
		unsigned int result;
		__asm
		{
			mov   eax, index
			call  fn
			mov   result, eax
		}
		return result;
	}
	// WaW sub_69A220 — usercall(inst@eax, value@ecx, index@stack0) -> void ; caller-cleans
	inline void Scr_GetVector(scriptInstance_t inst, float* value, unsigned int index)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_GetVector"));
		__asm
		{
			push  index
			mov   eax, inst
			mov   ecx, value
			call  fn
			add   esp, 4
		}
	}
	// WaW sub_69A330 — usercall(inst@eax, retstr@stack0, index@stack1) -> scr_entref_t*@eax (struct-ret buf) ; caller-cleans
	inline scr_entref_t* Scr_GetEntityRef(scriptInstance_t inst, scr_entref_t* retstr, unsigned int index)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_GetEntityRef"));
		scr_entref_t* result;
		__asm
		{
			push  index
			push  retstr
			mov   eax, inst
			call  fn
			add   esp, 8
			mov   result, eax
		}
		return result;
	}
	// WaW sub_69A460 — usercall(inst@eax) -> VariableUnion@eax ; caller-cleans
	inline VariableUnion Scr_GetObject(scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_GetObject"));
		VariableUnion result;
		__asm
		{
			mov   eax, inst
			call  fn
			mov   dword ptr result, eax
		}
		return result;
	}
	// WaW sub_69A4E0 — usercall(inst@eax, index@ecx) -> VariableType@eax ; caller-cleans
	inline VariableType Scr_GetType(scriptInstance_t inst, unsigned int index)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_GetType"));
		VariableType result;
		__asm
		{
			mov   eax, inst
			mov   ecx, index
			call  fn
			mov   result, eax
		}
		return result;
	}
	// WaW sub_69A530 — usercall(inst@eax) -> const char*@eax ; caller-cleans
	inline const char* Scr_GetTypeName(scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_GetTypeName"));
		const char* result;
		__asm
		{
			mov   eax, inst
			call  fn
			mov   result, eax
		}
		return result;
	}
	// WaW sub_69A580 — usercall(inst@eax, a2@ecx) -> VariableType@eax ; caller-cleans
	inline VariableType Scr_GetPointerType(scriptInstance_t inst, unsigned int a2)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_GetPointerType"));
		VariableType result;
		__asm
		{
			mov   eax, inst
			mov   ecx, a2
			call  fn
			mov   result, eax
		}
		return result;
	}
	// WaW sub_69A610 — usercall(inst@eax, value@stack0) -> void ; caller-cleans
	inline void Scr_AddInt(scriptInstance_t inst, int value)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_AddInt"));
		__asm
		{
			push  value
			mov   eax, inst
			call  fn
			add   esp, 4
		}
	}
	// WaW sub_69A670 — usercall(inst@eax, value@stack0 float-as-dword) -> void ; caller-cleans
	inline void Scr_AddFloat(scriptInstance_t inst, float value)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_AddFloat"));
		__asm
		{
			push  value
			mov   eax, inst
			call  fn
			add   esp, 4
		}
	}
	// WaW sub_69A720 — usercall(inst@eax) -> void ; caller-cleans
	inline void Scr_AddUndefined(scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_AddUndefined"));
		__asm
		{
			mov   eax, inst
			call  fn
		}
	}
	// WaW sub_69A770 — usercall(inst@eax, entid@esi) -> void ; caller-cleans
	inline void Scr_AddObject(scriptInstance_t inst, unsigned int entid)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_AddObject"));
		__asm
		{
			mov   eax, inst
			mov   esi, entid
			call  fn
		}
	}
	// WaW sub_69A7E0 — usercall(inst@eax, string@stack0) -> void ; caller-cleans
	inline void Scr_AddString(scriptInstance_t inst, const char* string)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_AddString"));
		__asm
		{
			push  string
			mov   eax, inst
			call  fn
			add   esp, 4
		}
	}
	// WaW sub_69A860 — usercall(string@esi) -> void ; caller-cleans, no stack args
	inline void Scr_AddIString(char* string)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_AddIString"));
		__asm
		{
			mov   esi, string
			call  fn
		}
	}
	// WaW sub_69A8D0 — usercall(inst@eax, id@esi) -> void ; caller-cleans, no stack args
	inline void Scr_AddConstString(scriptInstance_t inst, unsigned int id)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_AddConstString"));
		__asm
		{
			mov   esi, id
			mov   eax, inst
			call  fn
		}
	}
	// WaW sub_69A940 — usercall(inst@eax, value@stack0) -> void ; caller-cleans
	inline void Scr_AddVector(scriptInstance_t inst, float* value)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_AddVector"));
		__asm
		{
			push  value
			mov   eax, inst
			call  fn
			add   esp, 4
		}
	}
	// WaW sub_69A9D0 — usercall(inst@eax) -> void ; caller-cleans, no stack args
	inline void Scr_MakeArray(scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_MakeArray"));
		__asm
		{
			mov   eax, inst
			call  fn
		}
	}
	// WaW sub_69AAF0 — usercall(id@ecx, inst@edi) -> void ; caller-cleans, no stack args
	inline void Scr_AddArrayStringIndexed(unsigned int id, scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_AddArrayStringIndexed"));
		__asm
		{
			mov   ecx, id
			mov   edi, inst
			call  fn
		}
	}
	// WaW sub_69AB70 — usercall(err@ecx, inst@edi, is_terminal@stack0) -> void ; caller-cleans
	inline void Scr_Error(const char* err, scriptInstance_t inst, int is_terminal)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_Error"));
		__asm
		{
			push  is_terminal
			mov   ecx, err
			mov   edi, inst
			call  fn
			add   esp, 4
		}
	}
	// WaW sub_69ABD0 — usercall(inst@eax, Source@stack0) -> void ; caller-cleans
	inline void Scr_TerminalError(scriptInstance_t inst, const char* Source)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_TerminalError"));
		__asm
		{
			push  Source
			mov   eax, inst
			call  fn
			add   esp, 4
		}
	}
	// WaW sub_69AC00 — usercall(a1@eax, a2@ecx, Source@stack0) -> void ; caller-cleans
	inline void Scr_ParamError(int a1, scriptInstance_t a2, const char* Source)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_ParamError"));
		__asm
		{
			push  Source
			mov   eax, a1
			mov   ecx, a2
			call  fn
			add   esp, 4
		}
	}
	// WaW sub_69AC30 — usercall(inst@eax, a2@ecx) -> void ; caller-cleans, no stack args
	inline void Scr_ObjectError(scriptInstance_t inst, const char* a2)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_ObjectError"));
		__asm
		{
			mov   ecx, a2
			mov   eax, inst
			call  fn
		}
	}
	// WaW sub_69AC50 — usercall(inst@edi, fieldOffset@eax, classnum@ecx, entnum@stack0, clientNum@stack1, value@stack2) -> bool@al ; caller-cleans
	// NB: 'offset' is a MASM operator → param renamed fieldOffset to keep inline asm valid.
	inline bool SetEntityFieldValue(scriptInstance_t inst, int fieldOffset, int entnum, classNum_e classnum, int clientNum, VariableValue* value)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("SetEntityFieldValue"));
		bool result;
		__asm
		{
			push  value
			push  clientNum
			push  entnum
			mov   edi, inst
			mov   eax, fieldOffset
			mov   ecx, classnum
			call  fn
			add   esp, 0Ch
			mov   result, al
		}
		return result;
	}
	// WaW sub_69ACE0 — usercall(fieldOffset@eax, classnum@ecx, inst@stack0, entnum@stack1, clientNum@stack2) -> VariableValue in edx:eax ; caller-cleans
	// NB: 'offset' is a MASM operator → param renamed fieldOffset to keep inline asm valid.
	inline VariableValue GetEntityFieldValue(int fieldOffset, int entnum, scriptInstance_t inst, int classnum, int clientNum)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("GetEntityFieldValue"));
		VariableValue result;
		__asm
		{
			push  clientNum
			push  entnum
			push  inst
			mov   eax, fieldOffset
			mov   ecx, classnum
			call  fn
			add   esp, 0Ch
			lea   ecx, result
			mov   [ecx], eax        // VariableUnion value
			mov   [ecx+4], edx      // type tag
		}
		return result;
	}
	// WaW sub_69AD50 — usercall(a1@eax, a2@ecx, inst@stack0) -> void ; caller-cleans
	inline void Scr_SetStructField(unsigned int a1, unsigned int a2, scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_SetStructField"));
		__asm
		{
			push  inst
			mov   eax, a1
			mov   ecx, a2
			call  fn
			add   esp, 4
		}
	}
	// WaW sub_69ADE0 — usercall(inst@eax) -> void ; caller-cleans, no stack args
	inline void Scr_IncTime(scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_IncTime"));
		__asm
		{
			mov   eax, inst
			call  fn
		}
	}
	// WaW sub_69AE30 — usercall(inst@esi) -> void ; caller-cleans, no stack args
	inline void Scr_RunCurrentThreads(scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_RunCurrentThreads"));
		__asm
		{
			mov   esi, inst
			call  fn
		}
	}
	// WaW sub_69AE60 — usercall(inst@eax) -> void ; caller-cleans, no stack args
	inline void Scr_ResetTimeout(scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_ResetTimeout"));
		__asm
		{
			mov   eax, inst
			call  fn
		}
	}

	// Warning Adress unknow for now
	WEAK symbol<void(scriptInstance_t inst, unsigned int id, VariableValue* value)>SetVariableFieldValue{ "SetVariableFieldValue" };
	// Warning Adress unknow for now
	WEAK symbol<void(scriptInstance_t inst, unsigned int id, VariableValue* value)>SetNewVariableValue{ "SetNewVariableValue" };
	// Warning Adress unknow for now
	WEAK symbol<void(scriptInstance_t inst)>Scr_ClearErrorMessage{ "Scr_ClearErrorMessage" };
	// Warning Adress unknow for now
	WEAK symbol<void(scriptInstance_t inst)>VM_Shutdown{ "VM_Shutdown" };
	// Warning Adress unknow for now
	WEAK symbol<void(scriptInstance_t inst)>Scr_ShutdownVariables{ "Scr_ShutdownVariables" };
	// Warning Adress unknow for now
	WEAK symbol<void(scriptInstance_t inst, unsigned int id)>ClearVariableValue{ "ClearVariableValue" };
	// Warning Adress unknow for now
	WEAK symbol<unsigned int(scriptInstance_t inst, unsigned int startLocalId)>Scr_GetThreadNotifyName{ "Scr_GetThreadNotifyName" };
	// Warning Adress unknow for now
	WEAK symbol<void(scriptInstance_t inst, unsigned int startLocalId)>Scr_RemoveThreadNotifyName{ "Scr_RemoveThreadNotifyName" };
	// Warning Adress unknow for now
	WEAK symbol<unsigned int(scriptInstance_t inst, unsigned int id)>GetArraySize{ "GetArraySize" };
	// Warning Adress unknow for now
	WEAK symbol<void(scriptInstance_t inst)>IncInParam{ "IncInParam" };
	// Warning Adress unknow for now
	WEAK symbol<unsigned int(scriptInstance_t inst, unsigned int threadId)>GetParentLocalId{ "GetParentLocalId" };
	// Warning Adress unknow for now
	WEAK symbol<void(scriptInstance_t inst, unsigned int startLocalId)>Scr_ClearWaitTime{ "Scr_ClearWaitTime" };
	// Warning Adress unknow for now
	WEAK symbol<void(scriptInstance_t inst, unsigned int startLocalId, unsigned int waitTime)>Scr_SetThreadWaitTime{ "Scr_SetThreadWaitTime" };
	// Warning Adress unknow for now
	WEAK symbol<void(scriptInstance_t inst, unsigned int startLocalId, unsigned int stringValue)>Scr_SetThreadNotifyName{ "Scr_SetThreadNotifyName" };
	// Warning Adress unknow for now
	WEAK symbol<void(scriptInstance_t inst, int topThread)>Scr_DebugTerminateThread{ "Scr_DebugTerminateThread" };
	// Warning Adress unknow for now
	WEAK symbol<unsigned int(scriptInstance_t inst, unsigned int startLocalId)>Scr_GetThreadWaitTime{ "Scr_GetThreadWaitTime" };
	// Warning Adress unknow for now
	WEAK symbol<const char*(scriptInstance_t inst, unsigned int endLocalId, VariableStackBuffer* stackValue, bool killThread)>Scr_GetStackThreadPos{ "Scr_GetStackThreadPos" };
	// Warning Adress unknow for now
	WEAK symbol<unsigned int(scriptInstance_t inst, unsigned int threadId)>Scr_GetSelf{ "Scr_GetSelf" };
	// Warning Adress unknow for now
	WEAK symbol<unsigned int(scriptInstance_t inst, unsigned int id)>GetVariableKeyObject{ "GetVariableKeyObject" };
	// Warning Adress unknow for now
	WEAK symbol<int(scriptInstance_t inst, int oldNumBytes, int newNumbytes)>MT_Realloc{ "MT_Realloc" };
	// Warning Adress unknow for now
	WEAK symbol<void(classNum_e classnum, int entnum, int clientNum, int offset)>CScr_GetObjectField{ "CScr_GetObjectField" };
	// Warning Adress unknow for now
	WEAK symbol<int(classNum_e classnum, int entnum, int clientNum, int offset)>CScr_SetObjectField{ "CScr_SetObjectField" };
	// Warning Adress unknow for now
	WEAK symbol<void(scriptInstance_t inst, const char* error)>Scr_SetErrorMessage{ "Scr_SetErrorMessage" };
	// Warning Adress unknow for now
	WEAK symbol<bool(scriptInstance_t inst)>Scr_IsStackClear{ "Scr_IsStackClear" };
	// Warning Adress unknow for now
	WEAK symbol<void(scriptInstance_t inst, unsigned int stringValue)>SL_CheckExists{ "SL_CheckExists" };
	// Warning Adress unknow for now
	WEAK symbol<const char*(scriptInstance_t inst, const char** pos)>Scr_ReadCodePos{ "Scr_ReadCodePos" };
	// Warning Adress unknow for now
	WEAK symbol<unsigned int(scriptInstance_t inst, const char** pos)>Scr_ReadUnsignedInt{ "Scr_ReadUnsignedInt" };
	// Warning Adress unknow for now
	WEAK symbol<unsigned short(scriptInstance_t inst, const char** pos)>Scr_ReadUnsignedShort{ "Scr_ReadUnsignedShort" };
	// Warning Adress unknow for now
	WEAK symbol<unsigned char(scriptInstance_t inst, const char** pos)>Scr_ReadUnsignedByte{ "Scr_ReadUnsignedByte" };
	// Warning Adress unknow for now
	WEAK symbol<float(scriptInstance_t inst, const char** pos)>Scr_ReadFloat{ "Scr_ReadFloat" };
	// Warning Adress unknow for now
	WEAK symbol<const float*(scriptInstance_t inst, const char** pos)>Scr_ReadVector{ "Scr_ReadVector" };
	// Warning Adress unknow for now
	WEAK symbol<BOOL(scriptInstance_t inst, unsigned int id)>IsFieldObject{ "IsFieldObject" };
	// Warning Adress unknow for now
	WEAK symbol<void(scriptInstance_t inst, unsigned int parentId, unsigned int index)>RemoveVariableValue{ "RemoveVariableValue" };
	// Warning Adress unknow for now
	WEAK symbol<VariableStackBuffer*(scriptInstance_t inst, int id)>GetRefVariableStackBuffer{ "GetRefVariableStackBuffer" };
	// Warning Adress unknow for now
	WEAK symbol<unsigned int(scriptInstance_t inst, unsigned int parentId, unsigned int id)>GetNewObjectVariableReverse{ "GetNewObjectVariableReverse" };
	// Warning Adress unknow for now
	WEAK symbol<unsigned int(scriptInstance_t inst, unsigned int parentId, unsigned int name)>GetNewVariableIndexReverseInternal{ "GetNewVariableIndexReverseInternal" };
	// Warning Adress unknow for now
	WEAK symbol<unsigned int(scriptInstance_t inst, int pos)>Scr_GetLocalVar{ "Scr_GetLocalVar" };
	// Warning Adress unknow for now
	WEAK symbol<void(scriptInstance_t inst, VariableValue* value)>Scr_EvalBoolNot{ "Scr_EvalBoolNot" };
	// Warning Adress unknow for now
	WEAK symbol<unsigned int(scriptInstance_t inst, unsigned int unsignedValue)>GetInternalVariableIndex{ "GetInternalVariableIndex" };
	// Warning Adress unknow for now
	WEAK symbol<const char*(scriptInstance_t inst, const char** pos, unsigned int count)>Scr_ReadData{ "Scr_ReadData" };
	// Warning Adress unknow for now
	WEAK symbol<unsigned int(scriptInstance_t inst)>Scr_GetNumParam{ "Scr_GetNumParam" };
} } // namespace T4::engine
