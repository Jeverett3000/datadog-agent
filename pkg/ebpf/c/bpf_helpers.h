#ifndef __BPF_HELPERS_H
#define __BPF_HELPERS_H

#include <linux/version.h>
#include <uapi/linux/bpf.h>

/* Macro to output debug logs to /sys/kernel/debug/tracing/trace_pipe
 */
#if DEBUG == 1
#define log_debug(fmt, ...)                                        \
    ({                                                             \
        char ____fmt[] = fmt;                                      \
        bpf_trace_printk(____fmt, sizeof(____fmt), ##__VA_ARGS__); \
    })
#else
// No op
#define log_debug(fmt, ...)
#endif

#ifndef __always_inline
#define __always_inline __attribute__((always_inline))
#endif

/* helper macro to place programs, maps, license in
 * different sections in elf_bpf file. Section names
 * are interpreted by elf_bpf loader
 */
#define SEC(NAME) __attribute__((section(NAME), used))

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"

/* helper functions called from eBPF programs written in C */
static void* (*bpf_map_lookup_elem)(void* map, void* key) = (void*)BPF_FUNC_map_lookup_elem;
static int (*bpf_map_update_elem)(void* map, void* key, void* value,
    unsigned long long flags)
    = (void*)BPF_FUNC_map_update_elem;
static int (*bpf_map_delete_elem)(void* map, void* key) = (void*)BPF_FUNC_map_delete_elem;
static int (*bpf_probe_read)(void* dst, int size, void* unsafe_ptr) = (void*)BPF_FUNC_probe_read;
static unsigned long long (*bpf_ktime_get_ns)(void) = (void*)BPF_FUNC_ktime_get_ns;
static int (*bpf_trace_printk)(const char* fmt, int fmt_size, ...) = (void*)BPF_FUNC_trace_printk;
static unsigned long long (*bpf_get_smp_processor_id)(void) = (void*)BPF_FUNC_get_smp_processor_id;
static unsigned long long (*bpf_get_current_pid_tgid)(void) = (void*)BPF_FUNC_get_current_pid_tgid;
static unsigned long long (*bpf_get_current_uid_gid)(void) = (void*)BPF_FUNC_get_current_uid_gid;
static int (*bpf_get_current_comm)(void* buf, int buf_size) = (void*)BPF_FUNC_get_current_comm;
static int (*bpf_perf_event_read)(void* map, int index) = (void*)BPF_FUNC_perf_event_read;
static int (*bpf_clone_redirect)(void* ctx, int ifindex, int flags) = (void*)BPF_FUNC_clone_redirect;
static int (*bpf_redirect)(int ifindex, int flags) = (void*)BPF_FUNC_redirect;
static int (*bpf_perf_event_output)(void* ctx, void* map,
    unsigned long long flags, void* data,
    int size)
    = (void*)BPF_FUNC_perf_event_output;
static int (*bpf_skb_get_tunnel_key)(void* ctx, void* key, int size, int flags) = (void*)BPF_FUNC_skb_get_tunnel_key;
static int (*bpf_skb_set_tunnel_key)(void* ctx, void* key, int size, int flags) = (void*)BPF_FUNC_skb_set_tunnel_key;
static unsigned long long (*bpf_get_prandom_u32)(void) = (void*)BPF_FUNC_get_prandom_u32;
static int (*bpf_skb_store_bytes)(void* ctx, int off, void* from, int len, int flags) = (void*)BPF_FUNC_skb_store_bytes;
static int (*bpf_l3_csum_replace)(void* ctx, int off, int from, int to, int flags) = (void*)BPF_FUNC_l3_csum_replace;
static int (*bpf_l4_csum_replace)(void* ctx, int off, int from, int to, int flags) = (void*)BPF_FUNC_l4_csum_replace;

/* at some point in kernel < 4.15, a duplicate bpf_tail_call symbol got included in the kernel headers.
   This breaks eBPF builds with a symbol redefinition error.
   It is fixed in 4.15 with commit https://github.com/torvalds/linux/commit/035226b964c820f65e201cdf123705a8f1d7c670
   Using bpf_tail_call_compat as a symbol name avoids this issue.
 */
static int (*bpf_tail_call_compat)(void* ctx, void* map, int key) = (void*)BPF_FUNC_tail_call;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0)
static long (*bpf_skb_load_bytes)(const void *skb, u32 offset, void *to, u32 len) = (void*)BPF_FUNC_skb_load_bytes;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0) || RHEL_MAJOR == 7
static u64 (*bpf_get_current_task)(void) = (void*)BPF_FUNC_get_current_task;
static int (*bpf_probe_write_user)(void *dst, const void *src, int size) = (void *) BPF_FUNC_probe_write_user;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0) || RHEL_MAJOR == 7
static int (*bpf_probe_read_str)(void* dst, int size, void* unsafe_ptr) = (void*)BPF_FUNC_probe_read_str;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 5, 0)
static int (*bpf_probe_read_user_str)(void* dst, int size, void* unsafe_ptr) = (void*)BPF_FUNC_probe_read_user_str;
static int (*bpf_probe_read_kernel_str)(void* dst, int size, void* unsafe_ptr) = (void*)BPF_FUNC_probe_read_kernel_str;
static int (*bpf_probe_read_user)(void* dst, int size, void* unsafe_ptr) = (void*)BPF_FUNC_probe_read_user;
static int (*bpf_probe_read_kernel)(void* dst, int size, void* unsafe_ptr) = (void*)BPF_FUNC_probe_read_kernel;
#else
/* cilium/ebpf loader will fixup bpf_probe_read_{user,kernel} call to bpf_probe_read on kernel older than 5.5.0
   but our runtime compilation would need to do that here as BPF_FUNC_xxx would not be defined in uapi headers
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
static int (*bpf_probe_read_user_str)(void* dst, int size, void* unsafe_ptr) = (void*)BPF_FUNC_probe_read_str;
static int (*bpf_probe_read_kernel_str)(void* dst, int size, void* unsafe_ptr) = (void*)BPF_FUNC_probe_read_str;
#endif
static int (*bpf_probe_read_user)(void* dst, int size, void* unsafe_ptr) = (void*)BPF_FUNC_probe_read;
static int (*bpf_probe_read_kernel)(void* dst, int size, void* unsafe_ptr) = (void*)BPF_FUNC_probe_read;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
static int (*bpf_ringbuf_output)(void *ringbuf, void *data, u64 size, u64 flags) = (void*)BPF_FUNC_ringbuf_output;
#endif

#pragma clang diagnostic pop

/* llvm builtin functions that eBPF C program may use to
 * emit BPF_LD_ABS and BPF_LD_IND instructions
 */
