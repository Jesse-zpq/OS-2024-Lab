#include "kernel/types.h"
#include "user/user.h"

#define R 0
#define W 1
#define N 35

const uint INT_LEN = sizeof(int);

/**
 * @brief 读取左邻居的第一个数据
 * @param lpipe 左邻居的管道符
 * @param pfirst 用于存储第一个数据的地址
 * @return 如果没有数据返回-1,有数据返回0
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
 * @brief 读取左邻居的数据，将不能被first整除的写入右邻居
 * @param lpipe 左邻居的管道符
 * @param rpipe 右邻居的管道符
 * @param first 左邻居的第一个数据
 */
void transmit_data(int lpipe[2], int rpipe[2], int first)
{
    int data;
    // 读取数据
    while (read(lpipe[R], &data, sizeof(int)) == sizeof(int)) 
    {
        // 将无法整除的数传递入右管道
        if (data % first)
        {
            write(rpipe[W], &data, sizeof(int));
        }
    }
    close(lpipe[R]);
    close(rpipe[W]);
}

/**
 * @brief 素数筛法寻找素数，每次筛后最小的元素一定是素数
 * @param lpipe 左邻居
 */
void primes(int lpipe[2])
{
    close(lpipe[W]);
    int first;
    if (lpipe_first_data(lpipe, &first) == 0) 
    {
        int p[2];
        pipe(p); //当前管道
        transmit_data(lpipe, p, first);

        if (fork() == 0) 
        {
            primes(p);    //递归
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

    for (int i = 2; i <= N; ++i) //写入初始数据
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
