require "enbf"
local cli = require "cliargs"

cli:splat("SCRIPT_FILES", "Lua scripts that can parse and modify syntax tree", nil, 999)

local args, err = cli:parse(arg)
if not args and err then
	print(err)
	os.exit(1)
end

local src = io.read("*a")

local node
local ctx = new_token_ctx(1)
ctx = next_token(src, ctx)

local global_decls = {}
while true do
	node, ctx = global_decl(src, ctx)
	table.insert(global_decls, node)
	if ctx.i == nil then break end
	ctx = next_token(src, ctx)
	if ctx.i == nil then break end
end

for _, fpath in ipairs(args.SCRIPT_FILES) do
	local script, err = loadfile(fpath)
	if not script then
		print("Cannot load Lua script '" .. fpath .. "':\n\t" .. err)
	end
	local script_func = script()
	script_func(global_decls)
end
