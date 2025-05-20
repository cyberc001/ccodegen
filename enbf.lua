require "token"
require "os"

nodes = {
	num = 1, str = 2, char = 3,
	call = 10, enum = 11, var = 12, params = 13, decl = 14, func = 15, enum_decl = 16,
	cast = 20,
	un_op = 30, bin_op = 31, cond = 32, index = 33,
	parantheses = 40, braces = 41, comma = 42,

	_if = 50, _while = 51
}

node = {}

function node:new(o)
	o = o or {}
	setmetatable(o, self)
	self.__index = self
	return o
end
function node:__tostring()
	self.dbg = self.dbg or ""
	return self.dbg .. (self.print and self:print() or "(generic node: type " .. self._type .. ", value [" .. tostring(self.value) .. "])")
end

---------------------[[ Выражения ]]---------------------
function node:new_call(name, args)
	return node:new({_type = nodes.call, value = args, name = name,
	print = function(self)
		local s = "(fun call '" .. name .. "'\n" .. self.dbg .. "\targs [\n"
		for _, v in ipairs(args) do
			v.dbg = self.dbg .. "\t"
			s = s .. tostring(v) .. ",\n"
		end
		return s .. self.dbg .. "])"
	end})
end
function node:new_enum(name, value)
	return node:new({_type = nodes.enum, value = value, name = name,
	print = function(self)
		return "(enum '" .. name .. "', value " .. value .. ")"
	end})
end
function node:new_var(name)
	return node:new({_type = nodes.var, value = name,
	print = function(self)
		return "(var '" .. name .. "')"
	end})
end
function node:new_cast(cast_type, value)
	return node:new({_type = nodes.cast, cast_type = cast_type, value = value,
	print = function(self)
		self.value.dbg = self.dbg .. "\t"
		return "(cast\n" .. self.dbg .. "\ttype " .. tostring(self.cast_type) .. "\n" .. self.dbg .. "\tvalue\n" .. tostring(value) .. "\n" .. self.dbg .. ")"
	end})
end

function node:new_un_op(op, x, postfix)
	return node:new({_type = nodes.un_op, op = op, value = x, postfix = postfix,
	print = function(self)
		self.value.dbg = self.dbg .. "\t"
		return "(unary op " .. token_to_str(self.op) .. "\n" .. self.dbg .. "\tvalue\n" .. tostring(self.value) .. "\n" .. self.dbg .. (postfix and "postfix)" or ")")
	end})
end
function node:new_bin_op(op, x, y)
	return node:new({_type = nodes.bin_op, op = op, value = {x, y},
	print = function(self)
		self.value[1].dbg = self.dbg .. "\t"
		self.value[2].dbg = self.dbg .. "\t"
		return "(bin op " .. token_to_str(self.op) .. "\n\t" .. self.dbg .. "values\n" .. tostring(self.value[1]) .. ",\n" .. tostring(self.value[2]) .. "\n" .. self.dbg .. ")"
	end})
end
function node:new_cond(cond, a, b)
	return node:new({_type = nodes.cond, value = {cond, a, b},
	print = function(self)
		self.value[1].dbg = self.dbg .. "\t"
		self.value[2].dbg = self.dbg .. "\t"
		self.value[3].dbg = self.dbg .. "\t"
		return "(cond op\n" .. self.dbg .. "\tvalues\n" .. tostring(self.value[1]) .. ",\n" .. tostring(self.value[2]) .. ",\n" .. tostring(self.value[3]) .. "\n" .. self.dbg .. ")"
	end})
end
function node:new_index(x, i)
	return node:new({_type = nodes.index, value = {x, i},
	print = function(self)
		self.value[1].dbg = self.dbg .. "\t"
		self.value[2].dbg = self.dbg .. "\t"
		return "(index, values\n" .. tostring(self.value[1]) .. ",\n" .. tostring(self.value[2]) .. "\n" .. self.dbg .. ")"
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
	end})
