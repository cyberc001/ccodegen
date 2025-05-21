require "token"
require "os"

nodes = {
	num = 1, str = 2, char = 3,
	call = 10, var = 12, params = 13, decl = 14, func = 15, enum_decl = 16,
	cast = 20,
	un_op = 30, bin_op = 31, cond = 32, index = 33,
	parantheses = 40, braces = 41, comma = 42,

	_if = 50, _while = 51
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
	return self.dbg .. (self.print and self:print() or "(generic node: type " .. self._type .. ", value [" .. tostring(self.value) .. "])")
end
-- возвращает исходный код узла
function node:_src()
	return tostring(self.value)
end
function node:src()
	return self.ws_before .. self:_src() .. self.ws_after
end

---------------------[[ Выражения ]]---------------------
function node:new_call(name, args)
	return node:new({_type = nodes.call, value = args, name = name,
	arg_ws_after = "", ws_after_name = "", ws_before_params = "",
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
				s = s .. v:src() .. self.arg_ws_after[i]
			end
		end
		return s .. ")"
	end})
end
function node:new_var(name)
	return node:new({_type = nodes.var, value = name,
	print = function(self)
		return "(var '" .. name .. "')"
	end,
	_src = function(self)
		return self.value
	end})
end
function node:new_cast(cast_type, value)
	return node:new({_type = nodes.cast, cast_type = cast_type, value = value,
	print = function(self)
		self.value.dbg = self.dbg .. "\t"
		return "(cast\n" .. self.dbg .. "\ttype " .. tostring(self.cast_type) .. "\n" .. self.dbg .. "\tvalue\n" .. tostring(value) .. "\n" .. self.dbg .. ")"
	end,
	_src = function(self)
		return "(" .. self.cast_type:src() .. ")" .. self.value:src()
	end})
end

function node:new_un_op(op, x, postfix)
	return node:new({_type = nodes.un_op, op = op, value = x, postfix = postfix,
	print = function(self)
		self.value.dbg = self.dbg .. "\t"
		return "(unary op " .. token_to_str(self.op) .. "\n" .. self.dbg .. "\tvalue\n" .. tostring(self.value) .. "\n" .. self.dbg .. (postfix and "postfix)" or ")")
	end,
	_src = function(self)
		return token_to_str(self.op) .. self.value:src()
	end})
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
	end})
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
	end})
end
function node:new_index(x, i)
	return node:new({_type = nodes.index, value = {x, i},
	print = function(self)
		self.value[1].dbg = self.dbg .. "\t"
		self.value[2].dbg = self.dbg .. "\t"
		return "(index, values\n" .. tostring(self.value[1]) .. ",\n" .. tostring(self.value[2]) .. "\n" .. self.dbg .. ")"
	end,
	_src = function(self)
		return self.value[1]:src() .. "[" .. self.value[2]:src() .. "]"
	end})
end

---------------------[[ Утверждения ]]---------------------
function node:new_if(cond, body, else_body)
	return node:new({_type = nodes._if, value = else_body and {body, else_body} or {body}, cond = cond,
	print = function(self)
		self.cond.dbg = self.dbg .. "\t"
		self.value[1].dbg = self.dbg .. "\t"
		if self.value[2] then
			self.value[2].dbg = self.dbg .. "\t"
		end
		return "(if\n" .. self.dbg .. "\tcond\n" .. tostring(self.cond) .. "\n" .. self.dbg .. "\tbody\n" .. tostring(self.value[1]) .. ",\n" .. self.dbg .. (self.value[2] and "\telse\n" .. tostring(self.value[2]) .. "\n" .. self.dbg .. "\t)" or "\n" .. self.dbg .. ")")
	end,
	_src = function(self)
		return "if(" .. self.cond:src() .. ")" .. self.value[1]:src() .. (self.value[2] and "else " .. self.value[2]:src() or "")
	end})
end
function node:new_while(cond, body)
	return node:new({_type = nodes._while, value = body, cond = cond,
	print = function(self)
		self.cond.dbg = self.dbg .. "\t"
		self.value.dbg = self.dbg .. "\t"
		return "(while\n" .. self.dbg .. "\tcond\n" .. tostring(self.cond) .. ",\n" .. self.dbg .. "\tbody\n" .. tostring(self.value) .. "\n" .. self.dbg .. "\t)"
	end,
	_src = function(self)
		return "while(" .. self.cond:src() .. ")" .. self.value:src()
	end})
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
		return "return" .. (self.value and " " .. self.value:src() or "")
	end})
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
	end})
