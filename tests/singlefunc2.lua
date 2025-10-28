require "tests.test_init"
init_test_decls("singlefunc2.c")

TestSingleFunc2 = {}

function TestSingleFunc2:TestSingleGlobalDecl()
	lu.assertEquals(#global_decls, 1)
end
function TestSingleFunc2:TestGlobalDeclIsFunc()
	lu.assertEquals(global_decls[1]._type, nodes.func)
end

function TestSingleFunc2:TestReturnType()
	local ret_type = global_decls[1].return_type
	lu.assertEquals(#ret_type.mods, 0)
	lu.assertEquals(ret_type.name, "void")
end
function TestSingleFunc2:TestId()
	lu.assertEquals(global_decls[1].id.name, "thread_sched_pqueue_heapify")
end
function TestSingleFunc2:TestParams()
	local params = global_decls[1].params.value
	lu.assertEquals(#params, 1)

	lu.assertTrue(is_node_var_decl(params[1]))
	lu.assertTrue(params[1].var_type:has_mod("struct"))
	lu.assertEquals(params[1].var_type.name, "thread_sched_pqueue")
	lu.assertEquals(#params[1].var_type.pointers_ws, 1)
	lu.assertEquals(params[1].value[1].value.name, "q")
end

function TestSingleFunc2:TestBodyIsBraces()
	lu.assertEquals(global_decls[1].value._type, nodes.braces)
end
function TestSingleFunc2:TestBodyHasOneForLoop()
	local for_loops = global_decls[1].value:get_children_of_type(nodes._for)
	lu.assertEquals(#for_loops, 1)
end
function TestSingleFunc2:TestForLoopHead()
	local main_loop = global_decls[1].value:get_children_of_type(nodes._for)[1]

	lu.assertEquals(main_loop.begin._type, nodes.decl)
	lu.assertTrue(is_node_assign_decl(main_loop.begin))
	lu.assertTrue(main_loop.begin.var_type:has_mod("unsigned"))
	lu.assertTrue(main_loop.begin.var_type:has_mod("long"))
	lu.assertEquals(main_loop.begin.var_type.name, "int")
	lu.assertEquals(#main_loop.begin.var_type.pointers_ws, 0)

	lu.assertEquals(main_loop.cond._type, nodes.bin_op)
	lu.assertEquals(main_loop.cond.op, tokens.lt)

	lu.assertEquals(main_loop.iter._type, nodes.un_op)
	lu.assertEquals(main_loop.iter.op, tokens.inc)
	lu.assertEquals(main_loop.iter.value._type, nodes.var)
	lu.assertEquals(main_loop.iter.value.value.name, "i")
end
function TestSingleFunc2:TestForLoopBody()
	local main_loop = global_decls[1].value:get_children_of_type(nodes._for)[1]
	lu.assertEquals(main_loop.value._type, nodes.braces)

	lu.assertTrue(main_loop:has_any_children_that(function(x)
		return is_node_assign_decl(x)
			and x.var_type:has_mod("unsigned")
			and x.var_type:has_mod("long")
			and x.var_type.name == "int"
			and #x.var_type.pointers_ws == 0

			and x.value[1].value[1].value.name == "k"
			and x.value[1].value[2].value.name == "i"
	end, true))
	lu.assertTrue(main_loop:has_any_children_that(function(x)
		return is_node_assign_decl(x)
			and x.var_type:has_mod("struct")
			and x.var_type.name == "thread"
			and #x.var_type.pointers_ws == 1

			and x.value[1].value[1].value.name == "thr"
	end, true))

	local child_for = main_loop.value:get_children_of_type(nodes._for)
	lu.assertEquals(#child_for, 1)
end
function TestSingleFunc2:TestInnerForLoopHead()
	local main_loop = global_decls[1].value:get_children_of_type(nodes._for)[1]
	local child_for = main_loop.value:get_children_of_type(nodes._for)

	lu.assertNil(child_for.begin)
	lu.assertNil(child_for.cond)
	lu.assertNil(child_for.iter)
end
function TestSingleFunc2:TestInnerForLoopBody()
	local main_loop = global_decls[1].value:get_children_of_type(nodes._for)[1]
	local child_for = main_loop.value:get_children_of_type(nodes._for)[1]
	lu.assertEquals(child_for.value._type, nodes.braces)
	child_for = child_for.value

	local child_if = child_for:get_children_of_type(nodes._if)
	lu.assertEquals(#child_if, 2)
	lu.assertEquals(child_if[1].cond._type, nodes.bin_op)
	lu.assertEquals(child_if[1].cond.op, tokens.lt)
	lu.assertEquals(child_if[2].cond._type, nodes.bin_op)
	lu.assertEquals(child_if[2].cond.op, tokens.eq)

	lu.assertTrue(child_for:has_any_children_that(function(x)
		return x._type == nodes.bin_op and x.op == tokens.assign
	end, true))
	lu.assertTrue(child_for:has_any_children_that(function(x)
		return x._type == nodes.bin_op and x.op == tokens.div_assign
	end, true))
end


os.exit(lu.LuaUnit.run())
