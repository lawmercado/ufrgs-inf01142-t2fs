/*
 * hexdump.c - dumps a binary file in hex (vaguely readable)
 *
 * @version 2005.10.22
 * @author Andrew M
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>

typedef struct OPTIONS_STR
{
	int max;
	int printName;
} OPTIONS;

void set_default_options(OPTIONS *options)
{
	options->max = -1;
	options->printName = 0;
}

void set_options(OPTIONS *options, char *arg)
{
	if (strlen(arg) < 2)
	{
		return;
	}
	
	if (arg[0] != '-')
	{
		return;
	}
	
	if (arg[1] == 'n')
	{
		options->printName = 1;
	}
}

void usage()
{
    printf("usage: hexdump file1 [file2...]\n");
    return;
}

void do_file(FILE *in, FILE *out, OPTIONS *options)
{
	char ch;
    int loop = 0;
    char buf[81];
    buf[0] = '\0';

    while (options->max == -1 || loop < options->max)
	{
		if (feof(in))
		{
			break;
		}

		if (loop % 16 == 0)
		{
			if (strlen(buf) > 0)
			{
				fprintf(out, "   %s", buf);
                buf[0] = '\0';
			}
            fprintf(out, "\n");
            fprintf(out, "%08x: ", loop);
		}
		else if (loop % 8 == 0)
		{
			fprintf(out, " - ");
			strcat(buf, " ");
		}
		else
		{
			fprintf(out, " ");
		}
        fread(&ch, 1, 1, in);
		fprintf(out, "%02x", (int)(ch & 0x00FF));

		if (ch <= 0)
		{
			strcat(buf, ".");
		}
		else if (isalnum(ch))
		{
			char tmp[2];
			tmp[0] = ch;
			tmp[1] = '\0';
			strcat(buf, tmp);
		}
		else
		{
			strcat(buf, ".");
		}
		loop++;
	}
	int len = strlen(buf);
	if (len > 0)
	{
		int spaces = 3 + 3 * (17 - len);
		if (len < 8)
		{
			spaces--;
		}
		int loop;
		for (loop = 0; loop < spaces; loop++)
		{	
			fputs(" ", out);
		}
		fputs(buf, out);
	}
}

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		usage();
		return 1;
	}
	OPTIONS options;
	set_default_options(&options);
	
	int arg;
	for (arg = 1; arg < argc; arg++)
	{
		char *current = argv[arg];
		FILE *f = fopen(current, "rb");
		if (f == NULL)
		{
			fprintf(stdout, "Unable to open file '%s'", current);
		}
		else
		{
			do_file(f, stdout, &options);
			fclose(f);
		}
	}
	return 0;
}
