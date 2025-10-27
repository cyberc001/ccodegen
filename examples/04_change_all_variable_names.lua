return function(global_decls)
	print("\n===== Example: change all variable names =====")
	for _, v in ipairs(global_decls) do
		local functions = v:get_children_of_type(nodes.var, true)
		for _, var_node in ipairs(functions) do
			if var_node.value.name then
				local prev_value_str = tostring(var_node.value)
				var_node.value.name = var_node.value.name .. "_" .. tostring(random_int(10, 1000))
				print("Changed variable '" .. prev_value_str .. "' to '" .. tostring(var_node.value) .. "'")
			end
		end
	end
end
