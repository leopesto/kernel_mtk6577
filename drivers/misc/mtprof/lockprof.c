#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/kallsyms.h>
#include <linux/utsname.h>
#include <asm/uaccess.h>
#include <linux/stacktrace.h>
#include <asm/stacktrace.h>

#include <linux/pid.h>
#define SEQ_printf(m, x...)	    \
 do {			    \
    if (m)		    \
	seq_printf(m, x);	\
    else		    \
	printk(x);	    \
 } while (0)

#define MT_DEBUG_ENTRY(name) \
static int mt_##name##_show(struct seq_file *m, void *v);\
static int mt_##name##_write(struct file *filp, const char *ubuf, size_t cnt, loff_t *data);\
static int mt_##name##_open(struct inode *inode, struct file *file) \
{ \
    return single_open(file, mt_##name##_show, inode->i_private); \
} \
\
static const struct file_operations mt_##name##_fops = { \
    .open = mt_##name##_open, \
    .write = mt_##name##_write,\
    .read = seq_read, \
    .llseek = seq_lseek, \
    .release = single_release, \
};\
void mt_##name##_switch(int on);

#include <linux/mt_export.h>

/*
 * Ease the printing of nsec fields:
 */
static long long nsec_high(unsigned long long nsec)
{
    if ((long long)nsec < 0) {
	nsec = -nsec;
	do_div(nsec, 1000000);
	return -nsec;
    }
    do_div(nsec, 1000000);

    return nsec;
}

static unsigned long nsec_low(unsigned long long nsec)
{
    if ((long long)nsec < 0)
	nsec = -nsec;

    return do_div(nsec, 1000000);
}
#define SPLIT_NS(x) nsec_high(x), nsec_low(x)

static long long usec_high(unsigned long long usec)
{
    if ((long long)usec < 0) {
	usec = -usec;
	do_div(usec, 1000);
	return -usec;
    }
    do_div(usec, 1000);

    return usec;
}
static unsigned long usec_low(unsigned long long usec)
{
    if ((long long)usec < 0)
	usec = -usec;

    return do_div(usec, 1000);
}
#define SPLIT_US(x) usec_high(x), usec_low(x)
/*
kernel  back trace utility
*/
static void mt_dump_backtrace_entry(struct seq_file *m, unsigned long where, unsigned long from, unsigned long frame)
{
#ifdef CONFIG_KALLSYMS
	SEQ_printf(m, "[<%08lx>] (%pS) from [<%08lx>] (%pS)\n", where, (void *)where, from, (void *)from);
#else
	SEQ_printf(m, "Function entered at [<%08lx>] from [<%08lx>]\n", where, from);
#endif
}
extern int unwind_frame(struct stackframe *frame);
static void proc_stack(struct seq_file *m, struct task_struct *tsk)
{
	struct stackframe frame;
	register unsigned long current_sp asm ("sp");

	if (!tsk)
		tsk = current;

	if (tsk == current) {
		frame.fp = (unsigned long)__builtin_frame_address(0);
		frame.sp = current_sp;
		frame.lr = (unsigned long)__builtin_return_address(0);
		frame.pc = (unsigned long)proc_stack;
	} else {
		/* task blocked in __switch_to */
		frame.fp = thread_saved_fp(tsk);
		frame.sp = thread_saved_sp(tsk);
		/*
		 * The function calling __switch_to cannot be a leaf function
		 * so LR is recovered from the stack.
		 */
		frame.lr = 0;
		frame.pc = thread_saved_pc(tsk);
	}

	while (1) {
		int urc;
		unsigned long where = frame.pc;

		urc = unwind_frame(&frame);
		if (urc < 0)
			break;
		mt_dump_backtrace_entry(m, where, frame.pc, frame.sp - 4);
	}
}

