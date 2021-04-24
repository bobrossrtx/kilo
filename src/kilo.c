/*** includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/

#define KILO_VERSION "0.0.1"
#define CTRL_KEY(key) ((key) & 0x1f)

enum editorKey {
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
};


/*** data ***/

struct editorConfig {
  // Cursor positions
  int cx, cy;

  int screenrows;
  int screencols;

  // Original terminal attributes
  struct termios orig_termios;
};

struct editorConfig _editorConfig;


/*** terminal ***/

void die(const char *err) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(err);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &_editorConfig.orig_termios) == -1)
    die("tcsetattr");
}

void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &_editorConfig.orig_termios) == -1) die("tcgetattr");
  atexit(disableRawMode); // Resets terminal attributes to normal on exit

  struct termios raw = _editorConfig.orig_termios;          // Assigning orig_termios -> raw

  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON); // Disabled input flags
  raw.c_oflag &= ~(OPOST);                                  // Disabled output flags
  raw.c_cflag |= (CS8);                                     // bit mask
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);          // Disabled local flags
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;                                      // Refresh rate (tenths (1/10)second)/100 miliseconds

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

int editorReadKey() {
  int nread;
  char key;

  while ((nread = read(STDIN_FILENO, &key, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }
  
  if (key == '\x1b') {
    char seq[3];

    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
        if (seq[2] == '~') {
          switch (seq[1]) {
            case '1': return HOME_KEY;
            case '4': return END_KEY;
            case '5': return PAGE_UP;
            case '6': return PAGE_DOWN;
            case '7': return HOME_KEY;
            case '8': return END_KEY;
          }
        }
      } else {
        switch (seq[1]) {
          case 'A': return ARROW_UP;
          case 'B': return ARROW_DOWN;
          case 'C': return ARROW_RIGHT;
          case 'D': return ARROW_LEFT;
          case 'H': return HOME_KEY;
          case 'F': return END_KEY;
        }
      }
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
        case 'H': return HOME_KEY;
        case 'F': return END_KEY;
      }
    }

    return '\x1b';
  } else {
    return key;
  }
}

/**
 * @brief low level (terminal) input
 * @return char 
 */
int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if (buf[i] == 'R') break;
    i++;
  }
  buf[i] = '\0';

  if (buf[0] != '\x1b' || buf[1] != '[') return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

  return 0;
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}


/*** append buffer ***/

struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);

  if (new == NULL) return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void abFree(struct abuf *ab) {
  free(ab->b);
}


/*** output ***/

void editorDrawRows(struct abuf *ab) {
  for (int y = 0; y < _editorConfig.screenrows; y++) {
    if (y == _editorConfig.screenrows / 3) {
      char welcome[80];
      int welcomelen = snprintf(welcome, sizeof(welcome), 
        "Kilo editor -- version %s", KILO_VERSION);
      if (welcomelen > _editorConfig.screencols) welcomelen = _editorConfig.screencols;
      int padding = (_editorConfig.screencols - welcomelen) / 2;
      if (padding) {
        abAppend(ab, "~", 1);
        padding--;
      }
      while (padding--) abAppend(ab, " ", 1);
      abAppend(ab, welcome, welcomelen);
    } else {
      abAppend(ab, "~", 1);
    }

    abAppend(ab, "\x1b[K", 4);
    if (y < _editorConfig.screenrows - 1) {
      abAppend(ab, "\r\n", 2);
    }
  }
}

void editorRefreshScreen() {
  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l", 6);
  abAppend(&ab, "\x1b[H", 3);

  editorDrawRows(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", _editorConfig.cy + 1, _editorConfig.cx + 1);
  abAppend(&ab, buf, strlen(buf));

  abAppend(&ab, "\x1b[?25h", 6);

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}


/*** input ***/

void editorMoveCursor(int key) {
  switch (key) {
    // WASD Keys
    case ARROW_LEFT:
      if (_editorConfig.cx != 0) {
        _editorConfig.cx--;
      }
      break;
    case ARROW_RIGHT:
      if (_editorConfig.cx != _editorConfig.screencols - 1) {
        _editorConfig.cx++;
      }
      break;
    case ARROW_UP:
      if (_editorConfig.cy != 0) {
        _editorConfig.cy--;
      }
      break;
    case ARROW_DOWN:
      if (_editorConfig.cy != _editorConfig.screenrows - 1) {
        _editorConfig.cy++;
      }
      break;
  }
}

void editorProcessKeypress() {
  int KEY_IN = editorReadKey();

  switch (KEY_IN) {
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;

    case HOME_KEY:
      _editorConfig.cx = 0;
      break;

    case END_KEY:
      _editorConfig.cx = _editorConfig.screencols - 1;
      break;

    case PAGE_UP:
    case PAGE_DOWN: 
      {
        int times = _editorConfig.screenrows;
        while (times--) editorMoveCursor(KEY_IN == PAGE_UP ? ARROW_UP : ARROW_DOWN);
      }
      break;
    
    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editorMoveCursor(KEY_IN);
      break;
  }
}


/*** init ***/

void initEditor() {
  _editorConfig.cx = 0;
  _editorConfig.cy = 0;

  if (getWindowSize(&_editorConfig.screenrows, &_editorConfig.screencols) == -2)
    die("getWindowSize");
}

int main() {
  enableRawMode();
  initEditor();

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  };
  return 0;
}
