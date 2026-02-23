#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <cstring>


#define VGA_COLOR_BLACK   0x00
#define VGA_COLOR_BLUE    0x01
#define VGA_COLOR_GREEN   0x02
#define VGA_COLOR_CYAN    0x03
#define VGA_COLOR_RED     0x04
#define VGA_COLOR_MAGENTA 0x05
#define VGA_COLOR_BROWN   0x06
#define VGA_COLOR_LIGHT_GREY 0x07
#define VGA_COLOR_DARK_GREY  0x08
#define VGA_COLOR_LIGHT_BLUE 0x09
#define VGA_COLOR_LIGHT_GREEN 0x0A
#define VGA_COLOR_LIGHT_CYAN  0x0B
#define VGA_COLOR_LIGHT_RED   0x0C
#define VGA_COLOR_LIGHT_MAGENTA 0x0D
#define VGA_COLOR_LIGHT_BROWN   0x0E
#define VGA_COLOR_WHITE   0x0F
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

volatile uint16_t* VGA_BUFFER = (uint16_t*)0xB8000;
uint16_t cursor_position = 0;
char input_buffer[256];
size_t input_length = 0;
bool shift_pressed = false;

struct idt_entry {
    uint16_t base_lo;
    uint16_t sel;
    uint8_t  always0;
    uint8_t  flags;
    uint16_t base_hi;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

struct idt_entry idt[256];
struct idt_ptr idtp;

extern "C" uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

extern "C" void outb(uint16_t port, uint8_t value) {
    __asm__ __volatile__("outb %0, %1" : : "a"(value), "Nd"(port));
}

struct File {
    char name[32];
    char content[512]; 
    bool exists;
};

#define MAX_FILES 64
File filesystem[MAX_FILES];

void init_fs() {
    for(int i = 0; i < MAX_FILES; i++) filesystem[i].exists = false;
}

bool starts_with(const char* str, const char* prefix) {
    while(*prefix) {
        if(*prefix != *str) return false;
        prefix++; str++;
    }
    return true;
}

void str_copy(char* dest, const char* src, int n) {
    int i = 0;
    for (; i < n - 1 && src[i] != '\0'; i++)
        dest[i] = src[i];
    dest[i] = '\0';
}

const char* find_file(const char* name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (filesystem[i].exists && strcmp(name, filesystem[i].name) == 0) {
            return filesystem[i].content;
        }
    }
    return nullptr;
}

const char* get_args(const char* command, const char* cmd_name) {
    size_t len = 0;
    while (cmd_name[len]) len++;
    if (command[len] == ' ') return &command[len + 1];
    return "";
}

void pic_remap() {
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0x0);
    outb(0xA1, 0x0);
}

void pic_send_eoi() {
    outb(0x20, 0x20);
}

void update_cursor() {
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(cursor_position & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((cursor_position >> 8) & 0xFF));
}

void clear_screen() {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA_BUFFER[i] = (VGA_COLOR_BLACK << 8) | ' ';
    cursor_position = 0;
    update_cursor();
}



void scroll(uint8_t color) {
    for (int i = 0; i < VGA_WIDTH * (VGA_HEIGHT - 1); i++)
        VGA_BUFFER[i] = VGA_BUFFER[i + VGA_WIDTH];
    for (int i = VGA_WIDTH * (VGA_HEIGHT - 1); i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA_BUFFER[i] = (color << 8) | ' ';
    cursor_position -= VGA_WIDTH;
}

void vga_put_char(char c, uint8_t color) {
    if (c == '\n') {
        cursor_position += VGA_WIDTH - (cursor_position % VGA_WIDTH);
    } else if (c == '\b') {
        if (cursor_position > 0) {
            cursor_position--;
            VGA_BUFFER[cursor_position] = (color << 8) | ' ';
        }
    } else {
        VGA_BUFFER[cursor_position++] = (color << 8) | c;
    }
    if (cursor_position >= VGA_WIDTH * VGA_HEIGHT) scroll(color);
    update_cursor();
}

void vga_put_string(const char* str, uint8_t color) {
    while (*str) vga_put_char(*str++, color);
}

int strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) { a++; b++; }
    return *(const unsigned char*)a - *(const unsigned char*)b;
}

char scancode_to_char(uint8_t scancode) {
    static const char map[] = {
        0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
        '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',0,
        'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\',
        'z','x','c','v','b','n','m',',','.','/',0,'*',0,' '
    };
    if (scancode >= sizeof(map)) return 0;
    char c = map[scancode];
    if (!c) return 0;
    if (shift_pressed) {
        if (c >= 'a' && c <= 'z') c -= 32;
        else if (c == '1') c = '!'; else if (c == '2') c = '@';
        else if (c == '3') c = '#'; else if (c == '4') c = '$';
        else if (c == '5') c = '%'; else if (c == '6') c = '^';
        else if (c == '7') c = '&'; else if (c == '8') c = '*';
        else if (c == '9') c = '('; else if (c == '0') c = ')';
    }
    return c;
}

void print_prompt() {
    vga_put_string("\nbyteW> ", VGA_COLOR_WHITE);
}

void reboot() {
    vga_put_string("\nSystem rebooting...", VGA_COLOR_LIGHT_RED);
    uint8_t good = 0x02;
    while (good & 0x02)
        good = inb(0x64);
    outb(0x64, 0xFE);
}



