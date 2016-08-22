/*
 * doc2rtf.c  -- program to convert Gnuplot .DOC format to MS windows
 * help (.rtf) format.
 *
 * This involves stripping all lines with a leading digit or
 * a leading @, #, or %.
 * Modified by Maurice Castro from doc2gih.c by Thomas Williams 
 * Further modified for PM GSview by Russell Lang  1996-10-15
 *   Write Win4 contents file.  Not working yet because Win4
 *     considers a book to end when another book starts.
 *     It does not support the hierarchical structure of
 *     the GSview help file where topics may occur at the
 *     same level as a book and after the book.
 *   First line of file is Window Title.
 *
 * usage:  doc2rtf file.doc file.rtf [file.cnt] [-d]
 *
 */

/* note that tables must begin in at least the second column to */
/* be formatted correctly and tabs are forbidden */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#define MAX_LINE_LEN	1024
#define TRUE 1
#define FALSE 0
char *hex = "0123456789ABCDEF";

struct LIST
{
	int level;
	int line;
	char *string;
	struct LIST *next;
	};

struct LIST *list = NULL;
struct LIST *head = NULL;

struct LIST *keylist = NULL;
struct LIST *keyhead = NULL;

int debug = FALSE;

void parse(FILE *a, FILE *b);
int lookup(char *s);
void putquoted(char *s, FILE *f);
void refs(int l, FILE *f);
void footnote(char c, char *s, FILE *b);
void convert(FILE *a, FILE *b);
void process_line(char *line, FILE *b);

int
main(int argc, char **argv)
{
FILE * infile;
FILE * outfile;
FILE * cntfile = NULL;
	if (argc>1 && argv[argc-1][0]=='-' && argv[argc-1][1]=='d') {
	    debug = TRUE;
	    argc--;
	}

        if ( (argc > 4) || (argc < 3) ) {
		fprintf(stderr,"Usage: %s infile outfile\n", argv[0]);
		return(1);
	}

	if ( (infile = fopen(argv[1],"r")) == (FILE *)NULL) {
		fprintf(stderr,"%s: Can't open %s for reading\n",
			argv[0], argv[1]);
		return(1);
	}
	if ( (outfile = fopen(argv[2],"w")) == (FILE *)NULL) {
		fprintf(stderr,"%s: Can't open %s for writing\n",
			argv[0], argv[2]);
	}
	if (argc==4) {
	  if ( (cntfile = fopen(argv[3],"w")) == (FILE *)NULL) {
	    fprintf(stderr,"%s: Can't open %s for writing\n",
		argv[0], argv[3]);
	  }
	}
	if (cntfile) {
	    char basename[256];
	    strcpy(basename, argv[2]);
	    strtok(basename, ".");
	    strcat(basename, ".hlp");
	    fprintf(cntfile, ":Base %s\n", basename);
	}

	parse(infile, cntfile);
	convert(infile,outfile);
	return(0);
}

/* scan the file and build a list of line numbers where particular levels are */
void parse(FILE *a, FILE *b)
{
    static char line[MAX_LINE_LEN];
	char *c;
	int lineno=0;
	int lastline=0;
    struct LIST *lasttopic=NULL;
    if (fgets(line, MAX_LINE_LEN, a)) {
	if (b)
	    fprintf(b, ":Title %s", line+1);
    }
    while (fgets(line,MAX_LINE_LEN,a)) 
    {
	lineno++;
	if (isdigit(line[0]))
	{
		if (list == NULL)	
			head = (list = (struct LIST *) malloc(sizeof(struct LIST)));
		else
			list = (list->next = (struct LIST *) malloc(sizeof(struct LIST)));
		list->line = lastline = lineno;
		list->level = line[0] - '0';
		list->string = (char *) malloc (strlen(line)+1);
		c = strtok(&(line[1]),"\n");
		strcpy(list->string, c);
		list->next = NULL;
		if (b) {
		    if (lasttopic) {
/* Doesn't work, so just dump all at the top level
			if (list->level > lasttopic->level) {
			    fprintf(b, "%d %s\n", lasttopic->level, lasttopic->string);
			    fprintf(b, "%d %s=loc%d\n", lasttopic->level+1, lasttopic->string, lasttopic->line);
			}
			else
*/
			    fprintf(b, "%d %s=loc%d\n", lasttopic->level, lasttopic->string, lasttopic->line);
		    }
		    lasttopic = list;
		}
	}
	if (line[0]=='?')
	{
		if (keylist == NULL)	
			keyhead = (keylist = (struct LIST *) malloc(sizeof(struct LIST)));
		else
			keylist = (keylist->next = (struct LIST *) malloc(sizeof(struct LIST)));
		keylist->line = lastline;
		keylist->level = line[0] - '0';
		keylist->string = (char *) malloc (strlen(line)+1);
		c = strtok(&(line[1]),"\n");
		strcpy(keylist->string, c);
		keylist->next = NULL;
	}
	}
	if (b)
	    fprintf(b, "%d %s=loc%d\n", lasttopic->level, lasttopic->string, lasttopic->line);
	rewind(a);
    }

