require "tests.test_init"
init_test_decls("error1.c")

TestError1 = {}

function TestError1:TestErrorLine()
	lu.assertEquals(global_decls.ctx.line, 4)
end
function TestError1:TestErrorMessage()
	lu.assertEquals(global_decls.error, "curly brace '{' was never closed")
end

os.exit(lu.LuaUnit.run())
