require "enbf"
require "sub"
require "utils"
local cli = require "cliargs"

cli:splat("SCRIPT_FILES", "Lua scripts that can parse and modify syntax tree", nil, 999)

local args, err = cli:parse(arg)
if not args and err then
	print(err)
	os.exit(1)
end

local src = io.read("*a")
src = do_c_subs(src)

enbf_debug = true
local global_node
local ctx = new_token_ctx(1)
ctx = next_token(src, ctx)

local global_decls = {}
while true do
	global_node, ctx = global_decl(src, ctx)
	if type(global_node) == "string" then
		print("Error on line " .. ctx.line .. ":\n" .. global_node)
		os.exit(1)
	end
	table.insert(global_decls, global_node)
	if ctx.i == nil then break end
end

for _, fpath in ipairs(args.SCRIPT_FILES) do
	local script_env = {}
	setmetatable(script_env, {__index = _ENV})

	local script, err = loadfile(fpath, nil, script_env)
	if not script then
		print("Cannot load Lua script '" .. fpath .. "':\n\t" .. err)
	end
	local script_func = script()
	script_func(global_decls)
end
