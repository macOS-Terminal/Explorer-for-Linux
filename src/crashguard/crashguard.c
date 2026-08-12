#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <wayland-client.h>
#include "wlr-layer-shell-protocol.h"

#define ANCHOR_ALL 15
#define LAYER_TOP 2
#define LAYER_BACKGROUND 0
#define KEY_EXCLUSIVE 1
#define KEY_NONE 0

#define KEY_E 18
#define KEY_F10 68
#define KEY_SHIFT_L 42
#define KEY_SHIFT_R 54
#define KEY_SUPER_L 125
#define KEY_SUPER_R 126

enum state_flags {
    STATE_QUIT = 1 << 0,
    STATE_DEMOTE = 1 << 1,
    STATE_PROMOTE = 1 << 2,
};

static volatile sig_atomic_t g_flags = 0;
static struct wl_display *dl;
static struct wl_compositor *compositor;
static struct zwlr_layer_shell_v1 *shell;
static struct wl_surface *surface;
static struct zwlr_layer_surface_v1 *layer;
static struct wl_shm *shm;
static struct wl_seat *seat;
static struct wl_keyboard *keyboard;
static struct wl_buffer *buffer;
static uint32_t buf_w, buf_h;
static uint32_t fill_color;
static int shift_down;
static int super_down;
static int g_layer = -1;

static void on_signal(int sig)
{
    switch (sig) {
    case SIGUSR1:
        g_flags |= STATE_DEMOTE;
        break;
    case SIGUSR2:
        g_flags |= STATE_PROMOTE;
        break;
    case SIGTERM:
    case SIGINT:
        g_flags |= STATE_QUIT;
        break;
    }
}

static void set_signal_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    prctl(PR_SET_PDEATHSIG, SIGTERM);
}

static void spawn_killall(void)
{
    const char *app = NULL;
    char candidate[8192];
    struct stat st;
    ssize_t n = readlink("/proc/self/exe", candidate, sizeof(candidate) - 1);
    if (n > 0) {
        char *slash = strrchr(candidate, '/');
        if (slash) {
            strcpy(slash + 1, "explorer-killall");
            if (stat(candidate, &st) == 0)
                app = candidate;
        }
    }
    if (!app) {
        const char *dir = getenv("EXPLORER_HELPER_DIR");
        if (dir && *dir) {
            snprintf(candidate, sizeof(candidate), "%s/explorer-killall", dir);
            if (stat(candidate, &st) == 0)
                app = candidate;
        }
    }
    pid_t pid = fork();
    if (pid == 0) {
        if (app)
            execl(app, "explorer-killall", "--session", NULL);
        execlp("explorer-killall", "explorer-killall", "--session", NULL);
        _exit(127);
    }
}

static void request_relaunch(void)
{
    const char nl = '\n';
    ssize_t ignored = write(STDOUT_FILENO, &nl, 1);
    (void)ignored;
}

static void handle_key(uint32_t keycode, uint32_t state)
{
    int pressed = (state & 1) == 1;
    int repeat = (state & 2) != 0;
    if (getenv("EXPLORER_CG_DEBUG"))
        dprintf(STDERR_FILENO, "cg-key: %u state=%u\n", keycode, state);
    if (keycode == KEY_SHIFT_L || keycode == KEY_SHIFT_R) {
        shift_down = pressed;
        return;
    }
    if (keycode == KEY_SUPER_L || keycode == KEY_SUPER_R) {
        super_down = pressed;
        return;
    }
    if (repeat || !pressed)
        return;
    if (keycode == KEY_E && super_down)
        request_relaunch();
    if (keycode == KEY_F10 && shift_down)
        spawn_killall();
}

static void keyboard_keymap(void *d, struct wl_keyboard *k, uint32_t fmt, int32_t fd, uint32_t size)
{
    close(fd);
}

static void keyboard_enter(void *d, struct wl_keyboard *k, uint32_t serial, struct wl_surface *s, struct wl_array *keys)
{
    (void)d; (void)k; (void)serial; (void)s; (void)keys;
}

