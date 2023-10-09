#include "parser.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include "string.h"
#include "stdlib.h"
#include <sys/types.h>
#include <sys/wait.h>


void command_exec(const struct command *cmd) {
    char **args = malloc(sizeof(char *) * (cmd->arg_count + 2));
    args[0] = cmd->exe;
    for (int i=0;i<cmd->arg_count;i++){
        args[i+1] = cmd->args[i];
    }
    args[cmd->arg_count + 1] = NULL;

    pid_t pid = fork();
    if (pid == 0) {
        execvp(cmd->exe, args);
        exit(0);
    } else
        waitpid(pid, NULL, 0);

    free(args);
}


static int
execute_command_line(const struct command_line *line)
{
	/* REPLACE THIS CODE WITH ACTUAL COMMAND EXECUTION */

	assert(line != NULL);
    printf("Output: ");
	if (line->out_type == OUTPUT_TYPE_STDOUT) {
		printf("stdout\n");
	} else if (line->out_type == OUTPUT_TYPE_FILE_NEW) {
		printf("new file - \"%s\"\n", line->out_file);
	} else if (line->out_type == OUTPUT_TYPE_FILE_APPEND) {
		printf("append file - \"%s\"\n", line->out_file);
	} else {
		assert(false);
	}
	const struct expr *e = line->head;
	while (e != NULL) {
        if (e->type == EXPR_TYPE_COMMAND && strcmp("cd", e->cmd.exe) == 0) {
            if (e->cmd.arg_count != 0)
                chdir(e->cmd.args[0]);
        } else if (e->type == EXPR_TYPE_COMMAND && strcmp("exit", e->cmd.exe) == 0) {
            printf("EXIT\n");
            return 1;
        } else if (e->type == EXPR_TYPE_COMMAND) {
            command_exec(&e->cmd);
        } else if (e->type == EXPR_TYPE_PIPE) {
			printf("\tPIPE\n");
		} else if (e->type == EXPR_TYPE_AND) {
			printf("\tAND\n");
		} else if (e->type == EXPR_TYPE_OR) {
			printf("\tOR\n");
		} else {
			assert(false);
		}
        e = e->next;
	}
    return 0;
}

int
main(void)
{
	const size_t buf_size = 1024;
	char buf[buf_size];
	int rc;
    int exit = 0;
	struct parser *p = parser_new();
	while (!exit && (rc = read(STDIN_FILENO, buf, buf_size)) > 0) {
		parser_feed(p, buf, rc);
		struct command_line *line = NULL;
		while (true) {
			enum parser_error err = parser_pop_next(p, &line);
			if (err == PARSER_ERR_NONE && line == NULL)
				break;
			if (err != PARSER_ERR_NONE) {
				printf("Error: %d\n", (int)err);
				continue;
			}
            exit = execute_command_line(line);
			command_line_delete(line);
        }
	}
	parser_delete(p);
	return 0;
}
