#include "register.h"

#include "alias.h"
#include "append.h"
#include "cat.h"
#include "cd.h"
#include "echo.h"
#include "help.h"
#include "ls.h"
#include "mkdir.h"
#include "mv.h"
#include "open.h"
#include "path.h"
#include "pwd.h"
#include "rm.h"
#include "sh.h"
#include "write.h"

#include "shell_commands.h"

void shell_bin_register_commands(void) {
    (void)shell_register_command("help", "show commands", bin_help_main);
    (void)shell_register_command("pwd", "show current directory", bin_pwd_main);
    (void)shell_register_command("cd", "change directory", bin_cd_main);
    (void)shell_register_command("ls", "list directory", bin_ls_main);
    (void)shell_register_command("mkdir", "create directory", bin_mkdir_main);
    (void)shell_register_command("cat", "print file", bin_cat_main);
    (void)shell_register_command("write", "write text to file", bin_write_main);
    (void)shell_register_command("append", "append text to file", bin_append_main);
    (void)shell_register_command("rm", "remove file or empty dir", bin_rm_main);
    (void)shell_register_command("mv", "rename or move", bin_mv_main);
    (void)shell_register_command("echo", "print text", bin_echo_main);
    (void)shell_register_command("path", "show or edit command PATH", bin_path_main);
    (void)shell_register_command("alias", "add or remove file associations", bin_alias_main);
    (void)shell_register_command(
        "open",
        "open a file or installed package",
        bin_open_main
    );
    (void)shell_register_command("sh", "run a shell script", bin_sh_main);
}
