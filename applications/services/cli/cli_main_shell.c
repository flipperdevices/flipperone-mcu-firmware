#include <cli/cli_main_shell.h>
#include <cli/cli_ansi.h>
#include <version/version.h>

void cli_main_motd(void* context) {
    UNUSED(context);
    printf(
        "\r\n"
        "              _.-------.._                    -,\r\n"
        "          .-\"```\"--..,,_/ /`-,               -,  \\ \r\n"
        "       .:\"          /:/  /'\\  \\     ,_...,  `. |  |\r\n"
        "      /       ,----/:/  /`\\ _\\~`_-\"`     _;\r\n"
        "     '      / /`\"\"\"'\\ \\ \\.~`_-'      ,-\"'/ \r\n"
        "    |      | |  0    | | .-'      ,/`  /\r\n"
        "   |    ,..\\ \\     ,.-\"`       ,/`    /\r\n"
        "  ;    :    `/`\"\"\\`           ,/--==,/-----,\r\n"
        "  |    `-...|        -.___-Z:_______J...---;\r\n"
        "  :         `                           _-'\r\n"
        " _L_  _     ___  ___  ___  ___  ____--\"`___  _     ___\r\n"
        "| __|| |   |_ _|| _ \\| _ \\| __|| _ \\   / __|| |   |_ _|\r\n"
        "| _| | |__  | | |  _/|  _/| _| |   /  | (__ | |__  | |\r\n"
        "|_|  |____||___||_|  |_|  |___||_|_\\   \\___||____||___|\r\n"
        "\r\n"
        "Welcome to Flipper One Command Line Interface!\r\n"
        "Read the manual: https://docs.flipper.net/one/development/cli\r\n"
        "Run `help` or `?` to list available commands\r\n"
        "\r\n");

    const Version* firmware_version = version_get();
    if(firmware_version) {
        printf(
            "Firmware version: %s %s (%s%s built on %s)\r\n",
            version_get_gitbranch(firmware_version),
            version_get_version(firmware_version),
            version_get_githash(firmware_version),
            version_get_dirty_flag(firmware_version) ? "-dirty" : "",
            version_get_builddate(firmware_version));
    }
}
