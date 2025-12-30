#define _GNU_SOURCE
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/termios.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifndef CMD_MAX
#define CMD_MAX 512
#endif

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

// --- Data Structures ---
typedef struct {
    pid_t pid;
    pid_t tid;
    uint64_t key; 

    uint64_t syscr;
    uint64_t syscw;
    uint64_t read_bytes;
    uint64_t write_bytes;
    uint64_t cpu_jiffies;

    double cpu_pct;
    double r_iops;
    double w_iops;
    double r_mib;
    double w_mib;

    char cmd[CMD_MAX];
} sample_t;

typedef struct {
    sample_t *data;
    size_t len;
    size_t cap;
} vec_t;

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

static int is_numeric_str(const char *s) {
    if (!s || !*s) return 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p) if (!isdigit(*p)) return 0;
    return 1;
}

static uint64_t make_key(pid_t pid, pid_t tid) {
    return ((uint64_t)(uint32_t)pid << 32) | (uint64_t)(uint32_t)tid;
}

static double now_monotonic(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

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
        if (out[0] != '\0') return 0;
    }
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    if (read_small_file(path, buf, sizeof(buf), &n) == 0 && n > 0) {
        sanitize_cmd(out, buf, (size_t)n);
        if (out[0] != '\0') return 0;
    }
    snprintf(out, CMD_MAX, "%s", "?");
    return -1;
}

static int read_io_file(const char *path, uint64_t *syscr, uint64_t *syscw, uint64_t *read_bytes, uint64_t *write_bytes) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    uint64_t v_syscr=0, v_syscw=0, v_rbytes=0, v_wbytes=0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char key[64]; unsigned long long val = 0;
        if (sscanf(line, "%63[^:]: %llu", key, &val) == 2) {
            if (strcmp(key, "syscr") == 0) v_syscr = (uint64_t)val;
            else if (strcmp(key, "syscw") == 0) v_syscw = (uint64_t)val;
            else if (strcmp(key, "read_bytes") == 0) v_rbytes = (uint64_t)val;
            else if (strcmp(key, "write_bytes") == 0) v_wbytes = (uint64_t)val;
        }
    }
    fclose(f);
    *syscr = v_syscr; *syscw = v_syscw; *read_bytes = v_rbytes; *write_bytes = v_wbytes;
    return 0;
}

static int read_cpu_jiffies_from_stat(const char *path, uint64_t *cpu_jiffies_out) {
    char buf[4096]; ssize_t n = 0;
    if (read_small_file(path, buf, sizeof(buf), &n) != 0 || n <= 0) return -1;
    char *rparen = strrchr(buf, ')');
    if (!rparen) return -1;
    char *p = rparen + 2; 
    char *save = NULL; char *tok = strtok_r(p, " ", &save);
    int idx = 0; uint64_t utime=0, stime=0;
    while (tok) {
        if (idx == 11) utime = strtoull(tok, NULL, 10);
        else if (idx == 12) { stime = strtoull(tok, NULL, 10); break; }
        idx++; tok = strtok_r(NULL, " ", &save);
    }
    *cpu_jiffies_out = utime + stime;
    return 0;
}

static int pid_in_filter(pid_t pid, const pid_t *filter, size_t n) {
    if (!filter || n == 0) return 1;
    for (size_t i = 0; i < n; i++) if (filter[i] == pid) return 1;
    return 0;
}

