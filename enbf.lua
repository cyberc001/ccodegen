require "token"
require "os"

nodes = {
	num = 1, str = 2, char = 3,
	call = 10, var = 12, params = 13, decl = 14, func = 15, enum_decl = 16,
	cast = 20,
	un_op = 30, bin_op = 31, cond = 32, index = 33,
	parantheses = 40, braces = 41, comma = 42,

	_if = 50, _while = 51, _for = 52,

	compound = 60
}

node = {}

function node:new(o)
	o = o or {}
	o.ws_before = o.ws_before or ""
	o.ws_after = o.ws_after or ""
	setmetatable(o, self)
	self.__index = self
	return o
end
function node:__tostring()
	self.dbg = self.dbg or ""
	local dbg_append = ""
	if type(self.value) == "table" then
		self.value.dbg = self.dbg .. "\t"
	else
		dbg_append = self.dbg .. "\t"
	end

	local val = tostring(self.value)
	if self._type == nodes.num then
		if self.token == tokens.num_hex then
			val = string.format("0x%x", val)
		elseif self.token == tokens.num_oct then
			val = string.format("0%o", val)
		end
	end

	return self.dbg .. (self.print and self:print() or "(generic node: type " .. self._type .. ", value [\n" .. dbg_append .. val .. "\n" .. self.dbg .. "])")
end

-- возвращает исходный код узла
function node:_src()
	local val = tostring(self.value)
	if self._type == nodes.num then
		if self.token == tokens.num_hex then
			val = string.format("0x%x", val)
		elseif self.token == tokens.num_oct then
			val = string.format("0%o", val)
		end
	end

	return val
end
function node:src()
	return self.ws_before .. self:_src() .. self.ws_after
end

-- итерация по дочерним узлам
function node:get_children()
	if type(self.value) ~= "table" then return {} end
	return self.value[1] and self.value or {self.value}
end
function _get_children_array(self)
	local children = {}
	for _, v in ipairs(self.value) do
		table.insert(children, v)
	end
	return children
end

---------------------[[ Выражения ]]---------------------
function node:new_call(name, args)
	return node:new({_type = nodes.call, value = args, name = name,
	ws_after_name = "", ws_before_params = "",
	print = function(self)
		local s = "(fun call '" .. name .. "'\n" .. self.dbg .. "\targs [\n"
		for _, v in ipairs(self.value) do
			v.dbg = self.dbg .. "\t"
			s = s .. tostring(v) .. ",\n"
		end
		return s .. self.dbg .. "])"
	end,
	_src = function(self)
		s = name .. self.ws_after_name .. "(" .. self.ws_before_params
		if #self.value > 0 then
			for i, v in ipairs(self.value) do
				if i > 1 then
					s = s .. ","
				end
				s = s .. v:src()
			end
		end
		return s .. ")"
	end,
	get_children = _get_children_array
	})
end
function node:new_var(id)
	return node:new({_type = nodes.var, value = id,
	print = function(self)
		return "(var '" .. tostring(self.value) .. "')"
	end,
	_src = function(self)
		return tostring(self.value)
	end,
	get_children = function(self)
		return {}
	end
	})
end
function node:new_cast(cast_type, value)
	return node:new({_type = nodes.cast, cast_type = cast_type, value = value,
	print = function(self)
		self.value.dbg = self.dbg .. "\t"
		return "(cast\n" .. self.dbg .. "\ttype " .. tostring(self.cast_type) .. "\n" .. self.dbg .. "\tvalue\n" .. tostring(value) .. "\n" .. self.dbg .. ")"
	end,
	_src = function(self)
		return "(" .. tostring(self.cast_type) .. ")" .. self.value:src()
	end
	})
end

function node:new_un_op(op, x, postfix)
	return node:new({_type = nodes.un_op, op = op, value = x, postfix = postfix,
	print = function(self)
		self.value.dbg = self.dbg .. "\t"
		return "(unary op " .. token_to_str(self.op) .. "\n" .. self.dbg .. "\tvalue\n" .. tostring(self.value) .. "\n" .. self.dbg .. (postfix and "postfix)" or ")")
	end,
	_src = function(self)
		return token_to_str(self.op) .. self.value:src()
	end
	})
end
function node:new_bin_op(op, x, y)
	return node:new({_type = nodes.bin_op, op = op, value = {x, y},
	print = function(self)
		self.value[1].dbg = self.dbg .. "\t"
		self.value[2].dbg = self.dbg .. "\t"
		return "(bin op " .. token_to_str(self.op) .. "\n\t" .. self.dbg .. "values\n" .. tostring(self.value[1]) .. ",\n" .. tostring(self.value[2]) .. "\n" .. self.dbg .. ")"
	end,
	_src = function(self)
		return self.value[1]:src() .. token_to_str(self.op) .. self.value[2]:src()
	end
	})
end
function node:new_cond(cond, a, b)
	return node:new({_type = nodes.cond, value = {cond, a, b},
	print = function(self)
		self.value[1].dbg = self.dbg .. "\t"
		self.value[2].dbg = self.dbg .. "\t"
		self.value[3].dbg = self.dbg .. "\t"
		return "(cond op\n" .. self.dbg .. "\tvalues\n" .. tostring(self.value[1]) .. ",\n" .. tostring(self.value[2]) .. ",\n" .. tostring(self.value[3]) .. "\n" .. self.dbg .. ")"
	end,
	_src = function(self)
		return self.value[1]:src() .. "?" .. self.value[2]:src() .. ":" .. self.value[3]:src()
	end
	})
