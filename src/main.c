#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/termios.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifndef CMD_MAX
#define CMD_MAX 512
#endif

#ifndef KVM_VERSION
#define KVM_VERSION "v1.0.1-dev"
#endif

typedef enum {
    MODE_PROCESS = 0,
    MODE_TREE,
    MODE_NETWORK,
    MODE_STORAGE
} display_mode_t;

// --- Data Structures ---
typedef struct {
    pid_t pid;
    pid_t tgid;
    uint64_t key; 

    uint64_t syscr;
    uint64_t syscw;
    uint64_t read_bytes;
    uint64_t write_bytes;
    uint64_t cpu_jiffies;
    uint64_t blkio_ticks;
    uint64_t start_time_ticks;
    
    char state;
    char user[32];

    uint64_t mem_virt_pages;
    uint64_t mem_res_pages;
    uint64_t mem_shr_pages;

    double cpu_pct;
    double r_iops;
    double w_iops;
    double io_wait_ms;
    double r_mib;
    double w_mib;

    char cmd[CMD_MAX];
} sample_t;

typedef struct {
    char name[32];
    unsigned long long rio;
    unsigned long long wio;
    unsigned long long rsect;
    unsigned long long wsect;
    
    double r_iops;
    double w_iops;
    double r_mib;
    double w_mib;
} disk_sample_t;

typedef struct {
    sample_t *data;
    size_t len;
    size_t cap;
} vec_t;

typedef struct {
    disk_sample_t *data;
    size_t len;
    size_t cap;
} vec_disk_t;

typedef struct {
    char name[32];
    char operstate[16]; 
    
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_errors;
    uint64_t tx_errors;

    int vmid;
    char vm_name[64];

    double rx_mbps;
    double tx_mbps;
    double rx_pps;
    double tx_pps;
    double rx_errs_ps;
    double tx_errs_ps;
} net_iface_t;

typedef struct {
    net_iface_t *data;
    size_t len;
    size_t cap;
} vec_net_t;

// --- Helper Functions ---

static void vec_init(vec_t *v) { v->data=NULL; v->len=0; v->cap=0; }
static void vec_free(vec_t *v) { free(v->data); v->data=NULL; v->len=0; v->cap=0; }
static void vec_push(vec_t *v, const sample_t *item) {
    if (v->len == v->cap) {
        size_t new_cap = v->cap ? v->cap * 2 : 4096;
        sample_t *p = (sample_t *)realloc(v->data, new_cap * sizeof(*p));
        if (!p) { fprintf(stderr, "OOM\n"); exit(2); }
        v->data = p;
        v->cap = new_cap;
    }
    v->data[v->len++] = *item;
}

static void vec_disk_init(vec_disk_t *v) { v->data=NULL; v->len=0; v->cap=0; }
static void vec_disk_free(vec_disk_t *v) { free(v->data); v->data=NULL; v->len=0; v->cap=0; }
static void vec_disk_push(vec_disk_t *v, const disk_sample_t *item) {
    if (v->len == v->cap) {
        size_t new_cap = v->cap ? v->cap * 2 : 64;
        disk_sample_t *p = (disk_sample_t *)realloc(v->data, new_cap * sizeof(*p));
        if (!p) { fprintf(stderr, "OOM\n"); exit(2); }
        v->data = p;
        v->cap = new_cap;
    }
    v->data[v->len++] = *item;
}

static void vec_net_init(vec_net_t *v) { v->data=NULL; v->len=0; v->cap=0; }
static void vec_net_free(vec_net_t *v) { free(v->data); v->data=NULL; v->len=0; v->cap=0; }
static void vec_net_push(vec_net_t *v, const net_iface_t *item) {
    if (v->len == v->cap) {
        size_t new_cap = v->cap ? v->cap * 2 : 64;
        net_iface_t *p = (net_iface_t *)realloc(v->data, new_cap * sizeof(*p));
        if (!p) { fprintf(stderr, "OOM\n"); exit(2); }
        v->data = p;
        v->cap = new_cap;
    }
    v->data[v->len++] = *item;
}

static uint64_t make_key(pid_t tid) {
    return (uint64_t)tid; 
}

static int is_numeric_str(const char *s) {
    if (!s || !*s) return 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p) if (!isdigit(*p)) return 0;
    return 1;
}

static double now_monotonic(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

// --- Terminal Handling ---
static struct termios orig_termios;
static int raw_mode_enabled = 0;

static void disable_raw_mode() {
    if (raw_mode_enabled) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        raw_mode_enabled = 0;
        printf("\033[?25h"); 
    }
}

static void enable_raw_mode() {
    if (!isatty(STDIN_FILENO)) return;
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    raw_mode_enabled = 1;
    printf("\033[?25l"); 
}

static int wait_for_input(double seconds) {
    struct timeval tv;
    tv.tv_sec = (long)seconds;
    tv.tv_usec = (long)((seconds - (double)tv.tv_sec) * 1e6);
    if (tv.tv_usec < 0) tv.tv_usec = 0;

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);

    int ret = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
    if (ret > 0) {
        unsigned char c;
        if (read(STDIN_FILENO, &c, 1) == 1) return c;
    }
    return 0; // Timeout
}

static int get_term_cols(void) {
    int cols = 120;
    if (isatty(STDOUT_FILENO)) {
        struct winsize ws;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) if (ws.ws_col > 0) cols = ws.ws_col;
    }
    return cols;
}

static void fprint_trunc(FILE *out, const char *s, int width) {
    if (width <= 0) return;
    int len = (int)strlen(s);
    if (len <= width) fprintf(out, "%-*s", width, s);
    else if (width <= 3) fprintf(out, "%.*s", width, s);
    else fprintf(out, "%.*s...", width - 3, s);
}

// --- File Reading Helpers ---

static int read_small_file(const char *path, char *buf, size_t buflen, ssize_t *nread_out) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    size_t n = fread(buf, 1, buflen - 1, f);
    fclose(f);
    buf[n] = '\0';
    if (nread_out) *nread_out = (ssize_t)n;
    return 0;
}

static void sanitize_cmd(char out[CMD_MAX], const char *in, size_t in_len) {
    size_t o = 0;
    int prev_space = 1;
    for (size_t i = 0; i < in_len && o + 1 < CMD_MAX; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c == '\0' || c == '\n' || c == '\r' || c == '\t') c = ' ';
        if (c == ' ') {
            if (prev_space) continue;
            prev_space = 1;
            out[o++] = ' ';
            continue;
        }
        prev_space = 0;
        if (c == '"') c = '\'';
        if (!isprint(c)) c = '?';
        out[o++] = (char)c;
    }
    while (o > 0 && out[o - 1] == ' ') o--;
    out[o] = '\0';
}

