#pragma once

namespace T4
{
	namespace engine
	{
		WEAK symbol<char* (scriptInstance_t inst, int unused, char* filename, const char* codepos, int archive)>Scr_ReadFile_FastFile{ "Scr_ReadFile_FastFile" };
		WEAK symbol<char* (scriptInstance_t inst, int unused_arg1, const char* filename, const char* codepos, int archive)>Scr_ReadFile_LoadObj{ "Scr_ReadFile_LoadObj" };
		WEAK symbol<void(scriptInstance_t inst, unsigned int codePos, const char* msg, ...)>CompileError{ "CompileError" };

		// WaW sub_68A840 — usercall(inst@eax) -> void ; no stack args
		inline void Scr_InitOpcodeLookup(scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_InitOpcodeLookup"));
			__asm
			{
				mov   eax, inst
				call  fn
			}
		}

		// WaW sub_68A950 — usercall(inst@ecx) -> void ; no stack args
		inline void Scr_ShutdownOpcodeLookup(scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_ShutdownOpcodeLookup"));
			__asm
			{
				mov   ecx, inst
				call  fn
			}
		}

		// WaW sub_68A9A0 — usercall(inst@eax, sourcePos@stack0, type_@stack1) -> void ; caller-cleans
		inline void AddOpcodePos(scriptInstance_t inst, unsigned int sourcePos, int type_)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("AddOpcodePos"));
			__asm
			{
				push  type_
				push  sourcePos
				mov   eax, inst
				call  fn
				add   esp, 8
			}
		}

		// WaW sub_68AB80 — usercall(inst@eax) -> void ; no stack args
		inline void RemoveOpcodePos(scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("RemoveOpcodePos"));
			__asm
			{
				mov   eax, inst
				call  fn
			}
		}

		// WaW sub_68ABF0 — usercall(inst@eax, sourcePos@stack0) -> void ; caller-cleans
		inline void AddThreadStartOpcodePos(scriptInstance_t inst, unsigned int sourcePos)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("AddThreadStartOpcodePos"));
			__asm
			{
				push  sourcePos
				mov   eax, inst
				call  fn
				add   esp, 4
			}
		}

		// WaW sub_68B270 — usercall(inst@eax, codePos@esi) -> uint@eax ; no stack args
		inline unsigned int Scr_GetSourceBuffer(scriptInstance_t inst, const char* codePos)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_GetSourceBuffer"));
			unsigned int result;
			__asm
			{
				mov   eax, inst
				mov   esi, codePos
				call  fn
				mov   result, eax
			}
			return result;
		}

		// WaW sub_68AC90 — usercall(startLine@edx, buf@ecx, sourcePos@stack0, col@stack1) -> uint@eax ; caller-cleans
		inline unsigned int Scr_GetLineNumInternal(const char** startLine, const char* buf, const char* sourcePos, int* col)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_GetLineNumInternal"));
			unsigned int result;
			__asm
			{
				push  col
				push  sourcePos
				mov   edx, startLine
				mov   ecx, buf
				call  fn
				add   esp, 8
				mov   result, eax
			}
			return result;
		}

		// WaW sub_68ACC0 — usercall(inst@eax) -> SourceBufferInfo*@eax ; no stack args
		inline SourceBufferInfo* Scr_GetNewSourceBuffer(scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_GetNewSourceBuffer"));
			SourceBufferInfo* result;
			__asm
			{
				mov   eax, inst
				call  fn
				mov   result, eax
			}
			return result;
		}

		// WaW sub_68AD50 — usercall(filename@eax, inst@stack0, codepos@stack1, buffer@stack2, len@stack3, archive@stack4) -> void ; caller-cleans
		inline void Scr_AddSourceBufferInternal(const char* filename, scriptInstance_t inst, const char* codepos, char* buffer, int len, int archive)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_AddSourceBufferInternal"));
			__asm
			{
				push  archive
				push  len
				push  buffer
				push  codepos
				push  inst
				mov   eax, filename
				call  fn
				add   esp, 14h
			}
		}

		// WaW sub_68AFA0 — usercall(codepos@edi, filename@esi, inst@stack0, unused@stack1) -> char*@eax ; caller-cleans
		inline char* Scr_ReadFile(const char* codepos, char* filename, scriptInstance_t inst, int unused)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_ReadFile"));
			char* result;
			__asm
			{
				push  unused
				push  inst
				mov   edi, codepos
				mov   esi, filename
				call  fn
				add   esp, 8
				mov   result, eax
			}
			return result;
		}

		// WaW sub_68B040 — usercall(inst@eax, unused_arg1@stack0, filename@stack1, codepos@stack2) -> char*@eax ; caller-cleans
		inline char* Scr_AddSourceBuffer(scriptInstance_t inst, int unused_arg1, char* filename, const char* codepos)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_AddSourceBuffer"));
			char* result;
			__asm
			{
				push  codepos
				push  filename
				push  unused_arg1
				mov   eax, inst
				call  fn
				add   esp, 0Ch
				mov   result, eax
			}
			return result;
		}

		// WaW sub_68B0E0 — usercall(rawLine@eax, line@stack0) -> void ; caller-cleans
		inline void Scr_CopyFormattedLine(const char* rawLine, char* line)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_CopyFormattedLine"));
			__asm
			{
				push  line
				mov   eax, rawLine
				call  fn
				add   esp, 4
			}
		}

		// WaW sub_68B140 — usercall(col@edx, buf@ecx, sourcePos@stack0, line@stack1) -> uint@eax ; caller-cleans
		inline unsigned int Scr_GetLineInfo(int* col, const char* buf, unsigned int sourcePos, char* line)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_GetLineInfo"));
			unsigned int result;
			__asm
			{
				push  line
				push  sourcePos
				mov   edx, col
				mov   ecx, buf
				call  fn
				add   esp, 8
				mov   result, eax
			}
			return result;
		}

		// WaW sub_68B190 — usercall(sourcePos@edx, buf@ecx, channel@esi, inst@stack0, file@stack1) -> void ; caller-cleans
		inline void Scr_PrintSourcePos(unsigned int sourcePos, const char* buf, con_channel_e channel, scriptInstance_t inst, const char* file)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_PrintSourcePos"));
			__asm
			{
				push  file
				push  inst
				mov   edx, sourcePos
				mov   ecx, buf
				mov   esi, channel
				call  fn
				add   esp, 8
			}
		}

		// WaW sub_68AC40 — usercall(inst@eax, codePos@edi) -> OpcodeLookup*@eax ; no stack args
		inline OpcodeLookup* Scr_GetPrevSourcePosOpcodeLookup(scriptInstance_t inst, const char* codePos)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_GetPrevSourcePosOpcodeLookup"));
			OpcodeLookup* result;
			__asm
			{
				mov   eax, inst
				mov   edi, codePos
				call  fn
				mov   result, eax
			}
			return result;
		}

		// WaW sub_68B2B0 — usercall(line@edx, codePos@ecx, inst@stack0) -> void ; caller-cleans
		inline void Scr_GetTextSourcePos(char* line, const char* codePos, scriptInstance_t inst)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_GetTextSourcePos"));
			__asm
			{
				push  inst
				mov   edx, line
				mov   ecx, codePos
				call  fn
				add   esp, 4
			}
		}

		// WaW sub_68B340 — usercall(codepos@eax, scriptInstance@stack0, channel@stack1, index@stack2) -> void ; caller-cleans
		inline void Scr_PrintPrevCodePos(const char* codepos, scriptInstance_t scriptInstance, con_channel_e channel, unsigned int index)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("Scr_PrintPrevCodePos"));
			__asm
			{
				push  index
				push  channel
				push  scriptInstance
				mov   eax, codepos
				call  fn
				add   esp, 0Ch
			}
		}

		// WaW sub_68B600 — variadic usercall(codePos@edi, a2@esi, msg, ...): cannot be an inline-asm thunk
		// (varargs). Address resolved via GetAddress; caller passes call_addr explicitly (see decl).
		inline void* CompileError2_ADDR() { return reinterpret_cast<void*>(T4M::GetAddress("CompileError2")); }
		void CompileError2(const char* codePos, scriptInstance_t a2, void* call_addr, const char* msg, ...);

		// WaW sub_68B6D0 — usercall(msg@eax, inst@edi, channel@stack0, codepos@stack1, index@stack2) -> void ; caller-cleans
		inline void RuntimeErrorInternal(const char* msg, scriptInstance_t inst, con_channel_e channel, const char* codepos, int index)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("RuntimeErrorInternal"));
			__asm
			{
				push  index
				push  codepos
				push  channel
				mov   eax, msg
				mov   edi, inst
				call  fn
				add   esp, 0Ch
			}
		}

		// WaW sub_68B790 — usercall(inst@eax, pos@stack0, error_index@stack1, err@stack2, err2@stack3) -> void ; caller-cleans
		inline void RuntimeError(scriptInstance_t inst, const char* pos, int error_index, const char* err, const char* err2)
		{
			static void* fn = reinterpret_cast<void*>(T4M::GetAddress("RuntimeError"));
			__asm
			{
				push  err2
				push  err
				push  error_index
				push  pos
				mov   eax, inst
				call  fn
				add   esp, 10h
			}
		}

		// Warning Adress unknow for now
		WEAK symbol<unsigned int(scriptInstance_t inst, const char* codePos, unsigned int index)>Scr_GetPrevSourcePos{ "Scr_GetPrevSourcePos" };
		WEAK symbol<void(scriptInstance_t inst)>Scr_ShutdownAllocNode{ "Scr_ShutdownAllocNode" };
	}
} // namespace T4::engine