/*** includes ***/

#define _DEFAULT_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>


/*** defines ***/

#define KILO_VERSION "0.0.1"
#define KILO_TAB_STOP 8

#define CTRL_KEY(k) ((k) & 0x1f)

#define TRUE 1
#define FALSE 0

enum editorKey {
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
};


/*** data ***/

typedef struct erow {
  int size;
  int rsize;
  char *chars;
  char *render;
} erow; // "Editor Row"

struct editorConfig {
  // Cursor positions
  int cx, cy;
  int rx;

  // Row offset
  int rowoff;
  int coloff;

  // Screen
  int screenrows;
  int screencols;

  // Text Rows
  int numrows;
  erow *row;

  // Original terminal attributes
  struct termios orig_termios;
};

struct editorConfig _editorConfig;


/*** terminal ***/

void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(s);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &_editorConfig.orig_termios) == -1)
    die("tcsetattr");
}

void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &_editorConfig.orig_termios) == -1) 
    die("tcgetattr");
  atexit(disableRawMode);

  struct termios raw = _editorConfig.orig_termios;
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) 
    die("tcsetattr");
}

int editorReadKey() {
  int nread;
  char key;
  while ((nread = read(STDIN_FILENO, &key, 1)) != 1)
    if (nread == -1 && errno != EAGAIN) die("read");

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
            case '3': return DEL_KEY;
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


/*** row operations ***/

int editorRowCxToRx(erow *row, int cx) {
  int rx = 0;
  for (int j = 0; j < cx; j++) {
    if (row->chars[j] == '\t')
      rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
    rx++;
  }
  return rx;
}

void editorUpdateRow(erow *row) {
  int tabs = 0;
  for (int j = 0; j < row->size; j++)
    if (row->chars[j] == '\t') tabs++;

  free(row->render);
  row->render = malloc(row->size + tabs*(KILO_TAB_STOP - 1) + 1);

  int idx = 0;
  for (int j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t') {
      row->render[idx++] = ' ';
      while (idx % KILO_TAB_STOP != 0) row->render[idx++] = ' ';
    } else {
      row->render[idx++] = row->chars[j];
    }
  }
  row->render[idx] = '\0';
  row->rsize = idx;
}

void editorAppendRow(char *s, size_t len) {
  _editorConfig.row = realloc(_editorConfig.row, sizeof(erow) * (_editorConfig.numrows + 1));

  int at = _editorConfig.numrows;
  _editorConfig.row[at].size = len;
  _editorConfig.row[at].chars = malloc(len + 1);
  memcpy(_editorConfig.row[at].chars, s, len);
  _editorConfig.row[at].chars[len] = '\0';

  _editorConfig.row[at].rsize = 0;
  _editorConfig.row[at].render = NULL;
  editorUpdateRow(&_editorConfig.row[at]);

  _editorConfig.numrows++;
}


/*** file i/o ***/

void editorOpen(char *filename) {
  FILE *fp = fopen(filename, "r");
  if (!fp) die("fopen");

  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 && (line[linelen - 1] == '\n' ||
                            line[linelen - 1] == '\r'))
      linelen--;
    editorAppendRow(line, linelen);
  }
  free(line);
  fclose(fp);
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

void editorScroll() {
  _editorConfig.rx = 0;
  if (_editorConfig.cy < _editorConfig.numrows)
    _editorConfig.rx = editorRowCxToRx(&_editorConfig.row[_editorConfig.cy], 
                                                          _editorConfig.cx);
  if (_editorConfig.cy < _editorConfig.rowoff)
    _editorConfig.rowoff = _editorConfig.cy;
  if (_editorConfig.cy >= _editorConfig.rowoff + _editorConfig.screenrows)
    _editorConfig.rowoff = _editorConfig.cy - _editorConfig.screenrows + 1;
  if (_editorConfig.rx < _editorConfig.coloff)
    _editorConfig.coloff = _editorConfig.rx;
  if (_editorConfig.rx >= _editorConfig.coloff + _editorConfig.screencols)
    _editorConfig.coloff = _editorConfig.rx - _editorConfig.screencols + 1;
}

