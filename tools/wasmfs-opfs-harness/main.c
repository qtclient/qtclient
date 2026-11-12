/*
 * Smoke: WasmFS OPFS backend under PROXY_TO_PTHREAD.
 * Mounts OPFS at /home/web_user, writes a tip-shaped path, then asks JS to
 * dump navigator.storage.getDirectory() so portal writers can match layout.
 */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <emscripten.h>
#include <emscripten/console.h>
#include <emscripten/wasmfs.h>

static const char* kHome = "/home/web_user";
static const char* kRelDir =
    "/home/web_user/.local/share/IdleOT/IdleOT/packages/IdleOT.app";
static const char* kFile =
    "/home/web_user/.local/share/IdleOT/IdleOT/packages/IdleOT.app/"
    "wasmfs-opfs-harness.txt";
static const char* kMsg = "idleot-wasmfs-opfs-ok";

static void mkdir_p(const char* path)
{
  char buf[512];
  size_t n = strlen(path);
  assert(n < sizeof(buf));
  memcpy(buf, path, n + 1);
  for (char* p = buf + 1; *p; ++p) {
    if (*p != '/')
      continue;
    *p = 0;
    if (mkdir(buf, 0777) != 0 && errno != EEXIST) {
      emscripten_console_errorf("mkdir %s failed errno=%d", buf, errno);
      assert(0);
    }
    *p = '/';
  }
  if (mkdir(buf, 0777) != 0 && errno != EEXIST) {
    emscripten_console_errorf("mkdir %s failed errno=%d", buf, errno);
    assert(0);
  }
}

int main(void)
{
  emscripten_console_log("wasmfs-opfs-harness: start");

  backend_t opfs = wasmfs_create_opfs_backend();
  assert(opfs);
  emscripten_console_log("wasmfs-opfs-harness: opfs backend created");

  /* /home may already exist on the memory backend; replace web_user with OPFS. */
  if (mkdir("/home", 0777) != 0 && errno != EEXIST) {
    emscripten_console_errorf("mkdir /home errno=%d", errno);
    assert(0);
  }
  int err = wasmfs_create_directory(kHome, 0777, opfs);
  if (err != 0) {
    emscripten_console_errorf("wasmfs_create_directory(%s) => %d errno=%d",
                              kHome, err, errno);
    assert(0);
  }
  emscripten_console_log("wasmfs-opfs-harness: mounted /home/web_user on OPFS");

  mkdir_p(kRelDir);

  int fd = open(kFile, O_RDWR | O_CREAT | O_TRUNC, 0666);
  assert(fd >= 0);
  ssize_t nw = write(fd, kMsg, strlen(kMsg));
  assert(nw == (ssize_t)strlen(kMsg));
  assert(fsync(fd) == 0);
  assert(close(fd) == 0);

  char buf[64] = {};
  fd = open(kFile, O_RDONLY);
  assert(fd >= 0);
  ssize_t nr = read(fd, buf, sizeof(buf) - 1);
  assert(nr == (ssize_t)strlen(kMsg));
  assert(strcmp(buf, kMsg) == 0);
  assert(close(fd) == 0);
  emscripten_console_logf("wasmfs-opfs-harness: roundtrip ok (%s)", buf);

  EM_ASM({
    Module.qtclientWasmfsOpfs = 1;
    Module.qtclientPersistentFsMounted = 1;
    if (typeof Module.onHarnessRoundtrip === 'function')
      Module.onHarnessRoundtrip();
  });

  emscripten_console_log("wasmfs-opfs-harness: done");
  return 0;
}
