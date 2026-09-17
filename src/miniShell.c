#include "readcmd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <wordexp.h>
#include "miniShell.h"

struct rlimit cpu_limit = { RLIM_INFINITY, RLIM_INFINITY };

// Structure pour stocker chaque processus en tâche de fond
struct BgProcess {
    pid_t pid;                  // PID du processus
    char *command;              // Commande exécutée
    struct BgProcess *next;     // Pointeur vers le prochain processus dans la liste
};

// Pointeur vers la tête de la liste des processus en tâche de fond
struct BgProcess *bg_processes = NULL;

void addBgProcess(pid_t pid, char **commandArgs) {
    struct BgProcess *new_process = malloc(sizeof(struct BgProcess));
    if (new_process == NULL) {
        perror("Failed to allocate memory for new background process");
        return;
    }
    new_process->pid = pid;

    // Calculer la longueur totale de la commande
    size_t command_length = 0;
    for (char **arg = commandArgs; *arg != NULL; arg++) {
        command_length += strlen(*arg) + 1; // +1 pour l'espace ou le caractère nul
    }

    // Allouer de la mémoire pour la commande
    new_process->command = malloc(command_length);
    if (new_process->command == NULL) {
        perror("Failed to allocate memory for command string");
        free(new_process);
        return;
    }

    // Construire la chaîne de la commande
    new_process->command[0] = '\0';
    for (char **arg = commandArgs; *arg != NULL; arg++) {
        strcat(new_process->command, *arg);
        strcat(new_process->command, " ");
    }

    new_process->next = bg_processes;
    bg_processes = new_process;
}

// Supprimer un processus terminé de la liste des processus en tâche de fond
void removeBgProcess(pid_t pid) {
    struct BgProcess **current = &bg_processes;
    while (*current) {
        if ((*current)->pid == pid) {
            struct BgProcess *to_free = *current;
            *current = (*current)->next;
            free(to_free->command);
            free(to_free);
            return;
        }
        current = &(*current)->next;
    }
}

void jobs() {
    struct BgProcess *current = bg_processes;
    printf("Background processes:\n");
    while (current) {
        printf("[%d] %s\n", current->pid, current->command);
        current = current->next;
    }
}

void updateBgProcesses() {
    struct BgProcess **current = &bg_processes;
    while (*current) {
        int status;
        pid_t result = waitpid((*current)->pid, &status, WNOHANG);
        if (result == -1) {
            // Error occurred; remove process from list
            perror("waitpid");
            struct BgProcess *to_free = *current;
            *current = (*current)->next;
            free(to_free->command);
            free(to_free);
        } else if (result == 0) {
            // Process is still running; move to next
            current = &(*current)->next;
        } else {
            // Process has finished; remove from list
            printf("Process %d finished\n", (*current)->pid);
            struct BgProcess *to_free = *current;
            *current = (*current)->next;
            free(to_free->command);
            free(to_free);
        }
    }
}


void executePipeline(struct cmdline* l) {
    if (l == NULL || l->seq[0] == NULL) {
        return;
    }

    int nb_cmds = 0;
    while (l->seq[nb_cmds] != NULL) {
        nb_cmds++;
    }

    int pipefd[2 * (nb_cmds - 1)];

    // Créer les pipes nécessaires
    for (int i = 0; i < nb_cmds - 1; i++) {
        if (pipe(pipefd + i * 2) == -1) {
            perror("pipe failed");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < nb_cmds; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        }

        if (pid == 0) {
            // Processus enfant

            // Appliquer les limites de temps CPU
            if (setrlimit(RLIMIT_CPU, &cpu_limit) != 0) {
                perror("Failed to set CPU time limit");
                exit(EXIT_FAILURE);
            }

            // Gestion de la redirection d'entrée
            if (i == 0 && l->in != NULL) {
                // Expansion de l->in
                wordexp_t p;
                int ret = wordexp(l->in, &p, 0);
                if (ret != 0) {
                    fprintf(stderr, "wordexp failed for input redirection");
                    exit(EXIT_FAILURE);
                }
                if (p.we_wordc != 1) {
                    fprintf(stderr, "Ambiguous input redirection\n");
                    wordfree(&p);
                    exit(EXIT_FAILURE);
                }
                int fd_in = open(p.we_wordv[0], O_RDONLY);
                if (fd_in < 0) {
                    perror("Failed to open input file");
                    wordfree(&p);
                    exit(EXIT_FAILURE);
                }
                dup2(fd_in, STDIN_FILENO);
                close(fd_in);
                wordfree(&p);
            }

            // Gestion de la redirection de sortie
            if (i == nb_cmds - 1 && l->out != NULL) {
                // Expansion de l->out
                wordexp_t p;
                int ret = wordexp(l->out, &p, 0);
                if (ret != 0) {
                    fprintf(stderr, "wordexp failed for output redirection");
                    exit(EXIT_FAILURE);
                }
                if (p.we_wordc != 1) {
                    fprintf(stderr, "Ambiguous output redirection\n");
                    wordfree(&p);
                    exit(EXIT_FAILURE);
                }
                int fd_out = open(p.we_wordv[0], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd_out < 0) {
                    perror("Failed to open output file");
                    wordfree(&p);
                    exit(EXIT_FAILURE);
                }
                dup2(fd_out, STDOUT_FILENO);
                close(fd_out);
                wordfree(&p);
            }

            // Redirections pour les pipes
            if (i != 0) {
                dup2(pipefd[(i - 1) * 2], STDIN_FILENO);
            }
            if (i != nb_cmds - 1) {
                dup2(pipefd[i * 2 + 1], STDOUT_FILENO);
            }

            // Fermer tous les descripteurs de fichiers
            for (int j = 0; j < 2 * (nb_cmds - 1); j++) {
                close(pipefd[j]);
            }

            // Expansion des arguments
            char **args = NULL;
            int argc = 0;
            int capacity = 0;
            for (int k = 0; l->seq[i][k] != NULL; k++) {
                wordexp_t p;
                int ret = wordexp(l->seq[i][k], &p, 0);
                if (ret != 0) {
                    fprintf(stderr, "wordexp failed");
                    continue;
                }

                if (argc + p.we_wordc >= capacity) {
                    capacity += p.we_wordc + 10;
                    args = realloc(args, capacity * sizeof(char *));
                    if (args == NULL) {
                        perror("Failed to allocate memory for arguments");
                        wordfree(&p);
                        exit(EXIT_FAILURE);
                    }
                }

                for (size_t j = 0; j < p.we_wordc; j++) {
                    args[argc] = strdup(p.we_wordv[j]);
                    argc++;
                }

                wordfree(&p);
            }
            args[argc] = NULL;

            // Exécuter la commande
            execvp(args[0], args);
            perror("Command not found");
            exit(EXIT_FAILURE);
        }
    }

    // Fermer tous les descripteurs de fichiers dans le processus parent
    for (int i = 0; i < 2 * (nb_cmds - 1); i++) {
        close(pipefd[i]);
    }

    // Attendre tous les processus enfants
    for (int i = 0; i < nb_cmds; i++) {
        wait(NULL);
    }
}


