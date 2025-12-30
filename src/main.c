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
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifndef CMD_MAX
#define CMD_MAX 512
#endif

typedef struct {
    pid_t pid;
    pid_t tid; // == pid when not in --threads mode
    uint64_t key; // (pid<<32)|tid

    uint64_t syscr;
    uint64_t syscw;
    uint64_t read_bytes;
    uint64_t write_bytes;

    uint64_t cpu_jiffies; // utime+stime in USER_HZ ticks

    char cmd[CMD_MAX];
} sample_t;

typedef struct {
    sample_t *data;
    size_t len;
    size_t cap;
} vec_t;

static void vec_init(vec_t *v) {
    v->data = NULL;
    v->len = 0;
    v->cap = 0;
}

static void vec_free(vec_t *v) {
    free(v->data);
    v->data = NULL;
    v->len = 0;
    v->cap = 0;
}

static void vec_push(vec_t *v, const sample_t *item) {
    if (v->len == v->cap) {
        size_t new_cap = v->cap ? v->cap * 2 : 4096;
        sample_t *p = (sample_t *)realloc(v->data, new_cap * sizeof(*p));
        if (!p) {
            fprintf(stderr, "Out of memory\n");
            exit(2);
        }
        v->data = p;
        v->cap = new_cap;
    }
    v->data[v->len++] = *item;
}

static int is_numeric_str(const char *s) {
    if (!s || !*s) return 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
        if (!isdigit(*p)) return 0;
    }
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

static void sleep_seconds(double seconds) {
    if (seconds <= 0) return;
    struct timespec req;
    req.tv_sec = (time_t)seconds;
    req.tv_nsec = (long)((seconds - (double)req.tv_sec) * 1e9);
    if (req.tv_nsec < 0) req.tv_nsec = 0;
    while (nanosleep(&req, &req) == -1 && errno == EINTR) {
        // continue sleeping remaining time
    }
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
    // Convert cmdline (NUL separated args) into a single printable line.
    // Compress whitespace.
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

    // Trim trailing space
    while (o > 0 && out[o - 1] == ' ') o--;
    out[o] = '\0';
}

static int read_cmdline(pid_t pid, char out[CMD_MAX]) {
    char path[PATH_MAX];
    char buf[8192];
    ssize_t n = 0;

    // Prefer full cmdline
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    if (read_small_file(path, buf, sizeof(buf), &n) == 0 && n > 0) {
        sanitize_cmd(out, buf, (size_t)n);
        if (out[0] != '\0') return 0;
    }

    // Fallback (kernel threads etc.)
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    if (read_small_file(path, buf, sizeof(buf), &n) == 0 && n > 0) {
        sanitize_cmd(out, buf, (size_t)n);
        if (out[0] != '\0') return 0;
    }

    snprintf(out, CMD_MAX, "%s", "?");
    return -1;
}

static int read_io_file(const char *path,
                        uint64_t *syscr, uint64_t *syscw,
                        uint64_t *read_bytes, uint64_t *write_bytes) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    uint64_t v_syscr = 0, v_syscw = 0, v_rbytes = 0, v_wbytes = 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char key[64];
        unsigned long long val = 0;
        if (sscanf(line, "%63[^:]: %llu", key, &val) == 2) {
            if (strcmp(key, "syscr") == 0) v_syscr = (uint64_t)val;
            else if (strcmp(key, "syscw") == 0) v_syscw = (uint64_t)val;
            else if (strcmp(key, "read_bytes") == 0) v_rbytes = (uint64_t)val;
            else if (strcmp(key, "write_bytes") == 0) v_wbytes = (uint64_t)val;
        }
    }

    fclose(f);
    *syscr = v_syscr;
    *syscw = v_syscw;
    *read_bytes = v_rbytes;
    *write_bytes = v_wbytes;
    return 0;
}

