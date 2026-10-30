#include "crashlog.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>

#include <signal.h>

#if defined(_WIN32)
#include <io.h>
#define STDERR_FILENO _fileno(stderr)
using ssize_t = long long;
#else
#include <unistd.h>
#endif

#if defined(__APPLE__) || defined(__linux__)
#include <execinfo.h>
#define TIBIA_HAVE_BACKTRACE 1
#endif

namespace tibia::app {
namespace {

std::atomic_flag g_handling = ATOMIC_FLAG_INIT;

void writeLiteral(const char* text)
{
    if (!text)
        return;
    const auto len = static_cast<ssize_t>(std::strlen(text));
    if (len > 0)
        ::write(STDERR_FILENO, text, static_cast<size_t>(len));
}

void dumpBacktrace()
{
#if TIBIA_HAVE_BACKTRACE
    void* frames[64];
    const int count = ::backtrace(frames, 64);
    writeLiteral("tibia.crash: backtrace\n");
    ::backtrace_symbols_fd(frames, count, STDERR_FILENO);
#else
    writeLiteral("tibia.crash: no backtrace on this platform\n");
#endif
}

void crashHandler(int sig)
{
    if (g_handling.test_and_set())
        ::_exit(128 + sig);

    char header[96];
#if defined(_WIN32)
    std::snprintf(header, sizeof(header), "\ntibia.crash: signal %d\n", sig);
#else
    std::snprintf(header, sizeof(header), "\ntibia.crash: signal %d (%s)\n", sig, strsignal(sig));
#endif
    writeLiteral(header);
    dumpBacktrace();

    ::signal(sig, SIG_DFL);
    ::raise(sig);
}

void terminateHandler()
{
    writeLiteral("\ntibia.crash: std::terminate\n");
    dumpBacktrace();
    std::abort();
}

}

void installCrashLog()
{
    std::set_terminate(terminateHandler);

#if defined(_WIN32)
    ::signal(SIGSEGV, crashHandler);
    ::signal(SIGABRT, crashHandler);
    ::signal(SIGFPE, crashHandler);
    ::signal(SIGILL, crashHandler);
#else
    struct sigaction action{};
    action.sa_handler = crashHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESETHAND;

    ::sigaction(SIGSEGV, &action, nullptr);
    ::sigaction(SIGABRT, &action, nullptr);
    ::sigaction(SIGBUS, &action, nullptr);
    ::sigaction(SIGFPE, &action, nullptr);
    ::sigaction(SIGILL, &action, nullptr);
#endif
}

}
