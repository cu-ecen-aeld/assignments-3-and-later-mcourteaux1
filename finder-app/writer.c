
#include "stdint.h"
#include "stdio.h"

#include "syslog.h"

int main(int argc, char** argv)
{
	openlog(NULL, 0, LOG_USER);
	
	//first is application, therfore -1 to get user supplied args
	if ((argc-1) != 2) 
	{
		printf("Usage: write <path> <print string>\n");
		syslog(LOG_ERR, "not enough arguments got %d, expected 2", argc);
		return 1;
	}

	const char* filePath = argv[1];
	const char* printString = argv[2];

	syslog(LOG_DEBUG, "Writing message %s to file %s", filePath, printString);
	closelog();

	FILE* mFile = fopen(filePath, "wt"); //no append
	if (!mFile)
	{
		syslog(LOG_ERR, "could not create file %s", filePath);
		closelog();
		return 1;
	}

	fprintf(mFile, "%s\n", printString);
	fclose(mFile);
	syslog(LOG_USER, "string write sucess");
	closelog();
	return 0;
}
