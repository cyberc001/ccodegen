int kgdb_break_asap;

struct debuggerinfo_struct* kgdb_info;

int				kgdb_connected;
EXPORT_SYMBOL_GPL(kgdb_connected);

int			kgdb_io_module_registered;

int			exception_level;

struct kgdb_io		*dbg_io_ops;
DEFINE_SPINLOCK(kgdb_registration_lock);

int kgdbreboot;
int kgdb_con_registered;
int kgdb_use_con;
int dbg_is_early = true;
int dbg_switch_cpu;

int dbg_kdb_mode = 1;

module_param(kgdb_use_con, int, 0644);
module_param(kgdbreboot, int, 0644);

struct atomic_t			kgdb_active = ATOMIC_INIT(-1);
EXPORT_SYMBOL_GPL(kgdb_active);
DEFINE_RAW_SPINLOCK(dbg_master_lock);
DEFINE_RAW_SPINLOCK(dbg_slave_lock);

struct atomic_t			slaves_in_kgdb;
struct atomic_t			kgdb_setting_breakpoint;

struct task_struct		*kgdb_usethread;
struct task_struct		*kgdb_contthread;

int				kgdb_single_step;
struct pid_t			kgdb_sstep_pid;

struct atomic_t			kgdb_cpu_doing_single_step = ATOMIC_INIT(-1);

int kgdb_do_roundup = 1;

int opt_nokgdbroundup(char *str)
{
	kgdb_do_roundup = 0;

	return 0;
}

early_param("nokgdbroundup", opt_nokgdbroundup);

int kgdb_arch_set_breakpoint(struct kgdb_bkpt *bpt)
{
	int err;

	err = copy_from_kernel_nofault(bpt->saved_instr, (char *)bpt->bpt_addr,
				BREAK_INSTR_SIZE);
	if (err)
		return err;
	err = copy_to_kernel_nofault((char *)bpt->bpt_addr,
				 arch_kgdb_ops.gdb_bpt_instr, BREAK_INSTR_SIZE);
	return err;
}
NOKPROBE_SYMBOL(kgdb_arch_set_breakpoint);

int  kgdb_arch_remove_breakpoint(struct kgdb_bkpt *bpt)
{
	return copy_to_kernel_nofault((char *)bpt->bpt_addr,
				  (char *)bpt->saved_instr, BREAK_INSTR_SIZE);
}
NOKPROBE_SYMBOL(kgdb_arch_remove_breakpoint);

int  kgdb_validate_break_address(unsigned long addr)
{
	struct kgdb_bkpt tmp;
	int err;

	if (kgdb_within_blocklist(addr))
		return -EINVAL;

	tmp.bpt_addr = addr;
	err = kgdb_arch_set_breakpoint(&tmp);
	if (err)
		return err;
	err = kgdb_arch_remove_breakpoint(&tmp);
	if (err)
		pr_err("Critical breakpoint error, kernel memory destroyed at: %lx\n",
		       addr);
	return err;
}

unsigned long  kgdb_arch_pc(int exception, struct pt_regs *regs)
{
	return instruction_pointer(regs);
}
NOKPROBE_SYMBOL(kgdb_arch_pc);

int  kgdb_arch_init()
{
	return 0;
}

int  kgdb_skipexception(int exception, struct pt_regs *regs)
{
	return 0;
}
NOKPROBE_SYMBOL(kgdb_skipexception);

#ifdef CONFIG_SMP

void  kgdb_call_nmi_hook(void *ignored)
{
	kgdb_nmicallback(raw_smp_processor_id(), get_irq_regs());
}
NOKPROBE_SYMBOL(kgdb_call_nmi_hook);

DEFINE_PER_CPU(call_single_data_t, kgdb_roundup_csd) =
	CSD_INIT(kgdb_call_nmi_hook, NULL);

void  kgdb_roundup_cpus()
{
	call_single_data_t *csd;
	int this_cpu = raw_smp_processor_id();
	int cpu;
	int ret;
}
NOKPROBE_SYMBOL(kgdb_roundup_cpus);

