/*
The main file. 
This file manages the assembling process. 
It calls the first and second read methods, and then creates the output files.
*/

/* ======== Includes ======== */
#include "assembler.h"

#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>

/* ====== Global Data Structures ====== */
/* Labels */
labelInfo g_labelArr[MAX_LABELS_NUM]; 
int g_labelNum = 0;
/* Entry Lines */
lineInfo *g_entryLines[MAX_LABELS_NUM]; /**/
int g_entryLabelsNum = 0;
/* Data */
int g_dataArr[MAX_DATA_NUM];

/* ====== Methods ====== */

/* Print an error with the line number. */
void printError(int lineNum, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	printf("[Error] At line %d: ", lineNum);
	vprintf(format, args);
	printf("\n");
	va_end(args);
}

/* Puts in the given buffer a base 32 representation of num. */
int intToBase32(int num, char *buf)
{
    int numMasked; 
    numMasked = num & 1023;
	const int base = 32;
	const char digits[] = "!@#$%^&*<>abcdefghijklmnopqrstuv";

    buf[0] = digits[numMasked / base];
	buf[1] = digits[numMasked % base];

	return 0;
}

void printStrWithZeros(char *str, int strMinWidth)
{
	int i, numOfZeros = strMinWidth - strlen(str);

	for (i = 0; i < numOfZeros; i++)
	{
		printf("0");
	}
	printf("%s", str);
}

/* Prints a number in base 32 in the file. */
void fprintfBase32(FILE *file, int num, int strMinWidth)
{
	int numOfZeros, i;
	/* 2 chars are enough to represent 10 bits in base 32, and the last char is \0. */
	char buf[3] = { 0 }; 

	intToBase32(num, buf);
	fprintf(file, "%s", buf);
}

/* Creates a file (for writing) from a given name and ending, and returns a pointer to it. */
FILE *openFile(char *name, char *ending, const char *mode)
{
	FILE *file;
	char *mallocStr = (char *)malloc(strlen(name) + strlen(ending) + 1), *fileName = mallocStr;
	sprintf(fileName, "%s%s", name, ending);

	file = fopen(fileName, mode);
	free(mallocStr);

	return file;
}

/* Creates the .obj file, which contains the assembled lines in base 32. */
void createObjectFile(char *name, int IC, int DC, int *memoryArr)
{
	int i;
	FILE *file;
	file = openFile(name, ".ob", "w");

	/* Print header*/
	fprintf(file, "Base32 address  Base32 code\n");
	fprintf(file, "           m    f");

	/* Print all of memoryArr */
	for (i = 0; i < IC + DC; i++)
	{
		fprintf(file, "\n       ");
		fprintfBase32(file, FIRST_ADDRESS + i, 2);
		fprintf(file, "\t\t  ");
		fprintfBase32(file, memoryArr[i], 2);
	}

	fclose(file);
}

/* Creates the .ent file, which contains the addresses for the .entry labels in base 32. */
void createEntriesFile(char *name)
{
	int i;
	FILE *file;

	/* Don't create the entries file if there aren't entry lines */
	if (!g_entryLabelsNum)
	{
		return;
	}

	file = openFile(name, ".ent", "w");

	for (i = 0; i < g_entryLabelsNum; i++)
	{
		fprintf(file, "%s\t\t", g_entryLines[i]->lineStr);
		fprintfBase32(file, getLabel(g_entryLines[i]->lineStr)->address, 1);

		if (i != g_entryLabelsNum - 1)
		{
			fprintf(file, "\n");
		}
	}

	fclose(file);
}

