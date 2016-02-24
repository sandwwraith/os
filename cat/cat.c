#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#define BUF_SIZE 100

void transfer(char* buffer, int fd_from) 
{
	ssize_t br;
	while ((br=read(fd_from, buffer, BUF_SIZE))>0)
	{
		write(1, buffer, br);
	}
}

int main(int argc, char** argv)
{
	char* buffer = (char*) malloc(BUF_SIZE);
	
	if (argc == 1)
	{
		//Reopened stdin
		transfer(buffer,0);
	}
	else 
	{
		size_t i;
		for (i = 1; i<argc; ++i)
		{
			int fd_from = open(argv[i], O_RDONLY);
			if (fd_from == -1) continue;
			transfer(buffer,fd_from);
			close(fd_from);
		}
	}
		
	free(buffer);
	return 0;
}