static int read_cmdline(pid_t pid, char out[CMD_MAX]) {
    char path[PATH_MAX], buf[8192];
    ssize_t n = 0;
    
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    if (read_small_file(path, buf, sizeof(buf), &n) == 0 && n > 0) {
        sanitize_cmd(out, buf, (size_t)n);
        if (out[0] != '\0' && out[0] != ' ') return 0;
    }

    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    if (read_small_file(path, buf, sizeof(buf), &n) == 0 && n > 0) {
        sanitize_cmd(out, buf, (size_t)n); 
        if (out[0] != '\0' && out[0] != ' ') return 0;
    }

    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    if (read_small_file(path, buf, sizeof(buf), &n) == 0 && n > 0) {
        char *start = strchr(buf, '(');
        char *end = strrchr(buf, ')');
        if (start && end && end > start) {
            size_t len = (size_t)(end - start - 1);
            if (len >= CMD_MAX) len = CMD_MAX - 1;
            strncpy(out, start + 1, len);
            out[len] = '\0';
            return 0;
        }
    }

    snprintf(out, CMD_MAX, "[%d]", pid);
    return -1;
}

static int read_io_file(const char *path, uint64_t *syscr, uint64_t *syscw, uint64_t *read_bytes, uint64_t *write_bytes) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "syscr:", 6) == 0) {
            *syscr = strtoull(line + 6, NULL, 10);
        } else if (strncmp(line, "syscw:", 6) == 0) {
            *syscw = strtoull(line + 6, NULL, 10);
        } else if (strncmp(line, "read_bytes:", 11) == 0) {
            *read_bytes = strtoull(line + 11, NULL, 10);
        } else if (strncmp(line, "write_bytes:", 12) == 0) {
            *write_bytes = strtoull(line + 12, NULL, 10);
        }
    }
    fclose(f);
    return 0;
}

static int read_proc_stat_fields(const char *path, uint64_t *cpu_jiffies_out, uint64_t *blkio_ticks_out, char *state_out, uint64_t *start_time_out) {
    char buf[4096]; ssize_t n = 0;
    if (read_small_file(path, buf, sizeof(buf), &n) != 0 || n <= 0) return -1;
    
    char *rparen = strrchr(buf, ')');
    if (!rparen) return -1;
    
    char *p = rparen + 2; 
    if (*p) *state_out = *p; else *state_out = '?';

    char *save = NULL; char *tok = strtok_r(p, " ", &save);
    int idx = 0; 
    uint64_t utime=0, stime=0;
    *blkio_ticks_out = 0;
    *start_time_out = 0;
    
    while (tok) {
        if (idx == 11) utime = strtoull(tok, NULL, 10); 
        else if (idx == 12) stime = strtoull(tok, NULL, 10);
        else if (idx == 19) *start_time_out = strtoull(tok, NULL, 10);
        else if (idx == 39) { 
            *blkio_ticks_out = strtoull(tok, NULL, 10);
            break; 
        }
        idx++; tok = strtok_r(NULL, " ", &save);
    }
    *cpu_jiffies_out = utime + stime;
    return 0;
}

static void read_statm(pid_t pid, uint64_t *virt, uint64_t *res, uint64_t *shr) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/statm", pid);
    char buf[256]; ssize_t n;
    if (read_small_file(path, buf, sizeof(buf), &n) == 0 && n > 0) {
        unsigned long long v=0, r=0, s=0;
        if (sscanf(buf, "%llu %llu %llu", &v, &r, &s) >= 2) {
            *virt = v; *res = r; *shr = s;
            return;
        }
    }
    *virt = 0; *res = 0; *shr = 0;
}

static void get_proc_user(pid_t pid, char *out, size_t size) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d", pid);
    struct stat st;
    if (stat(path, &st) == 0) {
        struct passwd *pw = getpwuid(st.st_uid);
        if (pw) {
            strncpy(out, pw->pw_name, size-1);
            out[size-1]='\0';
            return;
        }
        snprintf(out, size, "%d", st.st_uid);
    } else {
        snprintf(out, size, "?");
    }
}

static void read_operstate(const char *ifname, char *buf, size_t buflen) {
    char path[256];
    snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", ifname);
    ssize_t n;
    if (read_small_file(path, buf, buflen, &n) == 0 && n > 0) {
        if (buf[n-1] == '\n') buf[n-1] = '\0';
    } else {
        strncpy(buf, "?", buflen);
    }
}

// --- System Stats ---

static int read_system_disk_iops(uint64_t *r_iops, uint64_t *w_iops) {
    FILE *f = fopen("/proc/diskstats", "r");
    if (!f) return -1;
    
    char line[512];
    uint64_t tr = 0, tw = 0;
    
    while (fgets(line, sizeof(line), f)) {
        int major, minor;
        char name[64];
        unsigned long long rio, rmerge, rsect, ruse;
        unsigned long long wio, wmerge, wsect, wuse;
        
        if (sscanf(line, "%d %d %s %llu %llu %llu %llu %llu %llu %llu %llu",
                   &major, &minor, name,
                   &rio, &rmerge, &rsect, &ruse,
                   &wio, &wmerge, &wsect, &wuse) == 11) {
            
            if (strncmp(name, "sd", 2) == 0 || 
                strncmp(name, "vd", 2) == 0 ||
                strncmp(name, "nvme", 4) == 0 ||
                strncmp(name, "xvd", 3) == 0) {
                tr += rio;
                tw += wio;
            }
        }
    }
    fclose(f);
    *r_iops = tr;
    *w_iops = tw;
    return 0;
}

static int collect_disks(vec_disk_t *out) {
    FILE *f = fopen("/proc/diskstats", "r");
    if (!f) return -1;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        int major, minor;
        char name[64];
        unsigned long long rio, rmerge, rsect, ruse;
        unsigned long long wio, wmerge, wsect, wuse;
        if (sscanf(line, "%d %d %s %llu %llu %llu %llu %llu %llu %llu %llu",
                   &major, &minor, name,
                   &rio, &rmerge, &rsect, &ruse,
                   &wio, &wmerge, &wsect, &wuse) == 11) {
            if (strncmp(name, "loop", 4) == 0 || strncmp(name, "ram", 3) == 0) continue;
            
            disk_sample_t ds; memset(&ds, 0, sizeof(ds));
            strncpy(ds.name, name, sizeof(ds.name)-1);
            ds.rio = rio; ds.wio = wio;
            ds.rsect = rsect; ds.wsect = wsect;
            vec_disk_push(out, &ds);
        }
    }
    fclose(f);
    return 0;
}

