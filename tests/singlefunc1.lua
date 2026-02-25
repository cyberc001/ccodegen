require "tests.test_init"
init_test_decls("singlefunc1.c")

TestSingleFunc1 = {}

function TestSingleFunc1:TestSrc()
	lu.assertTrue(test_src())
end

function TestSingleFunc1:TestSingleGlobalDecl()
	lu.assertEquals(#global_decls, 1)
end
function TestSingleFunc1:TestGlobalDeclIsFunc()
	lu.assertEquals(global_decls[1]._type, nodes.func)
end

function TestSingleFunc1:TestReturnType()
	local ret_type = global_decls[1].return_type
	lu.assertTrue(ret_type:has_mod("unsigned"))
	lu.assertEquals(ret_type.name, "char")
end
function TestSingleFunc1:TestId()
	lu.assertEquals(global_decls[1].id.name, "detect_cpus")
end
function TestSingleFunc1:TestParams()
	local params = global_decls[1].params.value
	lu.assertEquals(#params, 3)

	lu.assertTrue(is_node_var_decl(params[1]))
	lu.assertTrue(params[1].var_type:has_mod("unsigned"))
	lu.assertEquals(params[1].var_type.name, "char")
	lu.assertEquals(params[1].value[1].value.name, "rsdt")

	lu.assertTrue(is_node_var_decl(params[2]))
	lu.assertTrue(params[2].var_type:has_mod("unsigned"))
	lu.assertEquals(params[2].var_type.name, "char")
	lu.assertEquals(params[2].value[1].value.name, "lapic_ids")

	lu.assertTrue(is_node_var_decl(params[3]))
	lu.assertTrue(params[3].var_type:has_mod("unsigned"))
	lu.assertEquals(params[3].var_type.name, "char")
	lu.assertEquals(params[3].value[1].value.name, "bsp_lapic_id")
end

function TestSingleFunc1:TestBodyIsBraces()
	lu.assertEquals(global_decls[1].value._type, nodes.braces)
end
function TestSingleFunc1:TestBodyDeclaredVariables()
	lu.assertTrue(global_decls[1].value:has_any_children_that(function(x)
		return is_node_var_decl(x)
			and x.value[1].value.name == "ent"
			and x.var_type:has_mod("unsigned")
			and x.var_type.name == "char"
			and #x.var_type.pointers_ws == 1
	end, true))
	lu.assertTrue(global_decls[1].value:has_any_children_that(function(x)
		return is_node_var_decl(x)
			and x.value[1].value.name == "ent_end"
			and x.var_type:has_mod("unsigned")
			and x.var_type.name == "char"
			and #x.var_type.pointers_ws == 1
	end, true))
	lu.assertTrue(global_decls[1].value:has_any_children_that(function(x)
		return is_node_var_decl(x)
			and x.value[1].value.name == "ln"
			and x.var_type:has_mod("unsigned")
			and #x.var_type.pointers_ws == 0
	end, true))

	lu.assertTrue(global_decls[1].value:has_any_children_that(function(x)
		return is_node_assign_decl(x, 2)
			and x.value[1].value[1].value.name == "lapic_ptr"
			and x.value[1].value[2].value == 0
			and x.value[2].value[1].value.name == "ioapic_ptr"
			and x.value[2].value[2].value == 0
	end, true))
	lu.assertTrue(global_decls[1].value:has_any_children_that(function(x)
		return is_node_assign_decl(x)
			and x.value[1].value[1].value.name == "core_num"
			and x.value[1].value[2].value == 0
	end, true))

	lu.assertTrue(global_decls[1].value:has_any_children_that(function(x)
		return is_node_var_decl(x, 4)
			and x.value[1].value.name == "eax"
			and x.value[2].value.name == "ebx"
			and x.value[3].value.name == "ecx"
			and x.value[4].value.name == "edx"
			and x.var_type:has_mod("unsigned")
			and #x.var_type.pointers_ws == 0
	end, true))
end

function TestSingleFunc1:TestBodyHasOneForLoop()
	lu.assertEquals(#global_decls[1].value:get_children_of_type(nodes._for), 1)
end
function TestSingleFunc1:TestForLoopHead()
	local main_loop = global_decls[1].value:get_children_of_type(nodes._for)[1]

	lu.assertEquals(main_loop.begin._type, nodes.bin_op)
	lu.assertEquals(main_loop.begin.op, tokens.assign)

	lu.assertEquals(main_loop.cond._type, nodes.bin_op)
	lu.assertEquals(main_loop.cond.op, tokens.lt)

	lu.assertEquals(main_loop.iter._type, nodes.bin_op)
	lu.assertEquals(main_loop.iter.op, tokens.add_assign)
end
function TestSingleFunc1:TestForLoopBody()
	local main_loop = global_decls[1].value:get_children_of_type(nodes._for)[1]
	lu.assertEquals(main_loop.value._type, nodes.braces)

	local child_if = main_loop.value:get_children_of_type(nodes._if)
	lu.assertEquals(#child_if, 1)
	child_if = child_if[1].value
	lu.assertEquals(#child_if, 1)
	child_if = child_if[1]

	local if_child_for = child_if:get_children_of_type(nodes._for)
	lu.assertEquals(#if_child_for, 1)
end

function TestSingleFunc1:TestBodyHasOneDoWhileLoop()
	lu.assertEquals(#global_decls[1].value:get_children_of_type(nodes.do_while), 1)
end
function TestSingleFunc1:TestDoWhileLoopHead()
	local do_while = global_decls[1].value:get_children_of_type(nodes.do_while)[1]
	
	lu.assertEquals(do_while.cond._type, nodes.bin_op)
	lu.assertEquals(do_while.cond.op, tokens.gt)
	lu.assertEquals(do_while.cond.value[1]._type, nodes.var)
	lu.assertEquals(do_while.cond.value[1].value.name, "core_num")
	lu.assertEquals(do_while.cond.value[2].value, 0)
end

os.exit(lu.LuaUnit.run())
