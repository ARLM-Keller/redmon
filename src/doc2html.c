/*
 * doc2html.c  -- program to convert Gnuplot .DOC format to 
 *        World Wide Web (WWW) HyperText Markup Language (HTML) format
 *
 * Created by Russell Lang from doc2ipf by Roger Fearick from 
 *   doc2rtf by M Castro from doc2gih by Thomas Williams.
 *   1994-11-03
 * Modified by Russell Lang 1996-10-15
 *   obtain title from first line of doc file.
 *   Conform to HTML 3.2. 
 * Modified by Russell Lang 1997-11-28
 *   Convert {bml* file.bmp} to <IMG SRC="file.gif">
 * Modified by Russell Lang 1997-12-18
 *   Convert non-ascii characters to &#nnn;
 *
 * usage:  doc2html gnuplot.doc gnuplot.htm
 *
 */

/* note that tables must begin in at least the second column to */
/* be formatted correctly and tabs are forbidden */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>

#define MAX_LINE_LEN	1024
#define TRUE 1
#define FALSE 0

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
int nolinks = FALSE;

void parse();
void refs();
void convert();
void process_line();
int lookup();

main(argc,argv)
int argc;
char **argv;
{
FILE * infile;
FILE * outfile;

    if (argv[argc-1][0]=='-' && argv[argc-1][1]=='d') {
        debug = TRUE;
	argc--;
    }

    if (argv[argc-1][0]=='-' && argv[argc-1][1]=='n') {
        nolinks = TRUE;
	argc--;
    }

    if ( (argc > 3) || (argc == 1) ) {
        fprintf(stderr,"Usage: %s infile outfile\n", argv[0]);
        return(1);
    }
    if ( (infile = fopen(argv[1],"r")) == (FILE *)NULL) {
        fprintf(stderr,"%s: Can't open %s for reading\n",
            argv[0], argv[1]);
        return(1);
    }
    if (argc == 3) {
      if ( (outfile = fopen(argv[2],"w")) == (FILE *)NULL) {
        fprintf(stderr,"%s: Can't open %s for writing\n",
            argv[0], argv[2]);
      }
    }
    else {
        outfile = stdout;
    }
    parse(infile);
    convert(infile,outfile);
    return(0);
}

/* scan the file and build a list of line numbers where particular levels are */
void parse(a)
FILE *a;
{
    static char line[MAX_LINE_LEN];
    char *c;
    int lineno=0;
    int lastline=0;

    /* skip title line */
    fgets(line,MAX_LINE_LEN,a);

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
      }
      if (line[0]=='?')
      {
        if (keylist == NULL)    
            keyhead = (keylist = (struct LIST *) malloc(sizeof(struct LIST)));
        else
            keylist = (keylist->next = (struct LIST *) malloc(sizeof(struct LIST)));
        keylist->line = lastline;
        keylist->level = line[0] - '0';
        c = strtok(&(line[1]),"\n");
        if( c == NULL || *c == '\0' ) c = list->string ;
        keylist->string = (char *) malloc (strlen(c)+1);
        strcpy(keylist->string, c);
        keylist->next = NULL;
      }
    }
    rewind(a);
    }

/* look up an in text reference */
int
lookup(s)
char *s;
{
    char *c;
    char tokstr[MAX_LINE_LEN];
    char *match; 
    int l;

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
    return(-1);
    }

/* search through the list to find any references */
void
refs(l, f)
int l;
FILE *f;
{
    int curlevel;
    char str[MAX_LINE_LEN];
    char *c;
    int inlist = FALSE;

    /* find current line */
    list = head;
    while (list->line != l)
        list = list->next;
    curlevel = list->level;
    list = list->next;        /* look at next element before going on */
    if (list != NULL)
    {
	inlist = TRUE;
	fprintf(f,"<P>\n");
	}

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
	    if (nolinks)
	        fprintf(f,"<B>%s</B><BR>\n", c);
	    else
	        fprintf(f,"<A HREF=\042#%d\042>%s</A><BR>\n", list->line, c);
            }
        list = list->next;
        }
	if (inlist)
	    fprintf(f,"<P>\n");
    }