static int collect_net_dev(vec_net_t *out) {
    FILE *f = fopen("/proc/net/dev", "r");
    if (!f) return -1;
    char line[512];
    fgets(line, sizeof(line), f);
    fgets(line, sizeof(line), f);

    while (fgets(line, sizeof(line), f)) {
        net_iface_t ni; memset(&ni, 0, sizeof(ni));
        char *colon = strchr(line, ':');
        if (!colon) continue;
        *colon = '\0';
        
        char *name_start = line;
        while (isspace(*name_start)) name_start++;
        strncpy(ni.name, name_start, sizeof(ni.name)-1);

        char *stats = colon + 1;
        unsigned long long rb, rp, re, rd, rf, rfr, rc, rm;
        unsigned long long tb, tp, te, td, tf, tco, tca, tc;
        
        if (sscanf(stats, "%llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                   &rb, &rp, &re, &rd, &rf, &rfr, &rc, &rm,
                   &tb, &tp, &te, &td, &tf, &tco, &tca, &tc) >= 10) {
            ni.rx_bytes = rb; ni.rx_packets = rp; ni.rx_errors = re;
            ni.tx_bytes = tb; ni.tx_packets = tp; ni.tx_errors = te;
        }

        read_operstate(ni.name, ni.operstate, sizeof(ni.operstate));
        vec_net_push(out, &ni);
    }
    fclose(f);
    return 0;
}

static void map_kvm_interfaces(vec_net_t *nets) {
    DIR *proc = opendir("/proc");
    if (!proc) return;
    struct dirent *de;
    static char cmd[131072]; 

    while ((de = readdir(proc)) != NULL) {
        if (!is_numeric_str(de->d_name)) continue;
        pid_t pid = atoi(de->d_name);
        
        char path[256];
        snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
        
        ssize_t n;
        if (read_small_file(path, cmd, sizeof(cmd), &n) != 0) continue;
        
        for (ssize_t i=0; i<n; i++) if (cmd[i]=='\0') cmd[i]=' ';
        cmd[n-1]='\0';

        if (strstr(cmd, "kvm") || strstr(cmd, "qemu")) {
            int vmid = -1;
            char *id_ptr = strstr(cmd, " -id ");
            if (id_ptr) sscanf(id_ptr+5, "%d", &vmid);

            char vmname[64] = {0};
            char *name_ptr = strstr(cmd, " -name ");
            if (name_ptr) {
                char *start = name_ptr + 7;
                char *end = strpbrk(start, " ,");
                if (end) {
                    size_t len = (size_t)(end - start);
                    if (len >= sizeof(vmname)) len = sizeof(vmname)-1;
                    strncpy(vmname, start, len);
                } else {
                    strncpy(vmname, start, sizeof(vmname)-1);
                }
            }

            char *net_ptr = cmd;
            while ((net_ptr = strstr(net_ptr, "ifname=")) != NULL) {
                char *start = net_ptr + 7;
                char *end = strpbrk(start, " ,");
                char ifname[32] = {0};
                size_t len = end ? (size_t)(end - start) : strlen(start);
                if (len >= sizeof(ifname)) len = sizeof(ifname)-1;
                strncpy(ifname, start, len);

                for (size_t i=0; i<nets->len; i++) {
                    if (strcmp(nets->data[i].name, ifname) == 0) {
                        nets->data[i].vmid = vmid;
                        strncpy(nets->data[i].vm_name, vmname, sizeof(nets->data[i].vm_name)-1);
                    }
                }
                if (end) net_ptr = end; else break;
            }
        }
    }
    closedir(proc);
}

// ... Process Collection ...

static int pid_in_filter(pid_t pid, const pid_t *filter, size_t n) {
    if (!filter || n == 0) return 1;
    for (size_t i = 0; i < n; i++) if (filter[i] == pid) return 1;
    return 0;
}

static int collect_samples(vec_t *out, const pid_t *filter_pids, size_t filter_n) {
    DIR *proc = opendir("/proc");
    if (!proc) { perror("opendir(/proc)"); return -1; }
    struct dirent *de;
    
    while ((de = readdir(proc)) != NULL) {
        if (!is_numeric_str(de->d_name)) continue;
        pid_t pid = (pid_t)atoi(de->d_name); // This is the TGID
        
        // Filter by TGID (Process ID)
        if (filter_n > 0 && !pid_in_filter(pid, filter_pids, filter_n)) continue;

        char cmd[CMD_MAX];
        read_cmdline(pid, cmd);

        char taskdir_path[PATH_MAX];
        snprintf(taskdir_path, sizeof(taskdir_path), "/proc/%d/task", pid);
        DIR *taskdir = opendir(taskdir_path);
        
        if (taskdir) {
            struct dirent *te;
            while ((te = readdir(taskdir)) != NULL) {
                if (!is_numeric_str(te->d_name)) continue;
                pid_t tid = (pid_t)atoi(te->d_name);
                
                sample_t s; memset(&s, 0, sizeof(s));
                s.pid = tid; 
                s.tgid = pid;
                s.key = make_key(tid);
                snprintf(s.cmd, sizeof(s.cmd), "%s", cmd); 

                char io_path[PATH_MAX], stat_path[PATH_MAX];
                snprintf(io_path, sizeof(io_path), "/proc/%d/task/%d/io", pid, tid);
                snprintf(stat_path, sizeof(stat_path), "/proc/%d/task/%d/stat", pid, tid);
                
                read_io_file(io_path, &s.syscr, &s.syscw, &s.read_bytes, &s.write_bytes);
                read_proc_stat_fields(stat_path, &s.cpu_jiffies, &s.blkio_ticks, &s.state, &s.start_time_ticks);
                read_statm(tid, &s.mem_virt_pages, &s.mem_res_pages, &s.mem_shr_pages);
                get_proc_user(pid, s.user, sizeof(s.user));

                vec_push(out, &s);
            }
            closedir(taskdir);
        } else {
            // Fallback
            sample_t s; memset(&s, 0, sizeof(s));
            s.pid = pid; 
            s.tgid = pid;
            s.key = make_key(pid);
            snprintf(s.cmd, sizeof(s.cmd), "%s", cmd);

            char io_path[PATH_MAX], stat_path[PATH_MAX];
            snprintf(io_path, sizeof(io_path), "/proc/%d/io", pid);
            snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", pid);
            
            read_io_file(io_path, &s.syscr, &s.syscw, &s.read_bytes, &s.write_bytes);
            read_proc_stat_fields(stat_path, &s.cpu_jiffies, &s.blkio_ticks, &s.state, &s.start_time_ticks);
            read_statm(pid, &s.mem_virt_pages, &s.mem_res_pages, &s.mem_shr_pages);
            get_proc_user(pid, s.user, sizeof(s.user));

            vec_push(out, &s);
        }
    }
    closedir(proc);
    return 0;
}

