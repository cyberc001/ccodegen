require "enbf"

src = io.open("in"):read("*a")

local node, token, token_val

ctx = new_token_ctx(1)
ctx = next_token(src, ctx)

while true do
	node, ctx = global_decl(src, ctx)
	print("----------------")
	print(node:src())
	print(node)
	--print(node, i, token, token_val, type(token_val) == "table" and token_val.name or '')
	if ctx.i == nil then break end
	ctx = next_token(src, ctx)
	if ctx.i == nil then break end
end
