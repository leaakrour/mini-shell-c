/*****************************************************
 * Copyright Grégory Mounié 2008-2015                *
 *           Simon Nieuviarts 2002-2009              *
 * This code is distributed under the GLPv3 licence. *
 * Ce code est distribué sous la licence GPLv3+.     *
 *****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "variante.h"
#include "readcmd.h"
#include "miniShell.h"


#ifndef VARIANTE
#error "Variante non défini !!"
#endif

/* Guile (1.8 and 2.0) is auto-detected by cmake */
/* To disable Scheme interpreter (Guile support), comment the
 * following lines.  You may also have to comment related pkg-config
 * lines in CMakeLists.txt.
 */

#if USE_GUILE == 1
#include <libguile.h>

int question6_executer(char *commandLine) {
    struct cmdline *l;

    // Dupliquer la ligne de commande car parsecmd la libère
    char *lineCopy = strdup(commandLine);
    if (lineCopy == NULL) {
        perror("Failed to duplicate command line");
        return -1;
    }

    // Parser la ligne de commande
    l = parsecmd(&lineCopy);

    if (l == NULL) {
        fprintf(stderr, "Failed to parse command line\n");
        // Ne pas appeler free(lineCopy); ici
        return -1;
    }

    if (l->err) {
        // Erreur de syntaxe
        printf("error: %s\n", l->err);
        // Ne pas appeler free(lineCopy); ici
        return -1;
    }

    // Exécuter la commande en utilisant vos fonctions existantes
    if (strcmp(l->seq[0][0], "jobs") == 0) {
        updateBgProcesses();  // Mettre à jour les processus en arrière-plan
        jobs();               // Afficher les processus en arrière-plan
    } else if (l->seq[1] != NULL) {
        // Il y a un pipe
        executePipeline(l);
    } else {
        // Commande simple
        executeCommand(l);
    }

    return 0;
}

SCM executer_wrapper(SCM x) {
    question6_executer(scm_to_locale_stringn(x, 0));
    return SCM_UNSPECIFIED;
}

#endif


void terminate(char *line) {
#if USE_GNU_READLINE == 1
	/* rl_clear_history() does not exist yet in centOS 6 */
	clear_history();
#endif
	if (line)
	  free(line);
	printf("exit\n");
	exit(0);
}


int main() {
    printf("Variante %d: %s\n", VARIANTE, VARIANTE_STRING);

#if USE_GUILE == 1
    scm_init_guile();
    /* register "executer" function in scheme */
    scm_c_define_gsubr("executer", 1, 0, 0, executer_wrapper);
#endif

    while (1) {

        updateBgProcesses();

        struct cmdline *l;
        char *line=0;
        char *prompt = "ensishell>";

        line = readline(prompt);
        if (line == 0 || ! strncmp(line,"exit", 4)) {
            terminate(line);
        }

#if USE_GNU_READLINE == 1
        add_history(line);
#endif

#if USE_GUILE == 1
        /* The line is a scheme command */
        if (line[0] == '(') {
            char catchligne[strlen(line) + 256];
            sprintf(catchligne, "(catch #t (lambda () %s) (lambda (key . parameters) (display \"mauvaise expression/bug en scheme\n\")))", line);
            scm_eval_string(scm_from_locale_string(catchligne));
            free(line);
            continue;
        }
#endif

        /* parsecmd free line and set it up to 0 */
        l = parsecmd(&line);

        /* If input stream closed, normal termination */
        if (!l) {
            terminate(0);
        }

        if (l->err) {
            /* Syntax error, read another command */
            printf("error: %s\n", l->err);
            continue;
        }

        // Ajouter la vérification pour la commande "ulimit"
        if (strcmp(l->seq[0][0], "ulimit") == 0) {
            if (l->seq[0][1] != NULL) {
                int soft_limit = atoi(l->seq[0][1]);
                if (soft_limit > 0) {
                    cpu_limit.rlim_cur = soft_limit;
                    cpu_limit.rlim_max = soft_limit + 5;
                    printf("CPU time limit set to %d seconds (soft), %ld seconds (hard)\n", soft_limit, cpu_limit.rlim_max);
                } else {
                    printf("ulimit: invalid limit: %s\n", l->seq[0][1]);
                }
            } else {
                printf("ulimit: missing limit argument\n");
            }
        }
        // Vérifier la commande "jobs"
        else if (strcmp(l->seq[0][0], "jobs") == 0) {
            updateBgProcesses();  // Update before showing jobs
            jobs();  // Appeler la fonction jobs pour afficher les processus en tâche de fond
        }
        // Vérifier s'il y a un pipe
        else if (l->seq[1] != NULL) {
            // Il y a un pipe (plus d'une commande)
            executePipeline(l);
        }
        else {
            executeCommand(l);  // Exécuter la commande normalement
        }
    }
}




int main2() {
        printf("Variante %d: %s\n", VARIANTE, VARIANTE_STRING);

#if USE_GUILE == 1
        scm_init_guile();
        /* register "executer" function in scheme */
        scm_c_define_gsubr("executer", 1, 0, 0, executer_wrapper);
#endif

	while (1) {

		updateBgProcesses();

		struct cmdline *l;
		char *line=0;
		//int i, j;
		char *prompt = "ensishell>";

		/* Readline use some internal memory structure that
		   can not be cleaned at the end of the program. Thus
		   one memory leak per command seems unavoidable yet */
		line = readline(prompt);
		if (line == 0 || ! strncmp(line,"exit", 4)) {
			terminate(line);
		}

#if USE_GNU_READLINE == 1
		add_history(line);
#endif


#if USE_GUILE == 1
		/* The line is a scheme command */
		if (line[0] == '(') {
			char catchligne[strlen(line) + 256];
			sprintf(catchligne, "(catch #t (lambda () %s) (lambda (key . parameters) (display \"mauvaise expression/bug en scheme\n\")))", line);
			scm_eval_string(scm_from_locale_string(catchligne));
			free(line);
                        continue;
                }
#endif

		/* parsecmd free line and set it up to 0 */
		l = parsecmd( & line);

		/* If input stream closed, normal termination */
		if (!l) {
		  
			terminate(0);
		}
		

		
		if (l->err) {
			/* Syntax error, read another command */
			printf("error: %s\n", l->err);
			continue;
		}

	// Ajouter la vérification pour la commande "jobs"
    if (strcmp(l->seq[0][0], "jobs") == 0) {
		updateBgProcesses();  // Update before showing jobs
        jobs();  // Appeler la fonction jobs pour afficher les processus en tâche de fond
    } else if (l->seq[1] != NULL){
		// Il y a un pipe (plus d'une commande)
            executePipeline(l);
	}
	
	
	else {
        executeCommand(l);  // Exécuter la commande normalement
    }
	}
}