void editorDrawRows(struct abuf *ab) {
  for (int y = 0; y < _editorConfig.screenrows; y++) {
    int filerow = y + _editorConfig.rowoff;
    if (filerow >= _editorConfig.numrows) {
      if (_editorConfig.numrows == 0 && y == _editorConfig.screenrows / 3) {
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
      } else abAppend(ab, "~", 1);
    } else {
      int len = _editorConfig.row[filerow].rsize - _editorConfig.coloff;
      if (len < 0) len = 0;
      if (len > _editorConfig.screencols) len = _editorConfig.screencols;
      abAppend(ab, &_editorConfig.row[filerow].render[_editorConfig.coloff], len);
    }

    abAppend(ab, "\x1b[K", 3);
    if (y < _editorConfig.screenrows - 1) abAppend(ab, "\r\n", 2);
  }
}

void editorRefreshScreen() {
  editorScroll();

  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l", 6);
  abAppend(&ab, "\x1b[H", 3);

  editorDrawRows(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (_editorConfig.cy - _editorConfig.rowoff) + 1,
                                            (_editorConfig.rx - _editorConfig.coloff) + 1);
  abAppend(&ab, buf, strlen(buf));

  abAppend(&ab, "\x1b[?25h", 6);

  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}


/*** input ***/

void editorMoveCursor(int key) {
  erow *row = (_editorConfig.cy >= _editorConfig.numrows) 
            ? NULL : &_editorConfig.row[_editorConfig.cy];

  switch (key) {
    case ARROW_LEFT:
      if (_editorConfig.cx != 0) _editorConfig.cx--;
      else if (_editorConfig.cy > 0) {
        _editorConfig.cy--;
        _editorConfig.cx = _editorConfig.row[_editorConfig.cy].size;
      }
      break;
    case ARROW_RIGHT:
      if (row && _editorConfig.cx < row->size) _editorConfig.cx++;
      else if (row && _editorConfig.cx == row->size) {
        _editorConfig.cy++;
        _editorConfig.cx = 0;
      }
      break;
    case ARROW_UP:
      if (_editorConfig.cy != 0) _editorConfig.cy--;
      break;
    case ARROW_DOWN:
      if (_editorConfig.cy < _editorConfig.numrows) _editorConfig.cy++;
      break;
  }

  row = (_editorConfig.cy >= _editorConfig.numrows) ? NULL : &_editorConfig.row[_editorConfig.cy];
  int rowlen = row ? row->size : 0;
  if (_editorConfig.cx > rowlen) _editorConfig.cx = rowlen;
}

void editorProcessKeypress() {
  int key = editorReadKey();

  switch (key) {
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
    case PAGE_DOWN: {
        int times = _editorConfig.screenrows;
        while (times--)
          editorMoveCursor(key == PAGE_UP ? ARROW_UP : ARROW_DOWN);
      }
      break;

    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editorMoveCursor(key);
      break;
  }
}


/*** init ***/

void initEditor() {
  _editorConfig.cx = 0;
  _editorConfig.cy = 0;
  _editorConfig.rx = 0;
  _editorConfig.rowoff = 0;
  _editorConfig.coloff = 0;
  _editorConfig.numrows = 0;
  _editorConfig.row = NULL;

  if (getWindowSize(&_editorConfig.screenrows, &_editorConfig.screencols) == -1) 
    die("getWindowSize");
}

int main(int argc, char *argv[]) {
  enableRawMode();
  initEditor();
  if (argc >= 2) {
    editorOpen(argv[1]);
  }
 
  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }
 
  return 0;
}


/*** Working On help ***/

// void help(char *option) {
//   if (option == "") printf("help");
//   if (option == "test") printf("tests");
// }

// int main(int argc, char **argv) {
//   if (argc >= 2) printf(argv[1], "\n");
//   if (argc >= 2) {
//     char arg[64];
//     strcpy(argv[1],arg);

//     char helpArg[64];

//     printf("%s", arg);

//     if (strcmp(arg, "--help") && sizeof(helpArg) != 0) help(helpArg);
//     if (strcmp(arg, "--help") || strcmp(arg, "-h")) help("");
//   }
  
//   printf(argv[1]);
//   if (argc >= 2) {
//     if (argv[1] == "--help" || argv[1] == "-h") {
//       printf("Help");
//       return 0;
//     } else;
//   }
//   while( TRUE ) {
//   }
//   if (argc >= 2) {
//     if (argv[1] == "--help" || argv[1] == "-h") {
//       printf("Help");
//       return 0;
//     } else {

//       enableRawMode();
//       initEditor();
//       editorOpen(argv[1]);
//     }
//   }

//   while (1) {
//     editorRefreshScreen();
//     editorProcessKeypress();
//   }

//   return 0;
// }
