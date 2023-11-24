#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <libevdev/libevdev.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>

static volatile int keepRunning = 1;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
void intHandler(int dummy) {
    keepRunning = 0;
}
void *mouseEventThread(void *arg) {
    int fd = *(int *)arg;

    while (keepRunning) {
        struct libevdev *dev = NULL;
        int err = libevdev_new_from_fd(fd, &dev);
        if (err < 0) {
            perror("Failed to initialize libevdev");
            close(fd);
            return NULL;
        }
        struct input_event ev;
        while (keepRunning && libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev) == 0) {
            pthread_mutex_lock(&mutex);
            struct timespec event_time;
            clock_gettime(CLOCK_REALTIME, &event_time);
            char timestamp[64];
            strftime(timestamp, sizeof(timestamp), "%H:%M:%S", localtime(&event_time.tv_sec));

            if (ev.type == EV_REL) {
                if (ev.code == REL_X) {
                    printf("[%s] Mouse moved in X direction: %d\n", timestamp, ev.value);
                } else if (ev.code == REL_Y) {
                    printf("[%s] Mouse moved in Y direction: %d\n", timestamp, ev.value);
                }
            } else if (ev.type == EV_KEY) {
                if (ev.code == BTN_LEFT) {
                    if (ev.value == 1) {
                        printf("[%s] Left mouse button pressed\n", timestamp);
                    } else if (ev.value == 0) {
                        printf("[%s] Left mouse button released\n", timestamp);
                    }
                } else if (ev.code == BTN_RIGHT) {
                    if (ev.value == 1) {
                        printf("[%s] Right mouse button pressed\n", timestamp);
                    } else if (ev.value == 0) {
                        printf("[%s] Right mouse button released\n", timestamp);
                    }
                }
            }
            pthread_mutex_unlock(&mutex);
        }
        libevdev_free(dev);
    }
    return NULL;
}

int main() {
    signal(SIGINT, intHandler);
    int fd = open("/dev/input/event17", O_RDONLY);
    if (fd < 0) {
        perror("Failed to open input device");
        return 1;
    }
    struct libevdev *dev = NULL;
    int err = libevdev_new_from_fd(fd, &dev);
    if (err < 0) {
        perror("Failed to initialize libevdev");
        close(fd);
        return 1;
    }
    if (!libevdev_has_event_type(dev, EV_REL) || !libevdev_has_event_code(dev, EV_REL, REL_X)) {
        printf("This device is not a mouse.\n");
        libevdev_free(dev);
        close(fd);
        return 1;
    }
    int x = 0;
    int y = 0;
    pthread_t thread;
    if (pthread_create(&thread, NULL, mouseEventThread, &fd)) {
        perror("Failed to create mouse event monitoring thread");
        return 1;
    }
    printf("Mouse testing program - Press 'q' to exit\n");
    while (keepRunning) {
        int c = getchar();
        if (c == 'q') {
            keepRunning = 0;
        }
    }
    pthread_join(thread, NULL);
    libevdev_free(dev);
    close(fd);

    return 0;
}