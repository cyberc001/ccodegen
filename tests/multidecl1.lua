require "tests.test_init"
init_test_decls("multidecl1.c")

TestMultiDecl1 = {}

function TestMultiDecl1:TestSrc()
	lu.assertTrue(test_src())
end

function TestMultiDecl1:TestGlobalDeclCount()
	lu.assertEquals(#global_decls, 8)
end
function TestMultiDecl1:TestGlobalDeclTypes()
	lu.assertEquals(global_decls[1]._type, nodes.typedef)
	lu.assertEquals(global_decls[2]._type, nodes.decl)
	lu.assertEquals(global_decls[3]._type, nodes.decl)
	lu.assertEquals(global_decls[4]._type, nodes.func)
	lu.assertEquals(global_decls[5]._type, nodes.enum_decl)
	lu.assertEquals(global_decls[6]._type, nodes.func)
	lu.assertEquals(global_decls[7]._type, nodes.decl)
	lu.assertEquals(global_decls[8]._type, nodes.enum_decl)
end

function TestMultiDecl1:TestStructs()
	local struct1 = global_decls[2].var_type
	lu.assertEquals(struct1._type, nodes.compound)
	lu.assertEquals(struct1.name.name, "vec")
	lu.assertEquals(#struct1.value, 2)
	lu.assertTrue(is_node_var_decl(struct1.value[1], 2))
	lu.assertEquals(struct1.value[1].var_type.name, "double")
	lu.assertEquals(struct1.value[1].value[1].value.name, "x")
	lu.assertEquals(struct1.value[1].value[2].value.name, "y")
	lu.assertTrue(is_node_var_decl(struct1.value[2]))
	lu.assertEquals(struct1.value[2].var_type.name, "double")
	lu.assertEquals(struct1.value[2].value[1].value.name, "z")

	lu.assertTrue(is_node_assign_decl(global_decls[2]))
	local struct1name1 = global_decls[2].value[1].value[1]
	lu.assertEquals(struct1name1._type, nodes.var)
	struct1name1 = struct1name1.value
	lu.assertEquals(struct1name1.name, "vec_one")
	local struct1val1 = global_decls[2].value[1].value[2]
	lu.assertEquals(struct1val1._type, nodes.braces)
	lu.assertEquals(#struct1val1.value, 1)
	struct1val1 = struct1val1.value[1]
	lu.assertEquals(struct1val1._type, nodes.comma)
	struct1val1 = struct1val1.value
	lu.assertEquals(struct1val1[1]._type, nodes.bin_op)
	lu.assertEquals(struct1val1[1].value[1]._type, nodes.un_op)
	lu.assertEquals(struct1val1[1].value[1].value.value.name, "y")
	lu.assertEquals(struct1val1[1].value[2].value, 1)
	lu.assertEquals(struct1val1[2]._type, nodes.num)
	lu.assertEquals(struct1val1[2].value, 0)

	local struct2 = global_decls[3].var_type
	lu.assertTrue(struct2.name:has_mod("struct"))
	lu.assertEquals(struct2.name.name, "neg_log_likelihood_ns_loss_args")
	lu.assertEquals(#struct2.value, 1)
	lu.assertTrue(is_node_var_decl(struct2.value[1]))
	lu.assertTrue(struct2.value[1].var_type:has_mod("struct"))
	lu.assertEquals(struct2.value[1].var_type.name, "vec")
	lu.assertEquals(struct2.value[1].value[1].value.name, "pre_y")
end

function TestMultiDecl1:TestFunctions()
	local func1 = global_decls[4]
	lu.assertEquals(func1.return_type.name, "double")
	lu.assertEquals(#func1.return_type.mods, 0)
	lu.assertEquals(#func1.return_type.pointers_ws, 0)
	lu.assertEquals(func1.id.name, "squared_loss")

	lu.assertEquals(func1.params._type, nodes.params)
	local params1 = func1.params.value
	lu.assertEquals(#params1, 3)

	lu.assertTrue(is_node_var_decl(params1[1]))
	lu.assertTrue(params1[1].var_type:has_mod("struct"))
	lu.assertEquals(#params1[1].var_type.pointers_ws, 0)
	lu.assertEquals(params1[1].var_type.name, "vec")
	lu.assertEquals(params1[1].value[1].value.name, "y")

	lu.assertTrue(is_node_var_decl(params1[2]))
	lu.assertEquals(#params1[2].var_type.pointers_ws, 0)
	lu.assertEquals(params1[2].var_type.name, "vec")
	lu.assertEquals(params1[2].value[1].value.name, "_y")

	lu.assertTrue(is_node_var_decl(params1[3]))
	lu.assertEquals(#params1[3].var_type.mods, 0)
	lu.assertEquals(#params1[3].var_type.pointers_ws, 1)
	lu.assertEquals(params1[3].var_type.name, "void")
	lu.assertEquals(params1[3].value[1].value.name, "args")

	lu.assertEquals(#func1.value:get_children_of_type(nodes._for, true), 1)
	lu.assertEquals(#func1.value:get_children_of_type(nodes._return, true), 1)


	local func2 = global_decls[6]
	lu.assertEquals(func2.return_type.name, "double")
	lu.assertEquals(#func2.return_type.mods, 0)
	lu.assertEquals(#func2.return_type.pointers_ws, 0)
	lu.assertEquals(func2.id.name, "neg_log_likelihood_ns_loss")

	lu.assertEquals(func2.params._type, nodes.params)
	local params2 = func2.params.value
	lu.assertEquals(#params2, 3)

	lu.assertTrue(is_node_var_decl(params2[1]))
	lu.assertEquals(#params2[1].var_type.pointers_ws, 0)
	lu.assertEquals(params2[1].var_type.name, "vec")
	lu.assertEquals(params2[1].value[1].value.name, "_y")

	lu.assertTrue(is_node_var_decl(params2[2]))
	lu.assertTrue(params2[2].var_type:has_mod("struct"))
	lu.assertEquals(#params2[2].var_type.pointers_ws, 0)
	lu.assertEquals(params2[2].var_type.name, "vec")
	lu.assertEquals(params2[2].value[1].value.name, "y")

	lu.assertTrue(is_node_var_decl(params2[3]))
	lu.assertEquals(#params2[3].var_type.mods, 0)
	lu.assertEquals(#params2[3].var_type.pointers_ws, 1)
	lu.assertEquals(params2[3].var_type.name, "void")
	lu.assertEquals(params2[3].value[1].value.name, "_args")

	lu.assertEquals(#func1.value:get_children_of_type(nodes._for, true), 1)
	lu.assertEquals(#func1.value:get_children_of_type(nodes._return, true), 1)
end

function TestMultiDecl1:TestEnums()
	local enum1 = global_decls[5]
	lu.assertEquals(#enum1.value, 4)
	lu.assertNil(enum1.name)
	lu.assertEquals(enum1.value[1]._type, nodes.bin_op)
	lu.assertEquals(enum1.value[1].op, tokens.assign)
	lu.assertEquals(enum1.value[1].value[1]._type, nodes.var)
	lu.assertEquals(enum1.value[1].value[1].value, "GRAD_OK")
	lu.assertEquals(enum1.value[1].value[2]._type, nodes.num)
	lu.assertEquals(enum1.value[1].value[2].value, 0)
	lu.assertEquals(enum1.value[2]._type, nodes.var)
	lu.assertEquals(enum1.value[2].value, "GRAD_STOPPED_THRES")
	lu.assertEquals(enum1.value[3]._type, nodes.var)
	lu.assertEquals(enum1.value[3].value, "GRAD_STOPPED_ITERS")
	lu.assertEquals(enum1.value[4]._type, nodes.bin_op)
	lu.assertEquals(enum1.value[4].op, tokens.assign)
	lu.assertEquals(enum1.value[4].value[1]._type, nodes.var)
	lu.assertEquals(enum1.value[4].value[1].value, "GRAD_INF")
	lu.assertEquals(enum1.value[4].value[2]._type, nodes.num)
	lu.assertEquals(enum1.value[4].value[2].value, -10)

	local enum2 = global_decls[8]
	lu.assertEquals(#enum2.value, 5)
	lu.assertEquals(enum2.name, "shader_result")
	lu.assertEquals(enum2.value[1]._type, nodes.var)
	lu.assertEquals(enum2.value[1].value, "SHADER_COMPILE_SYNTAX_ERROR")
	lu.assertEquals(enum2.value[2]._type, nodes.bin_op)
	lu.assertEquals(enum2.value[2].op, tokens.assign)
	lu.assertEquals(enum2.value[2].value[1]._type, nodes.var)
	lu.assertEquals(enum2.value[2].value[1].value, "SHADER_COMPILE_LINK_ERROR")
	lu.assertEquals(enum2.value[2].value[2]._type, nodes.num)
	lu.assertEquals(enum2.value[3]._type, nodes.bin_op)
	lu.assertEquals(enum2.value[3].op, tokens.assign)
	lu.assertEquals(enum2.value[3].value[1]._type, nodes.var)
	lu.assertEquals(enum2.value[3].value[1].value, "SHADER_RUNTIME_ERROR")
	lu.assertEquals(enum2.value[3].value[2]._type, nodes.num)
	lu.assertEquals(enum2.value[3].value[2].value, -3)
	lu.assertEquals(enum2.value[4]._type, nodes.var)
	lu.assertEquals(enum2.value[4].value, "SHADER_COMPILING")
	lu.assertEquals(enum2.value[5]._type, nodes.bin_op)
	lu.assertEquals(enum2.value[5].op, tokens.assign)
	lu.assertEquals(enum2.value[5].value[1]._type, nodes.var)
	lu.assertEquals(enum2.value[5].value[1].value, "SHADER_OK")
	lu.assertEquals(enum2.value[5].value[2]._type, nodes.num)
	lu.assertEquals(enum2.value[5].value[2].value, 0)
end

os.exit(lu.LuaUnit.run())