void process_command(const char* command) {
    vga_put_char('\n', VGA_COLOR_WHITE);

if (strcmp(command, "help") == 0) {
        vga_put_string("\nCommands: help, neofetch, clear, reboot, whoami, touch, ls, echo, cat, write, rm", VGA_COLOR_WHITE);
    } 
    else if (strcmp(command, "neofetch") == 0) {
        vga_put_string("\nOS: byteOS 0.2", VGA_COLOR_LIGHT_CYAN);
        vga_put_string("\nKERNEL: nbula 2.5", VGA_COLOR_LIGHT_MAGENTA);
        vga_put_string("\nARCH: x86 (i386)", VGA_COLOR_WHITE);
    } 
    else if (strcmp(command, "whoami") == 0) {
        vga_put_string("\nroot@nebula", VGA_COLOR_LIGHT_GREEN);
    } 
    else if (strcmp(command, "reboot") == 0) {
        reboot();
    } 

    else if (starts_with(command, "echo ")) {
        vga_put_string(command + 5, VGA_COLOR_WHITE);
    } 
    else if (starts_with(command, "touch ")) {
        const char* fname = command + 6;
        for(int i = 0; i < MAX_FILES; i++) {
            if(!filesystem[i].exists) {
                str_copy(filesystem[i].name, fname, 31);
                str_copy(filesystem[i].content, "(empty file)", 511);
                filesystem[i].exists = true;
                vga_put_string("File created.", VGA_COLOR_LIGHT_GREEN);
                break;
            }
        }
    }

else if (starts_with(command, "write ")) {
    const char* args = command + 6;
    int i = 0;
    char fname[32];
    while (args[i] && args[i] != ' ' && i < 31) {
        fname[i] = args[i];
        i++;
    }
    fname[i] = '\0';

    if (args[i] == ' ') i++;
    const char* content = &args[i];

    bool found = false;

    for (int j = 0; j < MAX_FILES; j++) {
        if (filesystem[j].exists && strcmp(filesystem[j].name, fname) == 0) {
            str_copy(filesystem[j].content, content, 511);
            vga_put_string("File written.", VGA_COLOR_LIGHT_GREEN);
            found = true;
            break;
        }
    }

    if (!found) {
        vga_put_string("File not found.", VGA_COLOR_LIGHT_RED);
    }
}
    else if (strcmp(command, "ls") == 0) {
        for(int i = 0; i < MAX_FILES; i++) {
            if(filesystem[i].exists) {
                vga_put_string(filesystem[i].name, VGA_COLOR_LIGHT_CYAN);
                vga_put_string("  ", VGA_COLOR_WHITE);
            }
        }
    }
    else if (starts_with(command, "cat ")) {
        const char* fname = command + 4;
        bool found = false;
        for(int i = 0; i < MAX_FILES; i++) {
            if(filesystem[i].exists && strcmp(filesystem[i].name, fname) == 0) {
                vga_put_string(filesystem[i].content, VGA_COLOR_WHITE);
                found = true;
                break;
            }
        }
        if(!found) vga_put_string("File not found.", VGA_COLOR_LIGHT_RED);
    }
    else if (strcmp(command, "clear") == 0) {
        clear_screen();
        return; 
    }

else if (starts_with(command, "rm ")) {
    const char* fname = command + 3;
    bool found = false;

    for (int i = 0; i < MAX_FILES; i++) {
        if (filesystem[i].exists && strcmp(filesystem[i].name, fname) == 0) {
            filesystem[i].exists = false;
            vga_put_string("File removed.", VGA_COLOR_LIGHT_GREEN);
            found = true;
            break;
        }
    }

    if (!found) vga_put_string("File not found.", VGA_COLOR_LIGHT_RED);
}
    else {
        vga_put_string("Unknown: ", VGA_COLOR_LIGHT_RED);
        vga_put_string(command, VGA_COLOR_WHITE);
    }

    print_prompt();
}



void handle_key(char key) {
    if (key == '\n') {
        input_buffer[input_length] = '\0';
        process_command(input_buffer);
        input_length = 0;
        return;
    }

    if (key == '\b') {
        if (input_length > 0) {
            input_length--;
            input_buffer[input_length] = '\0';
            vga_put_char('\b', VGA_COLOR_WHITE);
        }
        return;
    }

    if (input_length < sizeof(input_buffer) - 1) {
        input_buffer[input_length++] = key;
        vga_put_char(key, VGA_COLOR_WHITE);
    }
}

extern "C" __attribute__((interrupt, target("general-regs-only"))) 
void timer_irq_handler(void* frame) {
    pic_send_eoi(); 
}

extern "C" __attribute__((interrupt, target("general-regs-only"))) 
void keyboard_irq_handler(void* frame) {
    uint8_t scancode = inb(0x60);
    if (scancode == 0x2A || scancode == 0x36) shift_pressed = true;
    else if (scancode == 0xAA || scancode == 0xB6) shift_pressed = false;
    else if (!(scancode & 0x80)) {
        char key = scancode_to_char(scancode);
        if (key) handle_key(key);
    }
    pic_send_eoi();
}

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_lo = base & 0xFFFF;
    idt[num].base_hi = (base >> 16) & 0xFFFF;
    idt[num].sel = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags;
}

void init_idt() {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (uint32_t)&idt;

    idt_set_gate(0x20, (uint32_t)timer_irq_handler, 0x08, 0x8E);
    
    idt_set_gate(0x21, (uint32_t)keyboard_irq_handler, 0x08, 0x8E);

    __asm__ __volatile__("lidt %0" : : "m"(idtp));
}

extern "C" void kernel_main() {
    init_fs(); 
    clear_screen();
    vga_put_string("MIT/byteOS 0.2 x86\nkernel \"nbla 2.0\"\n", VGA_COLOR_LIGHT_GREEN);
    pic_remap();
    init_idt();
    print_prompt();
    __asm__ __volatile__("sti");
    while (1) { __asm__ __volatile__("hlt"); }
}
