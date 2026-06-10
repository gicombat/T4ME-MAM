#pragma once

namespace T4
{
	namespace engine
	{
		WEAK symbol<void(scriptInstance_t inst)>Scr_InitAllocNode{ "Scr_InitAllocNode" };
		WEAK symbol<sval_u()>node0{ "node0" };
		WEAK symbol<sval_u(scr_enum_t type, sval_u val1)>node1{ "node1" };
		WEAK symbol<sval_u(scr_enum_t type, sval_u val1, sval_u val2)>node2{ "node2" };
		WEAK symbol<sval_u(scr_enum_t type, sval_u val1, sval_u val2, sval_u val3)>node3{ "node3" };
		WEAK symbol<sval_u(scr_enum_t type, sval_u val1, sval_u val2, sval_u val3, sval_u val4)>node4{ "node4" };
		WEAK symbol<sval_u(scr_enum_t type, sval_u val1, sval_u val2, sval_u val3, sval_u val4, sval_u val5)>node5{ "node5" };
		WEAK symbol<sval_u(sval_u val1, sval_u val2, sval_u val3, sval_u val4, sval_u val5, sval_u val6)>node6{ "node6" };
		WEAK symbol<sval_u(sval_u val1, sval_u val2, sval_u val3, sval_u val4, sval_u val5, sval_u val6, sval_u val7)>node7{ "node7" };
		WEAK symbol<sval_u(sval_u val1, sval_u val2, sval_u val3, sval_u val4, sval_u val5, sval_u val6, sval_u val7, sval_u val8)>node8{ "node8" };
		WEAK symbol<sval_u(sval_u val1)>linked_list_end{ "linked_list_end" };
		WEAK symbol<sval_u(sval_u val1, sval_u val2)>prepend_node{ "prepend_node" };
		WEAK symbol<sval_u(sval_u val1, sval_u val2)>append_node{ "append_node" };

		sval_u* Scr_AllocNode(scriptInstance_t inst, int size);
	}
} // namespace T4::engine