end
function node:new_parantheses(value)
	return node:new({_type = nodes.parantheses, value = value,
	_src = function(self)
		return "(" .. self.value:src() .. ")"
	end})
end
function node:new_comma(statements)
	return node:new({_type = nodes.comma, value = statements,
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
	end})
end
function node:new_decl(_type, id)
	return node:new({_type = nodes.decl, _type = _type, value = id, ws_mid = "",
	print = function(self)
		return "(decl [" .. tostring(self._type) .. "] '" .. tostring(self.value) .. "')"
	end,
	_src = function(self)
		return tostring(self._type) .. self.ws_mid .. self.value
	end})
end


---------------------[[ Другое ]]---------------------
function node:new_enum_decl(decls, name)
	return node:new({_type = nodes.enum_decl, value = decls, name = name, ws_after_enum = "", ws_after_name = "",
	print = function(self)
		local s = "(enum\n" .. self.dbg .. "\tdeclarations [\n"
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
	end})
end

function node:new_params(params)
	return node:new({_type = nodes.params, value = params,
	print = function(self)
		local s = "(params [\n"
		for _,v in ipairs(self.value) do
			v.dbg = self.dbg .. "\t"
			s = s ..  tostring(v) .. ",\n"
		end
		return s .. self.dbg .. "\t])"
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
	end})
end

function node:new_func(params, body, name, return_type)
	return node:new({_type = nodes.func, value = body, params = params, name = name, return_type = return_type,
	ws_after_return_type = "",
	print = function(self)
		self.value.dbg = self.dbg .. "\t"
		self.params.dbg = self.dbg .. "\t"
		return "(func '" .. tostring(self.name) .. "', return type " .. tostring(self.return_type) .. "\n" .. self.dbg .. "\tparams\n" .. tostring(self.params) .. "\n" .. self.dbg .. "\tbody\n" .. tostring(self.value) .. "\n" .. self.dbg .. "\t)"
	end,
	_src = function(self)
		return (tostring(self.return_type) or "NORETURNTYPE") .. self.ws_after_return_type .. self.name .. self.params:src() .. self.value:src() 
	end})
end