end
function node:new_index(x, i)
	-- ws_before_closing_bracket нужен только в том случае, если self.value[2] == nil (пустые квадратные скобки)
	return node:new({_type = nodes.index, value = {x, i}, ws_before_brackets = "", ws_before_closing_bracket = "",
	print = function(self)
		self.value[1].dbg = self.dbg .. "\t"
		local value2_prefix = ""
		if self.value[2] then
			self.value[2].dbg = self.dbg .. "\t"
		else
			value2_prefix = self.dbg .. "\t"
		end
		return "(index, values\n" .. tostring(self.value[1]) .. ",\n\t" .. value2_prefix .. tostring(self.value[2]) .. "\n" .. self.dbg .. ")"
	end,
	_src = function(self)
		return self.value[1]:src() .. self.ws_before_brackets .. "[" .. (self.value[2] and self.value[2]:src() or self.ws_before_brackets) .. self.ws_before_closing_bracket .. "]"
	end
	})
end

---------------------[[ Утверждения ]]---------------------
function node:new_if(cond, body, else_body)
	return node:new({_type = nodes._if, value = else_body and {body, else_body} or {body}, cond = cond,
	ws_after_if = "",
	print = function(self)
		self.cond.dbg = self.dbg .. "\t"
		self.value[1].dbg = self.dbg .. "\t"
		if self.value[2] then
			self.value[2].dbg = self.dbg .. "\t"
		end
		return "(if\n" .. self.dbg .. "\tcond\n" .. tostring(self.cond) .. "\n" .. self.dbg .. "\tbody\n" .. tostring(self.value[1]) .. ",\n" .. self.dbg .. (self.value[2] and "\telse\n" .. tostring(self.value[2]) .. "\n" .. self.dbg .. "\t)" or "\n" .. self.dbg .. ")")
	end,
	_src = function(self)
		return "if" .. self.ws_after_if .. "(" .. self.cond:src() .. ")" .. self.value[1]:src() .. (self.value[2] and "else" .. self.value[2]:src() or "")
	end,
	get_children = function(self)
		return {self.cond, self.value[1], self.value[2]}
	end
	})
end
function node:new_while(cond, body)
	return node:new({_type = nodes._while, value = body, cond = cond, ws_after_while = "", ws_before_body = "",
	print = function(self)
		self.cond.dbg = self.dbg .. "\t"
		self.value.dbg = self.dbg .. "\t"
		return "(while\n" .. self.dbg .. "\tcond\n" .. tostring(self.cond) .. ",\n" .. self.dbg .. "\tbody\n" .. tostring(self.value) .. "\n" .. self.dbg .. "\t)"
	end,
	_src = function(self)
		return "while" .. self.ws_after_while .. "(" .. self.cond:src() .. ")" .. self.ws_before_body .. self.value:src()
	end,
	get_children = function(self)
		return {self.cond, self.value}
	end
	})
end
function node:new_for(begin, cond, iter, body)
	return node:new({_type = nodes._for, value = body, begin = begin, cond = cond, iter = iter, ws_after_for = "", ws_before_body = "", ws_for_nil_statements = {},
	print = function(self)
		if self.begin then self.begin.dbg = self.dbg .. "\t" end
		if self.cond then self.cond.dbg = self.dbg .. "\t" end
		if self.iter then self.iter.dbg = self.dbg .. "\t" end
		if self.value then
			self.value.dbg = self.dbg .. "\t"
		end
		return "(for\n"
			.. self.dbg .. "\tbegin" .. (self.begin and "\n" .. tostring(self.begin) or " nil") .. ",\n"
			.. self.dbg .. "\tcond" .. (self.cond and "\n" .. tostring(self.cond) or " nil") .. ",\n"
			.. self.dbg .. "\titer" .. (self.iter and "\n" .. tostring(self.iter) or " nil") .. ",\n"
			.. self.dbg .. "\tbody\n" .. tostring(self.value) .. "\n"
			.. self.dbg .. "\t)"
	end,
	_src = function(self)
		local s = "for" .. self.ws_after_for .. "("
		local nil_statement_i = 0
		local statements = {self.begin, self.cond, self.iter}
		for i = 1, 3 do
			if not statements[i] then
				nil_statement_i = nil_statement_i + 1
				s = s .. self.ws_for_nil_statements[nil_statement_i]
			else
				s = s .. statements[i]:src()
			end
			if i < 3 then
				s = s .. ";"
			end
		end
		return s .. ")" .. self.ws_before_body .. (self.value and self.value:src() or ";")
	end,
	get_children = function(self)
		return {self.cond, self.value}
	end
	})
end
function node:new_return(value)
	return node:new({_type = nodes._return, value = value,
	print = function(self)
		if self.value then
			self.value.dbg = self.dbg .. "\t"
		end
		return "(return" .. (self.value and "\n" .. self.dbg .. "\tvalue\n" .. tostring(self.value) .. "\n" .. self.dbg .. "\t)" or ")")
	end,
	_src = function(self)
		return "return" .. (self.value and self.value:src() or "")
	end
	})
end
function node:new_braces(statements)
	return node:new({_type = nodes.braces, value = statements, ws_after_opening = "",
	print = function(self)
		local s = "(braces\n" .. self.dbg .. "\tstatements [\n"
		for _,v in ipairs(self.value) do
			v.dbg = self.dbg .. "\t"
			s = s ..  tostring(v) .. ",\n"
		end
		return s .. self.dbg .. "\t])"
	end,
	_src = function(self)
		local s = "{" .. self.ws_after_opening
		if #self.value > 0 then
			for i, v in ipairs(self.value) do
				s = s .. v:src()
			end
		end
		return s .. "}"
	end,
	get_children = _get_children_array
	})