static void keyboard_leave(void *d, struct wl_keyboard *k, uint32_t serial, struct wl_surface *s)
{
    (void)d; (void)k; (void)serial; (void)s;
}

static void keyboard_key(void *d, struct wl_keyboard *k, uint32_t serial, uint32_t time, uint32_t keycode, uint32_t state)
{
    (void)d; (void)k; (void)serial; (void)time;
    handle_key(keycode, state);
}

static void keyboard_modifiers(void *d, struct wl_keyboard *k, uint32_t serial, uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group)
{
    (void)d; (void)k; (void)serial; (void)depressed; (void)latched; (void)locked; (void)group;
}

static void keyboard_repeat(void *d, struct wl_keyboard *k, int32_t rate, int32_t delay)
{
    (void)d; (void)k; (void)rate; (void)delay;
}

static const struct wl_keyboard_listener kb_listener = {
    keyboard_keymap,
    keyboard_enter,
    keyboard_leave,
    keyboard_key,
    keyboard_modifiers,
    keyboard_repeat,
};

static void seat_capabilities(void *d, struct wl_seat *s, uint32_t caps)
{
    (void)d;
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !keyboard)
        keyboard = wl_seat_get_keyboard(s);
}

static void seat_name(void *d, struct wl_seat *s, const char *name)
{
    (void)d; (void)s; (void)name;
}

static const struct wl_seat_listener seat_listener = {
    seat_capabilities,
    seat_name,
};

static void registry_global(void *d, struct wl_registry *reg, uint32_t name, const char *iface, uint32_t version)
{
    if (strcmp(iface, wl_compositor_interface.name) == 0) {
        compositor = wl_registry_bind(reg, name, &wl_compositor_interface, 4);
    } else if (strcmp(iface, wl_shm_interface.name) == 0) {
        shm = wl_registry_bind(reg, name, &wl_shm_interface, 1);
    } else if (strcmp(iface, wl_seat_interface.name) == 0) {
        uint32_t v = version < 7 ? version : 7;
        seat = wl_registry_bind(reg, name, &wl_seat_interface, v);
        wl_seat_add_listener(seat, &seat_listener, NULL);
    } else if (strcmp(iface, zwlr_layer_shell_v1_interface.name) == 0) {
        shell = wl_registry_bind(reg, name, &zwlr_layer_shell_v1_interface, 4);
    }
}

static void registry_remove(void *d, struct wl_registry *reg, uint32_t name)
{
    (void)d; (void)reg; (void)name;
}

static const struct wl_registry_listener registry_listener = {
    registry_global,
    registry_remove,
};

static void make_buffer(uint32_t w, uint32_t h)
{
    if (buffer && w == buf_w && h == buf_h)
        return;
    struct wl_buffer *newbuf = NULL;
    char path[64];
    snprintf(path, sizeof(path), "/explorer-cg-%d", (int)getpid());
    int fd = shm_open(path, O_CREAT | O_RDWR | O_EXCL, 0600);
    if (fd < 0)
        fd = mkstemp(path);
    if (fd < 0)
        return;
    int stride = (int)w * 4;
    size_t size = (size_t)stride * h;
    if (ftruncate(fd, (off_t)size) < 0)
        goto out;
    uint32_t *px = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (px == MAP_FAILED)
        goto out;
    for (size_t i = 0; i < (size_t)w * h; i++)
        px[i] = fill_color;
    munmap(px, size);
    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, (int32_t)size);
    newbuf = wl_shm_pool_create_buffer(pool, 0, (int32_t)w, (int32_t)h, stride, WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);
out:
    close(fd);
    shm_unlink(path);
    if (buffer)
        wl_buffer_destroy(buffer);
    buffer = newbuf;
    buf_w = w;
    buf_h = h;
}