end
function node:new_while(cond, body)
	return node:new({_type = nodes._while, value = body, cond = cond,
	print = function(self)
		self.cond.dbg = self.dbg .. "\t"
		self.value.dbg = self.dbg .. "\t"
		return "(while\n" .. self.dbg .. "\tcond\n" .. tostring(self.cond) .. ",\n" .. self.dbg .. "\tbody\n" .. tostring(self.value) .. "\n" .. self.dbg .. "\t)"
	end})
end
function node:new_return(value)
	return node:new({_type = nodes._return, value = value,
	print = function(self)
		if self.value then
			self.value.dbg = self.dbg .. "\t"
		end
		return "(return" .. (self.value and "\n" .. self.dbg .. "\tvalue\n" .. tostring(self.value) .. "\n" .. self.dbg .. "\t)" or ")")
	end})
end
function node:new_braces(statements)
	return node:new({_type = nodes.braces, value = statements,
	print = function(self)
		local s = "(braces\n" .. self.dbg .. "\tstatements [\n"
		for _,v in ipairs(self.value) do
			v.dbg = self.dbg .. "\t"
			s = s ..  tostring(v) .. ",\n"
		end
		return s .. self.dbg .. "\t])"
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
	end})
end
function node:new_decl(_type, id)
	return node:new({_type = nodes.decl, _type = _type, value = id,
	print = function(self)
		return "(decl [" .. tostring(self._type) .. "] '" .. tostring(self.value) .. "')"
	end})
end


---------------------[[ Другое ]]---------------------
function node:new_enum_decl(decls, name)
	return node:new({_type = nodes.enum_decl, value = decls, name = name,
	print = function(self)
		local s = "(enum\n" .. self.dbg .. "\tdeclarations [\n"
		for _,v in ipairs(self.value) do
			v.dbg = self.dbg .. "\t"
			s = s ..  tostring(v) .. ",\n"
		end
		return s .. self.dbg .. "\t])"
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
	end})
end

function node:new_func(params, body, name)
	return node:new({_type = nodes.func, value = body, params = params, name = name,
	print = function(self)
		self.value.dbg = self.dbg .. "\t"
		self.params.dbg = self.dbg .. "\t"
		return "(func '" .. tostring(self.name) .. "'\n" .. self.dbg .. "\tparams\n" .. tostring(self.params) .. "\n" .. self.dbg .. "\tbody\n" .. tostring(self.value) .. "\n" .. self.dbg .. "\t)"
	end})
end



