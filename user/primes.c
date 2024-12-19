#include "kernel/types.h"
#include "user/user.h"

#define R 0
#define W 1
#define N 35

const uint INT_LEN = sizeof(int);

/**
 * @brief ��ȡ���ھӵĵ�һ������
 * @param lpipe ���ھӵĹܵ���
 * @param pfirst ���ڴ洢��һ�����ݵĵ�ַ
 * @return ���û�����ݷ���-1,�����ݷ���0
 */
int lpipe_first_data(int lpipe[2], int* dst)
{
    if (read(lpipe[R], dst, sizeof(int)) == sizeof(int)) 
    {
        printf("prime %d\n", *dst);
        return 0;
    }
    return -1;
}

/**
 * @brief ��ȡ���ھӵ����ݣ������ܱ�first������д�����ھ�
 * @param lpipe ���ھӵĹܵ���
 * @param rpipe ���ھӵĹܵ���
 * @param first ���ھӵĵ�һ������
 */
void transmit_data(int lpipe[2], int rpipe[2], int first)
{
    int data;
    // ��ȡ����
    while (read(lpipe[R], &data, sizeof(int)) == sizeof(int)) 
    {
        // ���޷����������������ҹܵ�
        if (data % first)
        {
            write(rpipe[W], &data, sizeof(int));
        }
    }
    close(lpipe[R]);
    close(rpipe[W]);
}

/**
 * @brief ����ɸ��Ѱ��������ÿ��ɸ����С��Ԫ��һ��������
 * @param lpipe ���ھ�
 */
void primes(int lpipe[2])
{
    close(lpipe[W]);
    int first;
    if (lpipe_first_data(lpipe, &first) == 0) 
    {
        int p[2];
        pipe(p); //��ǰ�ܵ�
        transmit_data(lpipe, p, first);

        if (fork() == 0) 
        {
            primes(p);    //�ݹ�
        }
        else 
        {
            close(p[R]);
            wait(0);
        }
    }
    exit(0);
}

int main(int argc, char const* argv[])
{
    int p[2];
    pipe(p);

    for (int i = 2; i <= N; ++i) //д���ʼ����
    {
        write(p[W], &i, INT_LEN);
    }

    if (fork() == 0) 
    {
        primes(p);
    }
    else 
    {
        close(p[W]);
        close(p[R]);
        wait(0);
    }

    exit(0);
}
