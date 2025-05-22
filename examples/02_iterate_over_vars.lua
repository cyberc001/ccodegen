local function iterate_node(node)
	if node._type == nodes.var then
		print("var " .. node.value)
	end
	for _, v in ipairs(node:get_children()) do
		iterate_node(v)
	end
end

return function(global_decls)
	for _, v in ipairs(global_decls) do
		print("-------------------")
		iterate_node(v)
	end
end