function expression(level, src, i, token, token_value, dbg)
	dbg = dbg or ""

	if not token then
		print("line " .. line .. ": unexpected EOF of expression")
		os.exit(1)
	end

	-- временные локальные переменные
	local rnode, rnode2

	-- для использования в бинарных операторах
	local unit_node

	print(dbg .. "expression", token, token_value)
	if token == tokens.num then
		local val = token_value
		i, token, token_value = next_token(src, i)
		unit_node = node:new({_type = nodes.num, value = val})
	elseif token == tokens.char then
		local val = token_value
		i, token, token_value = next_token(src, i)
		unit_node = node:new({_type = nodes.char, value = val})
		print("TOKEN", unit_node)
	elseif token == tokens.str then
		local val = token_value
		i, token, token_value = next_token(src, i)
		unit_node = node:new({_type = nodes.str, value = val})
	elseif token == tokens.id then
		local id = token_value.name
		i, token, token_value = next_token(src, i)

		if token == '(' then -- вызов функции
			local args = {}
			i, token, token_value = next_token(src, i)
			while token ~= ')' do
				rnode, i, token, token_value = expression(tokens.assign, src, i, token, token_value, dbg .. "\t")
				table.insert(args, rnode)
				print(dbg .. "call token", token, i)
				if token == ',' then -- пропуск запятой
					i, token, token_value = next_token(src, i)
				end
			end
			i, token, token_value = next_token(src, i) -- пропуск закрывающей скобки ')'

			unit_node = node:new_call(id, args)
		elseif identifiers[id].class == classes.enum_decl then
			unit_node = node:new_enum(id, identifiers[token_value].value)
		else
			unit_node = node:new_var(id)
		end
	elseif token == '(' then
		i, token, token_value = next_token(src, i)
		if token == tokens.id and (identifiers[token_value.name].class == classes._type or identifiers[token_value.name].class == classes.type_mod) then -- приведение типов
			local cast_type = token_value
			i, token, token_value = next_token(src, i)
			while token == tokens.mul do -- пропуск указателей
				i, token, token_value = next_token(src, i)
				cast_type.pointers = cast_type.pointers + 1
			end

			if token ~= ')' then
				print("line " .. line .. ": expected ')' in type cast")
				os.exit(1)
			end

			i, token, token_value = next_token(src, i) -- пропуск ')'
			rnode, i, token, token_value = expression(tokens.inc, src, i, token, token_value, dbg .. "\t")
			unit_node = node:new_cast(cast_type, rnode)
		else -- скобки ()
			rnode, i, token, token_value = expression(tokens.assign, src, i, token, token_value, dbg .. "\t")
			if token ~= ')' then
				print("line " .. line .. ": expected ')' in parantheses")
				os.exit(1)
			end
			i, token, token_value = next_token(src, i) -- пропуск ')'
			unit_node = node:new({_type = nodes.parantheses, value = rnode})
		end
	elseif token == tokens.mul or token == tokens._and or token == tokens.lnot or token == tokens._lnot or token == tokens.add or token == tokens.inc or token == tokens.dec then -- унарные операторы
		local op = token
		i, token, token_value = next_token(src, i)
		rnode, i, token, token_value = expression(tokens.inc, src, i, token, token_value, dbg .. "\t")
		unit_node = node:new_un_op(op, rnode)
	elseif token == tokens.sub then -- отдельный случай с отрицательными числами
		i, token, token_value = next_token(src, i)
		if token == tokens.num then
			local val = token_value
			i, token, token_value = next_token(src, i)
			unit_node = node:new({_type = nodes.num, value = -val})
		else
			rnode, i, token, token_value = expression(tokens.inc, src, i, token, token_value, dbg .. "\t")
			unit_node = node:new_un_op(tokens.sub, rnode)
		end
	end

	-- бинарные и постфиксные операторы
	while type(token) == "number" and token >= level do
		print(dbg .. "token", token)
		if token == tokens.assign then
			i, token, token_value = next_token(src, i) -- пропуск '='
			rnode, i, token, token_value = expression(tokens.assign, src, i, token, token_value, dbg .. "\t")
			unit_node = node:new_bin_op(tokens.assign, unit_node, rnode)
		elseif token == tokens.cond then
			i, token, token_value = next_token(src, i) -- пропуск '?'
			rnode, i, token, token_value = expression(tokens.assign, src, i, token, token_value, dbg .. "\t")
			if token ~= ':' then
				print("line " .. line .. ": expected ':' in conditional operator")
				os.exit(1)
			end
			i, token, token_value = next_token(src, i) -- пропуск ':'
			rnode2, i, token, token_value = expression(tokens.cond, src, i, token, token_value, dbg .. "\t")
			unit_node = node:new_cond(unit_node, rnode, rnode2)
		elseif token >= tokens.lor and token <= tokens.mod then
			local op = token
			i, token, token_value = next_token(src, i)
			rnode, i, token, token_value = expression(op + 1, src, i, token, token_value, dbg .. "\t")
			unit_node = node:new_bin_op(op, unit_node, rnode)
		elseif token == tokens.inc or token == tokens.dec then
			unit_node = node:new_un_op(token, unit_node, true)
			i, token, token_value = next_token(src, i)
		elseif token == tokens.brack then
			i, token, token_value = next_token(src, i)
			rnode, i, token, token_value = expression(tokens.assign, src, i, token, token_value, dbg .. "\t")
			if token ~= ']' then
				print("line " .. line .. ": expected ']' to close indexing operator")
				os.exit(1)
			end
			i, token, token_value = next_token(src, i)
			unit_node = node:new_index(unit_node, rnode)
		else
			print("line " .. line .. ": unexpected end of expression")
			os.exit(1)
		end
		print(dbg .. "going next")
	end

	print(dbg .. "expression returning", token, token_value)
	return unit_node, i, token, token_value
