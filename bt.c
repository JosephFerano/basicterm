#define _GNU_SOURCE
#include <sys/select.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pty.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <raylib.h>

// PTY
int master;
int slave;

// Scrollback
char *scrollback;

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

        execle("/bin/dash", "-/bin/dash", (char *)NULL, (char *[]){ "TERM=dumb", NULL });
    } else {
        close(slave);
    }
}

float fontsize = 12;

Font *load_font() {
    unsigned int file_size = 0;
    char *font = "./fira.ttf";
    unsigned char *fontdata = LoadFileData(font, &file_size);
    Font *fontDefault = calloc(1, sizeof(Font));
    fontDefault->baseSize = fontsize;
    fontDefault->glyphCount = 95;

    fontDefault->glyphs = LoadFontData(fontdata, file_size, 16, 0, 95, FONT_DEFAULT);
    Image atlas = GenImageFontAtlas(fontDefault->glyphs, &fontDefault->recs, 95, 16, 4, 0);
    fontDefault->texture = LoadTextureFromImage(atlas);
    UnloadImage(atlas);
    return fontDefault;
}

int read_pty(char *buf) {
    ssize_t nread;
    char readbuf[256];
    switch (nread = read(master, readbuf, sizeof(readbuf))) {
    case -1:
        if (errno == EAGAIN) {
            return 0;
        } else {
            return 0;
        }
    case 0:
        printf("EOF\n");
        return 0;
    default:
        if (readbuf[nread - 1] == '\n') {
            printf("this\n");
            nread--;
        }
        readbuf[nread] = '\0';
        strcat(buf, readbuf);
        printf("Amount of bytes read %lu\n", nread);
        printf("FD: %i Buf %s\n", master, buf);
        return nread;
    }
}

int main(void) {
    spawn();

    const int screenWidth = 800;
    const int screenHeight = 550;

    InitWindow(screenWidth, screenHeight, "basic term");

    SetTargetFPS(120);

    Font *fontDefault = load_font();
    
    scrollback = malloc(4098);
    scrollback[0] = '\0';
    int sb_len = 0;
    char buf[128];
    int buf_len = 0;
    
    int flags = fcntl(master, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(master, F_SETFL, flags);
    fd_set rset;
    FD_ZERO(&rset);
    FD_SET(master, &rset);

    while (!WindowShouldClose()) {
        int key = GetCharPressed();
        fontsize += GetMouseWheelMove();
        while (key > 0) {
            if ((key >= 32) && (key <= 125)) {
                buf[buf_len] = (char)key;
                buf[buf_len + 1] = '\0';
                buf_len++;
                scrollback[sb_len] = (char)key;
                scrollback[sb_len + 1] = '\0';
                sb_len++;
            }
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_ENTER)) {
            sb_len -= buf_len;
            scrollback[sb_len] = '\0';
            write(master, buf, buf_len);
            write(master, "\r", 1);
            buf[0] = '\0';
            buf_len = 0;
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {
            if (buf_len > 0) {
                buf_len--;
                buf[buf_len] = '\0';
                sb_len--;
                scrollback[sb_len] = '\0';
            }
        }

        // Drawing
        BeginDrawing();
        /* update(buf, &buf_len, rset); */
        {
            int nread = read_pty(scrollback);
            if (nread > 0) {
                /* printf("Amount of bytes read %i\n", nread); */
                sb_len += nread;
            }
            ClearBackground(RAYWHITE);
            DrawTextEx(*fontDefault, scrollback, (Vector2){ 10 , 10 }, fontsize, 1, BLACK);
        }
        EndDrawing();
    }

    close(master);
    CloseWindow();
    return 0;
}
