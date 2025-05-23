function do_c_subs(src)
	local subs = {}
	local line = 1

	local is_lua = false
	local lua_start = 0
	for i = 1, src:len() do
		if src:sub(i, i) == '\n' then
			line = line + 1
		end

		if is_lua then
			if src:len() - i > 4 and src:sub(i, i + 3) == "$$*/" then
				is_lua = false
				local script, err = load(src:sub(lua_start, i - 1))
				if not script then
					print("line " .. tostring(line) .. ": Lua script error\n\t" .. err)
					os.exit(1)
				end

				local res = script()
				if res ~= nil then
					if type(res) == "table" then
						print("line " .. tostring(line) .. ": Lua tables are not supported")
						os.exit(1)
					end

					table.insert(subs, {str = tostring(res), beg = lua_start - 4, _end = i + 4})
				end
			end
		else
			if src:len() - i > 4 and src:sub(i, i + 3) == "/*$$" then
				is_lua = true
				lua_start = i + 4
			end
		end
	end

	res_str = ""
	local i = 1
	for _, v in ipairs(subs) do
		res_str = res_str .. src:sub(i, v.beg - 1) .. v.str
		i = v._end
	end
	res_str = res_str .. src:sub(i)

	return res_str
end