void
convert(a,b)
    FILE *a,*b;
{
    static char line[MAX_LINE_LEN];
    
    /* generate html header */
    fprintf(b,"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">\n");
    fprintf(b,"<HTML>\n");
    fprintf(b,"<HEAD>\n");
    fgets(line,MAX_LINE_LEN,a);
    strtok(line, "\n");
    fprintf(b,"<TITLE>%s</TITLE>\n", line+1);
    fprintf(b,"</HEAD>\n");
    fprintf(b,"<BODY>\n");
    fprintf(b,"<H1>%s</H1>\n", line+1);

    /* process each line of the file */
        while (fgets(line,MAX_LINE_LEN,a)) {
       process_line(line, b);
       }

    /* close final page and generate trailer */
    fprintf(b,"\n<P><HR>Created automatically by doc2html\n");
    fprintf(b,"</BODY>\n");
    fprintf(b,"</HTML>\n");
}

void
process_line(line, b)
    char *line;
    FILE *b;
{
    static int line_count = 0;
    static char line2[MAX_LINE_LEN];
    static int last_line;
    char hyplink1[64] ;
    char *pt, *tablerow ;
    int i;
    int j;
    static int startpage = 1;
    char str[MAX_LINE_LEN];
    char topic[MAX_LINE_LEN];
    int k, l;
    static int tabl=0;
    static int para=0;
    static int inquote = FALSE;
    static int inref = FALSE;

    line_count++;

    i = 0;
    j = 0;
    while (line[i] != '\0')
    {
        switch(line[i])
        {
            case '<':
                strcpy( &line2[j], "&lt;" ) ;
                j += strlen( "&lt." ) - 1 ;
                break ;

            case '>':
                strcpy( &line2[j], "&gt;" ) ;
                j += strlen( "&gt." ) - 1 ;
                break ;

            case '&':
                strcpy( &line2[j], "&amp;" ) ;
                j += strlen( "&amp;" ) - 1 ;
                break ;
                                
            case '\r':
            case '\n':
                break;
            case '`':    /* backquotes mean boldface or link */
                if ((!inref) && (!inquote))
                {
                    k=i+1;    /* index into current string */
                    l=0;    /* index into topic string */
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
			if (nolinks)
                            sprintf( hyplink1, "<B>") ;
			else
                            sprintf( hyplink1, "<A HREF=\042#%d\042>", k ) ;
                        strcpy( line2+j, hyplink1 ) ;
                        j += strlen( hyplink1 )-1 ;
                        
                        inref = k;
                        }
                    else
                    {
                        if (debug && k != last_line)
                            fprintf(stderr,"Can't make link for \042%s\042 on line %d\n",topic,line_count);
                        line2[j++] = '<';
                        line2[j++] = 'B';
                        line2[j] = '>';
                        inquote = TRUE;
                        }
                    }
                else
                {
                    if (inquote && inref)
                        fprintf(stderr, "Warning: Reference Quote conflict line %d\n", line_count);
                    if (inquote)
                    {
                        line2[j++] = '<';
                        line2[j++] = '/';
                        line2[j++] = 'B';
                        line2[j] = '>';
                        inquote = FALSE;
                        }
                    if (inref)
                    {
                        /* must be inref */
                        line2[j++] = '<';
                        line2[j++] = '/';
			if (nolinks)
                            line2[j++] = 'B';
			else
                            line2[j++] = 'A';
                        line2[j] = '>';
                        inref = 0;
                        }
                }
                break;
            default:
		if ((unsigned int)(line[i]) > 127) {
		    /* quote non-ASCII characters */
		    unsigned int value = line[i] & 0xff;
		    unsigned int digit;
		    line2[j++] = '&';
		    line2[j++] = '#';
		    digit = value / 100;
		    value -= digit * 100;
		    line2[j++] = digit + '0';
		    digit = value / 10;
		    value -= digit * 10;
		    line2[j++] = digit + '0';
		    line2[j++] = value + '0';
		    line2[j] = ';';
		}
		else
                    line2[j] = line[i];
            }
        i++;
        j++;
        line2[j] = '\0';
        }

    i = 1;

    switch(line[0]) {        /* control character */
       case '?': {            /* interactive help entry */
#ifdef FIXLATER
               if( intable ) intablebut = TRUE ;
               fprintf(b,"\n:i1. %s", &(line[1])); /* index entry */
#endif
               break;
       }
       case '@': {            /* start/end table */
          break;            /* ignore */
       }
       case '#': {            /* latex table entry */
          break;            /* ignore */
       }
       case '%': {            /* troff table entry */
          break;            /* ignore */
       }
       case '\n':            /* empty text line */
	    if (tabl)
	        fprintf(b,"</PRE>\n"); /* rjl */
            para = 0;
            tabl = 0;
            fprintf(b,"<P>");
            break;
       case ' ': {            /* normal text line */
          if ((line2[1] == '\0') || (line2[1] == '\n'))
          {
                fprintf(b,"<P>"); 
                para = 0;
		tabl = 0;
                }
          if (line2[1] == ' ') 
          {
                if (!tabl)
                {
		    fprintf(b,"<PRE>\n");
                    }
                fprintf(b,"%s\n",&line2[1]); 
                tabl = 1;
                para = 0;
                }
          else if (strncmp(line2+1, "{bml", 4)==0)
	  {
		/* assume bitmap is available as GIF */
		char *p;
		fprintf(b, "\n<IMG SRC=\042");
		for (p=line2+1; *p && *p!=' '; p++)
		   ; /* skip over bml text */
		for (; *p && *p==' '; p++)
		   ; /* skip over spaces */
		for (; *p && *p!='.' && *p!=' '; p++)
		    fprintf(b, "%c", *p);
		fprintf(b, ".gif\042>\n");
	      }
	  else 
          {
		if (tabl) {
		    fprintf(b,"</PRE>\n"); /* rjl */
                    fprintf(b,"<P>"); 	/* rjl */
		}
                tabl = 0;
                if (!para)
                    para = 1;        /* not in para so start one */
                  fprintf(b,"%s \n",&line2[1]); 
                }
          break;
       }
       default: {
          if (isdigit(line[0])) { /* start of section */
	    if (tabl)
	          fprintf(b,"</PRE>\n"); /* rjl */
            if (!startpage)
            {
                refs(last_line,b);
                }
            para = 0;                    /* not in a paragraph */
            tabl = 0;
            last_line = line_count;
            startpage = 0;
	    if (debug)
		fprintf( stderr, "%d: %s\n", line_count, &line2[1] ) ;
            k=lookup(&line2[1]) ;
	    /* output unique ID and section title */
	    if (nolinks)
                fprintf(b,"<HR>\n<H%c>", line[0]=='1'?line[0]:line[0]-1);
	    else
                fprintf(b,"<HR>\n<H%c><A NAME=\042%d\042>", line[0]=='1'?line[0]:line[0]-1, line_count);
            fprintf(b,&(line2[1])); /* title */
            fprintf(b,"</A></H%c>\n", line[0]=='1'?line[0]:line[0]-1) ;
          } else
            fprintf(stderr, "unknown control code '%c' in column 1, line %d\n",
                line[0], line_count);
          break;
       }
    }
}