static void layer_configure(void *d, struct zwlr_layer_surface_v1 *ls, uint32_t serial, uint32_t w, uint32_t h)
{
    (void)d;
    zwlr_layer_surface_v1_ack_configure(ls, serial);
    make_buffer(w, h);
    if (!buffer)
        return;
    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_damage(surface, 0, 0, (int32_t)w, (int32_t)h);
    wl_surface_commit(surface);
}

static void layer_closed(void *d, struct zwlr_layer_surface_v1 *ls)
{
    (void)d;
    g_flags |= STATE_QUIT;
}

static const struct zwlr_layer_surface_v1_listener layer_listener = {
    layer_configure,
    layer_closed,
};

static void relayer(int new_layer, int interactivity)
{
    if (g_layer == new_layer)
        return;
    if (layer) {
        zwlr_layer_surface_v1_destroy(layer);
        layer = NULL;
    }
    if (surface) {
        wl_surface_destroy(surface);
        surface = NULL;
    }
    if (!compositor || !shell)
        return;
    surface = wl_compositor_create_surface(compositor);
    layer = zwlr_layer_shell_v1_get_layer_surface(shell, surface, NULL, new_layer, "explorer-crashguard");
    zwlr_layer_surface_v1_add_listener(layer, &layer_listener, NULL);
    zwlr_layer_surface_v1_set_anchor(layer, ANCHOR_ALL);
    zwlr_layer_surface_v1_set_exclusive_zone(layer, -1);
    zwlr_layer_surface_v1_set_size(layer, 0, 0);
    zwlr_layer_surface_v1_set_keyboard_interactivity(layer, interactivity);
    wl_surface_commit(surface);
    wl_display_flush(dl);
    g_layer = new_layer;
}

static void apply_demote(void)
{
    relayer(LAYER_BACKGROUND, KEY_NONE);
}

static void apply_promote(void)
{
    relayer(LAYER_TOP, KEY_EXCLUSIVE);
}

int main(int argc, char **argv)
{
    int background = 0;

    dl = wl_display_connect(NULL);
    if (!dl)
        return 3;

    if (argc > 1 && strcmp(argv[1], "probe") == 0) {
        /* layer-shell 支持探测：winlogin 据此决定走 crashguard 还是 Qt 兜底窗 */
        struct wl_registry *reg = wl_display_get_registry(dl);
        wl_registry_add_listener(reg, &registry_listener, NULL);
        wl_display_roundtrip(dl);
        wl_display_disconnect(dl);
        return (compositor && shell) ? 0 : 1;
    }

    if (argc > 1) {
        if (strcmp(argv[1], "white") == 0)
            fill_color = 0x00FFFFFFu;
        else if (strcmp(argv[1], "black") == 0)
            fill_color = 0x00000000u;
        else
            return 2;
        if (argc > 2 && strcmp(argv[2], "background") == 0)
            background = 1;
    } else {
        return 2;
    }

    set_signal_handlers();

    struct wl_registry *reg = wl_display_get_registry(dl);
    wl_registry_add_listener(reg, &registry_listener, NULL);
    wl_display_roundtrip(dl);
    if (!compositor || !shm || !shell || !seat)
        return 4;

    relayer(background ? LAYER_BACKGROUND : LAYER_TOP,
            background ? KEY_NONE : KEY_EXCLUSIVE);

    for (;;) {
        if (g_flags & STATE_QUIT)
            break;
        if (g_flags & STATE_DEMOTE) {
            g_flags &= ~STATE_DEMOTE;
            apply_demote();
        }
        if (g_flags & STATE_PROMOTE) {
            g_flags &= ~STATE_PROMOTE;
            apply_promote();
        }
        while (wl_display_prepare_read(dl) != 0)
            wl_display_dispatch_pending(dl);
        struct pollfd pfd = { .fd = wl_display_get_fd(dl), .events = POLLIN };
        int ret = poll(&pfd, 1, 200);
        if (ret > 0)
            wl_display_read_events(dl);
        else
            wl_display_cancel_read(dl);
        wl_display_dispatch_pending(dl);
        wl_display_flush(dl);
    }

    wl_display_disconnect(dl);
    return 0;
}