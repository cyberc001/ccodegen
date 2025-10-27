return function(global_decls)
	print("\n===== Example: print all sources =====")
	for _, v in ipairs(global_decls) do
		print("------------------------------------------------")
		print(v:src())
	end
end