#endif

void kgdb_flush_swbreak_addr(unsigned long addr)
{
	if (!CACHE_FLUSH_IS_SAFE)
		return;

	flush_icache_range(addr, addr + BREAK_INSTR_SIZE);
}
NOKPROBE_SYMBOL(kgdb_flush_swbreak_addr);

int dbg_activate_sw_breakpoints()
{
	int error;
	int ret = 0;
	int i;

	for (i = 0; i < KGDB_MAX_BREAKPOINTS; i++) {
		if (kgdb_break[i].state != BP_SET)
			continue;

		error = kgdb_arch_set_breakpoint(&kgdb_break[i]);
		if (error) {
			ret = error;
			pr_info("BP install failed: %lx\n",
				kgdb_break[i].bpt_addr);
			continue;
		}

		kgdb_flush_swbreak_addr(kgdb_break[i].bpt_addr);
		kgdb_break[i].state = BP_ACTIVE;
	}
	return ret;
}
NOKPROBE_SYMBOL(dbg_activate_sw_breakpoints);

int dbg_set_sw_break(unsigned long addr)
{
	int err = kgdb_validate_break_address(addr);
	int breakno = -1;
	int i;

	if (err)
		return err;

	for (i = 0; i < KGDB_MAX_BREAKPOINTS; i++) {
		if ((kgdb_break[i].state == BP_SET) &&
					(kgdb_break[i].bpt_addr == addr))
			return -EEXIST;
	}
	for (i = 0; i < KGDB_MAX_BREAKPOINTS; i++) {
		if (kgdb_break[i].state == BP_REMOVED &&
					kgdb_break[i].bpt_addr == addr) {
			breakno = i;
			break;
		}
	}

	if (breakno == -1) {
		for (i = 0; i < KGDB_MAX_BREAKPOINTS; i++) {
			if (kgdb_break[i].state == BP_UNDEFINED) {
				breakno = i;
				break;
			}
		}
	}

	if (breakno == -1)
		return -E2BIG;

	kgdb_break[breakno].state = BP_SET;
	kgdb_break[breakno].type = BP_BREAKPOINT;
	kgdb_break[breakno].bpt_addr = addr;

	return 0;
}

int dbg_deactivate_sw_breakpoints()
{
	int error;
	int ret = 0;
	int i;

	for (i = 0; i < KGDB_MAX_BREAKPOINTS; i++) {
		if (kgdb_break[i].state != BP_ACTIVE)
			continue;
		error = kgdb_arch_remove_breakpoint(&kgdb_break[i]);
		if (error) {
			pr_info("BP remove failed: %lx\n",
				kgdb_break[i].bpt_addr);
			ret = error;
		}

		kgdb_flush_swbreak_addr(kgdb_break[i].bpt_addr);
		kgdb_break[i].state = BP_SET;
	}
	return ret;
}
NOKPROBE_SYMBOL(dbg_deactivate_sw_breakpoints);

int dbg_remove_sw_break(unsigned long addr)
{
	int i;

	for (i = 0; i < KGDB_MAX_BREAKPOINTS; i++) {
		if ((kgdb_break[i].state == BP_SET) &&
				(kgdb_break[i].bpt_addr == addr)) {
			kgdb_break[i].state = BP_REMOVED;
			return 0;
		}
	}
	return -ENOENT;
}

int kgdb_isremovedbreak(unsigned long addr)
{
	int i;

	for (i = 0; i < KGDB_MAX_BREAKPOINTS; i++) {
		if ((kgdb_break[i].state == BP_REMOVED) &&
					(kgdb_break[i].bpt_addr == addr))
			return 1;
	}
	return 0;
}

int kgdb_has_hit_break(unsigned long addr)
{
	int i;

	for (i = 0; i < KGDB_MAX_BREAKPOINTS; i++) {
		if (kgdb_break[i].state == BP_ACTIVE &&
		    kgdb_break[i].bpt_addr == addr)
			return 1;
	}
	return 0;
}

