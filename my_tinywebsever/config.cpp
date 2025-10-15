#include "config.h"

Config::Config()
{
    // 端口号 默认 9006
    port = 9006;

    // 日志写入方式
    LOGWrite = 0;

    // 触发组合方式 默认 listenfd LT + connfd LT
    TRIGMode = 0;
    LISTENTrigmode = 0;
    CONNTrigmode = 0;

    OPT_LINGER = 0;

    sql_num = 8;

    thread_num = 8;

    close_log = 0;

    // 并发模型 默认 proactor
    actor_model = 0;
}

void Config::parse_arg(int argc, char *argv[])
{
    int opt;
    const char *str = "p:l:m:o:s:t:c:a:";
    while ((opt = getopt(argc, argv, str)) != -1)
    {
        switch (opt)
        {
        case 'p':
        {
            port = atoi(optarg);
            break;
        }
        case 'l':
        {
            LOGWrite = atoi(optarg);
            break;
        }
        case 'm':
        {
            TRIGMode = atoi(optarg);
            break;
        }
        case 'o':
        {
            OPT_LINGER = atoi(optarg);
            break;
        }
        case 's':
        {
            sql_num = atoi(optarg);
            break;
        }
        case 't':
        {
            thread_num = atoi(optarg);
            break;
        }
        case 'c':
        {
            close_log = atoi(optarg);
            break;
        }
        case 'a':
        {
            actor_model = atoi(optarg);
            break;
        }
        default:
            break;
        }
    }
}