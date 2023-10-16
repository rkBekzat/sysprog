#include "parser.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include "string.h"
#include "stdlib.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

void
command_exec(const struct command *cmd) {
    char **args = malloc(sizeof(char *) * (cmd->arg_count + 2));
    args[0] = cmd->exe;
    for (int i = 0; i < cmd->arg_count; i++) {
        args[i + 1] = cmd->args[i];
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

void
show(struct command e) {
    printf("Command: %s\n Args: ", e.exe);
    for (int i = 0; i < e.arg_count; i++) {
        printf("%d %s\n", i, e.args[i]);
    }
    printf("\n");
}

struct tree_node {
    struct tree_node *left;
    struct tree_node *right;
    struct expr *expr;
};

struct tree_node *node_init() {
    struct tree_node *res = malloc(sizeof(res[0]));
    res->left = NULL;
    res->right = NULL;
    res->expr = NULL;
    return res;
}

void node_free(struct tree_node *res) {
    if (res == NULL) return;
    node_free(res->left);
    node_free(res->right);
    free(res);
}

struct command_tree {
    struct tree_node *root;
    struct command_tree *next;
};

struct tree_node *construct_tree(struct expr **expr) {
    struct tree_node *root = NULL;
    struct tree_node *node = NULL;
    struct expr *e = *expr;
    while (e != NULL) {
        node = node_init();
        node->expr = e;
        if (e->type == EXPR_TYPE_COMMAND) {
            if (root == NULL) {
                root = node;
            } else {
                root->right = node;
            }
        } else {
            node->left = root;
            root = node;
        }
        e = e->next;
    }
    return root;
}

struct command_tree *tree_init(struct command_line *line) {
    struct command_tree *res = malloc(sizeof(res[0]));
    res->next = NULL;
    res->root = NULL;
    if (line != NULL) {
        struct expr *e = line->head;
        res->root = construct_tree(&e);
    }
    return res;
}

void tree_free(struct command_tree *res) {
    if (res == NULL) return;
    node_free(res->root);
    free(res);
}


static int execute_node(struct tree_node *node) {
    int pid1, pid2;
    int fd[2];
    const struct expr *e = node->expr;
    if (e->type == EXPR_TYPE_PIPE) {
        pipe(fd);

        pid1 = fork();
        if (pid1 == 0) {
            close(fd[0]);
            dup2(fd[1], STDOUT_FILENO);
            close(fd[1]);
            execute_node(node->left);
            exit(0);
        }

        pid2 = fork();
        if (pid2 == 0) {
            close(fd[1]);
            dup2(fd[0], STDIN_FILENO);
            close(fd[0]);
            execute_node(node->right);
            exit(0);
        }

        close(fd[1]);
        close(fd[0]);
        waitpid(pid1, NULL, 0);
        waitpid(pid2, NULL, 0);
    } else if (e->type == EXPR_TYPE_AND) {
        execute_node(node->left);
        execute_node(node->right);
    } else if (e->type == EXPR_TYPE_OR) {
        int res = execute_node(node->left);
        if(res != 0){
            execute_node(node->right);
        }
    } else if (e->type == EXPR_TYPE_COMMAND) {
        if (strcmp("cd", e->cmd.exe) == 0) {
            if (e->cmd.arg_count != 0)
                chdir(e->cmd.args[0]);
        } else if (strcmp("exit", e->cmd.exe) == 0) {
            return 1;
        } else {
            command_exec(&e->cmd);
        }
    } else {
        assert(false);
    }
    return 0;
}

static int execute_command_line(struct command_tree *tree) {
    int result = 0;
    while (!result && tree != NULL) {
        result = execute_node(tree->root);
        tree = tree->next;
    }
    return result;
}

static int
execute_out_type(struct command_line *line) {

    assert(line != NULL);
    int result;
    int file;
    int io;

    if (line->out_type == OUTPUT_TYPE_FILE_NEW) {
        file = open(line->out_file, O_RDWR | O_CREAT | O_TRUNC | O_APPEND, 0600);
        io = dup(fileno(stdout));
        dup2(file, fileno(stdout));
    } else if (line->out_type == OUTPUT_TYPE_FILE_APPEND) {
        file = open(line->out_file, O_RDWR | O_CREAT | O_APPEND, 0600);
        io = dup(fileno(stdout));
        dup2(file, fileno(stdout));
    } else if (line->out_type != OUTPUT_TYPE_STDOUT) {
        assert(false);
    }

    struct command_tree *tree = tree_init(line);
    result = execute_command_line(tree);
    tree_free(tree);


    if (line->out_type == OUTPUT_TYPE_FILE_NEW) {
        close(file);
        dup2(io, fileno(stdout));
        close(io);
    } else if (line->out_type == OUTPUT_TYPE_FILE_APPEND) {
        fflush(stdout);
        close(file);
        dup2(io, fileno(stdout));
        close(io);
    }

    return result;
}

int
main(void) {
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
                printf("Error: %d\n", (int) err);
                continue;
            }
//            printf("head : %s (%s)\ntail : %s(%s)\n",line->head->cmd.exe,line->head->cmd.args[0],line->tail->cmd.exe,line->head->cmd.args[0]);
            exit = execute_out_type(line);
            command_line_delete(line);
        }
    }
    parser_delete(p);
    return 0;
}
