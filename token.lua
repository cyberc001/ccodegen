tokens = {
	num = 1, num_hex = 2, num_oct = 3,
	fun = 10, sys = 11, glo = 12, loc = 13, id = 14,
	char = 15, str = 16,

	assign = 20, bor_assign = 21, xor_assign = 22, band_assign = 23, shr_assign = 24, shl_assign = 25, mod_assign = 26, div_assign = 27, mul_assign = 28, sub_assign = 29, add_assign = 30,
	cond = 40,
	lor = 50,
	land = 51,
	_or = 52,
	xor = 53,
	_and = 54,
	eq = 55, ne = 56,
	lt = 57, gt = 58, le = 59, ge = 60,
	shl = 70, shr = 71,
	add = 80, sub = 81,
	mul = 82, div = 83, mod = 84,
	lnot = 90, _not = 91,
	member = 100, member_ptr = 101, inc = 102, dec = 103, brack = 104
}

function is_token_num(token)
	return type(token) == "number" and token >= tokens.num and token <= tokens.num_oct
end
function is_token_binary_op(token)
	return token ~= tokens.lnot and token ~= tokens._not
	and token >= tokens.lor and token <= tokens.member_ptr
	or token >= tokens.assign and token <= tokens.add_assign
end

function token_to_str(token)
	if token == tokens.bor_assign then return '|=' end
	if token == tokens.xor_assign then return '^=' end
	if token == tokens.band_assign then return '&=' end
	if token == tokens.shr_assign then return '>>=' end
	if token == tokens.shl_assign then return '<<=' end
	if token == tokens.mod_assign then return '%=' end
	if token == tokens.div_assign then return '/=' end
	if token == tokens.mul_assign then return '*=' end
	if token == tokens.sub_assign then return '-=' end
	if token == tokens.add_assign then return '+=' end
	if token == tokens.assign then return '=' end

	if token == tokens.cond then return '?' end

	if token == tokens.lor then return '||' end

	if token == tokens.land then return '&&' end

	if token == tokens._or then return '|' end

	if token == tokens.xor then return '^' end

	if token == tokens._and then return '&' end

	if token == tokens.eq then return '==' end
	if token == tokens.ne then return '!=' end

	if token == tokens.lt then return '<' end
	if token == tokens.gt then return '>' end
	if token == tokens.le then return '<=' end
	if token == tokens.ge then return '>=' end

	if token == tokens.shl then return '<<' end
	if token == tokens.shr then return '>>' end

	if token == tokens.add then return '+' end
	if token == tokens.sub then return '-' end

	if token == tokens.mul then return '*' end
	if token == tokens.div then return '/' end
	if token == tokens.mod then return '%' end

	if token == tokens.lnot then return '!' end
	if token == tokens._not then return '~' end

	if token == tokens.member then return '.' end
	if token == tokens.member_ptr then return '->' end
	if token == tokens.inc then return '++' end
	if token == tokens.dec then return '--' end
	if token == tokens.brack then return '[' end

	return tostring(token)
end

classes = {
	enum = 1, enum_decl = 2, _type = 3, type_mod = 4,
	keyword = 5
}

function is_valid_id_char(c_code, begin)
	begin = begin or false
	return (c_code >= string.byte('a') and c_code <= string.byte('z')) or (c_code >= string.byte('A') and c_code <= string.byte('Z')) or c_code == string.byte('_') or (not begin and (c_code >= string.byte('0') and c_code <= string.byte('9')))
end
function is_number(c_code)
	return c_code and c_code >= string.byte('0') and c_code <= string.byte('9')
end
function is_ws(c)
	return c == ' ' or c == '\t' or c == '\n' or c == '\r'
end

id = {}
function id:new(o)
	if type(o) == "string" then
		local src = o .. ";"
		local ctx = next_token(src, new_token_ctx(1, ""))
		if ctx.token ~= tokens.id then
			print("Cannot initialize id from string '" .. o .. "'")
			os.exit(1)
		end
		o = ctx.token_value
		ctx = next_token(src, ctx)
		while ctx.token == tokens.mul do -- пропуск указателей
			table.insert(o.pointers_ws, ctx.ws)
			ctx = next_token(src, ctx)
		end
	else
		o = o or {}
		o.mods = o.mods or {}
		o.mods_ws = o.mods_ws or {}
		o.pointers_ws = o.pointers_ws or {}
	end
	setmetatable(o, self)
	self.__index = self
	return o
