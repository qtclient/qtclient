#pragma once

namespace tibia {
namespace app {

/// SIGSEGV / SIGABRT / SIGBUS / SIGFPE / SIGILL → stderr backtrace, then re-raise.
/// Install before QGuiApplication so early faults are covered too.
void installCrashLog();

}
}
