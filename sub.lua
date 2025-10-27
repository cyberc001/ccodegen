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

				local script_env = {}
				setmetatable(script_env, {__index = _ENV})
				script_env.node = node

				local script, err = load(src:sub(lua_start, i - 1), nil, nil, script_env)
				if not script then
					print("line " .. tostring(line) .. ": Lua script error\n\t" .. err)
					os.exit(1)
				end

				local res = script()
				if type(res) == "table" then
					if not res.src then
						print("line " .. tostring(line) .. ": table is not a node")
						os.exit(1)
					end
					res = res:src()
				end

				table.insert(subs, {str = res and tostring(res) or "", beg = lua_start - 4, _end = i + 4})
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