end

function statement(src, i, token, token_value, dbg)
	dbg = dbg or ""

	if not token then
		print("line " .. line .. ": unexpected EOF of statement")
		os.exit(1)
	end

	-- временные локальные переменные
	local rnode, rnode2

	print(dbg .. "statement", token, token_value)
	if token == tokens.id and (identifiers[token_value.name].class == classes._type or identifiers[token_value.name].class == classes.type_mod) then -- объявление переменной
		local type_id = token_value
		i, token, token_value = next_token(src, i)

		while token == tokens.mul do -- пропуск указателей
			i, token, token_value = next_token(src, i)
			type_id.pointers = type_id.pointers + 1
		end

		if token ~= tokens.id then
			print("line " .. line .. ": expected variable name in declaration")
			os.exit(1)
		end
		local var_name = token_value.name
		i, token, token_value = next_token(src, i)

		if token == tokens.assign then
			i, token, token_value = next_token(src, i)
			rnode, i, token, token_value = expression(tokens.assign, src, i, token, token_value, dbg .. "\t")
			return node:new_bin_op(tokens.assign, node:new_decl(type_id, var_name), rnode), i, token, token_value
		else
			return node:new_decl(type_id, var_name), i, token, token_value
		end
	elseif token == tokens.id and token_value.name == "if" then
		i, token, token_value = next_token(src, i)
		if token ~= '(' then
			print("line " .. line .. ": expected '(' after 'if'")
			os.exit(1)
		end
		i, token, token_value = next_token(src, i)
		rnode, i, token, token_value = expression(tokens.assign, src, i, token, token_value, dbg .. "\t")
		local cond = rnode
		if token ~= ')' then
			print("line " .. line .. ": expected ')' after if condition")
			os.exit(1)
		end
		i, token, token_value = next_token(src, i) -- пропуск ')'

		rnode, i, token, token_value = statement(src, i, token, token_value, dbg .. "\t")
		if token == tokens.id and token_value.name == "else" then
			i, token, token_value = next_token(src, i) -- пропуск ')'
			rnode2, i, token, token_value = statement(src, i, token, token_value, dbg .. "\t")
		end
		return node:new_if(cond, rnode, rnode2), i, token, token_value
	elseif token == tokens.id and token_value.name == "while" then
		i, token, token_value = next_token(src, i)
		if token ~= '(' then
			print("line " .. line .. ": expected '(' after 'while'")
			os.exit(1)
		end
		i, token, token_value = next_token(src, i)
		rnode, i, token, token_value = expression(tokens.assign, src, i, token, token_value, dbg .. "\t")
		if token ~= ')' then
			print("line " .. line .. ": expected ')' after while condition")
			os.exit(1)
		end
		i, token, token_value = next_token(src, i) -- пропуск ')'
		rnode2, i, token, token_value = statement(src, i, token, token_value, dbg .. "\t")
		return node:new_while(rnode, rnode2), i, token, token_value
	elseif token == '{' then
		i, token, token_value = next_token(src, i) -- пропуск '{'
		local statements = {}
		while token ~= '}' do
			rnode, i, token, token_value = statement(src, i, token, token_value, dbg .. "\t")
			if token == nil then
				print("line " .. line .. ": curly brace '{' was never closed")
				os.exit(1)
			end
			table.insert(statements, rnode)
		end
		i, token, token_value = next_token(src, i) -- пропуск '}'
		print(dbg .. "braces returning", token, token_value)
		return node:new_braces(statements), i, token, token_value
	elseif token == tokens.id and token_value.name == "return" then
		i, token, token_value = next_token(src, i)
		if token ~= ';' then
			rnode, i, token, token_value = statement(src, i, token, token_value, dbg .. "\t")
		end
		if token ~= ';' then
			print("line " .. line .. ": expected ';' after 'return'")
			os.exit(1)
		end
		i, token, token_value = next_token(src, i) -- пропуск ';'
		return node:new_return(rnode), i, token, token_value
	elseif token == ';' then -- пустое утверждение
		i, token, token_value = next_token(src, i)
		return nil, i, token, token_value
	else -- присваивание или вызов функции
		rnode, i, token, token_value = expression(tokens.assign, src, i, token, token_value, dbg .. "\t")
		if token ~= ';' then
			print("line " .. line .. ": expected ';' after statement")
			os.exit(1)
		end
		i, token, token_value = next_token(src, i)
		return rnode, i, token, token_value
	end
