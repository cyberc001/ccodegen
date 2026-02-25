require "tests.test_init"
init_test_decls("multifunc1.c")

TestMultiFunc1 = {}

function TestMultiFunc1:TestSrc()
	lu.assertTrue(test_src())
end

function TestMultiFunc1:TestGlobalDeclCount()
	lu.assertEquals(#global_decls, 3)
end
function TestMultiFunc1:TestGlobalDeclAreFunctions()
	for _, v in ipairs(global_decls) do
		lu.assertEquals(global_decls[1]._type, nodes.func)
	end
end

function TestMultiFunc1:TestReturnTypes()
	local ret_types = {}
	for _, v in ipairs(global_decls) do
		table.insert(ret_types, v.return_type)
	end

	lu.assertTrue(ret_types[1]:has_mod("unsigned"))
	lu.assertNil(ret_types[1].name)

	lu.assertEquals(#ret_types[2].mods, 0)
	lu.assertEquals(ret_types[2].name, "void")
	lu.assertEquals(#ret_types[2].pointers_ws, 1)

	lu.assertEquals(#ret_types[3].mods, 0)
	lu.assertEquals(ret_types[3].name, "void")
	lu.assertEquals(#ret_types[3].pointers_ws, 0)
end

function TestMultiFunc1:TestId()
	lu.assertEquals(global_decls[1].id.name, "map_alloc")
	lu.assertEquals(global_decls[2].id.name, "map_phys")
	lu.assertEquals(global_decls[3].id.name, "unmap")
end

function TestMultiFunc1:TestParams()
	local params = {}
	for _, v in ipairs(global_decls) do
		table.insert(params, v.params.value)
	end
	
	lu.assertEquals(#params[1], 3)

	lu.assertTrue(is_node_var_decl(params[1][1]))
	lu.assertEquals(#params[1][1].var_type.mods, 0)
	lu.assertEquals(#params[1][1].var_type.pointers_ws, 1)
	lu.assertEquals(params[1][1].var_type.name, "void")
	lu.assertEquals(params[1][1].value[1].value.name, "vaddr")

	lu.assertTrue(is_node_var_decl(params[1][2]))
	lu.assertTrue(params[1][2].var_type:has_mod("unsigned"))
	lu.assertTrue(params[1][2].var_type:has_mod("long"))
	lu.assertEquals(#params[1][2].var_type.pointers_ws, 0)
	lu.assertNil(params[1][2].var_type.name)
	lu.assertEquals(params[1][2].value[1].value.name, "usize")

	lu.assertTrue(is_node_var_decl(params[1][3]))
	lu.assertEquals(#params[1][3].var_type.mods, 0)
	lu.assertEquals(#params[1][3].var_type.pointers_ws, 0)
	lu.assertEquals(params[1][3].var_type.name, "int")
	lu.assertEquals(params[1][3].value[1].value.name, "flags")

	lu.assertEquals(#params[2], 4)

	lu.assertTrue(is_node_var_decl(params[2][1]))
	lu.assertEquals(#params[2][1].var_type.mods, 0)
	lu.assertEquals(#params[2][1].var_type.pointers_ws, 1)
	lu.assertEquals(params[2][1].var_type.name, "void")
	lu.assertEquals(params[2][1].value[1].value.name, "vaddr")

	lu.assertTrue(is_node_var_decl(params[2][2]))
	lu.assertEquals(#params[2][2].var_type.mods, 0)
	lu.assertEquals(#params[2][2].var_type.pointers_ws, 2)
	lu.assertEquals(params[2][2].var_type.name, "void")
	lu.assertEquals(params[2][2].value[1].value.name, "paddr")

	lu.assertTrue(is_node_var_decl(params[2][3]))
	lu.assertTrue(params[2][3].var_type:has_mod("unsigned"))
	lu.assertTrue(params[2][3].var_type:has_mod("long"))
	lu.assertEquals(#params[2][3].var_type.pointers_ws, 0)
	lu.assertEquals(params[2][3].var_type.name, "int")
	lu.assertEquals(params[2][3].value[1].value.name, "usize")

	lu.assertTrue(is_node_var_decl(params[2][4]))
	lu.assertEquals(#params[2][4].var_type.mods, 0)
	lu.assertEquals(#params[2][4].var_type.pointers_ws, 0)
	lu.assertEquals(params[2][4].var_type.name, "int")
	lu.assertEquals(params[2][4].value[1].value.name, "flags")

	lu.assertEquals(#params[3], 3)

	lu.assertTrue(is_node_var_decl(params[3][1]))
	lu.assertEquals(#params[3][1].var_type.mods, 0)
	lu.assertEquals(#params[3][1].var_type.pointers_ws, 1)
	lu.assertEquals(params[3][1].var_type.name, "void")
	lu.assertEquals(params[3][1].value[1].value.name, "vaddr")

	lu.assertTrue(is_node_var_decl(params[3][2]))
	lu.assertTrue(params[3][2].var_type:has_mod("unsigned"))
	lu.assertEquals(#params[3][2].var_type.pointers_ws, 0)
	lu.assertNil(params[3][2].var_type.name)
	lu.assertEquals(params[3][2].value[1].value.name, "usize")

	lu.assertTrue(is_node_var_decl(params[3][3]))
	lu.assertEquals(#params[3][3].var_type.mods, 0)
	lu.assertEquals(#params[3][3].var_type.pointers_ws, 1)
	lu.assertEquals(params[3][3].var_type.name, "int")
	lu.assertEquals(params[3][3].value[1].value.name, "flags")

end

function TestMultiFunc1:TestBodiesAreBraces()
	for _, v in ipairs(global_decls) do
		lu.assertEquals(v.value._type, nodes.braces)
	end
end

function TestMultiFunc1:TestBodyContents()
	local bodies = {}
	for _, v in ipairs(global_decls) do
		table.insert(bodies, v.value)
	end

	local if_count = #bodies[1]:get_children_of_type(nodes._if, true)
	lu.assertEquals(if_count, 3)
	local for_count = #bodies[1]:get_children_of_type(nodes._for, true)
	lu.assertEquals(for_count, 6)
	local func_decls = bodies[1]:get_children_of_type(nodes.decl, true)
	lu.assertTrue(is_node_assign_decl(func_decls[1]))
	lu.assertEquals(func_decls[1].value[1].value[1].value.name, "err")
	lu.assertEquals(func_decls[1].value[1].value[2].value, 0)
	local func_gotos = bodies[1]:get_children_of_type(nodes._goto, true)
	lu.assertEquals(#func_gotos, 1)
	lu.assertEquals(func_gotos[1].value, "cleanup")
	local func_labels = bodies[1]:get_children_of_type(nodes.label, true)
	lu.assertEquals(#func_labels, 1)
	lu.assertEquals(func_labels[1].value, "cleanup")

	if_count = #bodies[2]:get_children_of_type(nodes._if, true)
	lu.assertEquals(if_count, 1)
	for_count = #bodies[2]:get_children_of_type(nodes._for, true)
	lu.assertEquals(for_count, 3)
	local decl_count = #bodies[2]:get_children_of_type(nodes.decl, true)
	lu.assertEquals(decl_count, 0)

	if_count = #bodies[3]:get_children_of_type(nodes._if, true)
	lu.assertEquals(if_count, 1)
	for_count = #bodies[3]:get_children_of_type(nodes._for, true)
	lu.assertEquals(for_count, 3)
	decl_count = #bodies[3]:get_children_of_type(nodes.decl, true)
	lu.assertEquals(decl_count, 0)
end

function TestMultiFunc1:TestForLoopHeads()
	local loops = {}
	for _, v in ipairs(global_decls) do
		local inner_loops = v:get_children_of_type(nodes._for, true)
		for _, v in ipairs(inner_loops) do
			table.insert(loops, v)
		end
	end

	for _, v in ipairs(loops) do
		lu.assertEquals(v.begin._type, nodes.semicolon)
		lu.assertEquals(v.cond._type, nodes.bin_op)
		lu.assertTrue(v.cond.op == tokens.ge or v.cond.op == tokens.gt
				or v.cond.op == tokens.land)
		lu.assertEquals(v.iter._type, nodes.bin_op)
		lu.assertTrue(v.iter.op == tokens.add_assign or v.iter.op == tokens.sub_assign)
	end
end

os.exit(lu.LuaUnit.run())
