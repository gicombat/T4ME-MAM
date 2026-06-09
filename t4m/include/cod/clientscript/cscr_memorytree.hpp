#pragma once

namespace T4 { namespace engine {
	WEAK symbol<int(scriptInstance_t inst, int nodeNum)>MT_GetSubTreeSize{ "MT_GetSubTreeSize" };
	WEAK symbol<void(scriptInstance_t inst)>MT_DumpTree{ "MT_DumpTree" };
	WEAK symbol<void(scriptInstance_t inst, int newNode, int size)>MT_AddMemoryNode{ "MT_AddMemoryNode" };
	WEAK symbol<char(scriptInstance_t inst, int oldNode, int size)>MT_RemoveMemoryNode{ "MT_RemoveMemoryNode" };
	WEAK symbol<void(scriptInstance_t inst, int size)>MT_RemoveHeadMemoryNode{ "MT_RemoveHeadMemoryNode" };
	WEAK symbol<void(void* p, int numBytes, scriptInstance_t inst)>MT_Free{ "MT_Free" };

	// WaW sub_68A010 — usercall(inst@ecx) -> void ; no stack args
	inline void MT_InitBits(scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("MT_InitBits"));
		__asm
		{
			mov   ecx, inst
			call  fn
		}
	}
	
	// WaW sub_68A080 — usercall(inst@edx, num@stack0) -> int@eax ; caller-cleans
	inline int MT_GetScore(scriptInstance_t inst, int num)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("MT_GetScore"));
		int result;
		__asm
		{
			push  num
			mov   edx, inst
			call  fn
			add   esp, 4
			mov   result, eax
		}
		return result;
	}
	
	// WaW sub_68A4F0 — usercall(inst@edi) -> void ; no stack args
	inline void MT_Init(scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("MT_Init"));
		__asm
		{
			mov   edi, inst
			call  fn
		}
	}
	
	// WaW sub_68A580 — usercall(inst@eax, funcName@stack0, numBytes@stack1) -> void ; caller-cleans
	inline void MT_Error(scriptInstance_t inst, const char* funcName, int numBytes)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("MT_Error"));
		__asm
		{
			push  numBytes
			push  funcName
			mov   eax, inst
			call  fn
			add   esp, 8
		}
	}
	
	// WaW sub_68A5D0 — usercall(numBytes@eax, inst@ecx) -> int@eax ; no stack args
	inline int MT_GetSize(int numBytes, scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("MT_GetSize"));
		int result;
		__asm
		{
			mov   eax, numBytes
			mov   ecx, inst
			call  fn
			mov   result, eax
		}
		return result;
	}
	
	// WaW sub_68A630 — usercall(inst@edi, size_@stack0) -> u16@ax ; caller-cleans
	inline unsigned __int16 MT_AllocIndex(scriptInstance_t inst, int size_)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("MT_AllocIndex"));
		unsigned __int16 result;
		__asm
		{
			push  size_
			mov   edi, inst
			call  fn
			add   esp, 4
			mov   result, ax
		}
		return result;
	}
	
	// WaW sub_68A750 — usercall(numBytes@eax, inst@stack0, nodeNum@stack1) -> void ; caller-cleans
	inline void MT_FreeIndex(int numBytes, scriptInstance_t inst, int nodeNum)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("MT_FreeIndex"));
		__asm
		{
			push  nodeNum
			push  inst
			mov   eax, numBytes
			call  fn
			add   esp, 8
		}
	}
	
	// WaW sub_68A7D0 — usercall(numBytes@eax, inst@ecx) -> char*@eax ; no stack args
	inline char* MT_Alloc(int numBytes, scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("MT_Alloc"));
		char* result;
		__asm
		{
			mov   eax, numBytes
			mov   ecx, inst
			call  fn
			mov   result, eax
		}
		return result;
	}

	// Warning Adress unknow for now
	WEAK symbol<RefVector*(scriptInstance_t inst, unsigned int id)>GetRefVector{ "GetRefVector" };
} } // namespace T4::engine