end
function node:new_parantheses(value)
	return node:new({_type = nodes.parantheses, value = value,
	_src = function(self)
		return "(" .. self.value:src() .. ")"
	end
	})
end
function node:new_comma(value)
	return node:new({_type = nodes.comma, value = value,
	print = function(self)
		local s = "(comma\n" .. self.dbg .. "\tstatements [\n"
		for _,v in ipairs(self.value) do
			v.dbg = self.dbg .. "\t"
			s = s ..  tostring(v) .. ",\n"
		end
		return s .. self.dbg .. "\t])"
	end,
	_src = function(self)
		local s = ""
		if #self.value > 0 then
			for i, v in ipairs(self.value) do
				if i > 1 then
					s = s .. ","
				end
				s = s .. v:src()
			end
		end
		return s
	end,
	get_children = _get_children_array
	})
end
function node:new_decl(var_type, vars)
	if vars.value then -- один узел вместо таблицы
		vars = {vars}
	end

	return node:new({_type = nodes.decl, var_type = var_type, value = vars,
	print = function(self)
		local s = "(decl [" .. tostring(self.var_type) .. "]\n"
		for i, v in ipairs(self.value) do
			if i > 1 then
				s = s .. "\n"
			end
			v.dbg = self.dbg .. "\t"
			s = s .. tostring(v) .. ""
		end
		return s .. "\n" .. self.dbg .. ")"
	end,
	_src = function(self)
		local s = (self.var_type._type == nodes.compound and self.var_type:src() or tostring(self.var_type))
		for i, v in ipairs(self.value) do
			if i > 1 then
				s = s .. ','
			end
			s = s .. v:src()
		end
		return s
	end,
	get_children = _get_children_array
	})
end

function node:new_compound(decls, compound_type, name)
	return node:new({_type = nodes.compound, value = decls, name = name, ws_after_type = "", ws_after_name = "",
	print = function(self)
		local s = "(compound '" .. (self.name and tostring(self.name) or "") .. "'\n" .. self.dbg .. "\tdeclarations [\n"
		for _,v in ipairs(self.value) do
			v.dbg = self.dbg .. "\t"
			s = s .. tostring(v) .. ",\n"
		end
		return s .. self.dbg .. "\t])"
	end,
	_src = function(self)
		local s = (tostring(self.name) or "") .. self.ws_after_type .. self.ws_after_name .. "{"
		if #self.value > 0 then
			for i, v in ipairs(self.value) do
				s = s .. v:src()
			end
		end
		return s .. "}"
	end,
	get_children = _get_children_array
	})
end

---------------------[[ Другое ]]---------------------
function node:new_enum_decl(decls, name)
	return node:new({_type = nodes.enum_decl, value = decls, name = name, ws_after_enum = "", ws_after_name = "",
	print = function(self)
		local s = "(enum '" .. (self.name and self.name or "") .. "'\n" .. self.dbg .. "\tdeclarations [\n"
		for _,v in ipairs(self.value) do
			v.dbg = self.dbg .. "\t"
			s = s ..  tostring(v) .. ",\n"
		end
		return s .. self.dbg .. "\t])"
	end,
	_src = function(self)
		local s = "enum " .. self.ws_after_enum .. (self.name or "") .. self.ws_after_name .. "{"
		if #self.value > 0 then
			for i, v in ipairs(self.value) do
				if i > 1 then
					s = s .. ","
				end
				s = s .. v:src()
			end
		end
		return s .. "}"
	end,
	get_children = _get_children_array
	})
end

function node:new_params(params)
	return node:new({_type = nodes.params, value = params,
	print = function(self)
		local s = "(params [\n"
		for _,v in ipairs(self.value) do
			v.dbg = self.dbg .. "\t"
			s = s ..  tostring(v) .. ",\n"
		end
		return s .. self.dbg .. "])"
	end,
	_src = function(self)
		local s = "("
		if #self.value > 0 then
			for i, v in ipairs(self.value) do
				if i > 1 then
					s = s .. ","
				end
				s = s .. v:src()
			end
		end
		return s .. ")"
	end,
	get_children = _get_children_array
	})
end

function node:new_func(params, body, id, return_type)
	return node:new({_type = nodes.func, value = body, params = params, id = id, return_type = return_type,
	ws_after_return_type = "",
	print = function(self)
		self.value.dbg = self.dbg .. "\t"
		self.params.dbg = self.dbg .. "\t"
		return "(func '" .. tostring(self.id) .. "', return type " .. tostring(self.return_type) .. "\n" .. self.dbg .. "\tparams\n" .. tostring(self.params) .. "\n" .. self.dbg .. "\tbody\n" .. tostring(self.value) .. "\n" .. self.dbg .. "\t)"
	end,
	_src = function(self)
		return (tostring(self.return_type) or "NORETURNTYPE") .. self.ws_after_return_type .. tostring(self.id) .. self.params:src() .. self.value:src()
	end,
	get_children = function(self)
		local children = {}
		table.insert(children, self.params)
		table.insert(children, self.value)
		return children
	end
	})
end


