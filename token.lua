tokens = {
	num = 1, fun = 2, sys = 3, glo = 4, loc = 5, id = 6,
	char = 7, str = 8,
	assign = 15, cond = 16, lor = 17, land = 18, _or = 19, xor = 20, _and = 21, eq = 22, ne = 23, lt = 24, gt = 25, le = 26, ge = 27, shl = 28, shr = 29, add = 30, sub = 31, mul = 32, div = 33, mod = 34, lnot = 35, _not = 36, inc = 37, dec = 38, brack = 39
}

function token_to_str(token)
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
	if token == tokens.inc then return '++' end
	if token == tokens.dec then return '--' end
	if token == tokens.brack then return '[]' end

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
	o = o or {}
	o.mods = o.mods or {}
	o.pointers = o.pointers or 0
	setmetatable(o, self)
	self.__index = self
	return o
end
function id:__tostring()
	s = ""
	for _, v in ipairs(self.mods) do
		s = s .. v .. " "
	end
	s = s .. self.name
	for i = 1, self.pointers do
		s = s .. "*"
	end
	return s
end

line = 1
identifiers = {
	const = { class = classes.type_mod },
	short = { class = classes.type_mod },
	long = { class = classes.type_mod },
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

function next_char(src, i)
	return i + 1, src:sub(i, i), string.byte(src:sub(i, i))
end

function new_token_ctx(i, ws, token, token_value)
	return {i = i, token = token, token_value = token_value, ws = ws or ""}
end

function next_token(src, ctx)
	local i = ctx.i
	local c, c_code
	local ws = ""
	while i and i <= src:len() do
		i, c, c_code = next_char(src, i)

		if c == '\n' then
			line = line + 1
			ws = ws .. c
		elseif is_ws(c) then -- пропуск пробелов
			ws = ws .. c
		elseif c == '#' then -- пропустить макросы
			while i <= src:len() and c ~= '\n' do
				i, c, c_code = next_char(src, i)
			end
			ws = "\n"
		elseif is_valid_id_char(c_code, true) then -- идентификатор
			local mods = {}
			repeat
				while is_ws(c) do -- пропуск пробелов до следующего идентификатора
					ws = ws .. c
					i, c, c_code = next_char(src, i)
				end

				if not is_valid_id_char(c_code, true) then
					break
				end

				local beg_i = i - 1
				while is_valid_id_char(c_code) do
					i, c, c_code = next_char(src, i)
				end
				id_name = src:sub(beg_i, i - 2)
				if not identifiers[id_name] then
					identifiers[id_name] = {}
				end
				table.insert(mods, id_name)
			until identifiers[id_name].class ~= classes.type_mod
			
			mods[#mods] = nil

			return new_token_ctx(i - 1, ws, tokens.id, id:new({name = id_name, mods = mods}))
		elseif is_number(c_code) then -- распарсить число
			local ch0 = string.byte('0')
			local val = c_code - ch0

			if val > 0 then -- десятичное число
				i, c, c_code = next_char(src, i)
				while is_number(c_code) do
					val = val * 10 + (c_code - ch0)
					i, c, c_code = next_char(src, i)
				end
			else
				if c == 'x' or c == 'X' then -- шестнадцатиричное число
					i, c, c_code = next_char(src, i)
					while is_number(c_code) or (c_code >= string.byte('a') and c_code <= string.byte('f')) or (c_code >= string.byte('A') and c_code <= string.byte('F')) do
						val = val * 16 + (c_code & 15) + (c_code >= string.byte('A') and 9 or 0)
						i, c, c_code = next_char(src, i)
					end
				else -- восьмеричное число
					while c_code >= ch0 and c_code <= string.byte('7') do
						val = val * 8 + (c_code - ch0)
						i, c, c_code = next_char(src, i)
					end
				end
			end

			return new_token_ctx(i - 1, ws, tokens.num, val)
		elseif c == '/' then
			_, next_c, next_c_code = next_char(src, i)
			if next_c == '/' then -- пропуск комментариев
				while i <= src:len() and c ~= '\n' do
					i, c, c_code = next_char(src, i)
				end
				ws = "\n"
			else -- оператор деления
				return new_token_ctx(i, ws, tokens.div)
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

			return new_token_ctx(i, ws, beg_c == '"' and tokens.str or tokens.char, src:sub(beg_i, i - 1))
		elseif c == '=' then
			_, next_c, _ = next_char(src, i)
			if next_c == '=' then
				i, _, _ = next_char(src, i)
				return new_token_ctx(i, ws, tokens.eq)
			else
				return new_token_ctx(i, ws, tokens.assign)
			end
		elseif c == '+' then
			_, next_c, _ = next_char(src, i)
			if next_c == '+' then
				i, _, _ = next_char(src, i)
				return new_token_ctx(i, ws, tokens.inc)
			else
				return new_token_ctx(i, ws, tokens.add)
			end
		elseif c == '-' then
			_, next_c, _ = next_char(src, i)
			if next_c == '-' then
				i, _, _ = next_char(src, i)
				return new_token_ctx(i, ws, tokens.dec)
			else
				return new_token_ctx(i, ws, tokens.sub)
			end
		elseif c == '!' then
			_, next_c, _ = next_char(src, i)
			if next_c == '=' then
				i, _, _ = next_char(src, i)
				return new_token_ctx(i, ws, tokens.ne)
			else
				return new_token_ctx(i, ws, tokens.lnot)
			end
		elseif c == '<' then
			_, next_c, _ = next_char(src, i)
			if next_c == '=' then
				i, _, _ = next_char(src, i)
				return new_token_ctx(i, ws, tokens.le)
			elseif next_c == '<' then
				i, _, _ = next_char(src, i)
				return new_token_ctx(i, ws, tokens.shl)
			else
				return new_token_ctx(i, ws, tokens.lt)
			end
		elseif c == '>' then
			_, next_c, _ = next_char(src, i)
			if next_c == '=' then
				i, _, _ = next_char(src, i)
				return new_token_ctx(i, ws, tokens.ge)
			elseif next_c == '<' then
				i, _, _ = next_char(src, i)
				return new_token_ctx(i, ws, tokens.shr)
			else
				return new_token_ctx(i, ws, tokens.gt)
			end
		elseif c == '|' then
			_, next_c, _ = next_char(src, i)
			if next_c == '|' then
				i, _, _ = next_char(src, i)
				return new_token_ctx(i, ws, tokens.lor)
			else
				return new_token_ctx(i, ws, tokens._or)
			end
		elseif c == '&' then
			_, next_c, _ = next_char(src, i)
			if next_c == '&' then
				i, _, _ = next_char(src, i)
				return new_token_ctx(i, ws, tokens.land)
			else
				return new_token_ctx(i, ws, tokens._and)
			end
		elseif c == '^' then
			return new_token_ctx(i, ws, tokens.xor)
		elseif c == '%' then
			return new_token_ctx(i, ws, tokens.mod)
		elseif c == '*' then
			return new_token_ctx(i, ws, tokens.mul)
		elseif c == '[' then
			return new_token_ctx(i, ws, tokens.brack)
		elseif c == '?' then
			return new_token_ctx(i, ws, tokens.cond)
		elseif c == '~' then
			return new_token_ctx(i, ws, tokens._not)
		else
			return new_token_ctx(i, ws, c)
		end
	end
	return new_token_ctx()
end