end

function enum_decl(src, i, token, token_value, dbg)
	dbg = dbg or ""

	if not token then
		print("line " .. line .. ": unexpected EOF of enum declaration")
		os.exit(1)
	end

	-- временные локальные переменные
	local rnode

	local decls = {}
	print(dbg .. "enum, decl", token, token_value)
	while token ~= '}' do
		if token ~= tokens.id then
			print("line " .. line .. ": expected an identifier for enum declaration")
			os.exit(1)
		end
		local id = token_value.name
		if identifiers[id].class then
			print("line " .. line .. ": attempting to re-define enum '" .. id .. "'")
			os.exit(1)
		end
		i, token, token_value = next_token(src, i)

		if token == tokens.assign then
			i, token, token_value = next_token(src, i) -- пропуск '='
			if token ~= tokens.num then
				print("line " .. line .. ": enum should be initialized with a number")
				os.exit(1)
			end
			table.insert(decls, node:new_bin_op(tokens.assign, node:new_var(id), node:new({_type = nodes.num, value = token_value})))
			i, token, token_value = next_token(src, i)
		else
			table.insert(decls, node:new_var(id))
		end

		identifiers[id].class = classes.enum_decl
		if token ~= ',' then
			print("line " .. line .. ": expected ',' after enum declaration")
			os.exit(1)
		end
		i, token, token_value = next_token(src, i)
	end

	return node:new_enum_decl(decls), i, token, token_value
end

function func_params(src, i, token, token_value, dbg)
	dbg = dbg or ""

	if not token then
		print("line " .. line .. ": unexpected EOF of function parameters")
		os.exit(1)
	end

	-- временные локальные переменные
	local rnode

	local params = {}
	print(dbg .. "func params", token, token_value)
	while token ~= ')' do
		if token ~= tokens.id then
			print("line " .. line .. ": expected a type identifier")
			os.exit(1)
		end
		if identifiers[token_value.name].class ~= classes._type and identifiers[token_value.name].class ~= classes.type_mod then
			print("line " .. line .. ": identifier is not a type")
			os.exit(1)
		end
		local type_id = token_value

		i, token, token_value = next_token(src, i)
		while token == tokens.mul do -- пропуск указателей
			i, token, token_value = next_token(src, i)
			type_id.pointers = type_id.pointers + 1
		end

		if token ~= tokens.id then
			print("line " .. line .. ": expected parameter name")
			os.exit(1)
		end

		table.insert(params, node:new_decl(type_id, token_value.name))

		i, token, token_value = next_token(src, i)
		if token == ',' then
			i, token, token_value = next_token(src, i)
		end
	end

	return node:new_params(params), i, token, token_value
end