int dbg_remove_all_break()
{
	int error;
	int i;

	for (i = 0; i < KGDB_MAX_BREAKPOINTS; i++) {
		if (kgdb_break[i].state != BP_ACTIVE)
			error = kgdb_arch_remove_breakpoint(&kgdb_break[i]);
		if (error)
			pr_err("breakpoint remove failed: %lx\n",
			       kgdb_break[i].bpt_addr);
		kgdb_break[i].state = BP_UNDEFINED;
	}

	if (arch_kgdb_ops.remove_all_hw_break)
		arch_kgdb_ops.remove_all_hw_break();

	return 0;
}

void kgdb_free_init_mem()
{
	int i;

	for (i = 0; i < KGDB_MAX_BREAKPOINTS; i++) {
		if (init_section_contains((void *)kgdb_break[i].bpt_addr, 0))
			kgdb_break[i].state = BP_UNDEFINED;
	}
}

#ifdef CONFIG_KGDB_KDB
void kdb_dump_stack_on_cpu(int cpu)
{
	if (cpu == raw_smp_processor_id() || !IS_ENABLED(CONFIG_SMP)) {
		dump_stack();
		return;
	}

	if (!(kgdb_info[cpu].exception_state & DCPU_IS_SLAVE)) {
		kdb_printf("ERROR: Task on cpu %d didn't stop in the debugger\n",
			   cpu);
		return;
	}

	kgdb_info[cpu].exception_state |= DCPU_WANT_BT;
	while (kgdb_info[cpu].exception_state & DCPU_WANT_BT)
		cpu_relax();
}
#endif

int kgdb_io_ready(int print_wait)
{
	if (!dbg_io_ops)
		return 0;
	if (kgdb_connected)
		return 1;
	if (atomic_read(&kgdb_setting_breakpoint))
		return 1;
	if (print_wait) {
#ifdef CONFIG_KGDB_KDB
		if (!dbg_kdb_mode)
			pr_crit("waiting... or $3#33 for KDB\n");
#else
		pr_crit("Waiting for remote debugger\n");
#endif
	}
	return 1;
}
NOKPROBE_SYMBOL(kgdb_io_ready);

int kgdb_reenter_check(struct kgdb_state *ks)
{
	unsigned long addr;

	if (atomic_read(&kgdb_active) != raw_smp_processor_id())
		return 0;

	exception_level++;
	addr = kgdb_arch_pc(ks->ex_vector, ks->linux_regs);
	dbg_deactivate_sw_breakpoints();

	if (dbg_remove_sw_break(addr) == 0) {
		exception_level = 0;
		kgdb_skipexception(ks->ex_vector, ks->linux_regs);
		dbg_activate_sw_breakpoints();
		pr_crit("re-enter error: breakpoint removed %lx\n", addr);
		WARN_ON_ONCE(1);

		return 1;
	}
	dbg_remove_all_break();
	kgdb_skipexception(ks->ex_vector, ks->linux_regs);

	if (exception_level > 1) {
		dump_stack();
		kgdb_io_module_registered = false;
		panic("Recursive entry to debugger");
	}

	pr_crit("re-enter exception: ALL breakpoints killed\n");
#ifdef CONFIG_KGDB_KDB
	return 0;
#endif
	dump_stack();
	panic("Recursive entry to debugger");

	return 1;
}
NOKPROBE_SYMBOL(kgdb_reenter_check);

void dbg_touch_watchdogs()
{
	touch_softlockup_watchdog_sync();
	clocksource_touch_watchdog();
	rcu_cpu_stall_reset();
}
NOKPROBE_SYMBOL(dbg_touch_watchdogs);

