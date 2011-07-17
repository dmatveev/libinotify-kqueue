#include "inotify.h"

int main (int argc, char *argv[]) {
    int fd = inotify_init();
    (void) fd;
    return 0;
}
