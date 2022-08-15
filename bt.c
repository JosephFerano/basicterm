#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <asm-generic/ioctls.h>
#include <stdio.h>
#include <stdlib.h>
#include <pty.h>
#include <ctype.h>

// PTY
int master;
int slave;

// X11
Display *display;
int screen;
Window win;
GC gc;
int x11fd;

// Scrollback
char *scrollback;

void init_x(void) {
    unsigned long black,white;
    display = XOpenDisplay((char *) 0);
    screen = DefaultScreen(display);
    black = BlackPixel(display, screen);
    white = WhitePixel(display, screen);

    win = XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0, 500, 800, 5, white, black);

    XSetStandardProperties(display, win, "The window", "Hello", None, NULL, 0, NULL);
    XSelectInput(display, win, ExposureMask|KeyPressMask);
    gc = XCreateGC(display, win, 0, 0);
    XSetBackground(display, gc, white);
    XSetForeground(display, gc, black);
    x11fd = ConnectionNumber(display);
    XClearWindow(display, win);
    XMapRaised(display, win);
}

void close_x(void) {
    XFreeGC(display, gc);
    XDestroyWindow(display, win);
    XCloseDisplay(display);
    exit(0);
}

void spawn(void) {
    openpty(&master, &slave, NULL, NULL, NULL);
    pid_t p = fork();
    if (p == 0) {
        close(master);

        setsid();
        ioctl(slave, TIOCSCTTY, NULL);
        dup2(slave, 0);
        dup2(slave, 1);
        dup2(slave, 2);

        close(slave);

        execle("/bin/dash", "-/bin/dash", (char *)NULL, (char *[]){ "TERM=dumb", NULL });
        exit(0);
    } else {
        close(slave);
    }
}

void update(void) {
    XEvent event;
    KeySym key;
    char buf[1];
    char x11inputbuf[255];
    fd_set readable;
    /* char tmp[512]; */
    scrollback = malloc(2048);
    scrollback[0] = '\0';
    int sbcount = 0;
    int maxfd = master > x11fd ? master : x11fd;

    for (;;) {
        FD_ZERO(&readable);
        FD_SET(master, &readable);
        FD_SET(x11fd, &readable);
        select(maxfd + 1, &readable, NULL, NULL, NULL);
        if (FD_ISSET(master, &readable)) {
            read(master, buf, 1);
            scrollback[sbcount++] = *buf;
            scrollback[sbcount] = '\0';
            XClearWindow(display, win);
            char *curr = scrollback;
            while (*curr) {
                if (!iscntrl(*curr)) {
                    XSetForeground(display, gc, 255);
                    XDrawString(display, win, gc, 10, 10, curr, sbcount);
                }
                curr++;
            }
        }
        while (XPending(display)) {
            XNextEvent(display, &event);
            switch (event.type) {
                // This gets called when the window is resized
                case Expose: // Resized
                    XClearWindow(display, win);
                    break;
                case KeyPress:
                    if (XLookupString(&event.xkey,x11inputbuf,255,&key,0) == 1) {
                        switch (x11inputbuf[0]) {
                            case 'q':
                                close_x();
                                break;
                            default:
                                write(master, &x11inputbuf[0], 1);
                        }
                    }
                    break;
            }
        }
    }
}

int main(void) {
    init_x();
    spawn();

    update();
    return 0;
}