function expression(level, src, ctx, dbg)
	dbg = dbg or ""

	if not ctx.token then
		print("line " .. line .. ": unexpected EOF of expression")
		os.exit(1)
	end

	-- временные локальные переменные
	local rnode, rnode2

	-- для использования в бинарных операторах
	local unit_node

	print(dbg .. "expression", ctx.token, ctx.token_value)
	if ctx.token == tokens.num then
		local val = ctx.token_value
		ctx = next_token(src, ctx)
		unit_node = node:new({_type = nodes.num, value = val})
	elseif ctx.token == tokens.char then
		local val = ctx.token_value
		ctx = next_token(src, ctx)
		unit_node = node:new({_type = nodes.char, value = val})
	elseif ctx.token == tokens.str then
		local val = ctx.token_value
		ctx = next_token(src, ctx)
		unit_node = node:new({_type = nodes.str, value = val})
	elseif ctx.token == tokens.id then
		local id = ctx.token_value.name
		ctx = next_token(src, ctx)

		if ctx.token == '(' then -- вызов функции
			local args = {}
			local arg_ws_after = {}
			local ws_after_name = ctx.ws
			ctx = next_token(src, ctx) -- пропуск '('
			local ws_before_params
			while ctx.token ~= ')' do
				if not ws_before_params then
					ws_before_params = ctx.ws
				end
				rnode, ctx = expression(tokens.assign, src, ctx, dbg .. "\t")
				table.insert(args, rnode)
				table.insert(arg_ws_after, ctx.ws)
				if ctx.token == ',' then -- пропуск запятой
					ctx = next_token(src, ctx)
				else
				end
			end
			if not ws_before_params then
				ws_before_params = ctx.ws
			end

			ctx = next_token(src, ctx) -- пропуск закрывающей скобки ')'

			unit_node = node:new_call(id, args)
			unit_node.ws_before_params = ws_before_params
			unit_node.ws_after_name = ws_after_name
			unit_node.arg_ws_after = arg_ws_after
		else
			unit_node = node:new_var(id)
			unit_node.ws_after = ctx.ws
		end
	elseif ctx.token == '(' then
		ctx = next_token(src, ctx)
		if ctx.token == tokens.id and (identifiers[ctx.token_value.name].class == classes._type or identifiers[ctx.token_value.name].class == classes.type_mod) then -- приведение типов
			local cast_type = ctx.token_value
			ctx = next_token(src, ctx)
			while ctx.token == tokens.mul do -- пропуск указателей
				ctx = next_token(src, ctx)
				cast_type.pointers = cast_type.pointers + 1
			end

			if ctx.token ~= ')' then
				print("line " .. line .. ": expected ')' in type cast")
				os.exit(1)
			end

			ctx = next_token(src, ctx) -- пропуск ')'
			rnode, ctx = expression(tokens.inc, src, ctx, dbg .. "\t")
			unit_node = node:new_cast(cast_type, rnode)
		else -- скобки ()
			local ws_before_value = ctx.ws
			rnode, ctx = expression(tokens.assign, src, ctx, dbg .. "\t")
			if ctx.token ~= ')' then
				print("line " .. line .. ": expected ')' in parantheses")
				os.exit(1)
			end
			ctx = next_token(src, ctx) -- пропуск ')'
			rnode.ws_before = rnode.ws_before .. ws_before_value
			unit_node = node:new_parantheses(rnode)
		end
	elseif ctx.token == tokens.mul or ctx.token == tokens._and or ctx.token == tokens.lnot or ctx.token == tokens._lnot or ctx.token == tokens.add or ctx.token == tokens.inc or ctx.token == tokens.dec then -- унарные операторы
		local op = ctx.token
		ctx = next_token(src, ctx)
		rnode, ctx = expression(tokens.inc, src, ctx, dbg .. "\t")
		unit_node = node:new_un_op(op, rnode)
	elseif ctx.token == tokens.sub then -- отдельный случай с отрицательными числами
		ctx = next_token(src, ctx)
		if ctx.token == tokens.num then
			local val = ctx.token_value
			ctx = next_token(src, ctx)
			unit_node = node:new({_type = nodes.num, value = -val})
		else
			rnode, ctx = expression(tokens.inc, src, ctx, dbg .. "\t")
			unit_node = node:new_un_op(tokens.sub, rnode)
		end
	end

	-- бинарные и постфиксные операторы
	while type(ctx.token) == "number" and ctx.token >= level do
		print(dbg .. "token", ctx.token)
		if ctx.token == tokens.assign then
			ctx = next_token(src, ctx) -- пропуск '='
			local ws_before_op2 = ctx.ws
			rnode, ctx = expression(tokens.assign, src, ctx, dbg .. "\t")
			rnode.ws_before = rnode.ws_before .. ws_before_op2
			unit_node = node:new_bin_op(tokens.assign, unit_node, rnode)
		elseif ctx.token == tokens.cond then
			ctx = next_token(src, ctx) -- пропуск '?'
			rnode, ctx = expression(tokens.assign, src, ctx, dbg .. "\t")
			if ctx.token ~= ':' then
				print("line " .. line .. ": expected ':' in conditional operator")
				os.exit(1)
			end
			ctx = next_token(src, ctx) -- пропуск ':'
			rnode2, ctx = expression(tokens.cond, src, ctx, dbg .. "\t")
			unit_node = node:new_cond(unit_node, rnode, rnode2)
		elseif ctx.token >= tokens.lor and ctx.token <= tokens.mod then
			local op = ctx.token
			ctx = next_token(src, ctx)
			rnode, ctx = expression(op + 1, src, ctx, dbg .. "\t")
			unit_node = node:new_bin_op(op, unit_node, rnode)
		elseif ctx.token == tokens.inc or ctx.token == tokens.dec then
			unit_node = node:new_un_op(ctx.token, unit_node, true)
			ctx = next_token(src, ctx)
		elseif ctx.token == tokens.brack then
			ctx = next_token(src, ctx)
			rnode, ctx = expression(tokens.assign, src, ctx, dbg .. "\t")
			if ctx.token ~= ']' then
				print("line " .. line .. ": expected ']' to close indexing operator")
				os.exit(1)
			end
			ctx = next_token(src, ctx)
			unit_node = node:new_index(unit_node, rnode)
		else
			print("line " .. line .. ": unexpected end of expression")
			os.exit(1)
		end
		print(dbg .. "going next")
	end

	print(dbg .. "expression returning", ctx.token, ctx.token_value)
	return unit_node, ctx