function expression(level, src, ctx, dbg)
	dbg = dbg or ""

	if not ctx.token then
		print("line " .. ctx.line .. ": unexpected EOF of expression")
		os.exit(1)
	end

	-- временные локальные переменные
	local rnode, rnode2

	-- для использования в бинарных операторах
	local unit_node

	if enbf_debug then print(dbg .. "expression", ctx.token, ctx.token_value) end
	if is_token_num(ctx.token) then
		local val = ctx.token_value
		unit_node = node:new({_type = nodes.num, value = val, token = ctx.token})
		ctx = next_token(src, ctx)
	elseif ctx.token == tokens.char then
		local val = ctx.token_value
		ctx = next_token(src, ctx)
		unit_node = node:new({_type = nodes.char, value = val})
	elseif ctx.token == tokens.str then
		local val = ctx.token_value
		ctx = next_token(src, ctx)
		unit_node = node:new({_type = nodes.str, value = val})
	elseif ctx.token == tokens.id then
		local id = ctx.token_value
		ctx = next_token(src, ctx)

		if ctx.token == '(' then -- вызов функции
			if #id.mods > 0 then
				print("line " .. ctx.line .. ": invalid function identifier (has modifiers)")
				os.exit(1)
			end
			local args = {}
			local ws_after_name = ctx.ws
			ctx = next_token(src, ctx) -- пропуск '('
			local ws_before_params
			while ctx.token ~= ')' do
				local arg_ws_before = ctx.ws
				if not ws_before_params then
					ws_before_params = ctx.ws
					arg_ws_before = ""
				end
				rnode, ctx = expression(tokens.assign, src, ctx, dbg .. "\t")
				rnode.ws_before = rnode.ws_before .. arg_ws_before
				rnode.ws_after = rnode.ws_after .. ctx.ws
				table.insert(args, rnode)

				if ctx.token == ',' then -- пропуск запятой
					ctx = next_token(src, ctx)
				end
			end
			if not ws_before_params then
				ws_before_params = ctx.ws
			end

			ctx = next_token(src, ctx) -- пропуск закрывающей скобки ')'

			unit_node = node:new_call(id.name, args)
			unit_node.ws_before_params = ws_before_params
			unit_node.ws_after_name = ws_after_name
			unit_node.arg_ws_after = arg_ws_after
		else
			-- TODO надо проверять, что ID не имеет модификаторов, но тогда sizeof нужно обрабатывать отдельно
			if is_id_token_a_type(id) then
				while ctx.token == tokens.mul do -- пропуск указателей
					table.insert(id.pointers_ws, ctx.ws)
					ctx = next_token(src, ctx)
				end
			end
			unit_node = node:new_var(id)
		end
	elseif ctx.token == '(' then
		ctx = next_token(src, ctx)
		if ctx.token == tokens.id and is_id_token_a_type(ctx.token_value) then -- приведение типов
			local cast_type = ctx.token_value
			ctx = next_token(src, ctx)
			while ctx.token == tokens.mul do -- пропуск указателей
				table.insert(cast_type.pointers_ws, ctx.ws)
				ctx = next_token(src, ctx)
			end

			if ctx.token ~= ')' then
				print("line " .. ctx.line .. ": expected ')' in type cast")
				os.exit(1)
			end

			ctx = next_token(src, ctx) -- пропуск ')'
			rnode, ctx = expression(tokens.inc, src, ctx, dbg .. "\t")
			unit_node = node:new_cast(cast_type, rnode)
		else -- скобки ()
			local ws_before_value = ctx.ws
			rnode, ctx = expression(tokens.assign, src, ctx, dbg .. "\t")
			if ctx.token ~= ')' then
				print("line " .. ctx.line .. ": expected ')' in parantheses")
				os.exit(1)
			end
			ctx = next_token(src, ctx) -- пропуск ')'
			rnode.ws_before = rnode.ws_before .. ws_before_value
			unit_node = node:new_parantheses(rnode)
		end
	elseif ctx.token == tokens.mul or ctx.token == tokens._and or ctx.token == tokens.lnot or ctx.token == tokens._not or ctx.token == tokens.add or ctx.token == tokens.inc or ctx.token == tokens.dec then -- унарные операторы
		local op = ctx.token
		ctx = next_token(src, ctx)
		rnode, ctx = expression(tokens.inc, src, ctx, dbg .. "\t")
		unit_node = node:new_un_op(op, rnode)
	elseif ctx.token == tokens.sub then -- отдельный случай с отрицательными числами
		ctx = next_token(src, ctx)
		if is_token_num(ctx.token) then
			local val = ctx.token_value
			unit_node = node:new({_type = nodes.num, value = -val, token = ctx.token})
			ctx = next_token(src, ctx)
		else
			rnode, ctx = expression(tokens.inc, src, ctx, dbg .. "\t")
			unit_node = node:new_un_op(tokens.sub, rnode)
		end
	elseif ctx.token == '{' then -- list-initializer
		ctx = next_token(src, ctx)
		local values = {}
		while ctx.token ~= '}' do
			local ws_before = ctx.ws
			rnode, ctx = expression(tokens.assign, src, ctx, dbg .. "\t")
			rnode.ws_before = ws_before
			table.insert(values, rnode)
			if ctx.token == ',' then
				ctx = next_token(src, ctx)
			elseif ctx.token ~= '}' then
				print("line " .. ctx.line .. ": expected '}' to end a list-initializer")
				os.exit(1)
			end
		end
		if #values > 0 then
			values[#values].ws_after = ctx.ws
		end
		ctx = next_token(src, ctx)
		return node:new_braces({node:new_comma(values)}), ctx
	end

	-- бинарные и постфиксные операторы
	while type(ctx.token) == "number" and ctx.token >= level do
		if enbf_debug then print(dbg .. "\ttoken", ctx.token, ctx.token_value) end

		if type(ctx.token) == "string" and (ctx.token == ';' or ctx.token == ')') then
			break
		end

		if ctx.token == tokens.assign then
			unit_node.ws_after = unit_node.ws_after .. ctx.ws
			ctx = next_token(src, ctx) -- пропуск '='

			local ws_before_op2 = ctx.ws
			rnode, ctx = expression(tokens.assign, src, ctx, dbg .. "\t")
			rnode.ws_before = rnode.ws_before .. ws_before_op2
			unit_node = node:new_bin_op(tokens.assign, unit_node, rnode)
		elseif ctx.token == tokens.cond then
			unit_node.ws_after = unit_node.ws_after .. ctx.ws
			ctx = next_token(src, ctx) -- пропуск '?'

			local ws_before_first_operand = ctx.ws
			rnode, ctx = expression(tokens.assign, src, ctx, dbg .. "\t")
			if ctx.token ~= ':' then
				print("line " .. ctx.line .. ": expected ':' in conditional operator")
				os.exit(1)
			end
			if not rnode then
				print("line " .. ctx.line .. ": conditional operator has no 2nd operand")
				os.exit(1)
			end
			rnode.ws_before = ws_before_first_operand
			rnode.ws_after = ctx.ws

			ctx = next_token(src, ctx) -- пропуск ':'
			local ws_before_second_operand = ctx.ws
			rnode2, ctx = expression(tokens.cond, src, ctx, dbg .. "\t")
			rnode2.ws_before = ws_before_second_operand
			unit_node = node:new_cond(unit_node, rnode, rnode2)
		elseif is_token_binary_op(ctx.token) then
			if not unit_node then
				print("line " .. ctx.line .. ": operator '" .. token_to_str(ctx.token) .. "' does not have 1st operand")
				os.exit(1)
			end
			unit_node.ws_after = unit_node.ws_after .. ctx.ws
			local op = ctx.token
			ctx = next_token(src, ctx)
			local ws_before_op2 = ctx.ws

			rnode, ctx = expression(op + 1, src, ctx, dbg .. "\t")
			if not rnode then
				print("line " .. ctx.line .. ": expected a second operand for binary operator '" .. token_to_str(op) .."'")
				os.exit(1)
			end
			rnode.ws_before = rnode.ws_before .. ws_before_op2
			unit_node = node:new_bin_op(op, unit_node, rnode)
		elseif ctx.token == tokens.inc or ctx.token == tokens.dec then
			unit_node = node:new_un_op(ctx.token, unit_node, true)
			ctx = next_token(src, ctx)
		elseif ctx.token == tokens.brack then
			ctx = next_token(src, ctx)
			local ws_before_idx = ctx.ws
			rnode, ctx = expression(tokens.assign, src, ctx, dbg .. "\t")
			if ctx.token ~= ']' then
				print("line " .. ctx.line .. ": expected ']' to close indexing operator")
				os.exit(1)
			end
			rnode.ws_after = rnode.ws_after .. ctx.ws

			ctx = next_token(src, ctx)
			rnode.ws_before = rnode.ws_before .. ws_before_idx
			unit_node = node:new_index(unit_node, rnode)
		else
			print("line " .. ctx.line .. ": unexpected end of expression")
			os.exit(1)
		end
		if enbf_debug then print(dbg .. "going next") end
	end

	if enbf_debug then print(dbg .. "expression returning", ctx.token, ctx.token_value) end
	return unit_node, ctx
end

function statement(src, ctx, dbg)
	dbg = dbg or ""

	if not ctx.token then
		print("line " .. ctx.line .. ": unexpected EOF of statement")
		os.exit(1)
	end

	-- временные локальные переменные
	local rnode, rnode2

	if enbf_debug then print(dbg .. "statement", ctx.token, ctx.token_value) end
	if ctx.token == tokens.id and is_id_token_a_type(ctx.token_value) then -- объявление переменной или функции
		local type_id
		if enbf_debug then print(dbg .. "\tvariable or function declaration") end

		if ctx.token == tokens.id and is_id_token_compound(ctx.token_value) then
			local name_token = ctx.token_value
			ctx = next_token(src, ctx)
			if ctx.token == '{' then
				local ws_after_type = ctx.ws
				type_id, ctx = compound_decl(src, ctx, dbg .. "\t")
				type_id.ws_after_type = ws_after_type
				type_id.name = name_token
			else
				type_id = name_token
			end
		end

		if not type_id then -- обычный тип данных
			type_id = ctx.token_value
			ctx = next_token(src, ctx)
		end

		local insert_pointers_into = type_id._type == nodes.compound
						and type_id.name.pointers_ws
						or type_id.pointers_ws

		while ctx.token == tokens.mul do -- пропуск указателей
			table.insert(insert_pointers_into, ctx.ws)
			ctx = next_token(src, ctx)
		end

		if ctx.token ~= tokens.id then
			if ctx.token ~= ';' or not type_id then 
				print("line " .. ctx.line .. ": expected variable or function name in global declaration, got " .. token_to_str(ctx.token))
				os.exit(1)
			end
			-- объявление структуры без имени переменной
			return type_id, ctx
		end

		local vars = {}
		while ctx.token == tokens.id do
			local decl = node:new_var(ctx.token_value)
			decl.ws_before = ctx.ws

			ctx = next_token(src, ctx)
			while ctx.token == tokens.mul do -- пропуск указателей
				table.insert(type_id.pointers_ws, ctx.ws)
				ctx = next_token(src, ctx)
			end
			decl.ws_after = ctx.ws

			if ctx.token == '[' then
				ctx = next_token(src, ctx)
				local ws_before_idx = ctx.ws
				rnode, ctx = expression(tokens.assign, src, ctx, dbg .. "\t")
				if ctx.token ~= ']' then
					print("line " .. ctx.line .. ": expected ']' to close indexing operator")
					os.exit(1)
				end

				local ws_before_closing_bracket = ctx.ws
				if rnode then
					rnode.ws_before = rnode.ws_before .. ws_before_idx
					rnode.ws_after = rnode.ws_after .. ws_before_closing_bracket
					ws_before_closing_bracket = ""
				end
				ctx = next_token(src, ctx)
				local idx = node:new_index(decl, rnode)
				idx.ws_before_closing_bracket = ws_before_closing_bracket
				idx.ws_after = ctx.ws
				decl = idx
			end

			if ctx.token == tokens.assign then -- объявление переменной с инициализацией
				ctx = next_token(src, ctx)
	
				local ws_after_assign = ctx.ws
				rnode, ctx = expression(tokens.assign, src, ctx, dbg .. "\t")
				rnode.ws_before = ws_after_assign
				local assign_op = node:new_bin_op(tokens.assign, decl, rnode)
				table.insert(vars, assign_op)
			else
				table.insert(vars, decl)
			end

			if ctx.token ~= ',' then
				break
			end
			ctx = next_token(src, ctx)
		end


		if ctx.token == '(' then -- объявление функции
			rnode, ctx = func_decl(src, ctx, dbg .. "\t")
			rnode.ws_after_return_type = vars[1].ws_before
			rnode.id = vars[1].value
			rnode.return_type = type_id
			return rnode, ctx
		end
		-- объявление переменной (переменных)
		if ctx.token ~= ';' then
			print("line " .. ctx.line .. ": expected ';' after variable declaration, got " .. token_to_str(ctx.token))
			os.exit(1)
		end
		if #vars > 0 then
			vars[#vars].ws_after = "" -- избегаем дублирования с ws_after самого утверждения
		end
		rnode = node:new_decl(type_id, vars)

		for _, v in ipairs(vars) do -- объявление идентификаторов
			identifiers[v.value] = {}
		end

		return rnode, ctx
	elseif ctx.token == tokens.id and ctx.token_value.name == "if" then
		ctx = next_token(src, ctx)
		local ws_after_if = ctx.ws
		if ctx.token ~= '(' then
			print("line " .. ctx.line .. ": expected '(' after 'if'")
			os.exit(1)
		end
		ctx = next_token(src, ctx)
		rnode, ctx = expression(tokens.assign, src, ctx, dbg .. "\t")
		local cond = rnode
		if ctx.token ~= ')' then
			print("line " .. ctx.line .. ": expected ')' after if condition")
			os.exit(1)
		end
		ctx = next_token(src, ctx) -- пропуск ')'
		
		local ws_before_body = rnode.ws_after .. ctx.ws
		rnode, ctx = statement(src, ctx, dbg .. "\t")
		rnode.ws_before = rnode.ws_before .. ws_before_body
		if ctx.token == ';' then
			rnode.ws_after = ctx.ws .. ';'
			ctx = next_token(src, ctx)
		end
		if ctx.token == tokens.id and ctx.token_value.name == "else" then
			rnode.ws_after = rnode.ws_after .. ctx.ws
			ctx = next_token(src, ctx) -- пропуск ')'
			local ws_before_else = ctx.ws
			rnode2, ctx = statement(src, ctx, dbg .. "\t")
			rnode2.ws_before = rnode2.ws_before .. ws_before_else
		end

		local res = node:new_if(cond, rnode, rnode2)
		res.ws_after_if = ws_after_if
		return res, ctx
	elseif ctx.token == tokens.id and ctx.token_value.name == "while" then
		ctx = next_token(src, ctx)
		local ws_after_while = ctx.ws
		if ctx.token ~= '(' then
			print("line " .. ctx.line .. ": expected '(' after 'while'")
			os.exit(1)
		end
		ctx = next_token(src, ctx)

		local ws_before_cond = ctx.ws
		rnode, ctx = expression(tokens.assign, src, ctx, dbg .. "\t")
		rnode.ws_before = ws_before_cond
		rnode.ws_after = ctx.ws
		if ctx.token ~= ')' then
			print("line " .. ctx.line .. ": expected ')' after while condition")
			os.exit(1)
		end
		ctx = next_token(src, ctx) -- пропуск ')'
		local ws_before_body = ctx.ws
		rnode2, ctx = statement(src, ctx, dbg .. "\t")

		local res = node:new_while(rnode, rnode2)
		res.ws_after_while = ws_after_while
		res.ws_before_body = ws_before_body
		return res, ctx
	elseif ctx.token == tokens.id and ctx.token_value.name == "for" then
		ctx = next_token(src, ctx)
		local ws_after_for = ctx.ws
		if ctx.token ~= '(' then
			print("line " .. ctx.line .. ": expected '(' after 'for'")
			os.exit(1)
		end

		local res = node:new_for(begin, cond, iter, rnode2)

		ctx = next_token(src, ctx) -- пропуск '('
		local ws_before = ctx.ws
		res.begin, ctx = statement(src, ctx, dbg .. "\t")
		if res.begin then
			res.begin.ws_before = ws_before
			res.begin.ws_after = ctx.ws
		else
			table.insert(res.ws_for_nil_statements, ws_before)
		end
		if ctx.token ~= ';' and res.begin then
			print("line " .. ctx.line .. ": expected ';' after beginning statement of 'for' loop, got " .. token_to_str(ctx.token))
			os.exit(1)
		end

		if res.begin then -- если res.begin == nil, то пустое утверждение ';' уже было пропущено
			ctx = next_token(src, ctx) -- пропуск ';'
		end
		ws_before = ctx.ws
		res.cond, ctx = statement(src, ctx, dbg .. "\t")
		if res.cond then
			res.cond.ws_before = ws_before
			res.cond.ws_after = ctx.ws
		else
			table.insert(res.ws_for_nil_statements, ws_before)
		end
		if ctx.token ~= ';' and res.cond then
			print("line " .. ctx.line .. ": expected ';' after conditional statement of 'for' loop, got " .. token_to_str(ctx.token))
			os.exit(1)
		end

		if res.cond then -- если res.cond == nil, то пустое утверждение ';' уже было пропущено
			ctx = next_token(src, ctx) -- пропуск ';'
		end
		ws_before = ctx.ws
		res.iter, ctx = statement(src, ctx, dbg .. "\t")
		if res.iter then
			res.iter.ws_before = ws_before
			res.iter.ws_after = ctx.ws
		else
			table.insert(res.ws_for_nil_statements, ws_before)
		end

		if ctx.token ~= ')' then
			print("line " .. ctx.line .. ": expected ')' after iterating statement of 'for' loop")
			os.exit(1)
		end

		ctx = next_token(src, ctx) -- пропуск ')'
		local ws_before_body = ctx.ws
		res.value, ctx = statement(src, ctx, dbg .. "\t")

		res.ws_after_for = ws_after_for
		res.ws_before_body = ws_before_body
		return res, ctx
	elseif ctx.token == '{' then
		ctx = next_token(src, ctx) -- пропуск '{'
		local ws_after_opening = ctx.ws
		ctx.ws = ""
		local statements = {}
		while ctx.token ~= '}' do
			if #statements > 0 then
				statements[#statements].ws_after = statements[#statements].ws_after .. ctx.ws
			end

			rnode, ctx = statement(src, ctx, dbg .. "\t")
			if rnode == nil then -- ';'
				statements[#statements].ws_after = statements[#statements].ws_after .. ';'
			end
			if ctx.token == nil then
				print("line " .. ctx.line .. ": curly brace '{' was never closed")
				os.exit(1)
			end
			table.insert(statements, rnode)
		end
		if #statements > 0 then
			statements[#statements].ws_after = statements[#statements].ws_after .. ctx.ws
		end

		local prev = copy_token_ctx(ctx)
		ctx = next_token(src, ctx) -- пропуск '}'
		local braces = node:new_braces(statements)
		braces.ws_after_opening = ws_after_opening
		ctx.prev = prev
		return braces, ctx
	elseif ctx.token == tokens.id and ctx.token_value.name == "return" then
		ctx = next_token(src, ctx)
		rnode = nil
		if ctx.token ~= ';' then
			local ws_before = ctx.ws
			rnode, ctx = expression(tokens.assign, src, ctx, dbg .. "\t")
			rnode.ws_before = ws_before
		end
		rnode = node:new_return(rnode)
		if ctx.token ~= ';' then
			print("line " .. ctx.line .. ": expected ';' after 'return', got " .. token_to_str(ctx.token))
			os.exit(1)
		end
		rnode.ws_after = ctx.ws .. ';'

		ctx = next_token(src, ctx) -- пропуск ';'
		return rnode, ctx
	elseif ctx.token == ';' then -- пустое утверждение
		ctx = next_token(src, ctx)
		return nil, ctx
	else -- присваивание или вызов функции
		rnode, ctx = expression(tokens.assign, src, ctx, dbg .. "\t")
		if ctx.token ~= ';' and ctx.token ~= ')' then
			print("line " .. ctx.line .. ": expected ';' or ')' after statement, got " .. token_to_str(ctx.token))
			os.exit(1)
		end
		return rnode, ctx
	end
end

function enum_decl(src, ctx, dbg)
	dbg = dbg or ""

	if not ctx.token then
		print("line " .. ctx.line .. ": unexpected EOF of enum declaration")
		os.exit(1)
	end

	-- временные локальные переменные
	local rnode

	local decls = {}
	if enbf_debug then print(dbg .. "enum, decl", ctx.token, ctx.token_value) end
	while ctx.token ~= '}' do
		if ctx.token ~= tokens.id then
			print("line " .. ctx.line .. ": expected an identifier for enum declaration")
			os.exit(1)
		end
		local id = ctx.token_value.name
		if identifiers[id] and identifiers[id].class then
			print("line " .. ctx.line .. ": attempting to re-define enum '" .. id .. "'")
			os.exit(1)
		end
		local ws_before_var = ctx.ws
		ctx = next_token(src, ctx)

		if ctx.token == tokens.assign then
			local var = node:new_var(id)
			var.ws_before = ws_before_var
			var.ws_after = ctx.ws
			ctx = next_token(src, ctx) -- пропуск '='
			local ws_before_val = ctx.ws

			local is_minus = false
			if ctx.token == tokens.sub then -- частный случай: унарный '-'
				is_minus = true
				ctx = next_token(src, ctx)
			end
			if not is_token_num(ctx.token) then
				print("line " .. ctx.line .. ": enum should be initialized with a number")
				os.exit(1)
			end
			if is_minus then
				ctx.token_value = -ctx.token_value
			end
			local val = node:new({_type = nodes.num, value = ctx.token_value, token = ctx.token})

			val.ws_before = ws_before_val
			table.insert(decls, node:new_bin_op(tokens.assign, var, val))
			ctx = next_token(src, ctx)
			val.ws_after = ctx.ws
		else
			local val = node:new_var(id)
			val.ws_before = ws_before_var
			val.ws_after = ctx.ws
			table.insert(decls, val)
		end

		identifiers[id] = {class = classes.enum_decl}
		if ctx.token ~= ',' and ctx.token ~= '}' then
			print("line " .. ctx.line .. ": expected ',' or '}' after enum declaration")
			os.exit(1)
		end

		if ctx.token == ',' then
			ctx = next_token(src, ctx)
		end
	end

	return node:new_enum_decl(decls), ctx
end

function func_params(src, ctx, dbg)
	dbg = dbg or ""

	if not ctx.token then
		print("line " .. ctx.line .. ": unexpected EOF of function parameters")
		os.exit(1)
	end

	-- временные локальные переменные
	local rnode

	local params = {}
	if enbf_debug then print(dbg .. "func params", ctx.token, ctx.token_value) end
	while ctx.token ~= ')' do
		if ctx.token ~= tokens.id then
			print("line " .. ctx.line .. ": expected a type identifier")
			os.exit(1)
		end
		if not is_id_token_a_type(ctx.token_value) then
			print("line " .. ctx.line .. ": identifier '" .. tostring(ctx.token_value) .. "' is not a type")
			os.exit(1)
		end
		local ws_before_type = ctx.ws
		local type_id = ctx.token_value

		ctx = next_token(src, ctx)
		while ctx.token == tokens.mul do -- пропуск указателей
			table.insert(type_id.pointers_ws, ctx.ws)
			ctx = next_token(src, ctx)
		end

		if ctx.token ~= tokens.id then
			print("line " .. ctx.line .. ": expected parameter name")
			os.exit(1)
		end

		local var = node:new_var(ctx.token_value)
		var.ws_before = ctx.ws
		ctx = next_token(src, ctx)
		var.ws_after = ctx.ws
		local decl = node:new_decl(type_id, {var})
		decl.ws_before = ws_before_type
		table.insert(params, decl)
		if ctx.token == ',' then
			ctx = next_token(src, ctx)
		end
	end

	return node:new_params(params), ctx
end

function func_decl(src, ctx, dbg)
	dbg = dbg or ""

	if not ctx.token then
		print("line " .. ctx.line .. ": unexpected EOF of function declaration")
		os.exit(1)
	end

	local params, body

	if enbf_debug then print(dbg .. "func decl", token, token_value) end
	if ctx.token ~= '(' then
		print("line " .. ctx.line .. ": expected '(' in function declaration")
		os.exit(1)
	end
	local ws_before_params = ctx.ws
	ctx = next_token(src, ctx)
	params, ctx = func_params(src, ctx, dbg .. "\t")
	params.ws_before = ws_before_params
	if ctx.token ~= ')' then
		print("line " .. ctx.line .. ": expected ')' in function declaration")
		os.exit(1)
	end
	ctx = next_token(src, ctx)
	local ws_before_body = ctx.ws

	if ctx.token ~= '{' then
		print("line " .. ctx.line .. ": expected '{' in function declaration")
		os.exit(1)
	end
	body, ctx = statement(src, ctx, dbg .. "\t")
	body.ws_before = ws_before_body
	if enbf_debug then print(dbg .. "tokens after body", ctx.token, ctx.token_value) end

	-- грязный хак (фигурные скобки{} возвращают токен следующий за ними, но нам нужно вернуть })
	ctx.i = ctx.prev.i
	ctx.token = ctx.prev.token
	ctx.token_value = ctx.prev.token_value
	ctx.line = ctx.prev.line
	return node:new_func(params, body), ctx
end

function compound_decl(src, ctx, dbg)
	dbg = dbg or ""

	if not ctx.token then
		print("line " .. ctx.line .. ": unexpected EOF of compound declaration")
		os.exit(1)
	end

	if enbf_debug then print(dbg .. "compound decl", ctx.token, ctx.token_value) end

	local decls = {}
	ctx = next_token(src, ctx) -- пропуск '{'
	while ctx.token ~= '}' and ctx.token do
		local ws_before = ctx.ws
		local decl
		decl, ctx = statement(src, ctx, dbg .. "\t")
		if ctx.token ~= ';' then
			print("line " .. ctx.line .. ": expected ';' after a statement inside a compound")
			os.exit(1)
		end
		decl.ws_after = ctx.ws .. ';'

		decl.ws_before = ws_before
		table.insert(decls, decl)
		ctx = next_token(src, ctx)
	end
	if not ctx.token then
		print("line " .. ctx.line .. ": unexpected EOF of compound declaration")
		os.exit(1)
	end

	if #decls > 0 then
		decls[#decls].ws_after = decls[#decls].ws_after .. ctx.ws
	end

	ctx = next_token(src, ctx) -- пропуск '}'

	return node:new_compound(decls), ctx
end



function global_decl(src, ctx, dbg)
	dbg = dbg or ""

	if not ctx.token then
		print("line " .. ctx.line .. ": unexpected EOF of global declaration")
		os.exit(1)
	end

	-- временные локальные переменные
	local rnode, type_id

	if enbf_debug then print(dbg .. "global decl", ctx.token, ctx.token_value) end

	if ctx.token == tokens.id and ctx.token_value.name == "enum" then
		ctx = next_token(src, ctx)
		local ws_after_enum = ctx.ws
		local ws_after_name = ""
		local enum_id
		if ctx.token ~= '{' then
			if ctx.token ~= tokens.id then
				print("line " .. ctx.line .. ": expected enum identifier in global declaration")
				os.exit(1)
			end
			enum_id = ctx.token_value.name
			ctx = next_token(src, ctx)
			ws_after_name = ctx.ws
		end

		if ctx.token == '{' then -- тела может и не быть
			ctx = next_token(src, ctx)
			rnode, ctx = enum_decl(src, ctx, dbg .. "\t")
			ctx = next_token(src, ctx) -- пропуск '}'
			rnode.name = enum_id
		else
			rnode = node:new_enum_decl({}, enum_id)
		end

		rnode.ws_after_enum = ws_after_enum
		rnode.ws_after_name = ws_after_name
		return rnode, ctx
	end

	rnode, ctx = statement(src, ctx, dbg .. "\t")
	rnode.ws_after = rnode.ws_after .. ctx.ws
	if ctx.token == ';' then
		rnode.ws_after = rnode.ws_after .. ';'
	end
	return rnode, ctx
end