end
function id:has_mod(mod)
	for _, v in ipairs(self.mods) do
		if v == mod then
			return true
		end
	end
	return false
end
function id:__tostring()
	s = ""
	for i, v in ipairs(self.mods) do
		s = s .. self.mods_ws[i] .. v
	end
	s = s .. self.mods_ws[#self.mods_ws]
	if self.name then s = s .. self.name end
	for _, v in ipairs(self.pointers_ws) do
		s = s .. v .. "*"
	end
	return s
end

identifiers = {
	struct = { class = classes.type_mod },
	union = { class = classes.type_mod },

	const = { class = classes.type_mod },
	static = { class = classes.type_mod },
	inline = { class = classes.type_mod },

	short = { class = classes.type_mod },
	long = { class = classes.type_mod },
	unsigned = { class = classes.type_mod },
	signed = { class = classes.type_mod },

	void = { class = classes._type },
	int = { class = classes._type },
	char = { class = classes._type },
	float = { class = classes._type },
	double = { class = classes._type },
	size_t = { class = classes._type }
}
identifiers["if"] = { class = classes.keyword }
identifiers["else"] = { class = classes.keyword }
identifiers["while"] = { class = classes.keyword }
identifiers["return"] = { class = classes.keyword }

function is_id_token_compound(token_value)
	return token_value:has_mod("struct") or token_value:has_mod("union")
end
function is_id_token_a_type(token_value)
	if token_value:has_mod("unsigned") or token_value:has_mod("signed") or token_value:has_mod("short") or token_value:has_mod("long") then
		return true -- невяно указан тип int
	end
	if not token_value.name then
		return false
	end
	return is_id_token_compound(token_value) or (identifiers[token_value.name] and identifiers[token_value.name].class == classes._type)
end



function next_char(src, i)
	return i + 1, src:sub(i, i), string.byte(src:sub(i, i))
end

function new_token_ctx(i, ws, token, token_value, line)
	line = line or 1
	return {i = i, token = token, token_value = token_value, ws = ws or "", line = line}
end
function copy_token_ctx(ctx)
	return new_token_ctx(ctx.i, ctx.ws, ctx.token, ctx.token_value, ctx.line)
end

function next_token(src, ctx)
	local i = ctx.i
	local c, c_code
	local ws = ""
	while i and i <= src:len() do
		i, c, c_code = next_char(src, i)

		if c == '\n' then
			ctx.line = ctx.line + 1
			ws = ws .. c
		elseif is_ws(c) then -- пропуск пробелов
			ws = ws .. c
		elseif c == '#' then -- пропустить макросы
			while i <= src:len() and c ~= '\n' do
				i, c, c_code = next_char(src, i)
			end
			ws = "\n"
			ctx.line = ctx.line + 1
		elseif is_valid_id_char(c_code, true) then -- идентификатор
			local prev_i
			local mods = {}
			local mods_ws = {}
			local compound = false
			local cur_ws
			repeat
				prev_i = i
				cur_ws = ""
				while is_ws(c) do -- пропуск пробелов до следующего идентификатора
					cur_ws = cur_ws .. c
					i, c, c_code = next_char(src, i)
				end

				if not is_valid_id_char(c_code, true) then
					table.insert(mods, "") -- заглушка, чтобы строка mods[#mods] не стирала модификаторы, когда name отсутствует
					table.insert(mods_ws, "")
					break
				end

				local beg_i = i - 1
				while is_valid_id_char(c_code) do
					i, c, c_code = next_char(src, i)
				end
				id_name = src:sub(beg_i, i - 2)
				table.insert(mods, id_name)
				table.insert(mods_ws, cur_ws)

				if id_name == "struct" or id_name == "union" then
					compound = true
				end
			until not identifiers[id_name] or identifiers[id_name].class ~= classes.type_mod

			mods[#mods] = nil

			-- два условия для неявно указанного тип переменных
			if #mods > 0 and not identifiers[id_name] and not compound then -- тип, за которым следует имя (объявление переменной)
				mods_ws[#mods_ws] = ""
				i = prev_i
				id_name = nil
			end
			if identifiers[id_name] and identifiers[id_name].class == classes.type_mod then -- тип без следующего за ним имени (возвращаемый тип функции, каст)
				i = prev_i
				id_name = nil
			end

			return new_token_ctx(i - 1, ws, tokens.id, id:new({name = id_name, mods = mods, mods_ws = mods_ws}), ctx.line)
		elseif is_number(c_code) then -- распарсить число
			local token_type
			local ch0 = string.byte('0')
			local val = c_code - ch0
			_, next_c, next_c_code = next_char(src, i)

			if c_code == ch0 and next_c == 'x' or next_c == 'X' then -- шестнадцатиричное число
				i, _, _ = next_char(src, i)
				i, c, c_code = next_char(src, i)
				while is_number(c_code) or (c_code >= string.byte('a') and c_code <= string.byte('f')) or (c_code >= string.byte('A') and c_code <= string.byte('F')) do
					val = val * 16 + (c_code & 15) + (c_code >= string.byte('A') and 9 or 0)
					i, c, c_code = next_char(src, i)
				end
				token_type = tokens.num_hex
			elseif c_code == ch0 and is_number(next_c_code) then -- восьмеричное число
				while c_code >= ch0 and c_code <= string.byte('7') do
					val = val * 8 + (c_code - ch0)
					i, c, c_code = next_char(src, i)
				end
				token_type = tokens.num_oct
			elseif val >= 0 then -- десятичное число
				i, c, c_code = next_char(src, i)
				while is_number(c_code) do
					val = val * 10 + (c_code - ch0)
					i, c, c_code = next_char(src, i)
				end
				token_type = tokens.num
			end
			return new_token_ctx(i - 1, ws, token_type, val, ctx.line)
		elseif c == '/' then
			_, next_c, next_c_code = next_char(src, i)
			if next_c == '/' then -- пропуск комментариев
				while i <= src:len() and c ~= '\n' do
					i, c, c_code = next_char(src, i)
				end
				ctx.line = ctx.line + 1
				ws = "\n"
			elseif next_c == '*' then -- пропуск многострочных комментариев
				i, _, _ = next_char(src, i) -- пропуск *
				while i <= src:len() do
					i, c, c_code = next_char(src, i)
					if c == '*' and i < src:len() then
						_, next_c, next_c_code = next_char(src, i)
						if next_c == '/' then
							i, _, _ = next_char(src, i) -- пропуск *
							break
						end
					end
				end
			elseif next_c == '=' then
				i, _, _ = next_char(src, i)
				return new_token_ctx(i, ws, tokens.div_assign, nil, ctx.line)
			else -- оператор деления
				return new_token_ctx(i, ws, tokens.div, nil, ctx.line)
			end
		elseif c == '"' or c == "'" then
			beg_i = i - 1
			beg_c = c
			while i <= src:len() do
				if c == '\\' then -- escape-последовательность
					i, c, c_code = next_char(src, i)
				end
				i, c, c_code = next_char(src, i)
				if c == beg_c then
					break
				end
			end

			return new_token_ctx(i, ws, beg_c == '"' and tokens.str or tokens.char, src:sub(beg_i, i - 1), ctx.line)
		elseif c == '=' then
			_, next_c, _ = next_char(src, i)
			if next_c == '=' then
				i, _, _ = next_char(src, i)
				return new_token_ctx(i, ws, tokens.eq, nil, ctx.line)
			else
				return new_token_ctx(i, ws, tokens.assign, nil, ctx.line)
			end
		elseif c == '+' then
			_, next_c, _ = next_char(src, i)
			if next_c == '+' then
				i, _, _ = next_char(src, i)
				return new_token_ctx(i, ws, tokens.inc, nil, ctx.line)
			elseif next_c == '=' then
				i, _, _ = next_char(src, i)
				return new_token_ctx(i, ws, tokens.add_assign, nil, ctx.line)
			else
				return new_token_ctx(i, ws, tokens.add, nil, ctx.line)
			end
		elseif c == '-' then
			_, next_c, _ = next_char(src, i)
			if next_c == '-' then
				i, _, _ = next_char(src, i)
				return new_token_ctx(i, ws, tokens.dec, nil, ctx.line)
			elseif next_c == '=' then
				i, _, _ = next_char(src, i)
				return new_token_ctx(i, ws, tokens.sub_assign, nil, ctx.line)
			elseif next_c == '>' then
				i, _, _ = next_char(src, i)
				return new_token_ctx(i, ws, tokens.member_ptr, nil, ctx.line)
			else
				return new_token_ctx(i, ws, tokens.sub, nil, ctx.line)
			end
		elseif c == '!' then
			_, next_c, _ = next_char(src, i)
			if next_c == '=' then
				i, _, _ = next_char(src, i)
				return new_token_ctx(i, ws, tokens.ne, nil, ctx.line)
			else
				return new_token_ctx(i, ws, tokens.lnot, nil, ctx.line)
			end
		elseif c == '<' then
			_, next_c, _ = next_char(src, i)
			if next_c == '=' then
				i, _, _ = next_char(src, i)
				return new_token_ctx(i, ws, tokens.le, nil, ctx.line)
			elseif next_c == '<' then
				i, _, _ = next_char(src, i)
				_, next_c, _ = next_char(src, i)
				if next_c == '=' then
					i, _, _ = next_char(src, i)
					return new_token_ctx(i, ws, tokens.shl_assign, nil, ctx.line)
				else
					return new_token_ctx(i, ws, tokens.shl, nil, ctx.line)
				end
			else
				return new_token_ctx(i, ws, tokens.lt, nil, ctx.line)
			end
		elseif c == '>' then
			_, next_c, _ = next_char(src, i)
			if next_c == '=' then
				i, _, _ = next_char(src, i)
				return new_token_ctx(i, ws, tokens.ge, nil, ctx.line)
			elseif next_c == '>' then
				i, _, _ = next_char(src, i)
				_, next_c, _ = next_char(src, i)
				if next_c == '=' then
					i, _, _ = next_char(src, i)
					return new_token_ctx(i, ws, tokens.shr_assign, nil, ctx.line)
				else
					return new_token_ctx(i, ws, tokens.shr, nil, ctx.line)
				end
			else
				return new_token_ctx(i, ws, tokens.gt, nil, ctx.line)
			end
		elseif c == '|' then
			_, next_c, _ = next_char(src, i)
			if next_c == '|' then
				i, _, _ = next_char(src, i)
				return new_token_ctx(i, ws, tokens.lor, nil, ctx.line)
			elseif next_c == '=' then
				i, _, _ = next_char(src, i)
				return new_token_ctx(i, ws, tokens.bor_assign, nil, ctx.line)
			else
				return new_token_ctx(i, ws, tokens._or, nil, ctx.line)
			end
		elseif c == '&' then
			_, next_c, _ = next_char(src, i)
			if next_c == '&' then
				i, _, _ = next_char(src, i)
				return new_token_ctx(i, ws, tokens.land, nil, ctx.line)
			elseif next_c == '=' then
				i, _, _ = next_char(src, i)
				return new_token_ctx(i, ws, tokens.band_assign, nil, ctx.line)
			else
				return new_token_ctx(i, ws, tokens._and, nil, ctx.line)
			end
		elseif c == '^' then
			_, next_c, _ = next_char(src, i)
			if next_c == '=' then
				i, _, _ = next_char(src, i)
				return new_token_ctx(i, ws, tokens.xor_assign, nil, ctx.line)
			else
				return new_token_ctx(i, ws, tokens.xor, nil, ctx.line)
			end
		elseif c == '%' then
			_, next_c, _ = next_char(src, i)
			if next_c == '=' then
				i, _, _ = next_char(src, i)
				return new_token_ctx(i, ws, tokens.mod_assign, nil, ctx.line)
			else
				return new_token_ctx(i, ws, tokens.mod, nil, ctx.line)
			end
		elseif c == '*' then
			_, next_c, _ = next_char(src, i)
			if next_c == '=' then
				i, _, _ = next_char(src, i)
				return new_token_ctx(i, ws, tokens.mul_assign, nil, ctx.line)
			else
				return new_token_ctx(i, ws, tokens.mul, nil, ctx.line)
			end
		elseif c == '?' then
			return new_token_ctx(i, ws, tokens.cond, nil, ctx.line)
		elseif c == '~' then
			return new_token_ctx(i, ws, tokens._not, nil, ctx.line)
		elseif c == '.' then
			return new_token_ctx(i, ws, tokens.member, nil, ctx.line)
		elseif c == '[' then
			return new_token_ctx(i, ws, tokens.brack, nil, ctx.line)
		else
			return new_token_ctx(i, ws, c, nil, ctx.line)
		end
	end
	return new_token_ctx(nil, nil, nil, nil, ctx.line)
end