static int collect_samples(vec_t *out, int include_threads, const pid_t *filter_pids, size_t filter_n) {
    DIR *proc = opendir("/proc");
    if (!proc) { perror("opendir(/proc)"); return -1; }
    struct dirent *de;
    while ((de = readdir(proc)) != NULL) {
        if (!is_numeric_str(de->d_name)) continue;
        pid_t pid = (pid_t)atoi(de->d_name);
        if (pid <= 0) continue;
        if (!pid_in_filter(pid, filter_pids, filter_n)) continue;

        char cmd[CMD_MAX];
        read_cmdline(pid, cmd);

        if (!include_threads) {
            sample_t s; memset(&s, 0, sizeof(s));
            s.pid = pid; s.tid = pid; s.key = make_key(pid, pid);
            snprintf(s.cmd, sizeof(s.cmd), "%s", cmd);
            char io_path[PATH_MAX], stat_path[PATH_MAX];
            snprintf(io_path, sizeof(io_path), "/proc/%d/io", pid);
            snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", pid);
            
            // Try reading IO, ignore failure (s.syscr etc are already 0 from memset)
            read_io_file(io_path, &s.syscr, &s.syscw, &s.read_bytes, &s.write_bytes);
            
            read_cpu_jiffies_from_stat(stat_path, &s.cpu_jiffies);
            vec_push(out, &s);
        } else {
            char taskdir_path[PATH_MAX];
            snprintf(taskdir_path, sizeof(taskdir_path), "/proc/%d/task", pid);
            DIR *taskdir = opendir(taskdir_path);
            if (!taskdir) continue;
            struct dirent *te;
            while ((te = readdir(taskdir)) != NULL) {
                if (!is_numeric_str(te->d_name)) continue;
                pid_t tid = (pid_t)atoi(te->d_name);
                if (tid <= 0) continue;
                sample_t s; memset(&s, 0, sizeof(s));
                s.pid = pid; s.tid = tid; s.key = make_key(pid, tid);
                snprintf(s.cmd, sizeof(s.cmd), "%s", cmd);
                char io_path[PATH_MAX], stat_path[PATH_MAX];
                snprintf(io_path, sizeof(io_path), "/proc/%d/task/%d/io", pid, tid);
                snprintf(stat_path, sizeof(stat_path), "/proc/%d/task/%d/stat", pid, tid);
                
                // Try reading IO, ignore failure
                read_io_file(io_path, &s.syscr, &s.syscw, &s.read_bytes, &s.write_bytes);
                
                read_cpu_jiffies_from_stat(stat_path, &s.cpu_jiffies);
                vec_push(out, &s);
            }
            closedir(taskdir);
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

// Sort Comparators
typedef enum { SORT_PID=1, SORT_CPU, SORT_RIOPS, SORT_WIOPS, SORT_RMIB, SORT_WMIB } sort_col_t;

static int cmp_pid_desc(const void *a, const void *b) {
    const sample_t *x = (const sample_t *)a;
    const sample_t *y = (const sample_t *)b;
    return (x->pid < y->pid) - (x->pid > y->pid); 
}
static int cmp_cpu_desc(const void *a, const void *b) {
    const sample_t *x = (const sample_t *)a;
    const sample_t *y = (const sample_t *)b;
    return (x->cpu_pct < y->cpu_pct) - (x->cpu_pct > y->cpu_pct);
}
static int cmp_riops_desc(const void *a, const void *b) {
    const sample_t *x = (const sample_t *)a;
    const sample_t *y = (const sample_t *)b;
    return (x->r_iops < y->r_iops) - (x->r_iops > y->r_iops);
}
static int cmp_wiops_desc(const void *a, const void *b) {
    const sample_t *x = (const sample_t *)a;
    const sample_t *y = (const sample_t *)b;
    return (x->w_iops < y->w_iops) - (x->w_iops > y->w_iops);
}
static int cmp_rmib_desc(const void *a, const void *b) {
    const sample_t *x = (const sample_t *)a;
    const sample_t *y = (const sample_t *)b;
    return (x->r_mib < y->r_mib) - (x->r_mib > y->r_mib);
}
static int cmp_wmib_desc(const void *a, const void *b) {
    const sample_t *x = (const sample_t *)a;
    const sample_t *y = (const sample_t *)b;
    return (x->w_mib < y->w_mib) - (x->w_mib > y->w_mib);
}

int main(int argc, char **argv) {
    if (geteuid() != 0) {
        fprintf(stderr, "Warning: Not running as root. IO stats will be unavailable for other users' processes.\n");
        sleep(2);
    }

    double interval = 5.0; // Default to 5 seconds
    int include_threads = 0;
    int display_limit = 50; 
    
    pid_t *filter = NULL;
    size_t filter_n = 0, filter_cap = 0;

    static const struct option long_opts[] = {
        {"interval", required_argument, NULL, 'i'},
        {"pid", required_argument, NULL, 'p'},
        {"threads", no_argument, NULL, 't'},
        {"help", no_argument, NULL, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "i:p:th", long_opts, NULL)) != -1) {
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
            case 't': include_threads = 1; break;
            case 'h': default: return 0;
        }
    }

    long hz = sysconf(_SC_CLK_TCK);
    vec_t prev, curr;
    vec_init(&prev); vec_init(&curr);
    
    printf("Initializing (wait %.0fs)...\n", interval);
    if (collect_samples(&prev, include_threads, filter, filter_n) != 0) return 1;
    qsort(prev.data, prev.len, sizeof(sample_t), cmp_key);
    double t_prev = now_monotonic();

    enable_raw_mode();
    sort_col_t sort_col = SORT_CPU;

    while (1) {
        vec_free(&curr); vec_init(&curr);
        if (collect_samples(&curr, include_threads, filter, filter_n) != 0) break;
        
        double t_curr = now_monotonic();
        double dt = t_curr - t_prev;
        if (dt <= 0) dt = interval;

        // Calc metrics
        for (size_t i=0; i<curr.len; i++) {
            sample_t *c = &curr.data[i];
            const sample_t *p = find_prev(&prev, c->key);
            uint64_t d_cpu=0, d_scr=0, d_scw=0, d_rb=0, d_wb=0;
            if (p) {
                d_cpu = (c->cpu_jiffies >= p->cpu_jiffies) ? c->cpu_jiffies - p->cpu_jiffies : 0;
                d_scr = (c->syscr >= p->syscr) ? c->syscr - p->syscr : 0;
                d_scw = (c->syscw >= p->syscw) ? c->syscw - p->syscw : 0;
                d_rb  = (c->read_bytes >= p->read_bytes) ? c->read_bytes - p->read_bytes : 0;
                d_wb  = (c->write_bytes >= p->write_bytes) ? c->write_bytes - p->write_bytes : 0;
            }
            c->cpu_pct = ((double)d_cpu * 100.0) / (dt * (double)hz);
            c->r_iops = (double)d_scr / dt;
            c->w_iops = (double)d_scw / dt;
            c->r_mib  = ((double)d_rb / dt) / 1048576.0;
            c->w_mib  = ((double)d_wb / dt) / 1048576.0;
        }

        int dirty = 1;
        double start_wait = now_monotonic();

        while (1) {
            if (dirty) {
                switch(sort_col) {
                    case SORT_PID: qsort(curr.data, curr.len, sizeof(sample_t), cmp_pid_desc); break;
                    case SORT_CPU: qsort(curr.data, curr.len, sizeof(sample_t), cmp_cpu_desc); break;
                    case SORT_RIOPS: qsort(curr.data, curr.len, sizeof(sample_t), cmp_riops_desc); break;
                    case SORT_WIOPS: qsort(curr.data, curr.len, sizeof(sample_t), cmp_wiops_desc); break;
                    case SORT_RMIB: qsort(curr.data, curr.len, sizeof(sample_t), cmp_rmib_desc); break;
                    case SORT_WMIB: qsort(curr.data, curr.len, sizeof(sample_t), cmp_wmib_desc); break;
                }

                printf("\033[2J\033[H"); // Clear screen
                
                int cols = get_term_cols();
                
                // Top status bar with Refresh rate
                printf("kvmtop - Refresh=%.1fs\n", interval);

                int pidw = include_threads ? 11 : 7;
                int cpuw = 8, iopsw = 10, mibw = 10;
                int fixed = pidw+1 + cpuw+1 + iopsw+1 + iopsw+1 + mibw+1 + mibw+1;
                int cmdw = cols - fixed; if (cmdw < 10) cmdw = 10;

                // Headers aligned to match %%*.*f formatting
                char h_pid[32], h_cpu[32], h_ri[32], h_wi[32], h_rm[32], h_wm[32];
                snprintf(h_pid, 32, "[1] %s", "PID");
                snprintf(h_cpu, 32, "[2] %s", "CPU%%");
                snprintf(h_ri, 32, "[3] %s", "R_IOPS");
                snprintf(h_wi, 32, "[4] %s", "W_IOPS");
                snprintf(h_rm, 32, "[5] %s", "R_MiB/s");
                snprintf(h_wm, 32, "[6] %s", "W_MiB/s");

                printf("%*s %*s %*s %*s %*s %*s ",
                    pidw, h_pid, cpuw, h_cpu, iopsw, h_ri, iopsw, h_wi, mibw, h_rm, mibw, h_wm);
                fprint_trunc(stdout, "COMMAND", cmdw);
                printf("\n");
                
                for(int i=0; i<cols; i++) putchar('-');
                putchar('\n');

                int limit = display_limit; 
                if ((size_t)limit > curr.len) limit = curr.len;
                
                for (int i=0; i<limit; i++) {
                    const sample_t *c = &curr.data[i];
                    char pidbuf[32];
                    if (include_threads) snprintf(pidbuf, sizeof(pidbuf), "%d:%d", c->pid, c->tid);
                    else snprintf(pidbuf, sizeof(pidbuf), "%d", c->pid);
                    
                    printf("%*s %*.*f %*.*f %*.*f %*.*f %*.*f ",
                        pidw, pidbuf,
                        cpuw, 2, c->cpu_pct,
                        iopsw, 2, c->r_iops,
                        iopsw, 2, c->w_iops,
                        mibw, 2, c->r_mib,
                        mibw, 2, c->w_mib);
                    fprint_trunc(stdout, c->cmd, cmdw);
                    putchar('\n');
                }
                fflush(stdout);
                dirty = 0;
            }

            double elapsed = now_monotonic() - start_wait;
            double remain = interval - elapsed;
            if (remain <= 0) break;

            int c = wait_for_input(remain);
            if (c > 0) {
                if (c == 'q' || c == 'Q') goto cleanup;
                if (c == '1' || c == 0x01) { sort_col = SORT_PID; dirty = 1; }
                if (c == '2' || c == 0x02) { sort_col = SORT_CPU; dirty = 1; }
                if (c == '3' || c == 0x03) { sort_col = SORT_RIOPS; dirty = 1; }
                if (c == '4' || c == 0x04) { sort_col = SORT_WIOPS; dirty = 1; }
                if (c == '5' || c == 0x05) { sort_col = SORT_RMIB; dirty = 1; }
                if (c == '6' || c == 0x06) { sort_col = SORT_WMIB; dirty = 1; }
            } else {
                break;
            }
        }

        qsort(curr.data, curr.len, sizeof(sample_t), cmp_key);
        vec_free(&prev); prev = curr; vec_init(&curr);
        t_prev = t_curr;
    }

cleanup:
    disable_raw_mode();
    vec_free(&prev);
    vec_free(&curr);
    free(filter);
    return 0;
}
