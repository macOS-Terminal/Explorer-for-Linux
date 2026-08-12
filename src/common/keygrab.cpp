#include "keygrab.h"

#ifdef EXPLORER_HAVE_X11GRAB
#include <QDebug>
#include <QThread>

#include <X11/X.h>
#include <X11/keysym.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xproto.h>
#endif

namespace {

class NoopBackend : public KeyBackend
{
public:
    explicit NoopBackend(QObject *parent = nullptr) : KeyBackend(parent) {}
    QString backendName() const override { return QStringLiteral("noop-crashscreen-only"); }
};

#ifdef EXPLORER_HAVE_X11GRAB
class X11GrabThread : public QThread
{
    Q_OBJECT
public:
    explicit X11GrabThread(QObject *parent = nullptr) : QThread(parent) {}

signals:
    void emergencyExitTriggered();
    void relaunchRequested();

protected:
    void run() override
    {
        xcb_connection_t *conn = xcb_connect(nullptr, nullptr);
        if (xcb_connection_has_error(conn)) {
            xcb_disconnect(conn);
            return;
        }
        const xcb_setup_t *setup = xcb_get_setup(conn);
        xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
        xcb_screen_t *screen = it.data;
        if (!screen) {
            xcb_disconnect(conn);
            return;
        }
        xcb_key_symbols_t *syms = xcb_key_symbols_alloc(conn);
        xcb_keycode_t *f10All = xcb_key_symbols_get_keycode(syms, XK_F10);
        xcb_keycode_t *eAll = xcb_key_symbols_get_keycode(syms, XK_e);
        xcb_keycode_t f10 = f10All ? f10All[0] : 0;
        xcb_keycode_t e = eAll ? eAll[0] : 0;

        xcb_grab_key(conn, 0, screen->root, XCB_MOD_MASK_SHIFT, f10,
                     XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
        xcb_grab_key(conn, 0, screen->root, XCB_MOD_MASK_LOCK | XCB_MOD_MASK_SHIFT, f10,
                     XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
        xcb_grab_key(conn, 0, screen->root, XCB_MOD_MASK_4, e,
                     XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
        xcb_flush(conn);

        for (;;) {
            xcb_generic_event_t *ev = xcb_wait_for_event(conn);
            if (!ev)
                break;
            uint8_t type = ev->response_type & ~0x80;
            if (type == XCB_KEY_PRESS && (ev->pad0 & 0x80) == 0) {
                auto *kp = reinterpret_cast<xcb_key_press_event_t *>(ev);
                bool shifted = (kp->state & XCB_MOD_MASK_SHIFT) != 0;
                bool superHeld = (kp->state & XCB_MOD_MASK_4) != 0;
                if (kp->detail == f10 && shifted)
                    emit emergencyExitTriggered();
                else if (kp->detail == e && superHeld)
                    emit relaunchRequested();
            }
            free(ev);
        }
        xcb_key_symbols_free(syms);
        xcb_disconnect(conn);
    }
};

class X11Backend : public KeyBackend
{
public:
    explicit X11Backend(QObject *parent = nullptr) : KeyBackend(parent)
    {
        m_thread = new X11GrabThread(this);
        connect(m_thread, &X11GrabThread::emergencyExitTriggered, this, &KeyBackend::emergencyExitTriggered);
        connect(m_thread, &X11GrabThread::relaunchRequested, this, &KeyBackend::relaunchRequested);
        m_thread->start();
    }
    ~X11Backend() override
    {
        m_thread->requestInterruption();
        m_thread->quit();
    }
    QString backendName() const override { return QStringLiteral("x11-xgrabkey"); }

private:
    X11GrabThread *m_thread;
};
#else
#endif

} // namespace

KeyBackend::~KeyBackend() = default;

KeyBackend *KeyBackend::create(QObject *parent)
{
#ifdef EXPLORER_HAVE_X11GRAB
    QByteArray session = qgetenv("XDG_SESSION_TYPE");
    if (session.isEmpty() || session == "x11" || session == "tty")
        return new X11Backend(parent);
    return new NoopBackend(parent);
#else
    return new NoopBackend(parent);
#endif
}

#ifdef EXPLORER_HAVE_X11GRAB
#include "keygrab.moc"
#endif