function func_decl(src, i, token, token_value, dbg)
	dbg = dbg or ""

	if not token then
		print("line " .. line .. ": unexpected EOF of function declaration")
		os.exit(1)
	end

	local params, body

	print(dbg .. "func decl", token, token_value)
	if token ~= '(' then
		print("line " .. line .. ": expected '(' in function declaration")
		os.exit(1)
	end
	i, token, token_value = next_token(src, i)
	params, i, token, token_value = func_params(src, i, token, token_value, dbg .. "\t")
	if token ~= ')' then
		print("line " .. line .. ": expected ')' in function declaration")
		os.exit(1)
	end
	i, token, token_value = next_token(src, i)

	if token ~= '{' then
		print("line " .. line .. ": expected '{' in function declaration")
		os.exit(1)
	end
	body, i, token, token_value = statement(src, i, token, token_value, dbg .. "\t")
	print(dbg .. "tokens after body", token, token_value)

	return node:new_func(params, body), i and i - 1 or nil, token, token_value -- грязный хак (фигурные скобки{} возвращают токен следующий за ними, но нам нужно вернуть })
end

function global_decl(src, i, token, token_value, dbg)
	dbg = dbg or ""

	if not token then
		print("line " .. line .. ": unexpected EOF of global declaration")
		os.exit(1)
	end

	-- временные локальные переменные
	local rnode

	print(dbg .. "global decl", token, token_value)
	if token == tokens.id and token_value.name == "enum" then
		i, token, token_value = next_token(src, i)
		local enum_id
		if token ~= '{' then
			i, token, token_value = next_token(src, i)
			if token ~= tokens.id then
				print("line " .. line .. ": expected enum identifier in global declaration")
				os.exit(1)
			end
			enum_id = token_value.name
		end

		if token == '{' then -- тела может и не быть
			i, token, token_value = next_token(src, i)
			rnode, i, token, token_value = enum_decl(src, i, token, token_value, dbg .. "\t")
			i, token, token_value = next_token(src, i) -- пропуск '}'
			rnode.name = enum_id
			return rnode, i, token, token_value
		end
		return node:new_enum_decl({}, enum_id), i, token, token_value
	end
	
	local type_id = token_value
	i, token, token_value = next_token(src, i)

	while token == tokens.mul do -- пропуск указателей
		i, token, token_value = next_token(src, i)
		type_id.pointers = type_id.pointers + 1
	end

	local unit_nodes = {}

	if token ~= tokens.id then
		print("line " .. line .. ": expected variable or function name in global declaration")
		os.exit(1)
	end
	local decl_name = token_value.name
	i, token, token_value = next_token(src, i)

	if token == tokens.assign then -- объявление переменной с инициализацией
		i, token, token_value = next_token(src, i)
		rnode, i, token, token_value = expression(tokens.assign, src, i, token, token_value, dbg .. "\t")
		unit_nodes[1] = node:new_bin_op(tokens.assign, node:new_decl(type_id, decl_name), rnode)
	elseif token == '(' then -- объявление функции
		rnode, i, token, token_value = func_decl(src, i, token, token_value, dbg .. "\t")
		print(dbg .. "tokens after func decl", token, token_value)
		rnode.name = decl_name
		return rnode, i, token, token_value
	else -- объявление переменной
		unit_nodes[1] = node:new_decl(type_id, decl_name)
	end

	while token ~= ';' and token ~= '}' do
		if token ~= tokens.id then
			print("line " .. line .. ": expected variable or function name in global declaration")
			os.exit(1)
		end
		local decl_name = token_value.name
		i, token, token_value = next_token(src, i)

		if token == tokens.assign then -- объявление переменной с инициализацией
			i, token, token_value = next_token(src, i)
			rnode, i, token, token_value = expression(tokens.assign, src, i, token, token_value, dbg .. "\t")
			table.insert(unit_nodes, node:new_bin_op(tokens.assign, node:new_var(decl_name), rnode))
		elseif token == '(' then -- объявление функции
			print("line " .. line .. ": unexpected function in global declaration")
			os.exit(1)
		else -- объявление переменной
			table.insert(unit_nodes, node:new_var(decl_name))
		end
	end

	return node:new_comma(unit_nodes), i, token, token_value
end
