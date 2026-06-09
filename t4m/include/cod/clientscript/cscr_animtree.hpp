#pragma once

namespace T4
{
	namespace engine
	{
		WEAK symbol<void(scriptInstance_t inst, const char* errorMsg)>AnimTreeCompileError{ "AnimTreeCompileError" };
		WEAK symbol<int(scriptInstance_t inst)>GetAnimTreeParseProperties{ "GetAnimTreeParseProperties" };
		WEAK symbol<char(scriptInstance_t inst, int parentId, int names, int bIncludeParent, int bLoop, int bComplete)>AnimTreeParseInternal{ "AnimTreeParseInternal" };
		WEAK symbol<int(scriptInstance_t inst, unsigned int parentNode)>Scr_GetAnimTreeSize{ "Scr_GetAnimTreeSize" };
		WEAK symbol<int(scriptInstance_t inst, unsigned int parentNode, unsigned int rootData, XAnim_s* animtree, unsigned int childIndex, const char* name, unsigned int parentIndex, unsigned int filename, int treeIndex, int flags)>Scr_CreateAnimationTree{ "Scr_CreateAnimationTree" };
		WEAK symbol<void(scriptInstance_t inst, unsigned int parentNode)>Scr_PrecacheAnimationTree{ "Scr_PrecacheAnimationTree" };
		WEAK symbol<void(const char* animtreeName)>Scr_SetAnimTreeConfigstring{ "Scr_SetAnimTreeConfigstring" };

		// WaW sub_67D6B0 — usercall(inst@edi, pos@stack0, animName@stack1, names@stack2) -> void ; caller-cleans
		inline void Scr_EmitAnimationInternal(scriptInstance_t inst, const char* pos, unsigned int animName, unsigned int names)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_EmitAnimationInternal"));
			__asm
			{
				push  names
				push  animName
				push  pos
				mov   edi, inst
				call  fn
				add   esp, 0Ch
			}
		}

		// WaW sub_67DDA0 — usercall(inst@eax, pos@edi, parentNode@stack0, names@stack1) -> void ; caller-cleans
		inline void Scr_AnimTreeParse(scriptInstance_t inst, const char* pos, unsigned int parentNode, unsigned int names)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_AnimTreeParse"));
			__asm
			{
				push  names
				push  parentNode
				mov   eax, inst
				mov   edi, pos
				call  fn
				add   esp, 8
			}
		}

		// WaW sub_67DEC0 — usercall(name@eax, names@edi, inst@stack0, index(w)@stack1, filename@stack2, treeIndex(w)@stack3) -> void ; caller-cleans
		inline void ConnectScriptToAnim(unsigned int name, unsigned int names, scriptInstance_t inst, int index, unsigned int filename, int treeIndex)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("ConnectScriptToAnim"));
			__asm
			{
				push  treeIndex
				push  filename
				push  index
				push  inst
				mov   eax, name
				mov   edi, names
				call  fn
				add   esp, 10h
			}
		}

		// WaW sub_67DF90 — usercall(anim@ecx) -> int@eax ; no stack
		inline int Scr_GetAnimsIndex(XAnim_s* anim)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_GetAnimsIndex"));
			int result;
			__asm
			{
				mov   ecx, anim
				call  fn
				mov   result, eax
			}
			return result;
		}

		// WaW sub_67E260 — usercall(names@eax, a2@ecx, filename@stack0) -> void ; caller-cleans
		inline void Scr_CheckAnimsDefined(unsigned int names, scriptInstance_t a2, unsigned int filename)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_CheckAnimsDefined"));
			__asm
			{
				push  filename
				mov   eax, names
				mov   ecx, a2
				call  fn
				add   esp, 4
			}
		}

		// WaW sub_67E420 — usercall(filename@eax, user@ecx, inst@stack0, index@stack1) -> uint@eax ; caller-cleans
		inline unsigned int Scr_UsingTreeInternal(const char* filename, int user, scriptInstance_t inst, unsigned int* index)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_UsingTreeInternal"));
			unsigned int result;
			__asm
			{
				push  index
				push  inst
				mov   eax, filename
				mov   ecx, user
				call  fn
				add   esp, 8
				mov   result, eax
			}
			return result;
		}

		// WaW sub_67E5F0 — usercall(a1@edi, filename@stack0, sourcePos@stack1) -> void ; caller-cleans
		inline void Scr_UsingTree(scriptInstance_t a1, const char* filename, unsigned int sourcePos)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_UsingTree"));
			__asm
			{
				push  sourcePos
				push  filename
				mov   edi, a1
				call  fn
				add   esp, 8
			}
		}

		// WaW sub_67E710 — usercall(animtreeName@eax, inst@ecx, parentNode@stack0, names@stack1) -> bool@al ; caller-cleans
		inline bool Scr_LoadAnimTreeInternal(const char* animtreeName, scriptInstance_t inst, unsigned int parentNode, unsigned int names)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_LoadAnimTreeInternal"));
			bool result;
			__asm
			{
				push  names
				push  parentNode
				mov   eax, animtreeName
				mov   ecx, inst
				call  fn
				add   esp, 8
				mov   result, al
			}
			return result;
		}

		// WaW sub_67E7D0 — usercall(inst@ecx, user@eax, index@stack0, Alloc@stack1, modCheckSum@stack2) -> void ; caller-cleans
		inline void Scr_LoadAnimTreeAtIndex(scriptInstance_t inst, int user, unsigned int index, void* (__cdecl* Alloc)(int), int modCheckSum)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_LoadAnimTreeAtIndex"));
			__asm
			{
				push  modCheckSum
				push  Alloc
				push  index
				mov   ecx, inst
				mov   eax, user
				call  fn
				add   esp, 0Ch
			}
		}

		// WaW sub_67EA70 — usercall(filename@eax) -> scr_animtree_t@eax (4 bytes) ; no stack
		inline scr_animtree_t Scr_FindAnimTree(const char* filename)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_FindAnimTree"));
			int raw;
			__asm
			{
				mov   eax, filename
				call  fn
				mov   raw, eax
			}
			return *reinterpret_cast<scr_animtree_t*>(&raw);
		}

		// WaW sub_67EB10 — usercall(animName@edx, a2@stack0, user@stack1) -> void ; caller-cleans
		inline void Scr_FindAnim(const char* animName, scr_anim_s a2, int user)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_FindAnim"));
			int a2v = *reinterpret_cast<int*>(&a2);
			__asm
			{
				push  user
				push  a2v
				mov   edx, animName
				call  fn
				add   esp, 8
			}
		}
	}
} // namespace T4::engine
