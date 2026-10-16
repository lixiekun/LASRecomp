// visionOS host: no subprocesses, downloaded executable code, or process re-exec.
#include <dlfcn.h>
#include <errno.h>
#include <execinfo.h>
#include <ftw.h>
#include <mach-o/dyld.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/thread_status.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ucontext.h>
#include <unistd.h>
#include <SDL3/SDL.h>
#include "../platform.h"

static mach_timebase_info_data_t tb;

// Opt-in snapshots from inside our own process. No debugger attachment or launch delay.
static void *startup_diagnostics(void *opaque) {
    char *path = opaque;
    FILE *out = fopen(path, "w");
    free(path);
    if (!out) return NULL;
    for (int round = 0; round < 3; round++) {
        sleep(round ? 5 : 3);
        fprintf(out, "\n=== startup snapshot %d ===\n", round + 1);
        thread_act_array_t threads = NULL;
        mach_msg_type_number_t count = 0;
        if (task_threads(mach_task_self(), &threads, &count) != KERN_SUCCESS) break;
        for (mach_msg_type_number_t i = 0; i < count; i++) {
            arm_thread_state64_t state;
            mach_msg_type_number_t n = ARM_THREAD_STATE64_COUNT;
            if (thread_get_state(threads[i], ARM_THREAD_STATE64, (thread_state_t)&state, &n) == KERN_SUCCESS) {
                char name[64] = {0};
                pthread_t thread = pthread_from_mach_thread_np(threads[i]);
                if (thread) pthread_getname_np(thread, name, sizeof name);
                fprintf(out, "thread %u %s\n", i, name);
                uint64_t pc = state.__pc, fp = state.__fp;
                for (int frame = 0; frame < 12 && pc; frame++) {
                    Dl_info info = {0};
                    dladdr((void *)(uintptr_t)pc, &info);
                    fprintf(out, "  %llx %s+%llx\n", (unsigned long long)pc,
                            info.dli_sname ? info.dli_sname : "?",
                            info.dli_saddr ? (unsigned long long)(pc - (uintptr_t)info.dli_saddr) : 0);
                    uint64_t pair[2];
                    vm_size_t got = 0;
                    if (!fp || vm_read_overwrite(mach_task_self(), fp, sizeof pair, (vm_address_t)pair, &got) != KERN_SUCCESS || got != sizeof pair) break;
                    pc = pair[1];
                    if (pair[0] <= fp || pair[0] - fp > (256ull << 20)) break;
                    fp = pair[0];
                }
            }
            mach_port_deallocate(mach_task_self(), threads[i]);
        }
        vm_deallocate(mach_task_self(), (vm_address_t)threads, count * sizeof(thread_t));
        fflush(out);
    }
    fclose(out);
    return NULL;
}

void plat_init(void) {
    char *path = SDL_GetPrefPath("local", "LASVision");
    if (path) {
        setenv("LARECOMP_DATA_ROOT", path, 1);
        char log[2048];
        snprintf(log, sizeof log, "%s/lasvision.log", path);
        freopen(log, "a", stderr);
        if (getenv("LARECOMP_STARTUP_DIAGNOSTICS")) {
            char file[2048];
            snprintf(file, sizeof file, "%s/startup-threads.log", path);
            pthread_t thread;
            char *copy = strdup(file);
            if (pthread_create(&thread, NULL, startup_diagnostics, copy) == 0) pthread_detach(thread);
            else free(copy);
        }
        SDL_free(path);
    }
}

bool plat_reserve_guest(uint64_t lo, uint64_t hi) {
    vm_address_t address = lo;
    // FIXED without OVERWRITE fails safely if another mapping occupies the range.
    return vm_allocate(mach_task_self(), &address, hi - lo, VM_FLAGS_FIXED) == KERN_SUCCESS;
}

bool plat_reset_guest(uint64_t lo, uint64_t hi) {
    vm_address_t address = lo;
    return vm_allocate(mach_task_self(), &address, hi - lo,
                       VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE) == KERN_SUCCESS;
}

void plat_commit(uint64_t address, uint64_t size) { (void)address; (void)size; }
int plat_written_pages(uint64_t a, uint64_t s, uint64_t *p, int m, uint64_t *page) {
    (void)a; (void)s; (void)p; (void)m; (void)page; return -1;
}
int plat_protect(uint64_t a, uint64_t s, int prot) {
    int p = prot == PLAT_PROT_NONE ? PROT_NONE : prot == PLAT_PROT_READ ? PROT_READ : PROT_READ | PROT_WRITE;
    return mprotect((void *)(uintptr_t)a, s, p);
}
uint64_t plat_now_ns(void) {
    if (!tb.denom) mach_timebase_info(&tb);
    return mach_absolute_time() * tb.numer / tb.denom;
}
void plat_sleep_until_ns(uint64_t t) {
    if (!tb.denom) mach_timebase_info(&tb);
    mach_wait_until(t * tb.denom / tb.numer);
}
void plat_thread_realtime(uint64_t period) {
    (void)period;
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
}
bool plat_exe_path(char *buf, size_t size) {
    uint32_t n = (uint32_t)size; return _NSGetExecutablePath(buf, &n) == 0;
}
void plat_exec(char *const argv[]) { (void)argv; errno = ENOTSUP; }
const char *plat_symbol(uint64_t pc, uint64_t *offset) {
    Dl_info d = {0};
    if (!dladdr((void *)(uintptr_t)pc, &d) || !d.dli_sname) return NULL;
    if (offset) *offset = pc - (uint64_t)(uintptr_t)d.dli_saddr;
    return d.dli_sname;
}
void plat_fatal_notice(void) {}
void plat_print_backtrace(void) {
    void *bt[64]; int n = backtrace(bt, 64); backtrace_symbols_fd(bt, n, STDERR_FILENO);
}
static void crash_handler(int sig, siginfo_t *si, void *ctx) {
    ucontext_t *uc = ctx;
    uint64_t address = (uint64_t)(uintptr_t)si->si_addr;
    uint64_t pc = uc->uc_mcontext->__ss.__pc, lr = uc->uc_mcontext->__ss.__lr;
    if (rt_fault_watch(address, pc)) return;
    char what[32]; snprintf(what, sizeof what, "signal %d", sig);
    rt_fault_report(what, address, pc, lr);
}
void plat_crash_handler_install(void) {
    stack_t ss = {.ss_sp = malloc(1 << 20), .ss_size = 1 << 20};
    sigaltstack(&ss, NULL);
    struct sigaction sa = {0}; sa.sa_sigaction = crash_handler; sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigaction(SIGSEGV, &sa, NULL); sigaction(SIGBUS, &sa, NULL); sigaction(SIGILL, &sa, NULL);
}
PlatProc plat_spawn(const char *const argv[], const char *log, bool append) {
    (void)argv; (void)log; (void)append; errno = ENOTSUP; return 0;
}
int plat_proc_wait(PlatProc p, bool block) { (void)p; (void)block; return 255; }
void plat_proc_kill(PlatProc p) { (void)p; }
static int rm_entry(const char *p, const struct stat *s, int type, struct FTW *f) {
    (void)s; (void)type; (void)f; return remove(p);
}
bool plat_remove_tree(const char *p) { return nftw(p, rm_entry, 16, FTW_DEPTH | FTW_PHYS) == 0; }