static void sem_traverse(struct seq_file *m)
{
    struct task_struct *g, *p;
    struct semaphore* sem;
    int i = 0;
    char state;
    read_lock(&tasklist_lock);
    SEQ_printf(m, "---------------------------\n");
    SEQ_printf(m, "[Semaphore list]-----------\n");
    do_each_thread(g, p){
        /*
         * It's not reliable to print a task's held locks
         * if it's not sleeping (or if it's not the current
         * task):
         */
        #if 0
        if(p->hold_mutex == NULL)
            continue;
        printk("%3d[%d:%s]------------------------\n",i, p->pid, p->comm);

        lock = p->hold_mutex;
        do{
            lock = list_entry(lock->mutex_list.next, struct mutex, mutex_list);
            printk("[%s] - 0x%8x\n", lock->mutex_name, lock);
        }while(lock != p->hold_mutex);
        #endif

        if(list_empty(&p->sem_head))
            continue;

        state = (p->state == 0) ? 'R' :
        (p->state < 0) ? 'U' :
        (p->state & TASK_UNINTERRUPTIBLE) ? 'D' :
        (p->state & TASK_STOPPED) ? 'T' :
        (p->state & TASK_TRACED) ? 'C' :
        (p->exit_state & EXIT_ZOMBIE) ? 'Z' :
        (p->exit_state & EXIT_DEAD) ? 'E' :
        (p->state & TASK_INTERRUPTIBLE) ? 'S' : '?';

        SEQ_printf(m, "------------------------------------\n", p->pid, p->comm);
        SEQ_printf(m, "[%d:%s] state:%c\n", p->pid, p->comm, state);
        list_for_each_entry(sem, &p->sem_head, sem_list){
            printk(m, "Sem Name:[%s]\nAddress:[0x%8x]\nCaller:[%pS]\n", sem->sem_name, sem, sem->caller);
        }
        if(state != 'R'){
            SEQ_printf(m, "Backtrace:\n");
            proc_stack(m, p);
        }
    } while_each_thread(g, p);
    read_unlock(&tasklist_lock);
    SEQ_printf(m, "[Semaphore onwer list End]---------\n");
}
static void lock_traverse(struct seq_file *m)
{
    struct task_struct *g, *p;
    struct mutex* lock;
    int i = 0;
    char state;
    read_lock(&tasklist_lock);
    SEQ_printf(m,"-----------------------\n");
    SEQ_printf(m,"[Mutex list]-----------\n");
    do_each_thread(g, p){
        /*
         * It's not reliable to print a task's held locks
         * if it's not sleeping (or if it's not the current
         * task):
         */
        #if 0
        if(p->hold_mutex == NULL)
            continue;
        printk("%3d[%d:%s]------------------------\n",i, p->pid, p->comm);

        lock = p->hold_mutex;
        do{
            lock = list_entry(lock->mutex_list.next, struct mutex, mutex_list);
            printk("[%s] - 0x%8x\n", lock->mutex_name, lock);
        }while(lock != p->hold_mutex);
        #endif

        if(list_empty(&p->mutex_head))
            continue;

        state = (p->state == 0) ? 'R' :
        (p->state < 0) ? 'U' :
        (p->state & TASK_UNINTERRUPTIBLE) ? 'D' :
        (p->state & TASK_STOPPED) ? 'T' :
        (p->state & TASK_TRACED) ? 'C' :
        (p->exit_state & EXIT_ZOMBIE) ? 'Z' :
        (p->exit_state & EXIT_DEAD) ? 'E' :
        (p->state & TASK_INTERRUPTIBLE) ? 'S' : '?';

        SEQ_printf(m, "------------------------------------\n", p->pid, p->comm);
        SEQ_printf(m, "[%d:%s] state:%c\n", p->pid, p->comm, state);
        list_for_each_entry(lock, &p->mutex_head, mutex_list){
            SEQ_printf(m, "Lock Name:[%s]\nAddress:[0x%8x]\nCaller:[%pS]\n", lock->mutex_name, lock, lock->caller);
        }
        if(state != 'R'){
            SEQ_printf(m, "Backtrace:\n");
            proc_stack(m, p);
        }
    } while_each_thread(g, p);
    read_unlock(&tasklist_lock);
    SEQ_printf(m, "[Mutex onwer list End]---------\n");
}
/*************/
//sample code 
#if 0
static DEFINE_SPINLOCK(mt_spin_lock);
static DEFINE_SEMAPHORE(mtprof_sem_static);
static struct semaphore* mtprof_sem_dyn;
static void sem_down()
{
    mtprof_sem_dyn = mt_sema_init(1);
    printk("down mtprof sem static...\n");
    down(&mtprof_sem_static);
    printk("down mtprof sem dyn..\n");
    down(mtprof_sem_dyn);
}
static void sem_up(){
    printk("up mtprof sem dyn..\n");
    up(mtprof_sem_dyn);
    printk("up mtprof sem static...\n");
    up(&mtprof_sem_static);
}
#endif
MT_DEBUG_ENTRY(locktb);
static int mt_locktb_show(struct seq_file *m, void *v)
{
    lock_traverse(m);
    printk("\n\n");
    sem_traverse(m);
    return 0; 
}
static ssize_t mt_locktb_write(struct file *filp, const char *ubuf,
	size_t cnt, loff_t *data)
{
    return cnt;
}
static int __init init_mtlock_prof(void)
{
    struct proc_dir_entry *pe;
    pe = proc_create("mtprof/locktb", 0666, NULL, &mt_locktb_fops);
    if (!pe)
        return -ENOMEM;
    return 0;
}
late_initcall(init_mtlock_prof);