static int read_cpu_jiffies_from_stat(const char *path, uint64_t *cpu_jiffies_out) {
    char buf[4096];
    ssize_t n = 0;
    if (read_small_file(path, buf, sizeof(buf), &n) != 0 || n <= 0) return -1;

    // /proc/<pid>/stat: pid (comm) state ...
    // utime is field 14, stime is field 15.
    // After ')', tokens begin at field 3 (state). So:
    // token index 11 -> field 14 (utime)
    // token index 12 -> field 15 (stime)
    char *rparen = strrchr(buf, ')');
    if (!rparen) return -1;
    char *p = rparen + 2; // ") "
    if (p < buf || p >= buf + n) return -1;

    char *save = NULL;
    char *tok = strtok_r(p, " ", &save);
    int idx = 0;
    uint64_t utime = 0, stime = 0;

    while (tok) {
        if (idx == 11) {
            errno = 0;
            unsigned long long v = strtoull(tok, NULL, 10);
            if (errno != 0) return -1;
            utime = (uint64_t)v;
        } else if (idx == 12) {
            errno = 0;
            unsigned long long v = strtoull(tok, NULL, 10);
            if (errno != 0) return -1;
            stime = (uint64_t)v;
            break;
        }
        idx++;
        tok = strtok_r(NULL, " ", &save);
    }

    *cpu_jiffies_out = utime + stime;
    return 0;
}

static int pid_in_filter(pid_t pid, const pid_t *filter, size_t n) {
    if (!filter || n == 0) return 1;
    for (size_t i = 0; i < n; i++) {
        if (filter[i] == pid) return 1;
    }
    return 0;
}

