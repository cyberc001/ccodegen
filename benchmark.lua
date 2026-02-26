require "enbf"
require "sub"
require "utils"

function init_bench_decls(file_name)
	reset_identifiers()
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
		if type(global_node) == "string" then
			print("Error on line " .. ctx.line .. ":\n" .. global_node)
			os.exit(1)
		end
		table.insert(global_decls, global_node)
		if ctx.i == nil then break end
	end
	return os.clock() - tbeg, chars, lines
end

local file_names = {"adfs_dir.c", "bfs_inode.c", "ext2_xattr.c", "ext4_fast_commit.c", "fatent.c", "futex_pi.c", "gfs2_acl.c", "iomap_buffered-io.c", "kcsan_test.c", "kernel_uprobes.c", "kernfs_mount.c", "kernfs_symlink.c", "module_main.c", "netfs_buffered_write.c", "netfs_direct_read.c", "netfs_fscache_main.c", "power_snapshot.c", "proc_fd.c", "proc_inode.c", "sched_rt.c", "smb_server_asn1.c", "smb_server_auth.c", "squashfs_block.c", "ufs_inode.c", "ufs_super.c", "v9fs.c", "verity_hash_algs.c", "xfs_bmap_util.c", "xfs_buf.c"}
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
