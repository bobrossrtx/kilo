/*** includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/
#define CTRL_KEY(key) ((key) & 0x1f)


/*** data ***/
struct editorConfig {
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

  struct termios raw = _editorConfig.orig_termios;                        // Assigning orig_termios -> raw

  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON); // Disabled input flags
  raw.c_oflag &= ~(OPOST);                                  // Disabled output flags
  raw.c_cflag |= (CS8);                                     // bit mask
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);          // Disabled local flags
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

char editorReadKey() {
  int nread;
  char key;

  while ((nread = read(STDIN_FILENO, &key, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }
  return key;
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

  if (1 || ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** output ***/

void editorDrawRows() {
  for (int y = 0; y < _editorConfig.screenrows; y++) {
    write(STDOUT_FILENO, "~\r\n", 3);
  }
}

void editorRefreshScreen() {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  editorDrawRows();

  write(STDOUT_FILENO, "\x1b[H", 3);
}


/*** input ***/

void editorProcessKeypress() {
  char KEY_IN = editorReadKey();

  switch (KEY_IN) {
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;
  }
}


/*** init ***/

void initEditor() {
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
