require "tests.test_init"
init_test_decls("error2.c")

TestError2 = {}

function TestError2:TestErrorLine()
	lu.assertEquals(global_decls.ctx.line, 12)
end
function TestError2:TestErrorMessage()
	lu.assertEquals(global_decls.error, "expected ';' or ')' after expression-statement, got identifier")
end

os.exit(lu.LuaUnit.run())
