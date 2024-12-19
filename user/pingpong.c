#include "kernel/types.h"
#include "user/user.h"

#define R 0 //pipe��read��
#define W 1 //pipe��write��

int main(int argc, char const* argv[]) 
{
    char buf = 'P'; //���ڴ��͵��ֽ�

    int fd_c_p[2]; //�ӽ���->������
    int fd_p_c[2]; //������->�ӽ���
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
    { //�ӽ���
        close(fd_p_c[W]);
        close(fd_c_p[R]);

        if (read(fd_p_c[R], &buf, sizeof(char)) != sizeof(char)) //ʧ��read
        {
            fprintf(2, "child read() error!\n");
            exit_status = 1; 
        }
        else //�ɹ�read
        {
            fprintf(1, "%d: received ping\n", getpid());
        }

        if (write(fd_c_p[W], &buf, sizeof(char)) != sizeof(char)) //ʧ��write
        {
            fprintf(2, "child write() error!\n");
            exit_status = 1;
        }

        close(fd_p_c[R]);
        close(fd_c_p[W]);

        exit(exit_status);
    }
    else 
    { //������
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