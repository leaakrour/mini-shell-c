#ifndef MINISHELL_H
#define MINISHELL_H

#include "readcmd.h"
#include <sys/resource.h>

extern struct rlimit cpu_limit;
extern struct BgProcess *bg_processes;

void addBgProcess(pid_t pid, char **commandArgs);
void removeBgProcess(pid_t pid);
void jobs();
void updateBgProcesses();
void executePipeline(struct cmdline* l);
void handleInternalCommands(struct cmdline *l);
void executeCommand(struct cmdline* l);

#endif // MINISHELL_H
