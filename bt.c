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

typedef struct scrollback {
    float height;
    float ypos;
    int capacity;
    int length;
    char *buf;
} scrollback;

// PTY
int master;
int slave;

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

int read_pty(scrollback *sb) {
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
        if (sb->length + nread > sb->capacity) {
            sb->capacity *= 2;
            sb->buf = realloc(sb->buf, sb->capacity);
        }
        if (readbuf[nread - 1] == '\n') {
            printf("this\n");
            nread--;
        }
        int nreturns = 0;
        for (int i = 0; i < nread; i++) {
            if (readbuf[i] == '\r') {
                nreturns++;
                for (int j = i; j < nread; j++) {
                    readbuf[j] = readbuf[j + 1];
                }
            }
        }
        readbuf[nread - nreturns] = '\0';
        strcat(sb->buf, readbuf);
        return nread - nreturns;
    }
}

int main(void) {
    spawn();
 
    scrollback sb = { 0 };

    const int screenWidth = 800;
    const int screenHeight = 550;

    InitWindow(screenWidth, screenHeight, "basic term");

    SetTargetFPS(120);

    Font *fontDefault = load_font();

    sb.capacity = 2048;
    sb.buf = malloc(sb.capacity);
    sb.buf[0] = '\0';
    sb.length = 0;
    char buf[128];
    int buf_len = 0;
    
    int flags = fcntl(master, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(master, F_SETFL, flags);
    fd_set rset;
    FD_ZERO(&rset);
    FD_SET(master, &rset);

    sb.ypos = 0;

    while (!WindowShouldClose()) {
        int key = GetCharPressed();
        fontsize += GetMouseWheelMove();
        while (key > 0) {
            if ((key >= 32) && (key <= 125)) {
                buf[buf_len] = (char)key;
                buf[buf_len + 1] = '\0';
                buf_len++;
                sb.buf[sb.length] = (char)key;
                sb.buf[sb.length + 1] = '\0';
                sb.length++;
            }
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_ENTER)) {
            sb.length -= buf_len;
            sb.buf[sb.length] = '\0';
            write(master, buf, buf_len);
            write(master, "\r", 1);
            buf[0] = '\0';
            buf_len = 0;
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {
            if (buf_len > 0) {
                buf_len--;
                buf[buf_len] = '\0';
                sb.length--;
                sb.buf[sb.length] = '\0';
            }
        }

        // Drawing
        BeginDrawing();
        {
            int nread = read_pty(&sb);
            if (nread > 0) {
                sb.length += nread;
                new_read = true;
            }
            ClearBackground(RAYWHITE);
            DrawTextEx(*fontDefault, sb.buf, (Vector2){ 0, sb.ypos }, fontsize, 1, BLACK);
        }
        EndDrawing();
    }

    free(sb.buf);
    close(master);
    CloseWindow();
}
