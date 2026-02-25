require "tests.test_init"
init_test_decls("error4.c")

TestError4 = {}

function TestError4:TestErrorLine()
	lu.assertEquals(global_decls.ctx.line, 9)
end
function TestError4:TestErrorMessage()
	lu.assertEquals(global_decls.error, "identifier 'data' is not a type")
end

os.exit(lu.LuaUnit.run())
