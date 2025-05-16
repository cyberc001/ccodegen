require "enbf"

--src = "(char *)(((int)data + sizeof(int)) & (-sizeof(int)))"
src = "*--sp = (int)(pc+1)"

local i = 1
local node, token, token_val

i, token, token_val = next_token(src, i)

while true do
	node, i, token, token_val = expression(tokens.assign, src, i, token, token_val)
	print("----------------")
	print(node, i, token, token_val, type(token_val) == "table" and token_val.name or '')
	if i == nil then break end
	i, token, token_val = next_token(src, i)
	if i == nil then break end
end
