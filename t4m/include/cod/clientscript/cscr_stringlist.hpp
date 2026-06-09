#pragma once

namespace T4 { namespace engine {
	WEAK symbol<unsigned int(const char* str, scriptInstance_t inst)>SL_FindLowercaseString{ "SL_FindLowercaseString" };
	WEAK symbol<unsigned int(scriptInstance_t inst, const char* string, unsigned int user, unsigned int len)>SL_GetStringOfSize{ "SL_GetStringOfSize" };
	WEAK symbol<unsigned int(scriptInstance_t inst, const char* str, unsigned int user, unsigned int len)>SL_GetLowercaseStringOfLen{ "SL_GetLowercaseStringOfLen" };
	WEAK symbol<unsigned int(scriptInstance_t inst, unsigned int stringVal, unsigned int user)>SL_ConvertToLowercase{ "SL_ConvertToLowercase" };
	WEAK symbol<void(scriptInstance_t inst, unsigned int stringValue, RefString* refStr, unsigned int len)>SL_FreeString{ "SL_FreeString" };
	WEAK symbol<void()>SL_TransferSystem{ "SL_TransferSystem" };
	WEAK symbol<unsigned int(scriptInstance_t inst, const char* filename)>Scr_CreateCanonicalFilename{ "Scr_CreateCanonicalFilename" };

