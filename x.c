static void run(void) {
    XEvent ev;
    int w = xw.w, h = xw.h;
    fd_set rfd;
    int xfd = XConnectionNumber(xw.dpy), xev, blinkset = 0, dodraw = 0;
    struct timespec drawtimeout, *tv = NULL, now, last, lastblink;
    long deltatime;

    /* Waiting for window mapping */
    do {
        XNextEvent(xw.dpy, &ev);
        /*
         * This XFilterEvent call is required because of XOpenIM. It
         * does filter out the key event and some client message for
         * the input method too.     */
        if(XFilterEvent(&ev, None)) continue;
        if (ev.type == ConfigureNotify) {
            w = ev.xconfigure.width;
            h = ev.xconfigure.height;
        }
    } while(ev.type != MapNotify);

    ttynew();
    cresize(w, h);

    clock_gettime(CLOCK_MONOTONIC, &last);
    lastblink = last;

    for (xev = actionfps;;) {
        FD_ZERO(&rfd);
        FD_SET(cmdfd, &rfd);
        FD_SET(xfd, &rfd);

        if (pselect(MAX(xfd, cmdfd) + 1, &rfd, NULL, NULL, tv, NULL) < 0) {
            if (errno == EINTR) continue;
            die("select failed: %s\n", strerror(errno));
        }
        if (FD_ISSET(cmdfd, &rfd)) {
            ttyread();
            if (blinktimeout) {
                blinkset = tattrset(ATTR_BLINK);
                if (!blinkset) MODBIT(term.mode, 0, MODE_BLINK);
            }
        }

        if (FD_ISSET(xfd, &rfd)) xev = actionfps;

        clock_gettime(CLOCK_MONOTONIC, &now);
        drawtimeout.tv_sec = 0;
        drawtimeout.tv_nsec =  (1000 * 1E6)/ xfps;
        tv = &drawtimeout;

        dodraw = 0;
        if (blinktimeout && TIMEDIFF(now, lastblink) > blinktimeout) {
            tsetdirtattr(ATTR_BLINK);
            term.mode ^= MODE_BLINK;
            lastblink = now;
            dodraw = 1;
        }
        deltatime = TIMEDIFF(now, last);
        if(deltatime > 1000 / (xev ? xfps : actionfps)) {
            dodraw = 1;
            last = now;
        }

        if (dodraw) {
            while (XPending(xw.dpy)) {
                XNextEvent(xw.dpy, &ev);
                if (XFilterEvent(&ev, None)) continue;
                if (handler[ev.type]) (handler[ev.type])(&ev);
            }

            draw();
            XFlush(xw.dpy);

            if (xev && !FD_ISSET(xfd, &rfd)) xev--;
            if (!FD_ISSET(cmdfd, &rfd) && !FD_ISSET(xfd, &rfd)) {
                if (blinkset) {
                    if (TIMEDIFF(now, lastblink) > blinktimeout) {
                        drawtimeout.tv_nsec = 1000;
                    } else {
                        drawtimeout.tv_nsec =
                            (1E6 * (blinktimeout - TIMEDIFF(now, lastblink)));
                    }
                    drawtimeout.tv_sec = drawtimeout.tv_nsec / 1E9;
                    drawtimeout.tv_nsec %= (long)1E9;
                } else {
                    tv = NULL;
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    xw.l = xw.t = 0;
    xw.isfixed = False;
    xw.cursor = cursorshape;

    ARGBEGIN {
        case 'a':
            allowaltscreen = 0;
            break;
        case 'c':
            opt_class = EARGF(usage());
            break;
        case 'e':
            if(argc > 0) --argc, ++argv;
            goto run;
        case 'f':
            opt_font = EARGF(usage());
            break;
        case 'g':
            xw.gm = XParseGeometry(EARGF(usage()), &xw.l, &xw.t, &cols, &rows);
            break;
        case 'i':
            xw.isfixed = 1;
            break;
        case 'o':
            opt_io = EARGF(usage());
            break;
        case 'l':
            opt_line = EARGF(usage());
            break;
        case 't':
        case 'T':
            opt_title = EARGF(usage());
            break;
        case 'w':
            opt_embed = EARGF(usage());
            break;
        case 'v':
        default:
            usage();
    }
    ARGEND;

run:
    if(argc > 0) {
        /* eat all remaining arguments */
        opt_cmd = argv;
        if (!opt_title && !opt_line) opt_title = basename(xstrdup(argv[0]));
    }
    setlocale(LC_CTYPE, "");
    XSetLocaleModifiers("");
    config_init();
#ifdef VIM_VERSION
    setenv("ST_TERM","TRUE",1);
#endif
    tnew(MAX(cols, 1), MAX(rows, 1));
    xinit();
    selinit();
    run();

    return 0;
}