static int cmp_key(const void *a, const void *b) {
    const sample_t *x = (const sample_t *)a;
    const sample_t *y = (const sample_t *)b;
    return (x->key > y->key) - (x->key < y->key);
}

static const sample_t *find_prev(const vec_t *prev, uint64_t key) {
    size_t lo = 0, hi = prev->len;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        uint64_t k = prev->data[mid].key;
        if (k == key) return &prev->data[mid];
        if (k < key) lo = mid + 1;
        else hi = mid;
    }
    return NULL;
}

// --- Global Sort State ---
static int sort_desc = 1;

// Sort Comparators
typedef enum { 
    SORT_PID=1, SORT_CPU, SORT_LOG_R, SORT_LOG_W, SORT_WAIT, SORT_RMIB, SORT_WMIB,
    SORT_NET_RX, SORT_NET_TX,
    SORT_MEM_RES, SORT_MEM_SHR, SORT_MEM_VIRT, SORT_USER, SORT_UPTIME 
} sort_col_t;

// Helper macro for numeric comparison
#define CMP_NUM(a, b) (sort_desc ? ((a) < (b) ? 1 : ((a) > (b) ? -1 : 0)) : ((a) > (b) ? 1 : ((a) < (b) ? -1 : 0)))

static int cmp_pid(const void *a, const void *b) {
    const sample_t *x = (const sample_t *)a;
    const sample_t *y = (const sample_t *)b;
    return CMP_NUM(x->pid, y->pid); 
}
static int cmp_cpu(const void *a, const void *b) {
    const sample_t *x = (const sample_t *)a;
    const sample_t *y = (const sample_t *)b;
    return CMP_NUM(x->cpu_pct, y->cpu_pct);
}
static int cmp_logr(const void *a, const void *b) {
    const sample_t *x = (const sample_t *)a;
    const sample_t *y = (const sample_t *)b;
    return CMP_NUM(x->r_iops, y->r_iops);
}
static int cmp_logw(const void *a, const void *b) {
    const sample_t *x = (const sample_t *)a;
    const sample_t *y = (const sample_t *)b;
    return CMP_NUM(x->w_iops, y->w_iops);
}
static int cmp_wait(const void *a, const void *b) {
    const sample_t *x = (const sample_t *)a;
    const sample_t *y = (const sample_t *)b;
    return CMP_NUM(x->io_wait_ms, y->io_wait_ms);
}
static int cmp_rmib(const void *a, const void *b) {
    const sample_t *x = (const sample_t *)a;
    const sample_t *y = (const sample_t *)b;
    return CMP_NUM(x->r_mib, y->r_mib);
}
static int cmp_wmib(const void *a, const void *b) {
    const sample_t *x = (const sample_t *)a;
    const sample_t *y = (const sample_t *)b;
    return CMP_NUM(x->w_mib, y->w_mib);
}
static int cmp_net_rx(const void *a, const void *b) {
    const net_iface_t *x = (const net_iface_t *)a;
    const net_iface_t *y = (const net_iface_t *)b;
    return CMP_NUM(x->rx_mbps, y->rx_mbps);
}
static int cmp_net_tx(const void *a, const void *b) {
    const net_iface_t *x = (const net_iface_t *)a;
    const net_iface_t *y = (const net_iface_t *)b;
    return CMP_NUM(x->tx_mbps, y->tx_mbps);
}
static int cmp_disk_rio(const void *a, const void *b) {
    const disk_sample_t *x = (const disk_sample_t *)a;
    const disk_sample_t *y = (const disk_sample_t *)b;
    return CMP_NUM(x->r_iops, y->r_iops);
}

static int cmp_tgid(const void *a, const void *b) {
    const sample_t *x = (const sample_t *)a;
    const sample_t *y = (const sample_t *)b;
    return (x->tgid > y->tgid) - (x->tgid < y->tgid);
}

// Aggregate threads into process-level stats
static void aggregate_by_tgid(const vec_t *src, vec_t *dst) {
    vec_init(dst);
    // 1. Deep copy
    for (size_t i=0; i<src->len; i++) {
        vec_push(dst, &src->data[i]);
    }
    
    // 2. Sort by TGID
    qsort(dst->data, dst->len, sizeof(sample_t), cmp_tgid);

    // 3. Merge adjacent with same TGID
    size_t write_idx = 0;
    if (dst->len > 0) {
        for (size_t i = 1; i < dst->len; i++) {
            if (dst->data[write_idx].tgid == dst->data[i].tgid) {
                // Merge i into write_idx
                dst->data[write_idx].cpu_pct += dst->data[i].cpu_pct;
                dst->data[write_idx].r_iops += dst->data[i].r_iops;
                dst->data[write_idx].w_iops += dst->data[i].w_iops;
                dst->data[write_idx].io_wait_ms += dst->data[i].io_wait_ms;
                dst->data[write_idx].r_mib += dst->data[i].r_mib;
                dst->data[write_idx].w_mib += dst->data[i].w_mib;
                
                dst->data[write_idx].pid = dst->data[write_idx].tgid; 
                dst->data[write_idx].state = dst->data[i].state; 
            } else {
                write_idx++;
                dst->data[write_idx] = dst->data[i];
                dst->data[write_idx].pid = dst->data[i].tgid; // Ensure PID column shows TGID
            }
        }
        dst->len = write_idx + 1;
    }
}

