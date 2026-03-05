require "tests.test_init"

TestError3 = {}
function TestError3:setUp()
	init_test_decls("error3.c")
end

function TestError3:TestErrorLine()
	lu.assertEquals(global_decls.ctx.line, 6)
end
function TestError3:TestErrorMessage()
	lu.assertEquals(global_decls.error, "expected ';' after conditional statement of 'for' loop, got )")
end
