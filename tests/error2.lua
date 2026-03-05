require "tests.test_init"

TestError2 = {}
function TestError2:setUp()
	init_test_decls("error2.c")
end

function TestError2:TestErrorLine()
	lu.assertEquals(global_decls.ctx.line, 9)
end
function TestError2:TestErrorMessage()
	lu.assertEquals(global_decls.error, "expected ';' or ')' after expression-statement, got identifier")
end
