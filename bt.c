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

typedef struct file_descriptors {
    int master;
    int child;
} file_descriptors;

void spawn(file_descriptors *fds) {
    openpty(&fds->master, &fds->child, NULL, NULL, NULL);
    pid_t p = fork();
    if (p == 0) {
        close(fds->master);

        setsid();
        ioctl(fds->child, TIOCSCTTY, NULL);
        dup2(fds->child, 0);
        dup2(fds->child, 1);
        dup2(fds->child, 2);

        execle("/bin/dash", "-/bin/dash", (char *)NULL, (char *[]){ "TERM=dumb", NULL });
    } else {
        close(fds->child);
    }
}

static float fontsize = 12;

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

int read_pty(file_descriptors *fds, scrollback *sb) {
    ssize_t nread;
    char readbuf[256];
    switch (nread = read(fds->master, readbuf, sizeof(readbuf))) {
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
            nread--;
        }
        readbuf[nread] = '\0';
        strcat(sb->buf, readbuf);
        return nread;
    }
}

int main(void) {
    scrollback sb = { 0 };
    file_descriptors fds = { 0 };

    spawn(&fds);

    const int screenWidth = 800;
    const int screenHeight = 550;

    InitWindow(screenWidth, screenHeight, "basic term");

    SetTargetFPS(60);

    Font *fontDefault = load_font();

    sb.capacity = 2048;
    sb.buf = malloc(sb.capacity);
    sb.buf[0] = '\0';
    sb.length = 0;
    char buf[128];
    int buf_len = 0;

    int flags = fcntl(fds.master, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(fds.master, F_SETFL, flags);
    fd_set rset;
    FD_ZERO(&rset);
    FD_SET(fds.master, &rset);

    sb.ypos = 0;

    bool new_read = false;
    bool new_char = false;
    while (!WindowShouldClose()) {
        float scroll_speed = 35.5f;
        sb.ypos += GetMouseWheelMoveV().y * scroll_speed;
        sb.height = MeasureTextEx(*fontDefault, sb.buf, fontsize, 1).y;

        if (new_read || new_char) {
            if (sb.height - abs((int)sb.ypos) + fontsize > screenHeight) {
                sb.ypos = -(sb.height - screenHeight) - fontsize;
            }
            new_read = false;
            new_char = false;
        } else {
            if (sb.ypos > 0) {
                sb.ypos = 0;
            } else if (abs((int)sb.ypos) > sb.height - fontsize) {
                sb.ypos = -(sb.height - fontsize);
            }
        }

        int key = GetCharPressed();
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
            new_char = true;
        }

        if (IsKeyPressed(KEY_ENTER)) {
            sb.length -= buf_len;
            sb.buf[sb.length] = '\0';
            write(fds.master, buf, buf_len);
            write(fds.master, "\r", 1);
            buf[0] = '\0';
            buf_len = 0;
            new_char = true;
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {
            if (buf_len > 0) {
                buf_len--;
                buf[buf_len] = '\0';
                sb.length--;
                sb.buf[sb.length] = '\0';
            }
            new_char = true;
        }

        // Drawing
        BeginDrawing();
        {
            int nread = read_pty(&fds, &sb);
            if (nread > 0) {
                sb.length += nread;
                new_read = true;
            }
            ClearBackground(BLACK);
            Color current_color = RAYWHITE;
            int col_max = 200;
            char row_buf[col_max];
            int nrow = 0;
            int ncol = 0;
            int row_height = 18;
            float row_posx = 0;
            for (int c = 0; c <= sb.length; c++) {
                if (sb.buf[c] == '\r') {
                    continue;
                } else if (sb.buf[c] == '\n' || sb.buf[c] == '\0' || ncol >= col_max) {
                    row_buf[ncol] = '\0';
                    Vector2 pos = { row_posx, nrow * row_height + sb.ypos };
                    DrawTextEx(*fontDefault, row_buf, pos, fontsize, 0, current_color);
                    nrow++;
                    ncol = 0;
                    row_posx = 0;
                } else if (sb.buf[c] == '\x1b') {
                    int c2 = c + 1;
                    // Control Sequence Introducer
                    int csi_args[16];
                    char csi_code;
                    int nargs = 0;
                    if (sb.buf[c2] == '[') {
                        char *esc_buf = sb.buf + c2 + 1;
                        while (true) {
                            char *substr;
                            long num = strtol(esc_buf, &substr, 10);
                            if (esc_buf != substr) {
                                csi_args[nargs++] = num;
                                esc_buf = substr;
                                if (substr[0] == ';') {
                                    esc_buf++;
                                }
                                continue;
                            }
                            csi_code = substr[0];
                            Color new_color = current_color;
                            switch (csi_code) {
                                case 'm':
                                    for (int i = 0; i < nargs; i++) {
                                        if (csi_args[i] == 31) {
                                            new_color = RED;
                                        } else if (csi_args[i] == 32) {
                                            new_color = GREEN;
                                        } else if (csi_args[i] == 33) {
                                            new_color = YELLOW;
                                        } else if (csi_args[i] == 34) {
                                            new_color = BLUE;
                                        } else if (csi_args[i] == 35) {
                                            new_color = MAGENTA;
                                        } else if (csi_args[i] == 36) {
                                            new_color = SKYBLUE;
                                        } else if (csi_args[i] == 0) {
                                            new_color = RAYWHITE;
                                        }
                                    }
                                    row_buf[ncol] = '\0';
                                    Vector2 pos = { row_posx, nrow * row_height + sb.ypos };
                                    DrawTextEx(*fontDefault, row_buf, pos, fontsize, 0, current_color);
                                    current_color = new_color;
                                    ncol = 0;
                                    c += (substr) - (sb.buf + c);
                                    int width = MeasureTextEx(*fontDefault, row_buf, fontsize, 1).x;
                                    row_posx += width + 0.85;
                                  break;
                            }
                            break;
                        }
                    }
                } else {
                    row_buf[ncol++] = sb.buf[c];
                }
            }
        }
        EndDrawing();
    }

    free(sb.buf);
    close(fds.master);
    CloseWindow();
}
