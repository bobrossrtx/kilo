/*** includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/*** data ***/

// Original terminal attributes
struct termios orig_termios;

/*** terminal ***/

void die(const char *err) {
  perror(err);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
    die("tcsetattr");
}

void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");
  atexit(disableRawMode); // Resets terminal attributes to normal on exit

  struct termios raw = orig_termios;                        // Assigning orig_termios -> raw

  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON); // Disabled input flags
  raw.c_oflag &= ~(OPOST);                                  // Disabled output flags
  raw.c_cflag |= (CS8);                                     // bit mask
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);          // Disabled local flags
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

/*** init ***/

int main() {
  enableRawMode();

  while (1) {
    char termIn = '\0';
    if (read(STDIN_FILENO, &termIn, 1) == -1 && errno != EAGAIN) die("read");
    if (iscntrl(termIn)) {
      printf("%d\r\n", termIn);
    } else {
      printf("%d ('%c')\r\n", termIn, termIn);
    }
    if (termIn == 'q') break;
  };
  return 0;
}
