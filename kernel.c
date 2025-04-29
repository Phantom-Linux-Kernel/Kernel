#define VGA_ADDRESS 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

unsigned short *terminal_buffer;
unsigned int terminal_row;
unsigned int terminal_column;

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// VGA and terminal display
void clearf(void) {
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            const int index = y * VGA_WIDTH + x;
            terminal_buffer[index] = (unsigned short)' ' | (unsigned short)0x07 << 8;
        }
    }
    terminal_row = 0;
    terminal_column = 0;
}

void putchar_at(char c, unsigned char color, unsigned int x, unsigned int y) {
    const int index = y * VGA_WIDTH + x;
    terminal_buffer[index] = (unsigned short)c | (unsigned short)color << 8;
}

void putchar(char c, unsigned char color) {
    if (c == '\n') {
        terminal_column = 0;
        terminal_row++;
    } else {
        putchar_at(c, color, terminal_column, terminal_row);
        terminal_column++;
        if (terminal_column >= VGA_WIDTH) {
            terminal_column = 0;
            terminal_row++;
        }
    }

    if (terminal_row >= VGA_HEIGHT) {
        clearf();
    }
}

void printf(char *str, unsigned char color) {
    int index = 0;
    while (str[index]) {
        putchar(str[index], color);
        index++;
    }
}

// Scancode to ASCII mappings
char scancode_to_ascii[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0, 'a','s','d','f','g','h','j','k','l',';','\'','`',
    0, '\\','z','x','c','v','b','n','m',',','.','/',0,'*',0,' ',0,
};

char scancode_to_ascii_shift[128] = {
    0,  27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0, 'A','S','D','F','G','H','J','K','L',':','"','~',
    0, '|','Z','X','C','V','B','N','M','<','>','?',0,'*',0,' ',0,
};

// String compare function (no standard lib)
int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++; s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

// Input handler
void scanf(char *buffer, int max_len, unsigned char color) {
    int i = 0;
    int shift = 0;

    while (i < max_len - 1) {
        unsigned char scancode = 0;

        // Wait for a key press (make code)
        do {
            while (!(inb(0x64) & 1)) {}  // Wait for keyboard buffer full
            scancode = inb(0x60);

            if (scancode == 0x2A || scancode == 0x36) {
                shift = 1;
                scancode = 0;
            } else if (scancode == 0xAA || scancode == 0xB6) {
                shift = 0;
                scancode = 0;
            }
        } while (scancode == 0 || (scancode & 0x80));  // Skip releases and non-char codes

        // Handle Enter key (0x1C)
        if (scancode == 0x1C) {
            break;
        }

        // Get character
        char c = shift ? scancode_to_ascii_shift[scancode] : scancode_to_ascii[scancode];

        // Handle Backspace
        if (c == '\b') {
            if (i > 0) {
                i--;
                terminal_column--;
                putchar_at(' ', color, terminal_column, terminal_row);
            }
        } else if (c) {
            buffer[i++] = c;
            putchar(c, color);
        }

        // Wait for key release before continuing
        unsigned char release_code;
        do {
            while (!(inb(0x64) & 1)) {}
            release_code = inb(0x60);
        } while ((release_code & 0x7F) != scancode);  // Wait for release of same key
    }

    buffer[i] = '\0';
    putchar('\n', color);
}



// Command parsing
#define MAX_ARGS 10

typedef struct {
    char *cmd;
    char *args[MAX_ARGS];
    int argc;
} Command;

void readcmd(char *input, Command *out_cmd) {
    out_cmd->argc = 0;

    while (*input == ' ') input++;
    if (*input == '\0') {
        out_cmd->cmd = 0;
        return;
    }

    out_cmd->cmd = input;

    while (*input && out_cmd->argc < MAX_ARGS) {
        if (*input == ' ') {
            *input = '\0';
            input++;
            while (*input == ' ') input++;
            if (*input) {
                out_cmd->args[out_cmd->argc++] = input;
            }
        } else {
            input++;
        }
    }
}

// Shell entry point
void main(void) {
    terminal_buffer = (unsigned short *)VGA_ADDRESS;
    terminal_row = 0;
    terminal_column = 0;
    clearf();

    char input[100];
    Command cmd;

    while (1) {
        printf("$> ", 02);
        scanf(input, sizeof(input), 15);
        readcmd(input, &cmd);

        if (cmd.cmd) {
            if (strcmp(cmd.cmd, "echo") == 0 && cmd.argc > 0) {
                for (int i = 0; i < cmd.argc; i++) {
                    printf(cmd.args[i], 15);
                    printf(" ", 15);
                }
                printf("\n", 15);
            } else if (strcmp(cmd.cmd, "clear") == 0) {
                clearf();
            } else if (strcmp(cmd.cmd, "help") == 0) {
                printf("Available commands: echo, clear, help\n", 14);
            } else {
                printf("Unknown command\n", 12);
            }
        }
    }
}