/* look up an in text reference */
int
lookup(char *s)
{
	char *c;
	char tokstr[MAX_LINE_LEN];

	strcpy(tokstr, s);

	/* first try the ? keyword entries */
	keylist = keyhead;
	while (keylist != NULL)
	{
		c = keylist->string;
		while (isspace(*c)) c++;
		if (!strcmp(s, c)) return(keylist->line);
		keylist = keylist->next;
		}

	/* then try titles */
#ifdef GNUPLOT
	match = strtok(tokstr, " \n\t");
	l = 0; /* level */
	
	list = head;
	while (list != NULL)
	{
		c = list->string;
		while (isspace(*c)) c++;
		if (!strcmp(match, c)) 
		{
			l = list->level;
			match = strtok(NULL, "\n\t ");
			if (match == NULL)
			{
				return(list->line);
				}
			}
		if (l > list->level)
			break;
		list = list->next;
		}
#else
    /* we list keys explicitly, rather than building them from multiple levels  */
    list = head;
    while (list != NULL)
    {
        c = list->string;
        while (isspace(*c)) c++;
        if (!strcmp(s, c)) return(list->line);
        list = list->next;
        }
#endif
	return(-1);
	}

void putquoted(char *s, FILE *f)
{
	for (; *s; s++) {
	    if (*s & 0x80) {
		fputc((char)0x5c, f);
		fputc((char)0x27, f);
		fputc(hex[(*s&0xf0)>>4], f);
		fputc(hex[(*s)&0x0f], f);
	    }
	    else
		fputc(*s, f);
	}
}


/* search through the list to find any references */
void
refs(int l, FILE *f)
{
	int curlevel;
	char *c;

	/* find current line */
	list = head;
	while (list->line != l)
		list = list->next;
	curlevel = list->level;
	list = list->next;		/* look at next element before going on */
	if (list != NULL)
		fprintf(f,"\\par");
	while (list != NULL)
	{
		/* we are onto the next topic so stop */
		if (list->level == curlevel)
			break;
		/* these are the next topics down the list */
		if (list->level == curlevel+1)
		{
			c = list->string;
			while (isspace(*c)) c++;
			fprintf(f,"\\par{\\uldb ");
			putquoted(c, f);
			fprintf(f,"}");
			fprintf(f,"{\\v loc%d}\n",list->line);
			}
		list = list->next;
		}
	}

/* generate an RTF footnote with reference char c and text s */
void footnote(char c, char *s, FILE *b)
{
/*  This is the new format, but it doesn't work for HC31
	fprintf(b,"%c{\\footnote ", c);
*/
	fprintf(b,"%c{\\footnote %c ", c, c);
	putquoted(s, b);
	fprintf(b,"}\n",s);
}

void
convert(FILE *a, FILE *b)
{
    static char line[MAX_LINE_LEN];
	
	/* generate rtf header */
	fprintf(b,"{\\rtf1\\ansi ");		/* vers 1 rtf, ansi char set */
	fprintf(b,"\\deff0");				/* default font font 0 */
	/* font table: font 0 proportional, font 1 fixed */
	fprintf(b,"{\\fonttbl{\\f0\\fswiss Arial;}{\\f1\\fmodern Courier New;}}\n");

	/* skip title line */
	fgets(line, MAX_LINE_LEN, a);

	/* process each line of the file */
    while (fgets(line,MAX_LINE_LEN,a)) {
	   process_line(line, b);
	   }

	/* close final page and generate trailer */
	fprintf(b,"}{\\plain \\page}\n");
	fprintf(b,"}\n");
}