// Tree view helper
static void print_threads_for_tgid(const vec_t *raw, pid_t tgid, int pidw, int cpuw, int iopsw, int waitw, int mibw, int cmdw) {
    for (size_t i = 0; i < raw->len; i++) {
        const sample_t *s = &raw->data[i];
        if (s->tgid == tgid && s->pid != tgid) { 
            char pidbuf[32];
            snprintf(pidbuf, sizeof(pidbuf), "  └─ %d", s->pid); // Indent
            
            printf("%*s %*.*f %*.*f %*.*f %*.*f %*.*f %*.*f %c ",
                pidw, pidbuf,
                cpuw, 2, s->cpu_pct,
                iopsw, 2, s->r_iops,
                iopsw, 2, s->w_iops,
                waitw, 2, s->io_wait_ms,
                mibw, 2, s->r_mib,
                mibw, 2, s->w_mib,
                s->state);
            fprint_trunc(stdout, s->cmd, cmdw);
            putchar('\n');
        }
    }
}

int main(int argc, char **argv) {
    if (geteuid() != 0) {
        fprintf(stderr, "Warning: Not running as root. IO stats will be unavailable for other users' processes.\n");
        sleep(2);
    }

    double interval = 5.0; 
    int display_limit = 50;
    int show_tree = 0;
    int frozen = 0;
    char filter_str[64] = {0};
    int in_filter_mode = 0;
    
    int in_limit_mode = 0;
    char limit_str[16] = {0};

    display_mode_t mode = MODE_PROCESS;
    
    pid_t *filter = NULL;
    size_t filter_n = 0, filter_cap = 0;

    static const struct option long_opts[] = {
        {"interval", required_argument, NULL, 'i'},
        {"pid", required_argument, NULL, 'p'},
        {"help", no_argument, NULL, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "i:p:h", long_opts, NULL)) != -1) {
        switch (opt) {
            case 'i': interval = strtod(optarg, NULL); if (interval <= 0) return 2; break;
            case 'p': {
                long v = strtol(optarg, NULL, 10);
                if (filter_n == filter_cap) {
                    filter_cap = filter_cap ? filter_cap * 2 : 8;
                    filter = realloc(filter, filter_cap * sizeof(*filter));
                }
                filter[filter_n++] = (pid_t)v;
                break;
            }
            case 'h': default: return 0;
        }
    }

    long hz = sysconf(_SC_CLK_TCK);
    vec_t prev, curr_raw, curr_proc;
    vec_init(&prev); vec_init(&curr_raw); vec_init(&curr_proc);
    
    vec_net_t prev_net, curr_net;
    vec_net_init(&prev_net); vec_net_init(&curr_net);

    vec_disk_t prev_disk, curr_disk;
    vec_disk_init(&prev_disk); vec_disk_init(&curr_disk);

    // System Disk Stats
    uint64_t prev_sys_r=0, prev_sys_w=0;
    uint64_t curr_sys_r=0, curr_sys_w=0;
    read_system_disk_iops(&prev_sys_r, &prev_sys_w);

    printf("Initializing (wait %.0fs)...\n", interval);
    
    if (collect_samples(&prev, filter, filter_n) != 0) return 1;
    collect_net_dev(&prev_net);
    collect_disks(&prev_disk);

    qsort(prev.data, prev.len, sizeof(sample_t), cmp_key);
    double t_prev = now_monotonic();
    
    double sys_r_iops = 0, sys_w_iops = 0;

    enable_raw_mode();
    sort_col_t sort_col_proc = SORT_CPU;
    sort_col_t sort_col_net = SORT_NET_TX;

    while (1) {
        double t_curr = 0;
        
        if (!frozen) {
            vec_free(&curr_raw); vec_init(&curr_raw);
            collect_samples(&curr_raw, filter, filter_n);
            
            vec_net_free(&curr_net); vec_net_init(&curr_net);
            collect_net_dev(&curr_net);
            map_kvm_interfaces(&curr_net);

            vec_disk_free(&curr_disk); vec_disk_init(&curr_disk);
            collect_disks(&curr_disk);

            read_system_disk_iops(&curr_sys_r, &curr_sys_w);

            t_curr = now_monotonic();
            double dt = t_curr - t_prev;
            if (dt <= 0) dt = interval;

            sys_r_iops = (double)(curr_sys_r - prev_sys_r) / dt;
            sys_w_iops = (double)(curr_sys_w - prev_sys_w) / dt;

            // Process Metrics
            for (size_t i=0; i<curr_raw.len; i++) {
                sample_t *c = &curr_raw.data[i];
                const sample_t *p = find_prev(&prev, c->key);
                uint64_t d_cpu=0, d_scr=0, d_scw=0, d_rb=0, d_wb=0, d_blk=0;
                if (p) {
                    d_cpu = (c->cpu_jiffies >= p->cpu_jiffies) ? c->cpu_jiffies - p->cpu_jiffies : 0;
                    d_scr = (c->syscr >= p->syscr) ? c->syscr - p->syscr : 0;
                    d_scw = (c->syscw >= p->syscw) ? c->syscw - p->syscw : 0;
                    d_rb  = (c->read_bytes >= p->read_bytes) ? c->read_bytes - p->read_bytes : 0;
                    d_wb  = (c->write_bytes >= p->write_bytes) ? c->write_bytes - p->write_bytes : 0;
                    d_blk = (c->blkio_ticks >= p->blkio_ticks) ? c->blkio_ticks - p->blkio_ticks : 0;
                }
                c->cpu_pct = ((double)d_cpu * 100.0) / (dt * (double)hz);
                c->r_iops = (double)d_scr / dt;
                c->w_iops = (double)d_scw / dt;
                c->r_mib  = ((double)d_rb / dt) / 1048576.0;
                c->w_mib  = ((double)d_wb / dt) / 1048576.0;
                c->io_wait_ms = ((double)d_blk * 1000.0) / (double)hz; 
            }

            // Net Metrics
            for (size_t i=0; i<curr_net.len; i++) {
                net_iface_t *cn = &curr_net.data[i];
                net_iface_t *pn = NULL;
                for(size_t j=0; j<prev_net.len; j++) {
                    if (strcmp(prev_net.data[j].name, cn->name) == 0) {
                        pn = &prev_net.data[j];
                        break;
                    }
                }
                if (pn) {
                    uint64_t dr = (cn->rx_bytes >= pn->rx_bytes) ? cn->rx_bytes - pn->rx_bytes : 0;
                    uint64_t dtb = (cn->tx_bytes >= pn->tx_bytes) ? cn->tx_bytes - pn->tx_bytes : 0;
                    uint64_t dp_r = (cn->rx_packets >= pn->rx_packets) ? cn->rx_packets - pn->rx_packets : 0;
                    uint64_t dp_t = (cn->tx_packets >= pn->tx_packets) ? cn->tx_packets - pn->tx_packets : 0;
                    uint64_t de_r = (cn->rx_errors >= pn->rx_errors) ? cn->rx_errors - pn->rx_errors : 0;
                    uint64_t de_t = (cn->tx_errors >= pn->tx_errors) ? cn->tx_errors - pn->tx_errors : 0;

                    cn->rx_mbps = ((double)dr * 8.0) / (dt * 1000000.0);
                    cn->tx_mbps = ((double)dtb * 8.0) / (dt * 1000000.0);
                    cn->rx_pps = (double)dp_r / dt;
                    cn->tx_pps = (double)dp_t / dt;
                    cn->rx_errs_ps = (double)de_r / dt;
                    cn->tx_errs_ps = (double)de_t / dt;
                }
            }

            // Disk Metrics
            for (size_t i=0; i<curr_disk.len; i++) {
                disk_sample_t *cd = &curr_disk.data[i];
                disk_sample_t *pd = NULL;
                for(size_t j=0; j<prev_disk.len; j++) {
                    if (strcmp(prev_disk.data[j].name, cd->name) == 0) {
                        pd = &prev_disk.data[j];
                        break;
                    }
                }
                if (pd) {
                    uint64_t drio = (cd->rio >= pd->rio) ? cd->rio - pd->rio : 0;
                    uint64_t dwio = (cd->wio >= pd->wio) ? cd->wio - pd->wio : 0;
                    uint64_t drs  = (cd->rsect >= pd->rsect) ? cd->rsect - pd->rsect : 0;
                    uint64_t dws  = (cd->wsect >= pd->wsect) ? cd->wsect - pd->wsect : 0;
                    
                    cd->r_iops = (double)drio / dt;
                    cd->w_iops = (double)dwio / dt;
                    cd->r_mib  = ((double)drs * 512.0) / (dt * 1048576.0);
                    cd->w_mib  = ((double)dws * 512.0) / (dt * 1048576.0);
                }
            }

            vec_free(&curr_proc); 
            aggregate_by_tgid(&curr_raw, &curr_proc);
            
            t_prev = t_curr;
            prev_sys_r = curr_sys_r;
            prev_sys_w = curr_sys_w;
        }

        int dirty = 1;
        double start_wait = now_monotonic();

        while (1) {
            if (dirty) {
                printf("\033[2J\033[H"); 
                int cols = get_term_cols();
                
                char left[128], right[128];
                snprintf(left, sizeof(left), "kvmtop %s", KVM_VERSION);
                
                if (in_filter_mode) {
                    snprintf(right, sizeof(right), "FILTER: %s_", filter_str);
                } else if (in_limit_mode) {
                    snprintf(right, sizeof(right), "LIMIT: %s_", limit_str);
                } else {
                    // Normal header
                    char f_info[40] = "";
                    if (strlen(filter_str) > 0) snprintf(f_info, sizeof(f_info), "Filter: %s | ", filter_str);
                    
                    snprintf(right, sizeof(right), "%sRefresh=%.1fs | [c] CPU | [s] Storage | [n] Net | [t] Tree | [l] Limit(%d) | [f] Freeze: %s | [/] Filter | [q] Quit", 
                             f_info, interval, display_limit, frozen ? "ON" : "OFF");
                }
                
                int pad = cols - (int)strlen(left) - (int)strlen(right);
                if (pad < 1) pad = 1;
                printf("%s%*s%s\n", left, pad, "", right);

                printf("System IOPS: Read %.0f | Write %.0f\n", sys_r_iops, sys_w_iops);

                if (mode == MODE_NETWORK) {
                    if (sort_col_net == SORT_NET_RX)
                        qsort(curr_net.data, curr_net.len, sizeof(net_iface_t), cmp_net_rx);
                    else
                        qsort(curr_net.data, curr_net.len, sizeof(net_iface_t), cmp_net_tx);

                    int namew=16, statw=10, ratew=12, pktw=10, errw=8;
                    char h_rx[32], h_tx[32];
                    snprintf(h_rx, 32, "[1] %s", "RX_Mbps");
                    snprintf(h_tx, 32, "[2] %s", "TX_Mbps");

                    printf("%*s %*s %*s %*s %*s %*s %*s %*s %-6s %s\n",
                        namew, "IFACE", statw, "STATE", 
                        ratew, h_rx, ratew, h_tx,
                        pktw, "RX_Pkts", pktw, "TX_Pkts",
                        errw, "RX_Err", errw, "TX_Err",
                        "VMID", "VM_NAME");
                    for(int i=0; i<cols; i++) putchar('-'); putchar('\n');

                    int count = 0;
                    for(size_t i=0; i<curr_net.len && count < 50; i++) {
                        net_iface_t *n = &curr_net.data[i];
                        if (strncmp(n->name, "fw", 2) == 0 || strcmp(n->name, "lo")==0) continue;

                        char vmid_buf[16] = "-";
                        if (n->vmid > 0) snprintf(vmid_buf, sizeof(vmid_buf), "%d", n->vmid);

                        // FILTER CHECK
                        if (strlen(filter_str) > 0) {
                            if (!strcasestr(n->name, filter_str) && 
                                !strcasestr(n->operstate, filter_str) &&
                                !strcasestr(vmid_buf, filter_str) &&
                                !strcasestr(n->vm_name, filter_str)) continue;
                        }

                        printf("%*s %*s %*.*f %*.*f %*.*f %*.*f %*.*f %*.*f %-6s %s\n",
                            namew, n->name, statw, n->operstate,
                            ratew, 2, n->rx_mbps, ratew, 2, n->tx_mbps,
                            pktw, 0, n->rx_pps, pktw, 0, n->tx_pps,
                            errw, 0, n->rx_errs_ps, errw, 0, n->tx_errs_ps,
                            vmid_buf, n->vm_name);
                        count++;
                    }
                } else if (mode == MODE_STORAGE) {
                    qsort(curr_disk.data, curr_disk.len, sizeof(disk_sample_t), cmp_disk_rio); 

                    int devw=16, iopsw=12, mibw=12;
                    char h_ri[32], h_wi[32], h_rm[32], h_wm[32];
                    snprintf(h_ri, 32, "R_IOPS");
                    snprintf(h_wi, 32, "W_IOPS");
                    snprintf(h_rm, 32, "R_MiB/s");
                    snprintf(h_wm, 32, "W_MiB/s");

                    printf("%*s %*s %*s %*s %*s\n",
                        devw, "DEVICE", iopsw, h_ri, iopsw, h_wi, mibw, h_rm, mibw, h_wm);
                    
                    for(int i=0; i<cols; i++) putchar('-'); putchar('\n');

                    for (size_t i=0; i<curr_disk.len; i++) {
                        const disk_sample_t *d = &curr_disk.data[i];
                        if (strlen(filter_str) > 0 && !strcasestr(d->name, filter_str)) continue;

                        printf("%*s %*.*f %*.*f %*.*f %*.*f\n",
                            devw, d->name,
                            iopsw, 2, d->r_iops,
                            iopsw, 2, d->w_iops,
                            mibw, 2, d->r_mib,
                            mibw, 2, d->w_mib);
                    }
                } else { // MODE_PROCESS
                    vec_t *view_list = &curr_proc; 

                    switch(sort_col_proc) {
                        case SORT_PID: qsort(view_list->data, view_list->len, sizeof(sample_t), cmp_pid); break;
                        case SORT_CPU: qsort(view_list->data, view_list->len, sizeof(sample_t), cmp_cpu); break;
                        case SORT_LOG_R: qsort(view_list->data, view_list->len, sizeof(sample_t), cmp_logr); break;
                        case SORT_LOG_W: qsort(view_list->data, view_list->len, sizeof(sample_t), cmp_logw); break;
                        case SORT_WAIT: qsort(view_list->data, view_list->len, sizeof(sample_t), cmp_wait); break;
                        case SORT_RMIB: qsort(view_list->data, view_list->len, sizeof(sample_t), cmp_rmib); break;
                        case SORT_WMIB: qsort(view_list->data, view_list->len, sizeof(sample_t), cmp_wmib); break;
                        default: qsort(view_list->data, view_list->len, sizeof(sample_t), cmp_cpu); break;
                    }

                    // Column Widths
                    int pidw = 10, cpuw = 8, memw = 10, userw = 10, uptimew=10, statew = 3, iopsw=10, waitw=8, mibw=10;
                    
                    // Headers
                    // Order: PID, User, Uptime, Res, Shr, Virt, R_Log, W_Log, Wait, R_MiB, W_MiB, CPU, State, COMMAND
                    
                    int fixed_width = pidw + 1 + cpuw + 1 + 
                                      memw + 1 + memw + 1 + memw + 1 + 
                                      uptimew + 1 + userw + 1 + 
                                      iopsw + 1 + iopsw + 1 + 
                                      waitw + 1 + 
                                      mibw + 1 + mibw + 1 + 
                                      statew + 1;
                                      
                    int cmdw = cols - fixed_width; 
                    if (cmdw < 10) cmdw = 10;

                    printf("%*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %s\n",
                        pidw, "[1] PID",
                        userw, "User",
                        uptimew, "Uptime",
                        memw, "Res(MiB)",
                        memw, "Shr(MiB)",
                        memw, "Virt(MiB)",
                        iopsw, "[3] R_Log",
                        iopsw, "[4] W_Log",
                        waitw, "[5] Wait",
                        mibw, "[6] R_MiB",
                        mibw, "[7] W_MiB",
                        cpuw, "[2] CPU%",
                        statew, "[8] S",
                        "COMMAND LINE"
                    );
                    
                    for(int i=0; i<cols; i++) putchar('-');
                    putchar('\n');

                    // Calc totals
                    double t_cpu=0, t_ri=0, t_wi=0, t_rm=0, t_wm=0, t_wt=0;
                    double t_res=0, t_shr=0, t_virt=0;
                    
                    for(size_t i=0; i<curr_raw.len; i++) {
                        t_cpu += curr_raw.data[i].cpu_pct;
                        t_ri  += curr_raw.data[i].r_iops;
                        t_wi  += curr_raw.data[i].w_iops;
                        t_rm  += curr_raw.data[i].r_mib;
                        t_wm  += curr_raw.data[i].w_mib;
                        t_wt  += curr_raw.data[i].io_wait_ms;
                        
                        t_res  += (double)curr_raw.data[i].mem_res_pages * 4096.0 / 1048576.0;
                        t_shr  += (double)curr_raw.data[i].mem_shr_pages * 4096.0 / 1048576.0;
                        t_virt += (double)curr_raw.data[i].mem_virt_pages * 4096.0 / 1048576.0;
                    }

                    int limit = display_limit; 
                    if ((size_t)limit > view_list->len) limit = view_list->len;
                    
                    struct sysinfo si;
                    sysinfo(&si);
                    long uptime_sec = si.uptime;

                    for (int i=0; i<limit; i++) {
                        const sample_t *c = &view_list->data[i];
                        char pidbuf[32];
                        snprintf(pidbuf, sizeof(pidbuf), "%d", c->tgid);

                        if (strlen(filter_str) > 0) {
                             if (!strcasestr(c->cmd, filter_str) && !strcasestr(pidbuf, filter_str) && !strcasestr(c->user, filter_str)) continue;
                        }
                        
                        double res_mib = (double)c->mem_res_pages * 4096.0 / 1048576.0;
                        double shr_mib = (double)c->mem_shr_pages * 4096.0 / 1048576.0;
                        double virt_mib = (double)c->mem_virt_pages * 4096.0 / 1048576.0;

                        long proc_uptime = uptime_sec - (c->start_time_ticks / hz);
                        char uptime_buf[32];
                        int days = proc_uptime / 86400;
                        int hrs = (proc_uptime % 86400) / 3600;
                        int mins = (proc_uptime % 3600) / 60;
                        int secs = proc_uptime % 60;
                        if (days > 0) snprintf(uptime_buf, 32, "%dd%02dh", days, hrs);
                        else snprintf(uptime_buf, 32, "%02d:%02d:%02d", hrs, mins, secs);

                        printf("%*s %*s %*s %*.0f %*.0f %*.0f %*.*f %*.*f %*.*f %*.*f %*.*f %*.*f %*c ",
                            pidw, pidbuf,
                            userw, c->user,
                            uptimew, uptime_buf,
                            memw, res_mib,
                            memw, shr_mib,
                            memw, virt_mib,
                            iopsw, 2, c->r_iops,
                            iopsw, 2, c->w_iops,
                            waitw, 2, c->io_wait_ms,
                            mibw, 0, c->r_mib,
                            mibw, 0, c->w_mib,
                            cpuw, 2, c->cpu_pct,
                            statew, c->state);
                        fprint_trunc(stdout, c->cmd, cmdw);
                        putchar('\n');

                        if (show_tree) {
                            print_threads_for_tgid(&curr_raw, c->tgid, pidw, cpuw, iopsw, waitw, mibw, cmdw);
                        }
                    }

                    for(int i=0; i<cols; i++) putchar('-');
                    putchar('\n');
                    printf("%*s %*s %*s %*.0f %*.0f %*.0f %*.*f %*.*f %*.*f %*.*f %*.*f %*.*f\n",
                            pidw, "TOTAL",
                            userw, "",
                            uptimew, "",
                            memw, t_res,
                            memw, t_shr,
                            memw, t_virt,
                            iopsw, 2, t_ri,
                            iopsw, 2, t_wi,
                            waitw, 2, t_wt,
                            mibw, 0, t_rm,
                            mibw, 0, t_wm,
                            cpuw, 2, t_cpu);
                }
                fflush(stdout);
                dirty = 0;
            }

            double elapsed = now_monotonic() - start_wait;
            double remain = interval - elapsed;
            if (remain <= 0) break;

            int c = wait_for_input(remain);
            if (c > 0) {
                if (in_filter_mode) {
                    if (c == 27) { // ESC
                        in_filter_mode = 0;
                        filter_str[0] = '\0';
                        dirty = 1;
                    } else if (c == 127 || c == 8) { // Backspace
                        size_t len = strlen(filter_str);
                        if (len > 0) filter_str[len-1] = '\0';
                        dirty = 1;
                    } else if (c == '\n' || c == '\r') {
                        in_filter_mode = 0;
                        dirty = 1;
                    } else if (isprint(c)) {
                        size_t len = strlen(filter_str);
                        if (len < sizeof(filter_str)-1) {
                            filter_str[len] = (char)c;
                            filter_str[len+1] = '\0';
                        }
                        dirty = 1;
                    }
                } else if (in_limit_mode) {
                    if (c == 27) { // ESC
                        in_limit_mode = 0;
                        limit_str[0] = '\0';
                        dirty = 1;
                    } else if (c == 127 || c == 8) {
                        size_t len = strlen(limit_str);
                        if (len > 0) limit_str[len-1] = '\0';
                        dirty = 1;
                    } else if (c == '\n' || c == '\r') {
                        if (strlen(limit_str) > 0) {
                            int val = atoi(limit_str);
                            if (val > 0) display_limit = val;
                        }
                        in_limit_mode = 0;
                        limit_str[0] = '\0';
                        dirty = 1;
                    } else if (isdigit(c)) {
                        size_t len = strlen(limit_str);
                        if (len < sizeof(limit_str)-1) {
                            limit_str[len] = (char)c;
                            limit_str[len+1] = '\0';
                        }
                        dirty = 1;
                    }
                } else {
                    if (c == '/') { in_filter_mode = 1; dirty = 1; }
                    if (c == 'l' || c == 'L') { in_limit_mode = 1; limit_str[0]='\0'; dirty = 1; }
                    if (c == 'q' || c == 'Q') goto cleanup;
                    if (c == 'f' || c == 'F') { frozen = !frozen; dirty = 1; }
                    if (c == 't' || c == 'T') { show_tree = !show_tree; mode = MODE_PROCESS; dirty = 1; }
                    if (c == 'n' || c == 'N') { mode = MODE_NETWORK; dirty = 1; }
                    if (c == 'c' || c == 'C') { mode = MODE_PROCESS; dirty = 1; }
                    if (c == 's' || c == 'S') { mode = MODE_STORAGE; dirty = 1; }
                    
                    if (mode == MODE_PROCESS) {
                        if (c == '1' || c == 0x01) { if (sort_col_proc == SORT_PID) sort_desc = !sort_desc; else { sort_col_proc = SORT_PID; sort_desc = 1; } dirty = 1; }
                        if (c == '2' || c == 0x02) { if (sort_col_proc == SORT_CPU) sort_desc = !sort_desc; else { sort_col_proc = SORT_CPU; sort_desc = 1; } dirty = 1; }
                        if (c == '3' || c == 0x03) { if (sort_col_proc == SORT_LOG_R) sort_desc = !sort_desc; else { sort_col_proc = SORT_LOG_R; sort_desc = 1; } dirty = 1; }
                        if (c == '4' || c == 0x04) { if (sort_col_proc == SORT_LOG_W) sort_desc = !sort_desc; else { sort_col_proc = SORT_LOG_W; sort_desc = 1; } dirty = 1; }
                        if (c == '5' || c == 0x05) { if (sort_col_proc == SORT_WAIT) sort_desc = !sort_desc; else { sort_col_proc = SORT_WAIT; sort_desc = 1; } dirty = 1; }
                        if (c == '6' || c == 0x06) { if (sort_col_proc == SORT_RMIB) sort_desc = !sort_desc; else { sort_col_proc = SORT_RMIB; sort_desc = 1; } dirty = 1; }
                        if (c == '7' || c == 0x07) { if (sort_col_proc == SORT_WMIB) sort_desc = !sort_desc; else { sort_col_proc = SORT_WMIB; sort_desc = 1; } dirty = 1; }
                    } else { // MODE_NETWORK
                        if (c == '1' || c == 0x01) { sort_col_net = SORT_NET_RX; dirty = 1; }
                        if (c == '2' || c == 0x02) { sort_col_net = SORT_NET_TX; dirty = 1; }
                    }
                }
            }
        }

        if (!frozen) {
            qsort(curr_raw.data, curr_raw.len, sizeof(sample_t), cmp_key);
            vec_free(&prev); prev = curr_raw; vec_init(&curr_raw);
            vec_net_free(&prev_net); prev_net = curr_net; vec_net_init(&curr_net);
            vec_disk_free(&prev_disk); prev_disk = curr_disk; vec_disk_init(&curr_disk);
            t_prev = t_curr;
            prev_sys_r = curr_sys_r;
            prev_sys_w = curr_sys_w;
        }
    }

cleanup:
    disable_raw_mode();
    vec_free(&prev);
    vec_free(&curr_raw);
    vec_free(&curr_proc);
    vec_net_free(&prev_net);
    vec_net_free(&curr_net);
    vec_disk_free(&prev_disk);
    vec_disk_free(&curr_disk);
    free(filter);
    return 0;
}