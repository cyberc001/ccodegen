lu = require "luaunit"

require "enbf"
require "sub"
require "utils"

function trim(s) -- http://lua-users.org/wiki/StringTrim
	return (s:gsub("^%s*(.-)%s*$", "%1"))
end

function init_test_decls(file_name)
	local file = io.open("tests/" .. file_name)
	src = file:read("*all")
	
	local ctx = new_token_ctx(1)
	ctx = next_token(src, ctx)
	global_decls = {}
	while true do
		global_node, ctx = global_decl(src, ctx)
		if type(global_node) == "string" then
			global_decls = {
				error = global_node,
				ctx = ctx
			}
			break
		end
		table.insert(global_decls, global_node)
		if ctx.i == nil then break end
	end
end

-- Сравнение исходного кода без комментариев и кода, восстановленного из дерева. Обе строки обрезаются с начала и с конца.
function test_src()
	local decl_src = ""
	for _, v in ipairs(global_decls) do
		decl_src = decl_src .. v:src()
	end

	local src_nocomment = ""
	local beg_i = 1

	local i = 1
	while i <= #src do
		local c = src:sub(i, i)
		if c == '/' then
			i = i + 1
			c = src:sub(i, i)
			if c == '/' then
				src_nocomment = src_nocomment .. src:sub(beg_i, i - 2)
				while src:sub(i, i) ~= '\n' and i < #src do
					i = i + 1
				end
				if i < #src then
					beg_i = i
				end
			elseif c == '*' then
				src_nocomment = src_nocomment .. src:sub(beg_i, i - 2)
				while src:sub(i, i) ~= '*' and src:sub(i + 1, i + 1) ~= '/'and i < #src - 1 do
					i = i + 1
				end
				if i < #src - 1 then
					beg_i = i + 1
				end
			end
		end
		i = i + 1
	end
	src_nocomment = src_nocomment .. src:sub(beg_i, i - 1)

	return trim(src_nocomment) == trim(decl_src)
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
