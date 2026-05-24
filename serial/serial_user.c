#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <poll.h>
#include <signal.h>

struct termios orig_termios;
int uart_fd = -1;

// Restore terminal settings cleanly on exit (Ctrl+C)
void cleanup_terminal(int sig) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    if (uart_fd >= 0) close(uart_fd);
    printf("\nDisconnected from UART.\n");
    exit(EXIT_SUCCESS);
}

void set_raw_mode(void) {
    struct termios raw;
    tcgetattr(STDIN_FILENO, &orig_termios);
    
    signal(SIGINT, cleanup_terminal);
    signal(SIGTERM, cleanup_terminal);

    raw = orig_termios;
    // REMOVED ISIG FROM THE MASK: Keep input signals active
    raw.c_lflag &= ~(ECHO | ICANON); 
    raw.c_iflag &= ~(IXON | ICRNL);
    
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <device_path>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Open UART with O_NONBLOCK matching our driver poll mechanism
    uart_fd = open(argv[1], O_RDWR | O_NONBLOCK);
    if (uart_fd < 0) {
        perror("Failed to open UART device");
        return EXIT_FAILURE;
    }

    printf("Entering raw mode terminal terminal interface. Press Ctrl+C to quit.\n");
    set_raw_mode();

    // Set up monitoring structures for poll()
    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO; // Monitor keyboard input
    fds[0].events = POLLIN;
    
    fds[1].fd = uart_fd;     // Monitor UART driver incoming data
    fds[1].events = POLLIN;

    char buf[256];

    while (1) {
        // Wait indefinitely until an input event occurs on either file descriptor
        int ret = poll(fds, 2, -1);
        if (ret < 0) {
            perror("Poll error");
            break;
        }

        // Case A: User typed a character on stdin
        if (fds[0].revents & POLLIN) {
            fprintf(stderr, "stdin rx\n");
            ssize_t r = read(STDIN_FILENO, buf, sizeof(buf));
            if (r > 0) {
                // Pipe directly down to the UART driver
                write(uart_fd, buf, r);
            }
        }

        // Case B: The UART received a character from remote end
        if (fds[1].revents & POLLIN) {
            fprintf(stderr, "uart rx\n");
            ssize_t r = read(uart_fd, buf, sizeof(buf));
            if (r > 0) {
                // Pipe directly out to user terminal screen
                write(STDOUT_FILENO, buf, r);
            }
        }
    }

    cleanup_terminal(0);
    return EXIT_SUCCESS;
}