void executeCommand(struct cmdline* l) {
    if (l == NULL || l->seq[0] == NULL) {
        return;
    }

    // Vérifier s'il y a un pipe
    if (l->seq[1] != NULL) {
        // Il y a un pipe, utiliser executePipeline
        executePipeline(l);
        return;
    }

    // Préparer les arguments avec expansion
    char **args = NULL;
    int argc = 0;
    int capacity = 0;

    // Parcourir les arguments de la commande
    for (int i = 0; l->seq[0][i] != NULL; i++) {
        wordexp_t p;
        int ret = wordexp(l->seq[0][i], &p, 0);
        if (ret != 0) {
            fprintf(stderr, "wordexp failed");
            continue;
        }

        // Ajuster la capacité du tableau d'arguments
        if (argc + p.we_wordc >= capacity) {
            capacity += p.we_wordc + 10;
            args = realloc(args, capacity * sizeof(char *));
            if (args == NULL) {
                perror("Failed to allocate memory for arguments");
                wordfree(&p);
                return;
            }
        }

        // Ajouter les mots expansés aux arguments
        for (size_t j = 0; j < p.we_wordc; j++) {
            args[argc] = strdup(p.we_wordv[j]);
            argc++;
        }

        wordfree(&p);
    }

    // Terminer le tableau d'arguments par NULL
    args[argc] = NULL;

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        // Libérer la mémoire allouée pour les arguments
        for (int i = 0; i < argc; i++) {
            free(args[i]);
        }
        free(args);
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {  // Processus enfant
        // Appliquer les limites de temps CPU
        if (setrlimit(RLIMIT_CPU, &cpu_limit) != 0) {
            perror("Failed to set CPU time limit");
            exit(EXIT_FAILURE);
        }

        // Gestion de la redirection d'entrée
        if (l->in != NULL) {
            // Expansion de l->in
            wordexp_t p;
            int ret = wordexp(l->in, &p, 0);
            if (ret != 0) {
                fprintf(stderr, "wordexp failed for input redirection");
                exit(EXIT_FAILURE);
            }
            if (p.we_wordc != 1) {
                fprintf(stderr, "Ambiguous input redirection\n");
                wordfree(&p);
                exit(EXIT_FAILURE);
            }
            int fd_in = open(p.we_wordv[0], O_RDONLY);
            if (fd_in < 0) {
                perror("Failed to open input file");
                wordfree(&p);
                exit(EXIT_FAILURE);
            }
            dup2(fd_in, STDIN_FILENO);
            close(fd_in);
            wordfree(&p);
        }

        // Gestion de la redirection de sortie
        if (l->out != NULL) {
            // Expansion de l->out
            wordexp_t p;
            int ret = wordexp(l->out, &p, 0);
            if (ret != 0) {
                fprintf(stderr, "wordexp failed for output redirection");
                exit(EXIT_FAILURE);
            }
            if (p.we_wordc != 1) {
                fprintf(stderr, "Ambiguous output redirection\n");
                wordfree(&p);
                exit(EXIT_FAILURE);
            }
            int fd_out = open(p.we_wordv[0], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd_out < 0) {
                perror("Failed to open output file");
                wordfree(&p);
                exit(EXIT_FAILURE);
            }
            dup2(fd_out, STDOUT_FILENO);
            close(fd_out);
            wordfree(&p);
        }

        // Exécuter la commande
        execvp(args[0], args);
        perror("Command not found"); // Si exec échoue
        exit(EXIT_FAILURE);
    } else {  // Processus parent
        if (l->bg) {
            printf("Processus %d running in background\n", pid);
            addBgProcess(pid, args);
            // Ne pas attendre les processus en arrière-plan
        } else {
            int status;
            waitpid(pid, &status, 0);  // Attendre la fin de l'enfant
        }
    }

    // Libérer la mémoire allouée pour les arguments
    for (int i = 0; i < argc; i++) {
        free(args[i]);
    }
    free(args);
}