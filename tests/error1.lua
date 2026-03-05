require "tests.test_init"

TestError1 = {}
function TestError1:setUp()
	init_test_decls("error1.c")
end

function TestError1:TestErrorLine()
	lu.assertEquals(global_decls.ctx.line, 4)
end
function TestError1:TestErrorMessage()
	lu.assertEquals(global_decls.error, "curly brace '{' was never closed")
end
