#pragma once
#include <stdio.h>
#include <pico/time.h>
#include <stdarg.h>
#include <pico/stdlib.h>

class Log {
#define COLOR(clr)  "\033[0;" clr "m"
#define COLOR_RESET "\033[0m"
#define COLOR_RED   COLOR("31")
#define COLOR_GREEN COLOR("32")
#define COLOR_BROWN COLOR("33")

private:
    static void print(const char* tag, const char* message, va_list args) {
        printf("%ld", to_ms_since_boot(get_absolute_time()));
        printf(tag);
        vprintf(message, args);
        printf("\n");
    }

public:
    static void init(void) {
        stdio_init_all();
        printf(COLOR_GREEN);
        printf("\n");
        printf("--------------------------------\n");
        printf("|        RP2350 Started        |\n");
        printf("--------------------------------\n");
        printf(COLOR_RESET);
    }

    static void info(const char* message, ...) {
        va_list args;
        va_start(args, message);
        print(COLOR_GREEN " [I] " COLOR_RESET, message, args);
        va_end(args);
    }

    static void debug(const char* message, ...) {
        va_list args;
        va_start(args, message);
        print(COLOR_BROWN " [D] " COLOR_RESET, message, args);
        va_end(args);
    }

    static void error(const char* message, ...) {
        va_list args;
        va_start(args, message);
        print(COLOR_RED " [E] " COLOR_RESET, message, args);
        va_end(args);
    }
};