static int collect_samples(vec_t *out, int include_threads,
                           const pid_t *filter_pids, size_t filter_n) {
    DIR *proc = opendir("/proc");
    if (!proc) {
        perror("opendir(/proc)");
        return -1;
    }

    struct dirent *de;
    while ((de = readdir(proc)) != NULL) {
        if (!is_numeric_str(de->d_name)) continue;
        pid_t pid = (pid_t)atoi(de->d_name);
        if (pid <= 0) continue;
        if (!pid_in_filter(pid, filter_pids, filter_n)) continue;

        char cmd[CMD_MAX];
        read_cmdline(pid, cmd);

        if (!include_threads) {
            sample_t s;
            memset(&s, 0, sizeof(s));
            s.pid = pid;
            s.tid = pid;
            s.key = make_key(pid, pid);
            snprintf(s.cmd, sizeof(s.cmd), "%s", cmd);

            char io_path[PATH_MAX];
            char stat_path[PATH_MAX];
            snprintf(io_path, sizeof(io_path), "/proc/%d/io", pid);
            snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", pid);

            if (read_io_file(io_path, &s.syscr, &s.syscw, &s.read_bytes, &s.write_bytes) != 0) {
                continue;
            }
            if (read_cpu_jiffies_from_stat(stat_path, &s.cpu_jiffies) != 0) {
                continue;
            }

            vec_push(out, &s);
        } else {
            char taskdir_path[PATH_MAX];
            snprintf(taskdir_path, sizeof(taskdir_path), "/proc/%d/task", pid);
            DIR *taskdir = opendir(taskdir_path);
            if (!taskdir) {
                continue;
            }
            struct dirent *te;
            while ((te = readdir(taskdir)) != NULL) {
                if (!is_numeric_str(te->d_name)) continue;
                pid_t tid = (pid_t)atoi(te->d_name);
                if (tid <= 0) continue;

                sample_t s;
                memset(&s, 0, sizeof(s));
                s.pid = pid;
                s.tid = tid;
                s.key = make_key(pid, tid);
                snprintf(s.cmd, sizeof(s.cmd), "%s", cmd);

                char io_path[PATH_MAX];
                char stat_path[PATH_MAX];
                snprintf(io_path, sizeof(io_path), "/proc/%d/task/%d/io", pid, tid);
                snprintf(stat_path, sizeof(stat_path), "/proc/%d/task/%d/stat", pid, tid);

                if (read_io_file(io_path, &s.syscr, &s.syscw, &s.read_bytes, &s.write_bytes) != 0) {
                    continue;
                }
                if (read_cpu_jiffies_from_stat(stat_path, &s.cpu_jiffies) != 0) {
                    continue;
                }

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
    if (x->key < y->key) return -1;
    if (x->key > y->key) return 1;
    return 0;
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
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
            if (ws.ws_col > 0) cols = ws.ws_col;
        }
    }
    return cols;
}

static void fprint_trunc(FILE *out, const char *s, int width) {
    if (width <= 0) return;
    int len = (int)strlen(s);
    if (len <= width) {
        fprintf(out, "%-*s", width, s);
        return;
    }
    if (width <= 3) {
        fprintf(out, "%.*s", width, s);
        return;
    }
    fprintf(out, "%.*s...", width - 3, s);
}

static void usage(const char *argv0) {
    fprintf(stderr,
        "Usage: %s [options]\n\n"
        "Aligned table of per-process (or per-thread) CPU and I/O rates using /proc.\n\n"
        "Columns: PID, CPU%%, read IOPS, write IOPS, read MiB/s, write MiB/s, command line\n\n"
        "Options:\n"
        "  -i, --interval SEC   Sampling interval in seconds (default 1.0)\n"
        "  -n, --samples N      Number of samples to print (default infinite)\n"
        "  -p, --pid PID        Monitor only this PID (repeatable)\n"
        "  -t, --threads        Show threads too (PID:TID in first column)\n"
        "  -h, --help           Show this help\n",
        argv0);
}

int main(int argc, char **argv) {
    printf("kvmtop (static build) starting...\n");
    double interval = 1.0;
    long samples = -1;
    int include_threads = 0;

    pid_t *filter = NULL;
    size_t filter_n = 0, filter_cap = 0;

    static const struct option long_opts[] = {
        {"interval", required_argument, NULL, 'i'},
        {"samples", required_argument, NULL, 'n'},
        {"pid", required_argument, NULL, 'p'},
        {"threads", no_argument, NULL, 't'},
        {"help", no_argument, NULL, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "i:n:p:th", long_opts, NULL)) != -1) {
        switch (opt) {
            case 'i': {
                interval = strtod(optarg, NULL);
                if (interval <= 0) {
                    fprintf(stderr, "interval must be > 0\n");
                    return 2;
                }
                break;
            }
            case 'n': {
                samples = strtol(optarg, NULL, 10);
                if (samples < 0) {
                    fprintf(stderr, "samples must be >= 0\n");
                    return 2;
                }
                break;
            }
            case 'p': {
                long v = strtol(optarg, NULL, 10);
                if (v <= 0 || v > INT_MAX) {
                    fprintf(stderr, "Invalid PID: %s\n", optarg);
                    return 2;
                }
                if (filter_n == filter_cap) {
                    size_t new_cap = filter_cap ? filter_cap * 2 : 8;
                    pid_t *p = (pid_t *)realloc(filter, new_cap * sizeof(*p));
                    if (!p) {
                        fprintf(stderr, "Out of memory\n");
                        return 2;
                    }
                    filter = p;
                    filter_cap = new_cap;
                }
                filter[filter_n++] = (pid_t)v;
                break;
            }
            case 't':
                include_threads = 1;
                break;
            case 'h':
            default:
                usage(argv[0]);
                return (opt == 'h') ? 0 : 2;
        }
    }

    const long hz = sysconf(_SC_CLK_TCK);
    if (hz <= 0) {
        fprintf(stderr, "Failed to get _SC_CLK_TCK\n");
        return 2;
    }

    // Fixed column widths (table-like)
    const int pidw  = include_threads ? 11 : 7;  // e.g. 12345:12345
    const int cpuw  = 8;   // 100.00
    const int iopsw = 10;  // 12345.67
    const int mibw  = 10;  // 12345.67

    vec_t prev, curr;
    vec_init(&prev);
    vec_init(&curr);

    if (collect_samples(&prev, include_threads, filter, filter_n) != 0) {
        fprintf(stderr, "Failed to collect samples\n");
        return 2;
    }
    qsort(prev.data, prev.len, sizeof(prev.data[0]), cmp_key);
    double t_prev = now_monotonic();

    // Header (fits terminal width; command column uses remaining space)
    int cols = get_term_cols();
    int fixed = pidw + 1 + cpuw + 1 + iopsw + 1 + iopsw + 1 + mibw + 1 + mibw + 1;
    int cmdw = cols - fixed;
    if (cmdw < 10) cmdw = 10;

    const char *pid_hdr = include_threads ? "PID:TID" : "PID";

    printf("%*s %*s %*s %*s %*s %*s ",
           pidw, pid_hdr,
           cpuw, "CPU%",
           iopsw, "R_IOPS",
           iopsw, "W_IOPS",
           mibw, "R_MiB/s",
           mibw, "W_MiB/s");
    fprint_trunc(stdout, "COMMAND", cmdw);
    printf("\n");

    int totalw = fixed + cmdw;
    for (int i = 0; i < totalw; i++) putchar('-');
    putchar('\n');
    fflush(stdout);

    long emitted = 0;
    while (samples < 0 || emitted < samples) {
        sleep_seconds(interval);

        vec_free(&curr);
        vec_init(&curr);

        (void)collect_samples(&curr, include_threads, filter, filter_n);
        qsort(curr.data, curr.len, sizeof(curr.data[0]), cmp_key);

        double t_curr = now_monotonic();
        double dt = t_curr - t_prev;
        if (dt <= 0) dt = interval;

        // Recompute cmd column width in case terminal resized
        cols = get_term_cols();
        fixed = pidw + 1 + cpuw + 1 + iopsw + 1 + iopsw + 1 + mibw + 1 + mibw + 1;
        cmdw = cols - fixed;
        if (cmdw < 10) cmdw = 10;

        for (size_t i = 0; i < curr.len; i++) {
            const sample_t *c = &curr.data[i];
            const sample_t *p = find_prev(&prev, c->key);

            uint64_t d_syscr = 0, d_syscw = 0, d_rbytes = 0, d_wbytes = 0, d_cpu = 0;
            if (p) {
                d_syscr  = (c->syscr >= p->syscr) ? (c->syscr - p->syscr) : 0;
                d_syscw  = (c->syscw >= p->syscw) ? (c->syscw - p->syscw) : 0;
                d_rbytes = (c->read_bytes >= p->read_bytes) ? (c->read_bytes - p->read_bytes) : 0;
                d_wbytes = (c->write_bytes >= p->write_bytes) ? (c->write_bytes - p->write_bytes) : 0;
                d_cpu    = (c->cpu_jiffies >= p->cpu_jiffies) ? (c->cpu_jiffies - p->cpu_jiffies) : 0;
            }

            // CPU% is relative to a single CPU (can exceed 100.00 for multi-threaded processes)
            double cpu_pct = p ? ((double)d_cpu * 100.0) / (dt * (double)hz) : 0.0;

            double r_iops  = (double)d_syscr / dt;
            double w_iops  = (double)d_syscw / dt;

            // Always show MiB/s
            double r_mib_s = ((double)d_rbytes / dt) / (1024.0 * 1024.0);
            double w_mib_s = ((double)d_wbytes / dt) / (1024.0 * 1024.0);

            char pidbuf[32];
            if (include_threads) snprintf(pidbuf, sizeof(pidbuf), "%d:%d", (int)c->pid, (int)c->tid);
            else snprintf(pidbuf, sizeof(pidbuf), "%d", (int)c->pid);

            printf("%*s %*.*f %*.*f %*.*f %*.*f %*.*f ",
                   pidw, pidbuf,
                   cpuw, 2, cpu_pct,
                   iopsw, 2, r_iops,
                   iopsw, 2, w_iops,
                   mibw, 2, r_mib_s,
                   mibw, 2, w_mib_s);
            fprint_trunc(stdout, c->cmd, cmdw);
            putchar('\n');
        }

        putchar('\n');
        fflush(stdout);

        vec_free(&prev);
        prev = curr;
        vec_init(&curr);
        t_prev = t_curr;
        emitted++;
    }

    vec_free(&prev);
    vec_free(&curr);
    free(filter);
    return 0;
}