#define _GNU_SOURCE 

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "list.h"
#include "mu.h"


#define CMD_INITIAL_CAP_ARGS 8

#define USAGE \
    "Usage: bsh [-h]\n" \
    "\n" \
    "optional arguments\n" \
    "   -h, --help\n" \
    "       Show usage statement and exit."

struct cmd {
    struct list_head list;

    char **args;
    size_t num_args;
    size_t cap_args;

    pid_t pid;
};

struct pipeline {
    struct list_head head;  /* cmds */
    size_t num_cmds;
    char *in_file;
    char *out_file;
    bool append;
};


static struct cmd *
cmd_new(void)
{
    MU_NEW(cmd, cmd);
    size_t i;

    cmd->cap_args = CMD_INITIAL_CAP_ARGS;
    cmd->args = mu_mallocarray(cmd->cap_args, sizeof(char *));

    for (i = 0; i < cmd->cap_args; i++)
        cmd->args[i] = NULL;

    return cmd;
}


static void
cmd_push_arg(struct cmd *cmd, const char *arg)
{
    if (cmd->num_args == cmd->cap_args) {
        cmd->args = mu_reallocarray(cmd->args, cmd->cap_args * 2, sizeof(char *));
        cmd->cap_args *= 2;
    }

    cmd->args[cmd->num_args] = mu_strdup(arg);
    cmd->num_args += 1;
}


static void
cmd_pop_arg(struct cmd *cmd)
{
    assert(cmd->num_args > 0);

    free(cmd->args[cmd->num_args - 1]);
    cmd->args[cmd->num_args - 1] = NULL;

    cmd->num_args--;
}


static void
cmd_free(struct cmd *cmd)
{
    size_t i;

    for (i = 0; i < cmd->num_args; i++)
        free(cmd->args[i]);

    free(cmd->args);
    free(cmd);
}


static void
cmd_print(const struct cmd *cmd)
{
    size_t i;

    printf("cmd {num_args:%zu, cap_args:%zu}:\n",
            cmd->num_args, cmd->cap_args);
    for (i = 0; i < cmd->num_args; i++)
        printf("\t[%zu] = \"%s\"\n", i, cmd->args[i]);
}


static struct pipeline *
pipeline_new(char *line)
{
    MU_NEW(pipeline, pipeline);
    struct cmd *cmd = NULL;
    char *s1, *s2, *command, *arg;
    char *saveptr1, *saveptr2;
    int i;

    INIT_LIST_HEAD(&pipeline->head);

    for (i = 0, s1 = line; ; i++, s1 = NULL) {
        /* break into commands */
        command = strtok_r(s1, "|", &saveptr1);
        if (command == NULL)
            break;

        cmd = cmd_new();

        /* parse the args of a single command */
        for (s2 = command; ; s2 = NULL) {
            arg = strtok_r(s2, " \t", &saveptr2);
            if (arg == NULL)
                break;
            cmd_push_arg(cmd, arg);
        }

        list_add_tail(&cmd->list, &pipeline->head);
        pipeline->num_cmds += 1;
    }

    /* TODO: parse I/O redirects */

    return pipeline;
}


static void
pipeline_free(struct pipeline *pipeline)
{
    struct cmd *cmd, *tmp;

    list_for_each_entry_safe(cmd, tmp, &pipeline->head, list) {
        list_del(&cmd->list);
        cmd_free(cmd);
    }

    free(pipeline);
}


static void
pipeline_print(const struct pipeline *pipeline)
{
    struct cmd *cmd;

    list_for_each_entry(cmd, &pipeline->head, list) {
        cmd_print(cmd);
    }
}

static void
cat(FILE * fp, int line_max){
    char * line;
    size_t n = 0;
    ssize_t len = 0;
    int line_num = 0;
    while (len >= 0 && line_num < line_max) {
        len = getline(&line, &n, fp);
        line_num++;
        printf("%s", line);
    }
    rewind(fp);
}

static void
evalcmd(struct cmd * cmd, FILE * fp){
    int line_max = 100;

    int opt, nargs;
    const char *short_opts = ":n:";
    struct option long_opts[] = {
            {"line", argument_required, NULL, 'n'},
            {NULL, 0, NULL, 0}
    };
    while (1) {
        opt = getopt_long(&cmd->num_args, &cmd->args, short_opts, long_opts, NULL);
        if (opt == -1)
            break;
        switch(opt){
            case 'n':
                mu_str_to_int(optarg, 10, &line_max);
                break;
            case '?':
                mu_die("unknown option '%c' (decimal: %d)", optopt, optopt);
            case ':':
                mu_die("missing option argument for option %c", optopt);
            default :
                mu_die("unexpected getopt_long return value: %c\n", (char)opt);
        }
    }

    int i;
    nargs = &cmd->num_args - optind;
    if(nargs){
        fp = fopen(argv[optind], "r");
        if (fp == NULL)
            mu_die_errno(errno, "can't create file");
        setlinebuf(fp);
    }


    for (i = 0; i < cmd->num_args; i++){
        if(strcmp((cmd->args[i]), "cat") == 0 || strcmp((cmd->args[i]), "head") == 0){
            cat(fp, line_max);
        }
    }
}

static void
usage(int status)
{
    puts(USAGE);
    exit(status);
}

int
main(int argc, char *argv[])
{
    ssize_t len_ret = 0;
    size_t len = 0;
    char *line = NULL;
    struct pipeline *pipeline = NULL;
    struct cmd *cmd, *tmp;
    FILE * fp;

    int opt, nargs;
    const char *short_opts = ":h";
    struct option long_opts[] = {
            {"help", no_argument, NULL, 'h'},
            {NULL, 0, NULL, 0}
    };
    while (1) {
        opt = getopt_long(argc, argv, short_opts, long_opts, NULL);
        if (opt == -1)
            break;
        switch(opt){
            case 'h':
                usage(0);
                break;
            case '?':
                mu_die("unknown option '%c' (decimal: %d)", optopt, optopt);
            case ':':
                mu_die("missing option argument for option %c", optopt);
            default :
                mu_die("unexpected getopt_long return value: %c\n", (char)opt);
        }
    }

  

    /* REPL */
    while (1) {
        if (isatty(fileno(stdin)))
            printf("> ");
        len_ret = getline(&line, &len, stdin);
        if (len_ret == -1)
            goto out;
        
        mu_str_chomp(line);
        pipeline = pipeline_new(line);
    
        pipeline_print(pipeline);
        list_for_each_entry(cmd, &pipeline->head, list) {
            evalcmd(cmd, fp);
        }
        
        

        pipeline_free(pipeline);
    }

out:
    free(line);
    return 0;
}