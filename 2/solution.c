#include "parser.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include "string.h"
#include "stdlib.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

struct tree_node {
    struct tree_node *left;
    struct tree_node *right;
    struct expr *expr;
    int result;
    int exit;
};


struct all_pointer{
    struct parser * p;
    struct command_line *l;
    struct tree_node* t;
};


void node_free(struct tree_node *res) {
    if (res == NULL) return;
    node_free(res->left);
    node_free(res->right);
    free(res);
}

void all_free(struct all_pointer *a){
    parser_delete(a->p);
    command_line_delete(a->l);
    node_free(a->t);
}

int
command_exec(const struct command *cmd, struct all_pointer *a) {
    char **args = malloc(sizeof(char *) * (cmd->arg_count + 2));
    args[0] = cmd->exe;
    for (uint32_t i = 0; i < cmd->arg_count; i++) {
        args[i + 1] = cmd->args[i];
    }
    args[cmd->arg_count + 1] = NULL;

    int status;
    int code=0;
    pid_t pid = fork();
    if (pid == 0) {
        execvp(cmd->exe, args);
        free(args);
        all_free(a);
        exit(0);
    } else
        waitpid(pid, &status, WUNTRACED);

    if(WIFEXITED(status)) {
        code = WEXITSTATUS(status);
    }
    free(args);
    return code;
}

struct tree_node *node_init() {
    struct tree_node *res = malloc(sizeof(res[0]));
    res->left = NULL;
    res->right = NULL;
    res->expr = NULL;
    res->result = 0;
    res->expr = 0;
    return res;
}


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
            if(EXPR_TYPE_PIPE == e->type) {
                if(strcmp(e->next->cmd.exe, "cat") == 0){
                    e = e->next->next;
                    node_free(node);
                    continue;
                }
            }
            node->left = root;
            root = node;
        }
        e = e->next;
    }
    return root;
}

static int execute_node(struct tree_node *node, struct  all_pointer *a) {
    int pid1, pid2, status;
    int fd[2];
    const struct expr *e = node->expr;
    node->exit = 0;
    if (e->type == EXPR_TYPE_PIPE) {
        pipe(fd);

        pid1 = fork();
        if (pid1 == 0) {
            close(fd[0]);
            dup2(fd[1], STDOUT_FILENO);
            close(fd[1]);

            execute_node(node->left, a);
            all_free(a);
            exit(0);
        }

        pid2 = fork();
        if (pid2 == 0) {
            close(fd[1]);
            dup2(fd[0], STDIN_FILENO);
            close(fd[0]);
            node->result = execute_node(node->right, a);
            all_free(a);
            exit(node->result);
        }

        close(fd[0]);
        close(fd[1]);
        waitpid(pid1, NULL, 0);
        waitpid(pid2, &status, WUNTRACED);
        node->result = WEXITSTATUS(status);
    } else if (e->type == EXPR_TYPE_AND) {
        node->result = execute_node(node->left, a);
        node->exit = node->left->exit;
        if (node->exit) return node->result;
        if(node->result == 0) {
            node->result = execute_node(node->right, a);
            node->exit = node->right->exit;
            if (node->exit) return node->result;
        }
    } else if (e->type == EXPR_TYPE_OR) {
        node->result = execute_node(node->left, a);
        node->exit = node->left->exit;
        if (node->exit) return node->result;
        if(node->result != 0){
            node->result = execute_node(node->right, a);
            node->exit = node->right->exit;
            if (node->exit) return node->result;
        }
    } else if (e->type == EXPR_TYPE_COMMAND) {
        if (strcmp("cd", e->cmd.exe) == 0) {
            if (e->cmd.arg_count != 0)
                chdir(e->cmd.args[0]);
        } else if (strcmp("exit", e->cmd.exe) == 0) {
            node->exit = 1;
            if(e->cmd.arg_count > 0){
                node->result = atoi(e->cmd.args[0]);
            }
        } else {
            node->result = command_exec(&e->cmd, a);
        }
    } else {
        assert(false);
    }
    return node->result;
}

static int
execute_out_type(struct command_line *line, int * program_result, struct all_pointer* a) {

    assert(line != NULL);
    int exit;
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

    struct tree_node *tree = construct_tree(&line->head);
    (*a).t = tree;
    execute_node(tree, a);
    exit = tree->exit;
    *program_result = tree->result;
    node_free(tree);


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

    return exit;
}

int
main(void) {
    const size_t buf_size = 1024;
    char buf[buf_size];
    int rc;
    int exit = 0;
    int program_result = 0;
    struct parser *p = parser_new();
    struct all_pointer a;
    a.p = p;
    while (!exit && (rc = read(STDIN_FILENO, buf, buf_size)) > 0) {
        parser_feed(p, buf, rc);
        struct command_line *line = NULL;
        while (true) {
            enum parser_error err = parser_pop_next(p, &line);
            a.l = line;
            if (err == PARSER_ERR_NONE && line == NULL)
                break;
            if (err != PARSER_ERR_NONE) {
                printf("Error: %d\n", (int) err);
                continue;
            }
//            printf("head : %s (%s)\ntail : %s(%s)\n",line->head->cmd.exe,line->head->cmd.args[0],line->tail->cmd.exe,line->head->cmd.args[0]);
            exit = execute_out_type(line, &program_result, &a);
            command_line_delete(line);
        }
    }
    parser_delete(p);
    return program_result;
}