end

function statement(src, ctx, dbg)
	dbg = dbg or ""

	if not ctx.token then
		print("line " .. line .. ": unexpected EOF of statement")
		os.exit(1)
	end

	-- временные локальные переменные
	local rnode, rnode2

	print(dbg .. "statement", ctx.token, ctx.token_value)
	if ctx.token == tokens.id and (identifiers[ctx.token_value.name].class == classes._type or identifiers[ctx.token_value.name].class == classes.type_mod) then -- объявление переменной
		local type_id = ctx.token_value
		ctx = next_token(src, ctx)
		local ws_after_type = ctx.ws

		while ctx.token == tokens.mul do -- пропуск указателей
			ctx = next_token(src, ctx)
			type_id.pointers = type_id.pointers + 1
		end

		if ctx.token ~= tokens.id then
			print("line " .. line .. ": expected variable name in declaration")
			os.exit(1)
		end
		local var_name = ctx.token_value.name
		ctx = next_token(src, ctx)
		local ws_after_decl = ctx.ws

		if ctx.token == tokens.assign then
			ctx = next_token(src, ctx)
			local ws_before_val = ctx.ws

			rnode, ctx = expression(tokens.assign, src, ctx, dbg .. "\t")
			rnode.ws_before = ws_before_val
			local decl = node:new_decl(type_id, var_name)
			decl.ws_mid = ws_after_type
			decl.ws_after = ws_after_decl
			return node:new_bin_op(tokens.assign, decl, rnode), ctx
		else
			return node:new_decl(type_id, var_name), ctx
		end
	elseif ctx.token == tokens.id and ctx.token_value.name == "if" then
		print(dbg .. "if")
		ctx = next_token(src, ctx)
		if ctx.token ~= '(' then
			print("line " .. line .. ": expected '(' after 'if'")
			os.exit(1)
		end
		ctx = next_token(src, ctx)
		rnode, ctx = expression(tokens.assign, src, ctx, dbg .. "\t")
		local cond = rnode
		if ctx.token ~= ')' then
			print("line " .. line .. ": expected ')' after if condition")
			os.exit(1)
		end
		ctx = next_token(src, ctx) -- пропуск ')'

		rnode, ctx = statement(src, ctx, dbg .. "\t")
		if ctx.token == tokens.id and ctx.token_value.name == "else" then
			ctx = next_token(src, ctx) -- пропуск ')'
			rnode2, ctx = statement(src, ctx, dbg .. "\t")
		end
		return node:new_if(cond, rnode, rnode2), ctx
	elseif ctx.token == tokens.id and ctx.token_value.name == "while" then
		ctx = next_token(src, ctx)
		if ctx.token ~= '(' then
			print("line " .. line .. ": expected '(' after 'while'")
			os.exit(1)
		end
		ctx = next_token(src, ctx)
		rnode, ctx = expression(tokens.assign, src, ctx, dbg .. "\t")
		if ctx.token ~= ')' then
			print("line " .. line .. ": expected ')' after while condition")
			os.exit(1)
		end
		ctx = next_token(src, ctx) -- пропуск ')'
		rnode2, ctx = statement(src, ctx, dbg .. "\t")
		return node:new_while(rnode, rnode2), ctx
	elseif ctx.token == '{' then
		ctx = next_token(src, ctx) -- пропуск '{'
		local ws_after_opening = ctx.ws
		ctx.ws = ""
		local statements = {}
		while ctx.token ~= '}' do
			print(dbg .. "ctx token " .. ctx.token)
			if #statements > 0 then
				statements[#statements].ws_after = statements[#statements].ws_after .. ctx.ws
			end

			rnode, ctx = statement(src, ctx, dbg .. "\t")
			print(dbg .. "token after " .. ctx.token)
			if rnode == nil then -- ';'
				statements[#statements].ws_after = statements[#statements].ws_after .. ';'
			end
			if ctx.token == nil then
				print("line " .. line .. ": curly brace '{' was never closed")
				os.exit(1)
			end
			table.insert(statements, rnode)
		end
		if #statements > 0 then
			statements[#statements].ws_after = statements[#statements].ws_after .. ctx.ws
		end

		ctx = next_token(src, ctx) -- пропуск '}'
		local braces = node:new_braces(statements)
		braces.ws_after_opening = ws_after_opening
		return braces, ctx
	elseif ctx.token == tokens.id and ctx.token_value.name == "return" then
		ctx = next_token(src, ctx)
		if ctx.token ~= ';' then
			rnode, ctx = statement(src, ctx, dbg .. "\t")
		end
		if ctx.token ~= ';' then
			print("line " .. line .. ": expected ';' after 'return'")
			os.exit(1)
		end
		ctx = next_token(src, ctx) -- пропуск ';'
		return node:new_return(rnode), ctx
	elseif ctx.token == ';' then -- пустое утверждение
		ctx = next_token(src, ctx)
		return nil, ctx
	else -- присваивание или вызов функции
		rnode, ctx = expression(tokens.assign, src, ctx, dbg .. "\t")
		if ctx.token ~= ';' then
			print("line " .. line .. ": expected ';' after statement")
			os.exit(1)
		end
		--ctx = next_token(src, ctx)
		return rnode, ctx
	end
end

function enum_decl(src, ctx, dbg)
	dbg = dbg or ""

	if not ctx.token then
		print("line " .. line .. ": unexpected EOF of enum declaration")
		os.exit(1)
	end

	-- временные локальные переменные
	local rnode

	local decls = {}
	print(dbg .. "enum, decl", ctx.token, ctx.token_value)
	while ctx.token ~= '}' do
		if ctx.token ~= tokens.id then
			print("line " .. line .. ": expected an identifier for enum declaration")
			os.exit(1)
		end
		local id = ctx.token_value.name
		if identifiers[id].class then
			print("line " .. line .. ": attempting to re-define enum '" .. id .. "'")
			os.exit(1)
		end
		local ws_before_var = ctx.ws
		ctx = next_token(src, ctx)

		if ctx.token == tokens.assign then
			local var = node:new_var(id)
			var.ws_before = ws_before_var
			var.ws_after = ctx.ws
			ctx = next_token(src, ctx) -- пропуск '='
			if ctx.token ~= tokens.num then
				print("line " .. line .. ": enum should be initialized with a number")
				os.exit(1)
			end
			local val = node:new({_type = nodes.num, value = ctx.token_value})
			val.ws_before = ctx.ws
			table.insert(decls, node:new_bin_op(tokens.assign, var, val))
			ctx = next_token(src, ctx)
			val.ws_after = ctx.ws
		else
			table.insert(decls, node:new_var(id))
		end

		identifiers[id].class = classes.enum_decl
		if ctx.token ~= ',' and ctx.token ~= '}' then
			print("line " .. line .. ": expected ',' or '}' after enum declaration")
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
		print("line " .. line .. ": unexpected EOF of function parameters")
		os.exit(1)
	end

	-- временные локальные переменные
	local rnode

	local params = {}
	print(dbg .. "func params", ctx.token, ctx.token_value)
	while ctx.token ~= ')' do
		if ctx.token ~= tokens.id then
			print("line " .. line .. ": expected a type identifier")
			os.exit(1)
		end
		if identifiers[ctx.token_value.name].class ~= classes._type and identifiers[ctx.token_value.name].class ~= classes.type_mod then
			print("line " .. line .. ": identifier is not a type")
			os.exit(1)
		end
		local type_id = token_value

		ctx = next_token(src, ctx)
		while ctx.token == tokens.mul do -- пропуск указателей
			ctx = next_token(src, ctx)
			type_id.pointers = type_id.pointers + 1
		end

		if ctx.token ~= tokens.id then
			print("line " .. line .. ": expected parameter name")
			os.exit(1)
		end

		table.insert(params, node:new_decl(type_id, token_value.name))

		ctx = next_token(src, ctx)
		if ctx.token == ',' then
			ctx = next_token(src, ctx)
		end
	end

	return node:new_params(params), ctx
end

function func_decl(src, ctx, dbg)
	dbg = dbg or ""

	if not ctx.token then
		print("line " .. line .. ": unexpected EOF of function declaration")
		os.exit(1)
	end

	local params, body

	print(dbg .. "func decl", token, token_value)
	if ctx.token ~= '(' then
		print("line " .. line .. ": expected '(' in function declaration")
		os.exit(1)
	end
	local ws_before_params = ctx.ws
	ctx = next_token(src, ctx)
	params, ctx = func_params(src, ctx, dbg .. "\t")
	params.ws_before = ws_before_params
	if ctx.token ~= ')' then
		print("line " .. line .. ": expected ')' in function declaration")
		os.exit(1)
	end
	ctx = next_token(src, ctx)
	local ws_before_body = ctx.ws

	if ctx.token ~= '{' then
		print("line " .. line .. ": expected '{' in function declaration")
		os.exit(1)
	end
	body, ctx = statement(src, ctx, dbg .. "\t")
	body.ws_before = ws_before_body
	print(dbg .. "tokens after body", token, token_value)

	-- грязный хак (фигурные скобки{} возвращают токен следующий за ними, но нам нужно вернуть })
	if ctx.i then 
		ctx.i = ctx.i - 1
	end
	return node:new_func(params, body), ctx
end

function global_decl(src, ctx, dbg)
	dbg = dbg or ""

	if not ctx.token then
		print("line " .. line .. ": unexpected EOF of global declaration")
		os.exit(1)
	end

	-- временные локальные переменные
	local rnode

	print(dbg .. "global decl", ctx.token, ctx.token_value)
	if ctx.token == tokens.id and ctx.token_value.name == "enum" then
		ctx = next_token(src, ctx)
		local ws_after_enum = ctx.ws
		local ws_after_name = ""
		local enum_id
		if ctx.token ~= '{' then
			if ctx.token ~= tokens.id then
				print("line " .. line .. ": expected enum identifier in global declaration")
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
	
	local type_id = ctx.token_value
	ctx = next_token(src, ctx)

	while ctx.token == tokens.mul do -- пропуск указателей
		ctx = next_token(src, ctx)
		type_id.pointers = type_id.pointers + 1
	end

	local unit_nodes = {}

	if ctx.token ~= tokens.id then
		print("line " .. line .. ": expected variable or function name in global declaration")
		os.exit(1)
	end
	local decl_name = ctx.token_value.name
	local ws_after_type = ctx.ws
	ctx = next_token(src, ctx)

	if ctx.token == tokens.assign then -- объявление переменной с инициализацией
		local ws_after_id = ctx.ws
		ctx = next_token(src, ctx)
		local ws_before_val = ctx.ws
		
		rnode, ctx = expression(tokens.assign, src, ctx, dbg .. "\t")
		rnode.ws_before = ws_before_val
		local decl = node:new_decl(type_id, decl_name)
		decl.ws_after = ws_after_id
		decl.ws_mid = ws_after_type
		unit_nodes[1] = node:new_bin_op(tokens.assign, decl, rnode)
	elseif ctx.token == '(' then -- объявление функции
		rnode, ctx = func_decl(src, ctx, dbg .. "\t")
		rnode.ws_after_return_type = ws_after_type
		print(dbg .. "tokens after func decl", token, token_value)
		rnode.name = decl_name
		rnode.return_type = type_id
		return rnode, ctx
	else -- объявление переменной
		unit_nodes[1] = node:new_decl(type_id, decl_name)
		unit_nodes[1].ws_mid = ws_mid_decl
	end

	while ctx.token ~= ';' and ctx.token ~= '}' do
		if ctx.token ~= tokens.id then
			print("line " .. line .. ": expected variable or function name in global declaration")
			os.exit(1)
		end
		local decl_name = ctx.token_value.name
		ctx = next_token(src, ctx)

		if ctx.token == tokens.assign then -- объявление переменной с инициализацией
			ctx = next_token(src, ctx)
			rnode, ctx = expression(tokens.assign, src, ctx, dbg .. "\t")
			table.insert(unit_nodes, node:new_bin_op(tokens.assign, node:new_var(decl_name), rnode))
		elseif ctx.token == '(' then -- объявление функции
			print("line " .. line .. ": unexpected function in global declaration")
			os.exit(1)
		else -- объявление переменной
			table.insert(unit_nodes, node:new_var(decl_name))
		end
	end

	local comma = node:new_comma(unit_nodes)
	if ctx.token == ';' then
		comma.ws_after = comma.ws_after .. ';'
	end
	return comma, ctx
end
