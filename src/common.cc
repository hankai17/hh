#include <cstring>
#include <errno.h>
#include <execinfo.h>
#include <string.h>
#include <sysexits.h>
#include <stdlib.h>
#include <cstdio>

#include "common.hh"

long AB = MAX_CODEPOINT + 1;
Mode opt_mod = Mode::cxx;

long action_label;
long action_label_base;
long call_label;
long call_label_base;
long collapse_label;
long collapse_label_base;

static const char *ENAME[] = {
    /*   0 */ "",
    /*   1 */ "EPERM", "ENOENT", "ESRCH", "EINTR", "EIO", "ENXIO",
    /*   7 */ "E2BIG", "ENOEXEC", "EBADF", "ECHILD",
    /*  11 */ "EAGAIN/EWOULDBLOCK", "ENOMEM", "EACCES", "EFAULT",
    /*  15 */ "ENOTBLK", "EBUSY", "EEXIST", "EXDEV", "ENODEV",
    /*  20 */ "ENOTDIR", "EISDIR", "EINVAL", "ENFILE", "EMFILE",
    /*  25 */ "ENOTTY", "ETXTBSY", "EFBIG", "ENOSPC", "ESPIPE",
    /*  30 */ "EROFS", "EMLINK", "EPIPE", "EDOM", "ERANGE",
    /*  35 */ "EDEADLK/EDEADLOCK", "ENAMETOOLONG", "ENOLCK", "ENOSYS",
    /*  39 */ "ENOTEMPTY", "ELOOP", "", "ENOMSG", "EIDRM", "ECHRNG",
    /*  45 */ "EL2NSYNC", "EL3HLT", "EL3RST", "ELNRNG", "EUNATCH",
    /*  50 */ "ENOCSI", "EL2HLT", "EBADE", "EBADR", "EXFULL", "ENOANO",
    /*  56 */ "EBADRQC", "EBADSLT", "", "EBFONT", "ENOSTR", "ENODATA",
    /*  62 */ "ETIME", "ENOSR", "ENONET", "ENOPKG", "EREMOTE",
    /*  67 */ "ENOLINK", "EADV", "ESRMNT", "ECOMM", "EPROTO",
    /*  72 */ "EMULTIHOP", "EDOTDOT", "EBADMSG", "EOVERFLOW",
    /*  76 */ "ENOTUNIQ", "EBADFD", "EREMCHG", "ELIBACC", "ELIBBAD",
    /*  81 */ "ELIBSCN", "ELIBMAX", "ELIBEXEC", "EILSEQ", "ERESTART",
    /*  86 */ "ESTRPIPE", "EUSERS", "ENOTSOCK", "EDESTADDRREQ",
    /*  90 */ "EMSGSIZE", "EPROTOTYPE", "ENOPROTOOPT",
    /*  93 */ "EPROTONOSUPPORT", "ESOCKTNOSUPPORT",
    /*  95 */ "EOPNOTSUPP/ENOTSUP", "EPFNOSUPPORT", "EAFNOSUPPORT",
    /*  98 */ "EADDRINUSE", "EADDRNOTAVAIL", "ENETDOWN", "ENETUNREACH",
    /* 102 */ "ENETRESET", "ECONNABORTED", "ECONNRESET", "ENOBUFS",
    /* 106 */ "EISCONN", "ENOTCONN", "ESHUTDOWN", "ETOOMANYREFS",
    /* 110 */ "ETIMEDOUT", "ECONNREFUSED", "EHOSTDOWN", "EHOSTUNREACH",
    /* 114 */ "EALREADY", "EINPROGRESS", "ESTALE", "EUCLEAN",
    /* 118 */ "ENOTNAM", "ENAVAIL", "EISNAM", "EREMOTEIO", "EDQUOT",
    /* 123 */ "ENOMEDIUM", "EMEDIUMTYPE", "ECANCELED", "ENOKEY",
    /* 127 */ "EKEYEXPIRED", "EKEYREVOKED", "EKEYREJECTED",
    /* 130 */ "EOWNERDEAD", "ENOTRECOVERABLE", "ERFKILL", "EHWPOISON"
};

#define MAX_ENAME 133

void output_error(bool use_err, const char *format, va_list ap) {
    char text[BUF_SIZE/2];
    char msg[BUF_SIZE/2];
    char buf[BUF_SIZE];
    vsnprintf(msg, sizeof(msg), format, ap);
    if (use_err) {
        snprintf(text, sizeof(text), "[%s %s] ",
            0 < errno && errno < MAX_ENAME ? ENAME[errno] : "?UNKNOWN?",
            strerror(errno));
    } else {
        strcpy(text, "");
    }
    snprintf(buf, BUF_SIZE, "%s%s\n", text, msg);
    fputs(buf, stderr);
    fflush(stderr);
}

void err_msg(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int saved = errno;
    output_error(errno > 0, format, ap);
    errno = saved;
    va_end(ap);
}

#define err_msg_g(...) ({err_msg(__VA_ARGS__); goto quit;})

void err_exit(int exitno, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int saved = errno;
    output_error(errno > 0, format, ap);
    errno = saved;
    va_end(ap);

    void *bt[99];
    char buf[1024];
    int nptrs = backtrace(bt, LEN_OF(buf));
    int i = sprintf(buf, "addr2line -Cfile %s", program_invocation_name);
    int j = 0;
    int len = sizeof(buf);
    while (j < nptrs && i + 30 < (int)sizeof(buf)) {
        i += snprintf(buf + i, len - i, " %p", bt[j++]);
    }
    strcat(buf, ">&2");
    fputs("\n", stderr);
    int ret = system(buf);
    if (ret < 0) {
        fputs("system failed", stderr);
    }
    //backtrace_sysbols_fd(buf, nptrs, STDERR_FILENO);
    exit(exitno);
}

long get_long(const char *arg) {
    char *end;
    errno = 0;
    long ret = strtol(arg, &end, 0);
    if (errno) {
        err_exit(EX_USAGE, "get_long: %s", arg);
    }
    if (*end) {
        err_exit(EX_USAGE, "get_long: nonnumeric character");
    }
    return ret;
}

void ident(FILE *f, int d) {
    fprintf(f, "%*s", 2 * d, "");
}

