require "enbf"
require "sub"
require "utils"

function init_bench_decls(file_name)
	local file = io.open("benchmarks/" .. file_name)
	local src = file:read("*a")
	_, lines = src:gsub("\n", "\n")
	chars = src:len()
	
	local tbeg = os.clock()
	local ctx = new_token_ctx(1)
	ctx = next_token(src, ctx)
	global_decls = {}
	while true do
		global_node, ctx = global_decl(src, ctx)
		table.insert(global_decls, global_node)
		if ctx.i == nil then break end
		ctx = next_token(src, ctx)
		if ctx.i == nil then break end
	end
	return os.clock() - tbeg, chars, lines
end

local file_names = {"bench_linux_pfsm_wakeup.c", "bench_linux_ucon.c", "bench_linux_hpet_example.c", "bench_linux_xxhash.c", "bench_linux_mei_amt_version.c", "bench_linux_stm32_omm.c", "bench_linux_gpio_fan.c", "bench_linux_debug_core.c", "bench_linux_slicoss.c", "bench_linux_sd.c", "bench_linux_adm1026.c"}
local test_amt = 1000

for _, v in ipairs(file_names) do
	local test_min = math.maxinteger
	local test_max = -1
	local test_avg = 0
	local chars, lines
	for i = 1, test_amt do
		local telapsed
		telapsed, chars, lines = init_bench_decls(v)
		if telapsed < test_min then test_min = telapsed end
		if telapsed > test_max then test_max = telapsed end
		test_avg = test_avg + telapsed
	end
	test_avg = test_avg / test_amt

	print(string.format("File '%s' c %u l %u\navg %fs min %fs max %fs", v, chars, lines, test_avg, test_min, test_max))
end
