require "tests.test_init"
init_test_decls("error5.c")

TestError5 = {}

function TestError5:TestErrorLine()
	lu.assertEquals(global_decls.ctx.line, 3)
end
function TestError5:TestErrorMessage()
	lu.assertEquals(global_decls.error, "expected a second operand for binary operator '%'")
end

os.exit(lu.LuaUnit.run())
