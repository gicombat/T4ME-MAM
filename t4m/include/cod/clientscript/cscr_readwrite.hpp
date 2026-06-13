#pragma once

namespace T4
{
	namespace engine
	{
		// WaW sub_68BC20 — usercall(inst@eax, name@stack0, index@stack1) -> uint@eax ; caller-cleans
		inline unsigned int FindVariableIndexInternal2(scriptInstance_t inst, unsigned int name, unsigned int index)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("FindVariableIndexInternal2"));
			unsigned int result;
			__asm
			{
				push  index
				push  name
				mov   eax, inst
				call  fn
				add   esp, 8
				mov   result, eax
			}
			return result;
		}
		// WaW sub_68BCA0 — usercall(parentId@edx, inst@esi) -> uint@eax ; no stack args
		inline unsigned int FindLastSibling(unsigned int parentId, scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("FindLastSibling"));
			unsigned int result;
			__asm
			{
				mov   edx, parentId
				mov   esi, inst
				call  fn
				mov   result, eax
			}
			return result;
		}

		// Warning Adress unknow for now
		WEAK symbol<unsigned int(scriptInstance_t inst, unsigned int parentId, unsigned int name)>FindVariableIndexInternal{ "FindVariableIndexInternal" };
	}
} // namespace T4::engine