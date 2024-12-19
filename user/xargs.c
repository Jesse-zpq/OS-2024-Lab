// xargs.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

// �������б�ִ��ĳ������
void run(char* program, char** args) 
{
	if (fork() == 0)
	{ // child exec
		exec(program, args);
		exit(0);
	}
	return; // parent return
}

int main(int argc, char* argv[]) 
{
	char buf[2048]; // ����ʱʹ�õ��ڴ��
	char* p = buf, * last_p = buf; // ��ǰ�����Ľ�������ʼָ��
	char* args_array[128]; // ȫ�������б��ַ���ָ�����飬���� argv �������Ĳ����� stdin ����Ĳ���
	char** args = args_array; // ָ�� argsbuf �е�һ���� stdin ����Ĳ���
	for (int i = 1; i < argc; i++) 
	{
		// �� argv �ṩ�Ĳ������뵽���յĲ����б���
		*args = argv[i];
		args++;
	}
	char** p_cur = args; // ��ʼ�������
	while (read(0, p, 1) != 0) 
	{
		if (*p == ' ' || *p == '\n') 
		{
			// ����һ���������
			*p = '\0';	// ���ո��滻Ϊ '\0' �ָ������������������ֱ��ʹ��array�е��ַ�����Ϊ�����ַ���

			*(p_cur++) = last_p;
			last_p = p + 1;

			if (*p == '\n') 
			{
				// ����һ�����
				*p_cur = 0; // �����б�ĩβ�� null ��ʶ�б����
				run(argv[1], args_array); // ִ�����һ��ָ��
				p_cur = args; // ���ö������ָ�룬׼��������һ��
			}
		}
		p++;
	}
	if (p_cur != args)
	{ // ������һ�в��ǿ���

		*p = '\0';
		*(p_cur++) = last_p;

		*p_cur = 0; // �����б�ĩβ�� null ��ʶ�б����
		// ִ�����һ��ָ��
		run(argv[1], args_array);
	}
	while (wait(0) != -1) 
	{}; // ѭ���ȴ������ӽ������
	exit(0);
}