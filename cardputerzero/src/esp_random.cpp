// SPDX-License-Identifier: AGPL-3.0-only
// SPDX-FileCopyrightText: 2026 CYD32 contributors
#include "esp_random.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/random.h>
extern "C" void esp_fill_random(void *buf, size_t len) {
  auto *p = static_cast<unsigned char *>(buf);
  while (len) {
    const ssize_t n = getrandom(p, len, 0);
    if (n > 0) { p += n; len -= (size_t)n; continue; }
    if (errno == EINTR) continue;
    const int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) abort();
    while (len) {
      const ssize_t r = read(fd, p, len);
      if (r < 0 && errno == EINTR) continue;
      if (r <= 0) { close(fd); abort(); }
      p += r;
      len -= (size_t)r;
    }
    close(fd);
  }
}
extern "C" uint32_t esp_random(void) {
  uint32_t v = 0;
  esp_fill_random(&v, sizeof v);
  return v;
}