/* Creates the .ext file, which contains the addresses for the extern labels operands in base 32. */
void createExternFile(char *name, lineInfo *linesArr, int linesFound)
{
	int i;
	labelInfo *label;
	bool firstPrint = TRUE; /* This bool meant to prevent the creation of the file if there aren't any externs */
	FILE *file = NULL;

	for (i = 0; i < linesFound; i++)
	{
		/* Check if the 1st operand is extern label, and print it. */
		if (linesArr[i].cmd && linesArr[i].cmd->numOfParams >= 2 && linesArr[i].op1.type == LABEL)
		{
			label = getLabel(linesArr[i].op1.str);
			if (label && label->isExtern)
			{
				if (firstPrint)
				{
					/* Create the file only if there is at least 1 extern */
					file = openFile(name, ".ext", "w");
				}
				else
				{
					fprintf(file, "\n");
				}

				fprintf(file, "%s\t\t", label->name);
				fprintfBase32(file, linesArr[i].op1.address, 1);
				firstPrint = FALSE;
			}
		}

		/* Check if the 2nd operand is extern label, and print it. */
		if (linesArr[i].cmd && linesArr[i].cmd->numOfParams >= 1 && linesArr[i].op2.type == LABEL)
		{
			label = getLabel(linesArr[i].op2.str);
			if (label && label->isExtern)
			{
				if (firstPrint)
				{
					/* Create the file only if there is at least 1 extern */
					file = openFile(name, ".ext", "w");
				}
				else
				{
					fprintf(file, "\n");
				}

				fprintf(file, "%s\t\t", label->name);
				fprintfBase32(file, linesArr[i].op2.address, 1);
				firstPrint = FALSE;
			}
		}
	}

	if (file)
	{
		fclose(file);
	}
}

/* Resets all the globals and free all the malloc blocks. */
void clearData(lineInfo *linesArr, int linesFound, int dataCount)
{
	int i;

	/* --- Reset Globals --- */

	/* Reset global labels */
	for (i = 0; i < g_labelNum; i++)
	{
		g_labelArr[i].address = 0;
		g_labelArr[i].isData = 0;
		g_labelArr[i].isExtern = 0;
	}
	g_labelNum = 0;

	/* Reset global entry lines */
	for (i = 0; i < g_entryLabelsNum; i++)
	{
		g_entryLines[i] = NULL;
	}
	g_entryLabelsNum = 0;

	/* Reset global data */
	for (i = 0; i < dataCount; i++)
	{
		g_dataArr[i] = 0;
	}

	/* Free malloc blocks */
	for (i = 0; i < linesFound; i++)
	{
		free(linesArr[i].originalString);
	}
}

/* Parsing a file, and creating the output files. */
void parseFile(char *fileName)
{
    removeMacros(fileName);
 	FILE *file = openFile(fileName, ".am", "r");
	lineInfo linesArr[MAX_LINES_NUM];
	int memoryArr[MAX_DATA_NUM] = { 0 }, IC = 0, DC = 0, numOfErrors = 0, linesFound = 0;

	/* Open File */
	if (file == NULL)
	{
		printf("[Info] Can't open the file \"%s.as\".\n", fileName);
		return;
	}
	printf("[Info] Successfully opened the file \"%s.as\".\n", fileName);

	/* First Read */
	numOfErrors += firstFileRead(file, linesArr, &linesFound, &IC, &DC);
	/* Second Read */
	numOfErrors += secondFileRead(memoryArr, linesArr, linesFound, IC, DC);

	/* Create Output Files */
	if (numOfErrors == 0)
	{
		/* Create all the output files */
		createObjectFile(fileName, IC, DC, memoryArr);
		createExternFile(fileName, linesArr, linesFound); 
		createEntriesFile(fileName);
		printf("[Info] Created output files for the file \"%s.as\".\n", fileName);
	}
	else
	{
		/* print the number of errors. */
		printf("[Info] A total of %d error%s found throughout \"%s.as\".\n", numOfErrors, (numOfErrors > 1) ? "s were" : " was", fileName);
	}

	/* Free all malloc pointers, and reset the globals. */
	clearData(linesArr, linesFound, IC + DC);

	/* Close File */
	fclose(file);
}

/* Main method. Calls the "parsefile" method for each file name in argv. */
int main(int argc, char *argv[])
{
	int i;

	if (argc < 2)
	{
		printf("[Info] no file names were observed.\n");
		return 1;
	}
		
	for (i = 1; i < argc; i++)
	{
		parseFile(argv[i]);
		printf("\n");
	}

	return 0;
}