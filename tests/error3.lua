require "tests.test_init"
init_test_decls("error3.c")

TestError3 = {}

function TestError3:TestErrorLine()
	lu.assertEquals(global_decls.ctx.line, 8)
end
function TestError3:TestErrorMessage()
	lu.assertEquals(global_decls.error, "expected ';' after conditional statement of 'for' loop, got )")
end

os.exit(lu.LuaUnit.run())