	// WaW sub_68D950 — usercall(stringValue@eax, inst@ecx) -> char*@eax ; no stack args
	inline char* SL_ConvertToString(unsigned int stringValue, scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("SL_ConvertToString"));
		char* result;
		__asm
		{
			mov   eax, stringValue
			mov   ecx, inst
			call  fn
			mov   result, eax
		}
		return result;
	}
	// WaW sub_68D970 — usercall(stringValue@eax, inst@ecx) -> int@eax ; no stack args
	inline int SL_GetStringLen(unsigned int stringValue, scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("SL_GetStringLen"));
		int result;
		__asm
		{
			mov   eax, stringValue
			mov   ecx, inst
			call  fn
			mov   result, eax
		}
		return result;
	}
	
	// WaW sub_68D9A0 — usercall(len@eax, str@edx) -> uint@eax ; no stack args
	inline unsigned int GetHashCode(unsigned int len, const char* str)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("GetHashCode"));
		unsigned int result;
		__asm
		{
			mov   eax, len
			mov   edx, str
			call  fn
			mov   result, eax
		}
		return result;
	}
	
	// WaW sub_68D9F0 — usercall(inst@eax) -> void ; no stack args
	inline void SL_Init(scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("SL_Init"));
		__asm
		{
			mov   eax, inst
			call  fn
		}
	}
	
	// WaW sub_68DA90 — usercall(inst@eax, str@stack0, len@stack1) -> uint@eax ; caller-cleans
	inline unsigned int SL_FindStringOfSize(scriptInstance_t inst, const char* str, unsigned int len)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("SL_FindStringOfSize"));
		unsigned int result;
		__asm
		{
			push  len
			push  str
			mov   eax, inst
			call  fn
			add   esp, 8
			mov   result, eax
		}
		return result;
	}
	// WaW sub_68DD20 — usercall(str@edx, inst@stack0) -> uint@eax ; caller-cleans
	inline unsigned int SL_FindString(const char* str, scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("SL_FindString"));
		unsigned int result;
		__asm
		{
			push  inst
			mov   edx, str
			call  fn
			add   esp, 4
			mov   result, eax
		}
		return result;
	}
	
	// WaW sub_68DDE0 — usercall(user@eax, refStr@edx) -> int32@eax ; no stack args
	inline signed __int32 SL_AddUserInternal(unsigned int user, RefString* refStr)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("SL_AddUserInternal"));
		signed __int32 result;
		__asm
		{
			mov   eax, user
			mov   edx, refStr
			call  fn
			mov   result, eax
		}
		return result;
	}
	
	// WaW sub_68DE10 — usercall(stringValue@eax) -> int@eax ; no stack args (instance 0 only)
	inline int Mark_ScriptStringCustom(unsigned int stringValue)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Mark_ScriptStringCustom"));
		int result;
		__asm
		{
			mov   eax, stringValue
			call  fn
			mov   result, eax
		}
		return result;
	}
	
	// WaW sub_68E330 — usercall(str@edx, inst@stack0, user@stack1) -> uint@eax ; caller-cleans
	inline unsigned int SL_GetString_(const char* str, scriptInstance_t inst, unsigned int user)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("SL_GetString_"));
		unsigned int result;
		__asm
		{
			push  user
			push  inst
			mov   edx, str
			call  fn
			add   esp, 8
			mov   result, eax
		}
		return result;
	}
	
	// WaW sub_68E360 — usercall(str@edx, user@stack0, inst@stack1) -> uint@eax ; caller-cleans
	inline unsigned int SL_GetString__0(const char* str, unsigned int user, scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("SL_GetString__0"));
		unsigned int result;
		__asm
		{
			push  inst
			push  user
			mov   edx, str
			call  fn
			add   esp, 8
			mov   result, eax
		}
		return result;
	}
	
	// WaW sub_68E420 — usercall(str@edx) -> uint@eax ; no stack args (instance 0 only)
	inline unsigned int SL_GetLowercaseString(const char* str)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("SL_GetLowercaseString"));
		unsigned int result;
		__asm
		{
			mov   edx, str
			call  fn
			mov   result, eax
		}
		return result;
	}
	
	// WaW sub_68E530 — usercall(stringValue@eax, user@ecx, inst@stack0) -> void ; caller-cleans
	inline void SL_TransferRefToUser(unsigned int stringValue, int user, scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("SL_TransferRefToUser"));
		__asm
		{
			push  inst
			mov   eax, stringValue
			mov   ecx, user
			call  fn
			add   esp, 4
		}
	}
	
	// WaW sub_68E680 — usercall(stringVal@edx, inst@esi) -> void ; no stack args
	inline void SL_RemoveRefToString(unsigned int stringVal, scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("SL_RemoveRefToString"));
		__asm
		{
			mov   edx, stringVal
			mov   esi, inst
			call  fn
		}
	}
	
	// WaW sub_68E6E0 — usercall(inst@eax, from@edi, to@stack0) -> void ; caller-cleans
	inline void Scr_SetString(scriptInstance_t inst, unsigned int from, unsigned __int16* to)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_SetString"));
		__asm
		{
			push  to
			mov   eax, inst
			mov   edi, from
			call  fn
			add   esp, 4
		}
	}
	
	// WaW sub_68E720 — usercall(from@edi, to@stack0) -> void ; caller-cleans
	inline void Scr_SetStringFromCharString(const char* from, unsigned short* to)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_SetStringFromCharString"));
		__asm
		{
			push  to
			mov   edi, from
			call  fn
			add   esp, 4
		}
	}
	
	// WaW sub_68E770 — usercall(str@edx, inst@stack0) -> uint@eax ; caller-cleans
	inline unsigned int GScr_AllocString(const char* str, scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("GScr_AllocString"));
		unsigned int result;
		__asm
		{
			push  inst
			mov   edx, str
			call  fn
			add   esp, 4
			mov   result, eax
		}
		return result;
	}
	
	// WaW sub_68E7A0 — usercall(value@xmm0, inst@stack0) -> uint@eax ; caller-cleans (float arg in xmm0)
	inline unsigned int SL_GetStringForFloat(float value, scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("SL_GetStringForFloat"));
		unsigned int result;
		__asm
		{
			movss xmm0, value
			push  inst
			call  fn
			add   esp, 4
			mov   result, eax
		}
		return result;
	}
	
	// WaW sub_68E800 — usercall(value@eax, inst@stack0) -> uint@eax ; caller-cleans
	inline unsigned int SL_GetStringForInt(int value, scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("SL_GetStringForInt"));
		unsigned int result;
		__asm
		{
			push  inst
			mov   eax, value
			call  fn
			add   esp, 4
			mov   result, eax
		}
		return result;
	}
	
	// WaW sub_68E850 — usercall(value@eax, inst@stack0) -> uint@eax ; caller-cleans
	inline unsigned int SL_GetStringForVector(float* value, scriptInstance_t inst)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("SL_GetStringForVector"));
		unsigned int result;
		__asm
		{
			push  inst
			mov   eax, value
			call  fn
			add   esp, 4
			mov   result, eax
		}
		return result;
	}
	
	// WaW sub_68E8D0 — usercall(inst@edi, user@stack0) -> void ; caller-cleans
	inline void SL_ShutdownSystem(scriptInstance_t inst, unsigned int user)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("SL_ShutdownSystem"));
		__asm
		{
			push  user
			mov   edi, inst
			call  fn
			add   esp, 4
		}
	}
	
	// WaW sub_68EA80 — usercall(filename@eax, newFilename@stack0) -> void ; caller-cleans
	inline void SL_CreateCanonicalFilename(const char* filename, char* newFilename)
	{
		static void* fn = reinterpret_cast<void*>(T4M::GetAddress("SL_CreateCanonicalFilename"));
		__asm
		{
			push  newFilename
			mov   eax, filename
			call  fn
			add   esp, 4
		}
	}

	// Warning Adress unknow for now
	WEAK symbol<RefString*(scriptInstance_t inst, unsigned int id)>GetRefString{ "GetRefString" };
	WEAK symbol<void(scriptInstance_t inst, unsigned int stringValue)>SL_AddRefToString{ "SL_AddRefToString" };
	WEAK symbol<void(scriptInstance_t inst, unsigned int stringValue, unsigned int len)>SL_RemoveRefToStringOfSize{ "SL_RemoveRefToStringOfSize" };
	WEAK symbol<int(RefString* refString)>SL_GetRefStringLen{ "SL_GetRefStringLen" };
	WEAK symbol<void(unsigned int stringValue, unsigned int user, scriptInstance_t inst)>SL_AddUser{ "SL_AddUser" };
	WEAK symbol<int(scriptInstance_t inst, const char* str)>SL_ConvertFromString{ "SL_ConvertFromString" };
	WEAK symbol<int(scriptInstance_t inst, RefString* refString)>SL_ConvertFromRefString{ "SL_ConvertFromRefString" };
	WEAK symbol<RefString*(scriptInstance_t inst, const char* str)>GetRefString_0{ "GetRefString_0" };
	WEAK symbol<const char*(unsigned int id, scriptInstance_t inst)>SL_ConvertToStringSafe{ "SL_ConvertToStringSafe" };
} } // namespace T4::engine