lu = require "luaunit"

require "enbf"
require "sub"
require "utils"

function init_test_decls(file_name)
	local file = io.open("tests/" .. file_name)
	local src = file:read("*all")
	
	local ctx = new_token_ctx(1)
	ctx = next_token(src, ctx)
	global_decls = {}
	while true do
		global_node, ctx = global_decl(src, ctx)
		table.insert(global_decls, global_node)
		if ctx.i == nil then break end
		ctx = next_token(src, ctx)
		if ctx.i == nil then break end
	end
end

function is_node_var_decl(decl, expected_ln)
	expected_ln = expected_ln or 1

	if decl._type ~= nodes.decl or #decl.value ~= expected_ln then
		return false
	end
	for _, v in ipairs(decl.value) do
		if v._type ~= nodes.var then
			return false
		end
	end
	return true
end
function is_node_assign_decl(decl, expected_ln)
	expected_ln = expected_ln or 1

	if decl._type ~= nodes.decl or #decl.value ~= expected_ln then
		return false
	end
	for _, v in ipairs(decl.value) do
		if v._type ~= nodes.bin_op or v.op ~= tokens.assign then
			return false
		end
	end

	return true
end
