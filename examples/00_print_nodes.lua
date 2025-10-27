return function(global_decls)
	print("\n===== Example: print all nodes =====")
	for _, v in ipairs(global_decls) do
		print("------------------------------------------------")
		print(v)
	end
end
