#include "kernel/types.h"
#include "user/user.h"

#define R 0 //pipe的read端
#define W 1 //pipe的write端

int main(int argc, char const* argv[]) 
{
    char buf = 'P'; //用于传送的字节

    int fd_c_p[2]; //子进程->父进程
    int fd_p_c[2]; //父进程->子进程
    pipe(fd_c_p);
    pipe(fd_p_c);

    int pid = fork();
    int exit_status = 0;

    if (pid < 0) 
    {
        fprintf(2, "fork() error!\n");
        close(fd_c_p[R]);
        close(fd_c_p[W]);
        close(fd_p_c[R]);
        close(fd_p_c[W]);
        exit(1);
    }
    else if (pid == 0) 
    { //子进程
        close(fd_p_c[W]);
        close(fd_c_p[R]);

        if (read(fd_p_c[R], &buf, sizeof(char)) != sizeof(char)) //失败read
        {
            fprintf(2, "child read() error!\n");
            exit_status = 1; 
        }
        else //成功read
        {
            fprintf(1, "%d: received ping\n", getpid());
        }

        if (write(fd_c_p[W], &buf, sizeof(char)) != sizeof(char)) //失败write
        {
            fprintf(2, "child write() error!\n");
            exit_status = 1;
        }

        close(fd_p_c[R]);
        close(fd_c_p[W]);

        exit(exit_status);
    }
    else 
    { //父进程
        close(fd_p_c[R]);
        close(fd_c_p[W]);

        if (write(fd_p_c[W], &buf, sizeof(char)) != sizeof(char)) 
        {
            fprintf(2, "parent write() error!\n");
            exit_status = 1;
        }

        if (read(fd_c_p[R], &buf, sizeof(char)) != sizeof(char)) 
        {
            fprintf(2, "parent read() error!\n");
            exit_status = 1; 
        }
        else 
        {
            fprintf(1, "%d: received pong\n", getpid());
        }

        close(fd_p_c[W]);
        close(fd_c_p[R]);

        exit(exit_status);
    }
}