struct sk_buff;
unsigned long long load_byte(void* skb,
    unsigned long long off) asm("llvm.bpf.load.byte");
unsigned long long load_half(void* skb,
    unsigned long long off) asm("llvm.bpf.load.half");
unsigned long long load_word(void* skb,
    unsigned long long off) asm("llvm.bpf.load.word");

/* a helper structure used by eBPF C program
 * to describe map attributes to elf_bpf loader
 */
#define BUF_SIZE_MAP_NS 256

struct bpf_map_def {
    unsigned int type;
    unsigned int key_size;
    unsigned int value_size;
    unsigned int max_entries;
    unsigned int map_flags;
    unsigned int pinning;
    char namespace[BUF_SIZE_MAP_NS];
};

#define PT_REGS_STACK_PARM(x,n)                                     \
({                                                                  \
    unsigned long p = 0;                                            \
    bpf_probe_read(&p, sizeof(p), ((unsigned long *)x->sp) + n);    \
    p;                                                              \
})

#if defined(__x86_64__)

#define PT_REGS_PARM1(x) ((x)->di)
#define PT_REGS_PARM2(x) ((x)->si)
#define PT_REGS_PARM3(x) ((x)->dx)
#define PT_REGS_PARM4(x) ((x)->cx)
#define PT_REGS_PARM5(x) ((x)->r8)
#define PT_REGS_PARM6(x) ((x)->r9)
#define PT_REGS_PARM7(x) PT_REGS_STACK_PARM(x,1)
#define PT_REGS_PARM8(x) PT_REGS_STACK_PARM(x,2)
#define PT_REGS_PARM9(x) PT_REGS_STACK_PARM(x,3)
#define PT_REGS_RET(x) ((x)->sp)
#define PT_REGS_FP(x) ((x)->bp)
#define PT_REGS_RC(x) ((x)->ax)
#define PT_REGS_SP(x) ((x)->sp)
#define PT_REGS_IP(x) ((x)->ip)

#elif defined(__aarch64__)

#define PT_REGS_PARM1(x) ((x)->regs[0])
#define PT_REGS_PARM2(x) ((x)->regs[1])
#define PT_REGS_PARM3(x) ((x)->regs[2])
#define PT_REGS_PARM4(x) ((x)->regs[3])
#define PT_REGS_PARM5(x) ((x)->regs[4])
#define PT_REGS_PARM6(x) ((x)->regs[5])
#define PT_REGS_PARM7(x) ((x)->regs[6])
#define PT_REGS_PARM8(x) ((x)->regs[7])
#define PT_REGS_PARM9(x) PT_REGS_STACK_PARM(x,1)
#define PT_REGS_RET(x) ((x)->regs[30])
#define PT_REGS_FP(x) ((x)->regs[29]) /* Works only with CONFIG_FRAME_POINTER */
#define PT_REGS_RC(x) ((x)->regs[0])
#define PT_REGS_SP(x) ((x)->sp)
#define PT_REGS_IP(x) ((x)->pc)

#else
#error "Unsupported platform"
#endif

#define BPF_KPROBE_READ_RET_IP(ip, ctx) ({ bpf_probe_read(&(ip), sizeof(ip), (void*)PT_REGS_RET(ctx)); })
#define BPF_KRETPROBE_READ_RET_IP(ip, ctx) ({ bpf_probe_read(&(ip), sizeof(ip), \
                                                  (void*)(PT_REGS_FP(ctx) + sizeof(ip))); })

#endif
