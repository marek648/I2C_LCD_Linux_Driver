// lcd_write.c - Simple application to write to LCD character device
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#define DEVICE_PATH "/dev/lcd0"
#define MAX_MESSAGE_LEN 256
#define LCD_ROWS 4

void print_usage(const char *progname)
{
    printf("Usage: %s <row> \"message\"\n", progname);
    printf("       %s clear\n", progname);
    printf("\n");
    printf("  row: 1-4 (1=first row, 4=fourth row)\n");
    printf("  clear: Clear entire display\n");
    printf("\nExamples:\n");
    printf("  %s 1 \"Hello World\"        # Write to first row\n", progname);
    printf("  %s 3 \"Temperature: 25C\"   # Write to third row\n", progname);
    printf("  %s 4 \"Status: OK\"         # Write to fourth row\n", progname);
    printf("  %s clear                   # Clear display\n", progname);
}

int main(int argc, char *argv[])
{
    int fd;
    ssize_t bytes_written;
    size_t msg_len, total_len;
    char *message;
    char buffer[MAX_MESSAGE_LEN + 10];  // Extra space for row prefix
    int row;
    char *endptr;
    int is_clear_command = 0;
    
    // Check arguments
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Error: Wrong number of arguments\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    
    // Check for clear command
    if (argc == 2 && strcmp(argv[1], "clear") == 0) {
        is_clear_command = 1;
        sprintf(buffer,"clear");
    } else if (argc == 3) {
        // Normal write command with row and message
        is_clear_command = 0;
    } else {
        fprintf(stderr, "Error: Wrong number of arguments\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    
    if (!is_clear_command) {
        // Parse row number
        row = strtol(argv[1], &endptr, 10);
    
        // Validate row number parsing
        if (*endptr != '\0' || endptr == argv[1]) {
            fprintf(stderr, "Error: Invalid row number '%s'\n", argv[1]);
            fprintf(stderr, "Row must be a number between 1 and %d\n", LCD_ROWS);
            return EXIT_FAILURE;
        }
        
        // Validate row range (1-4)
        if (row < 1 || row > LCD_ROWS) {
            fprintf(stderr, "Error: Row %d out of range (must be 1-%d)\n", row, LCD_ROWS);
            return EXIT_FAILURE;
        }
        
        message = argv[2];
        msg_len = strlen(message);
        
        // Validate message length
        if (msg_len == 0) {
            fprintf(stderr, "Error: Empty message\n");
            return EXIT_FAILURE;
        }
        
        if (msg_len > MAX_MESSAGE_LEN) {
            fprintf(stderr, "Warning: Message too long (%zu chars), truncating to %d\n", 
                    msg_len, MAX_MESSAGE_LEN);
            msg_len = MAX_MESSAGE_LEN;
        }
        
        // Format message as "ROW message" for kernel driver
        // Convert user's 1-4 to driver's 0-3
        snprintf(buffer, sizeof(buffer), "%d %.*s", row - 1, (int)msg_len, message);
    }
    total_len = strlen(buffer);
    
    
    // Open LCD device
    fd = open(DEVICE_PATH, O_WRONLY);
    if (fd < 0) {
        perror("Error opening device");
        fprintf(stderr, "\nMake sure:\n");
        fprintf(stderr, "  1. The kernel module is loaded (lsmod | grep lcd)\n");
        fprintf(stderr, "  2. Device file exists (ls -l %s)\n", DEVICE_PATH);
        fprintf(stderr, "  3. You have write permissions (try with sudo)\n");
        return EXIT_FAILURE;
    }
    
    // Write message to device
    bytes_written = write(fd, buffer, total_len);
    if (bytes_written < 0) {
        perror("Error writing to device");
        close(fd);
        return EXIT_FAILURE;
    }
    
    // Success
    if (is_clear_command) {
        printf("Successfully cleared LCD display\n");
    } else {
        printf("Successfully wrote to row %d: \"%s\"\n", row, message);
    }
    
    // Close device
    if (close(fd) < 0) {
        perror("Error closing device");
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}