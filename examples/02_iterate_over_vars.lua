local function iterate_node(in_node)
	if in_node._type == nodes.var then
		print("var " .. in_node.value)
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
