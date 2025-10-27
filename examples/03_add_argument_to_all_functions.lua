return function(global_decls)
	print("\n===== Example: add a parameter to every function =====")
	for _, func_node in ipairs(global_decls) do
		if func_node._type == nodes.func then
			local var = node:new_var(id:new("dbg" .. tostring(random_int(10, 1000))))
			var.ws_before = ' '
			local decl = node:new_decl(id:new("char*"), var)
			print("Added parameter '" .. tostring(var.value) .. "' to function '" .. tostring(func_node.id) .. "'")
			table.insert(func_node.params.value, decl)
		end
	end
end
