#pragma once

namespace tibia::app {

/// SIGSEGV / SIGABRT / SIGBUS / SIGFPE / SIGILL → stderr backtrace, then re-raise.
/// Install before QGuiApplication so early faults are covered too.
void installCrashLog();

}