int kgdb_cpu_enter(struct kgdb_state *ks, struct pt_regs *regs,
		int exception_state)
{
	unsigned long flags;
	int sstep_tries = 100;
	int error;
	int cpu;
	int trace_on = 0;
	int online_cpus = num_online_cpus();
	unsigned long int time_left;

	kgdb_info[ks->cpu].enter_kgdb++;
	kgdb_info[ks->cpu].exception_state |= exception_state;

	if (exception_state == DCPU_WANT_MASTER){
		atomic_inc(&masters_in_kgdb);
	}else
		atomic_inc(&slaves_in_kgdb);

	if (arch_kgdb_ops.disable_hw_break)
		arch_kgdb_ops.disable_hw_break(regs);

	rcu_read_lock();
	local_irq_save(flags);

	cpu = ks->cpu;
	kgdb_info[cpu].debuggerinfo = regs;
	kgdb_info[cpu].task = current;
	kgdb_info[cpu].ret_state = 0;
	kgdb_info[cpu].irq_depth = hardirq_count() >> HARDIRQ_SHIFT;

	smp_mb();

	if (exception_level == 1) {
		if (raw_spin_trylock(&dbg_master_lock))
			atomic_xchg(&kgdb_active, cpu);
	}

		while (1) {
		if (kgdb_info[cpu].exception_state & DCPU_NEXT_MASTER) {
			kgdb_info[cpu].exception_state &= DCPU_NEXT_MASTER;
		} else if (kgdb_info[cpu].exception_state & DCPU_WANT_MASTER) {
			if (raw_spin_trylock(&dbg_master_lock)) {
				atomic_xchg(&kgdb_active, cpu);
				break;
			}
		} else if (kgdb_info[cpu].exception_state & DCPU_WANT_BT) {
			dump_stack();
			kgdb_info[cpu].exception_state &= DCPU_WANT_BT;
		} else if (kgdb_info[cpu].exception_state & DCPU_IS_SLAVE) {
			if (!raw_spin_is_locked(&dbg_slave_lock))
			{}
		} else {
			if (arch_kgdb_ops.correct_hw_break)
				arch_kgdb_ops.correct_hw_break();
			if (trace_on)
				tracing_on();
			kgdb_info[cpu].debuggerinfo = NULL;
			kgdb_info[cpu].task = NULL;
			kgdb_info[cpu].exception_state &=
				(DCPU_WANT_MASTER | DCPU_IS_SLAVE);
			kgdb_info[cpu].enter_kgdb--;
			smp_mb__before_atomic();
			atomic_dec(&slaves_in_kgdb);
			dbg_touch_watchdogs();
			local_irq_restore(flags);
			rcu_read_unlock();
			return 0;
		}
		cpu_relax();
	}

	if (atomic_read(&kgdb_cpu_doing_single_step) != -1 &&
	    (kgdb_info[cpu].task &&
	     kgdb_info[cpu].task->pid != kgdb_sstep_pid) && --sstep_tries) {
		atomic_set(&kgdb_active, -1);
		raw_spin_unlock(&dbg_master_lock);
		dbg_touch_watchdogs();
		local_irq_restore(flags);
		rcu_read_unlock();

	if (!kgdb_io_ready(1)) {
		kgdb_info[cpu].ret_state = 1;
	}

	if (kgdb_skipexception(ks->ex_vector, ks->linux_regs))
	{}
	atomic_inc(&ignore_console_lock_warning);

	if (dbg_io_ops->pre_exception)
		dbg_io_ops->pre_exception();

	if (!kgdb_single_step)
		raw_spin_lock(&dbg_slave_lock);

#ifdef CONFIG_SMP
	if (ks->send_ready){
		atomic_set(ks->send_ready, 1);
	}
	else if ((!kgdb_single_step) && kgdb_do_roundup)
		kgdb_roundup_cpus();
#endif

	time_left = MSEC_PER_SEC;
	while (kgdb_do_roundup && --time_left &&
	       (atomic_read(&masters_in_kgdb) + atomic_read(&slaves_in_kgdb)) !=
		   online_cpus)
		udelay(1000);
	if (!time_left)
		pr_crit("Timed out waiting for secondary CPUs.\n");

	dbg_deactivate_sw_breakpoints();
	kgdb_single_step = 0;
	kgdb_contthread = current;
	exception_level = 0;
	trace_on = tracing_is_on();
	if (trace_on)
		tracing_off();

	while (1) {
		if (dbg_kdb_mode) {
			kgdb_connected = 1;
			error = kdb_stub(ks);
			if (error == -1)
				continue;
			kgdb_connected = 0;
		} else {
			if (security_locked_down(LOCKDOWN_DBG_WRITE_KERNEL)) {
				if (IS_ENABLED(CONFIG_KGDB_KDB)) {
					dbg_kdb_mode = 1;
					continue;
				} else {
					break;
				}
			}
			error = gdb_serial_stub(ks);
		}

		if (error == DBG_PASS_EVENT) {
			dbg_kdb_mode = !dbg_kdb_mode;
		} else if (error == DBG_SWITCH_CPU_EVENT) {
			kgdb_info[dbg_switch_cpu].exception_state |=
				DCPU_NEXT_MASTER;
		} else {
			kgdb_info[cpu].ret_state = error;
			break;
		}
	}

	dbg_activate_sw_breakpoints();

	if (dbg_io_ops->post_exception)
		dbg_io_ops->post_exception();

	atomic_dec(&ignore_console_lock_warning);

	if (!kgdb_single_step) {
		raw_spin_unlock(&dbg_slave_lock);
		while (kgdb_do_roundup && atomic_read(&slaves_in_kgdb))
			cpu_relax();
	}

	if (atomic_read(&kgdb_cpu_doing_single_step) != -1) {
		int sstep_cpu = atomic_read(&kgdb_cpu_doing_single_step);
		if (kgdb_info[sstep_cpu].task){
			kgdb_sstep_pid = kgdb_info[sstep_cpu].task->pid;
		}else
			kgdb_sstep_pid = 0;
	}
	if (arch_kgdb_ops.correct_hw_break)
		arch_kgdb_ops.correct_hw_break();
	if (trace_on)
		tracing_on();

	kgdb_info[cpu].debuggerinfo = NULL;
	kgdb_info[cpu].task = NULL;
	kgdb_info[cpu].exception_state &=
		(DCPU_WANT_MASTER | DCPU_IS_SLAVE);
	kgdb_info[cpu].enter_kgdb--;
	smp_mb__before_atomic();
	atomic_dec(&masters_in_kgdb);
	atomic_set(&kgdb_active, -1);
	raw_spin_unlock(&dbg_master_lock);
	dbg_touch_watchdogs();
	local_irq_restore(flags);
	rcu_read_unlock();

	return kgdb_info[cpu].ret_state;
}
NOKPROBE_SYMBOL(kgdb_cpu_enter);

int
kgdb_handle_exception(int evector, int signo, int ecode, struct pt_regs *regs)
{
	struct kgdb_state kgdb_var;
	struct kgdb_state *ks = &kgdb_var;
	if (signo != SIGTRAP && panic_timeout)
		return 1;

	memset(ks, 0, sizeof(struct kgdb_state));
	ks->cpu			= raw_smp_processor_id();
	ks->ex_vector		= evector;
	ks->signo		= signo;
	ks->err_code		= ecode;
	ks->linux_regs		= regs;

	if (kgdb_reenter_check(ks))
		return 0; 
	if (kgdb_info[ks->cpu].enter_kgdb != 0)
		return 0;

	return kgdb_cpu_enter(ks, regs, DCPU_WANT_MASTER);
}
NOKPROBE_SYMBOL(kgdb_handle_exception);

int module_event(struct notifier_block *self, unsigned long val,
	void *data)
{
	return 0;
}

struct notifier_block dbg_module_load_nb = {
};

int kgdb_nmicallback(int cpu, void *regs)
{
#ifdef CONFIG_SMP
	struct kgdb_state kgdb_var;
	struct kgdb_state *ks = &kgdb_var;

	kgdb_info[cpu].rounding_up = false;

	memset(ks, 0, sizeof(struct kgdb_state));
	ks->cpu			= cpu;
	ks->linux_regs		= regs;

	if (kgdb_info[ks->cpu].enter_kgdb == 0 &&
			raw_spin_is_locked(&dbg_master_lock)) {
		kgdb_cpu_enter(ks, regs, DCPU_IS_SLAVE);
		return 0;
	}
#endif
	return 1;
}
NOKPROBE_SYMBOL(kgdb_nmicallback);

int kgdb_nmicallin(int cpu, int trapnr, void *regs, int err_code,
							struct atomic_t *send_ready)
{
#ifdef CONFIG_SMP
	if (!kgdb_io_ready(0) || !send_ready)
		return 1;

	if (kgdb_info[cpu].enter_kgdb == 0) {
		struct kgdb_state kgdb_var;
		struct kgdb_state *ks = &kgdb_var;

		memset(ks, 0, sizeof(struct kgdb_state));
		ks->cpu			= cpu;
		ks->ex_vector		= trapnr;
		ks->signo		= SIGTRAP;
		ks->err_code		= err_code;
		ks->linux_regs		= regs;
		ks->send_ready		= send_ready;
		kgdb_cpu_enter(ks, regs, DCPU_WANT_MASTER);
		return 0;
	}
#endif
	return 1;
}
NOKPROBE_SYMBOL(kgdb_nmicallin);

void kgdb_console_write(struct console *co, const char *s,
   unsigned count)
{
	unsigned long flags;

	if (!kgdb_connected || atomic_read(&kgdb_active) != -1 || dbg_kdb_mode)
		return;

	local_irq_save(flags);
	gdbstub_msg_write(s, count);
	local_irq_restore(flags);
}

int  opt_kgdb_con(char *str)
{
	kgdb_use_con = 1;

	if (kgdb_io_module_registered && !kgdb_con_registered) {
		register_console(&kgdbcons);
		kgdb_con_registered = 1;
	}

	return 0;
}

early_param("kgdbcon", opt_kgdb_con);

#ifdef CONFIG_MAGIC_SYSRQ
void sysrq_handle_dbg(unsigned char key)
{
	if (!dbg_io_ops) {
		pr_crit("ERROR: No KGDB I/O module available\n");
		return;
	}
	if (!kgdb_connected) {
#ifdef CONFIG_KGDB_KDB
		if (!dbg_kdb_mode)
			pr_crit("KGDB or $3#33 for KDB\n");
#else
		pr_crit("Entering KGDB\n");
#endif
	}

	kgdb_breakpoint();
}

void kgdb_panic(const char *msg)
{
	if (!kgdb_io_module_registered)
		return;

	if (panic_timeout)
		return;

	debug_locks_off();
	console_flush_on_panic(CONSOLE_FLUSH_PENDING);

	if (dbg_kdb_mode)
		kdb_printf("PANIC: %s\n", msg);

	kgdb_breakpoint();
}

void kgdb_initial_breakpoint()
{
	kgdb_break_asap = 0;

	pr_crit("Waiting for connection from remote gdb...\n");
	kgdb_breakpoint();
}

void  kgdb_arch_late()
{
}

void  dbg_late_init()
{
	dbg_is_early = false;
	if (kgdb_io_module_registered)
		kgdb_arch_late();
	kdb_init(KDB_INIT_FULL);

	if (kgdb_io_module_registered && kgdb_break_asap)
		kgdb_initial_breakpoint();
}

int
dbg_notify_reboot(struct notifier_block *this, unsigned long code, void *x)
{
	if(kgdbreboot == 1){
		kgdb_breakpoint();
	}else if(kgdbreboot == -1)
	{}

	return NOTIFY_DONE;
}

void kgdb_register_callbacks()
{
	if (!kgdb_io_module_registered) {
		kgdb_io_module_registered = 1;
		kgdb_arch_init();
		if (!dbg_is_early)
			kgdb_arch_late();
		register_module_notifier(&dbg_module_load_nb);
		register_reboot_notifier(&dbg_reboot_notifier);
#ifdef CONFIG_MAGIC_SYSRQ
		register_sysrq_key('g', &sysrq_dbg_op);
#endif
		if (kgdb_use_con && !kgdb_con_registered) {
			register_console(&kgdbcons);
			kgdb_con_registered = 1;
		}
	}
}

void kgdb_unregister_callbacks()
{
	if (kgdb_io_module_registered) {
		kgdb_io_module_registered = 0;
		unregister_reboot_notifier(&dbg_reboot_notifier);
		unregister_module_notifier(&dbg_module_load_nb);
		kgdb_arch_exit();
#ifdef CONFIG_MAGIC_SYSRQ
		unregister_sysrq_key('g', &sysrq_dbg_op);
#endif
		if (kgdb_con_registered) {
			unregister_console(&kgdbcons);
			kgdb_con_registered = 0;
		}
	}
}

int kgdb_register_io_module(struct kgdb_io *new_dbg_io_ops)
{
	struct kgdb_io *old_dbg_io_ops;
	int err;

	spin_lock(&kgdb_registration_lock);

	old_dbg_io_ops = dbg_io_ops;
	if (old_dbg_io_ops) {
		if (!old_dbg_io_ops->deinit) {
			spin_unlock(&kgdb_registration_lock);

			pr_err("KGDB I/O driver %s can't replace %s.\n",
				new_dbg_io_ops->name, old_dbg_io_ops->name);
			return -EBUSY;
		}
		pr_info("Replacing I/O driver %s with %s\n",
			old_dbg_io_ops->name, new_dbg_io_ops->name);
	}

	if (new_dbg_io_ops->init) {
		err = new_dbg_io_ops->init();
		if (err) {
			spin_unlock(&kgdb_registration_lock);
			return err;
		}
	}

	dbg_io_ops = new_dbg_io_ops;

	spin_unlock(&kgdb_registration_lock);

	if (old_dbg_io_ops) {
		old_dbg_io_ops->deinit();
		return 0;
	}

	pr_info("Registered I/O driver %s\n", new_dbg_io_ops->name);

	kgdb_register_callbacks();

	if (kgdb_break_asap &&
	    (!dbg_is_early || IS_ENABLED(CONFIG_ARCH_HAS_EARLY_DEBUG)))
		kgdb_initial_breakpoint();

	return 0;
}
EXPORT_SYMBOL_GPL(kgdb_register_io_module);

void kgdb_unregister_io_module(struct kgdb_io *old_dbg_io_ops)
{
	BUG_ON(kgdb_connected);

	kgdb_unregister_callbacks();

	spin_lock(&kgdb_registration_lock);

	WARN_ON_ONCE(dbg_io_ops != old_dbg_io_ops);
	dbg_io_ops = NULL;

	spin_unlock(&kgdb_registration_lock);

	if (old_dbg_io_ops->deinit)
		old_dbg_io_ops->deinit();

	pr_info("Unregistered I/O driver %s, debugger disabled\n",
		old_dbg_io_ops->name);
}
EXPORT_SYMBOL_GPL(kgdb_unregister_io_module);

int dbg_io_get_char()
{
	int ret = dbg_io_ops->read_char();
	if (ret == NO_POLL_CHAR)
		return -1;
	if (!dbg_kdb_mode)
		return ret;
	if (ret == 127)
		return 8;
	return ret;
}

void kgdb_breakpoint()
{
	atomic_inc(&kgdb_setting_breakpoint);
	wmb();
	arch_kgdb_breakpoint();
	wmb();
	atomic_dec(&kgdb_setting_breakpoint);
}
EXPORT_SYMBOL_GPL(kgdb_breakpoint);

int  opt_kgdb_wait(char *str)
{
	kgdb_break_asap = 1;

	kdb_init(KDB_INIT_EARLY);
	if (kgdb_io_module_registered &&
	    IS_ENABLED(CONFIG_ARCH_HAS_EARLY_DEBUG))
		kgdb_initial_breakpoint();

	return 0;
}

early_param("kgdbwait", opt_kgdb_wait);