void
process_line(char *line, FILE *b)
{
    static int line_count = 0;
    static char line2[MAX_LINE_LEN];
    static int last_line;
	int i;
	int j;
	static int startpage = 1;
	char str[MAX_LINE_LEN];
	char topic[MAX_LINE_LEN];
	int k, l;
	static int tabl=0;
	static int para=0;
	static int llpara=0;
	static int inquote = FALSE;
	static int inref = FALSE;

	line_count++;

	i = 0;
	j = 0;
	while (line[i] != '\0')
	{
		switch(line[i])
		{
			case '\\':
			case '{':
			case '}':
				line2[j] = '\\';
				j++;
				line2[j] = line[i];
				break;
			case '\r':
			case '\n':
				break;
			case '`':	/* backquotes mean boldface or link */
				if (line[1]==' ')	/* tabular line */
					line2[j] = line[i];
				else if ((!inref) && (!inquote))
				{
					k=i+1;	/* index into current string */
					l=0;	/* index into topic string */
					while ((line[k] != '`') && (line[k] != '\0'))
					{
						topic[l] = line[k];
						k++;
						l++;
						}
					topic[l] = '\0';
					k = lookup(topic);
					if ((k > 0) && (k != last_line))
					{
						line2[j++] = '{';
						line2[j++] = '\\';
						line2[j++] = 'u';
						line2[j++] = 'l';
						line2[j++] = 'd';
						line2[j++] = 'b';
						line2[j] = ' ';
						inref = k;
						}
					else
					{
						if (debug)
							fprintf(stderr,"Can't make link for \042%s\042 on line %d\n",topic,line_count);
						line2[j++] = '{';
						line2[j++] = '\\';
						line2[j++] = 'b';
						line2[j] = ' ';
						inquote = TRUE;
						}
					}
				else
				{
					if (inquote && inref)
						fprintf(stderr, "Warning: Reference Quote conflict line %d\n", line_count);
					if (inquote)
					{
						line2[j] = '}';
						inquote = FALSE;
						}
					if (inref)
					{
						/* must be inref */
						sprintf(topic,"%d",inref);
						line2[j++] = '}';
						line2[j++] = '{';
						line2[j++] = '\\';
						line2[j++] = 'v';
						line2[j++] = ' ';
						line2[j++] = 'l';
						line2[j++] = 'o';
						line2[j++] = 'c';
						k = 0;
						while (topic[k] != '\0')
						{
							line2[j++] = topic[k];
							k++;
							}
						line2[j] = '}';
						inref = 0;
						}
				}
				break;
			default:
				if (line[i] & 0x80) {
				    /* extended characters so use hexadecimal */
				    line2[j++] = (char)0x5c; /* \ */
				    line2[j++] = (char)0x27; /* ' */
				    line2[j++] = hex[(line[i]&0xf0)>>4];
				    line2[j] = hex[(line[i]&0x0f)];
				}
				else 
				    line2[j] = line[i];
			}
		i++;
		j++;
		line2[j] = '\0';
		}

	i = 1;

    switch(line[0]) {		/* control character */
	   case '?': {			/* interactive help entry */
		if ((line2[1] != '\0') && (line2[1] != ' '))
			footnote('K',&(line2[1]),b);
		  break;
	   }
	   case '@': {			/* start/end table */
		  break;			/* ignore */
	   }
	   case '#': {			/* latex table entry */
		  break;			/* ignore */
	   }
	   case '%': {			/* troff table entry */
		  break;			/* ignore */
	   }
	   case '\n':			/* empty text line */
			fprintf(b,"\\par\n");
			llpara = para;
			para = 0;
			tabl = 0;
			break;
	   case ' ': {			/* normal text line */
		  if ((line2[1] == '\0') || (line2[1] == '\n'))
		  {
				fprintf(b,"\\par\n"); 
				llpara = para;
				para = 0;
				tabl = 0;
				}
		  if (line2[1] == ' ') 
		  {
				if (!tabl)
				{
					fprintf(b,"\\par\n"); 
					}
				fprintf(b,"{\\pard \\plain \\f1\\fs20 ");
			  	fprintf(b,"%s",&line2[1]); 
				fprintf(b,"}\\par\n");
				llpara = 0;
				para = 0;
				tabl = 1;
				}
		  else
		  {
				if (!para)
				{
					if (llpara)
						fprintf(b,"\\par\n"); /* blank line between paragraphs */
					llpara = 0;
					para = 1;		/* not in para so start one */
					tabl = 0;
					fprintf(b,"\\pard \\plain \\qj \\fs20 \\f0 ");
					}
			  	fprintf(b,"%s \n",&line2[1]); 
				}
		  break;
	   }
	   default: {
		  if (isdigit(line[0])) { /* start of section */
			if (!startpage)
			{
				refs(last_line,b);
				fprintf(b,"}{\\plain \\page}\n");
				}
			para = 0;					/* not in a paragraph */
			tabl = 0;
			last_line = line_count;
			startpage = 0;
			fprintf(b,"{\n");
			sprintf(str,"browse:%05d", line_count);
			footnote('+',str,b);
			footnote('$',&(line2[1]),b); /* title */
			fprintf(b,"{\\b \\fs24 %s}\\plain\\par\\par\n",&(line2[1]));
			/* output unique ID */
			sprintf(str,"loc%d", line_count);
			footnote('#',str,b);
		  } else
		    fprintf(stderr, "unknown control code '%c' in column 1, line %d\n",
			    line[0], line_count);
		  break;
	   }
    }
}

