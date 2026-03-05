local file_names = {"multidecl1.lua", "multifunc1.lua", "singlefunc1.lua", "singlefunc2.lua", "error1.lua", "error2.lua", "error3.lua", "error4.lua", "error5.lua"
}

for _, v in ipairs(file_names) do
	print("Loading unit test 'tests/" .. v .. "'")
	loadfile("tests/" .. v)()
end
print("\nRunning tests")
os.exit(lu.LuaUnit.run())
