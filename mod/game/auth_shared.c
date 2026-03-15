// Copyright (C) 2010-2012 Frozen Sand
//
//
// auth_shared.h
//


#ifdef USE_AUTH

#ifdef AUTH_CMD_LINE_TOOL

	 // command line tool only
	 // there is a makefile to build this code in the same directory
	 // just "cd" this directory and do a "./make"
	 // command line tool "auth" will be created in "build" directory of the main code

	#include "../game/q_shared.h"

	void QDECL Com_Error(int level, const char *error, ...)
	{
		va_list 	argptr;
		char		text[1024];

		va_start(argptr, error);
		vsprintf(text, error, argptr);
		va_end(argptr);

		printf("Error: %s", text);
	}

	void QDECL Com_Printf(const char *msg, ...)
	{
		va_list 	argptr;
		char		text[1024];

		va_start(argptr, msg);
		vsprintf(text, msg, argptr);
		va_end(argptr);

		printf("%s", text);
	}

	#include "../game/q_shared.c"

#endif

// Kalish & barbatos

// vars

static int	auth_argc;
static char	*auth_argv[AUTH_MAX_ARGS];

// functions

int Auth_rand_int(int min, int max)
{
	static int rand_init = 0;
	int num;
	if (rand_init == 0)
	{
		#ifdef AUTH_CMD_LINE_TOOL
		srand(time(NULL));
		#else
		srand(trap_Milliseconds());
		#endif
		rand_init = 1;
	}
	// num = rand() % (max - min + 1) + min;
	num = rand() % (max - min) + min;
	return (num);
}

int Auth_pow(int num, int count)
{
    int n;
    n = num;
	while(count>1) {
		if( num>1000000000 ) {
			Com_Printf( AUTH_PREFIX" too big integer %i to end pow", num);
			return num;
		}
		num = num * n;
		count--;
	}
	// Com_Printf("num: %i\n", num);
	return num;
}

char *Auth_Stripslashes( char* string )
{
	char*	d;
	char*	s;
	int		c;
	int		q;
	s = string;
	d = string;
	q = 0;
	while ((c = *s) != 0 ) {
		if ( q==1 ) {
			q = 0;
			if ( c==0x6E || c==0x72  )
				*d++ =	0xA;
			else
				*d++ =	c;
		} else if ( c==0x5C ) {
			q = 1;
		} else {
			q = 0;
			*d++ =	c;
		}
		s++;
	}
	*d = '\0';
	return string;
}

int Auth_xtoi(const char* string, int start, int len)
{
	int output, i, c;
	i = 0;
	if(len<1 || start+len>strlen(string)) {
		return -1;
	} else {
		output = 0;
		while (i<len) {
			c = string[start+i];
			if (c>=48 && c<=57) {
				c = c - 48;
			} else if (c>=65 && c<=70) {
				c = c -65 + 10;
			} else if (c>=97 && c<=102) {
				c = c -97 + 10;
			} else {
				return -1;
			};
			output = (output*16) + c;
			i++;
		}
	}
	return output;
}

void Auth_itox(int num, char *output, int len )
{
	// max size 4096
	static char hextable[17] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F','!'};
	char*	o;
	int i;
	o = output;
	i = 0;
	if( num<0 || num>=4096 || (len<2 && num>=16) || (len<3 && num>=256)) {
		while(i<=len) {
			*o = '!';
			o++;
			i++;
		}
	} else {
		i = 0;
		if(num>=256) {
			i = (int)(num/256);
			*o = hextable[i];
			num-= i*256;
			o++;
		} else if(len>2) {
			*o = '0';
			o++;
		}
		i = 0;
		if(num>=16) {
			i = (int)(num/16);
			*o = hextable[i];
			num-= i*16;
			o++;
		} else if(len>1) {
			*o = '0';
			o++;
		}
		*o = hextable[num];
		o++;
	}
	*o = '\0';
}

char *Auth_encrypt( char *string )
{
	char outc[4];
	char outk[2];
	char output[AUTH_MAX_STRING_CHARS];
	char*	s;
	char*	o;
	int		c, q, k, a, b;
	a = 2;
	b = 16;
	s = string;
	o = output;
	q = 0;
	while ((c = *s) != 0 ) {
		if( c>=' ' && c<='~' ) { // valid ascii only
			if(strlen(output)>AUTH_MAX_STRING_CHARS-16) {
				o++;
				break;
			}
			k = Auth_rand_int(a,b);
			c = c + 1 + k*240; // (4096-256/17)
			Auth_itox( k, outk, 1);
			Auth_itox( c, outc, 3);
			if ( q==1 ) {
				q = 0;
				*o = outk[0];
				o++;
				*o = outc[0];
				o++;
				*o = outc[1];
				o++;
				*o = outc[2];
				o++;
			} else {
				q = 1;
				*o = outc[0];
				o++;
				*o = outc[1];
				o++;
				*o = outc[2];
				o++;
				*o = outk[0];
				o++;
			}
			//Com_Printf("%c => [%s][%s] %i\n", *s, outc, outk, c);
		}
		s++;
	}
	*o = '\0';
	Com_sprintf( string, AUTH_MAX_STRING_CHARS, "%s", output);
	return string;
}

char *Auth_decrypt( char *string )
{
	char output[AUTH_MAX_STRING_CHARS];
	char*	o;
	int		c, q, k, i, len;
	o = output;
	q = 0;
	i = 0;
	*o = '\0';

	len = strlen(string) - 1;
	while ( i < len ) {
		if ( q==1 ) {
			q = 0;
			k = Auth_xtoi(string,i,1);
			c = Auth_xtoi(string,i+1,3);
		} else {
			q = 1;
			k = Auth_xtoi(string,i+3,1);
			c = Auth_xtoi(string,i,3);
		}
		*o = c - 1 - k*240;
		o++;
		i = i+4;
	}
	*o = '\0';
	Com_sprintf( string, AUTH_MAX_STRING_CHARS, "%s", output);
	return string;
}

void Auth_args( char *string )
{
	const char *s;
	char *a;
	char arg[AUTH_MAX_STRING_CHARS];
	auth_argc = 0;
	if ( !string ) return;
	s = string;
	a = arg;
	while ( 1 ) {
		if ( auth_argc == AUTH_MAX_ARGS ) break;
		while ( *s && *s <= ' ' ) {
			s++;
		}
		if ( !*s ) break;
		auth_argv[auth_argc] = a;
		auth_argc++;
		if ( *s == '"' ) {
			// quoted strings
			s++;
			while ( *s && *s != '"' ) {
				*a++ = *s++;
			}
			*a++ = '\0';
			if ( !*s ) break;
			s++;
		} else {
			// regular token
			while ( *s > ' ' && *s != '"' ) {
				*a++ = *s++;
			}
			*a++ = '\0';
			if ( !*s ) break;
		}
	}
	return;
}

int Auth_cat_str( char *output, int outlen, const char *input, int quoted )
{
	/*
	quoted:
	0	quotes ""
	1	no quote
	-1	raw (no quote, no check, no end space)
	*/

	char *o;
	int len;
	int i;

	// check

	if(quoted) len = 3; else len = 1;
	len = len + strlen(output) + strlen(input);
	if( len >= outlen ) {
		Com_Printf( AUTH_PREFIX" string output overrun max lenght (%i"), outlen);
		return 0;
	}

	// cat

	i = 0;
	len = strlen(input);
	o = output + strlen(output);
	if(quoted==1) *o++ = '"';
	if(len>0) {
		while(i<len) {
			if(quoted!=-1) {
				if( input[i]==';' || input[i]=='"' ) {
					Com_Printf( AUTH_PREFIX" invalid char <%c> in <%s>\n", input[i], input);
					return 0;
				}
				if( input[i]<32 || input[i]>126 ) {
					Com_Printf( AUTH_PREFIX" invalid ascii char <%i> in <%s>\n", input[i], input);
					return 0;
				}
			}
			*o++ = input[i];
			i++;
		}
	}
	if(quoted==1) *o++ = '"';
	if(quoted!=-1) *o++ = ' ';
	*o++ = '\0';
	return strlen(output);
}

int Auth_cat_int( char *output, int outlen, int num, int quoted )
{
	char input[12];

	if( num>1000000000 ) {
		Com_Printf( AUTH_PREFIX" too big integer %i (max 1000000000"), num);
		return 0;
	}
	if( num<-1000000000 ) {
		Com_Printf( AUTH_PREFIX" too big integer %i (min -1000000000"), num);
		return 0;
	}

	Com_sprintf( input, sizeof(input), "%i", num);

	return Auth_cat_str( output, outlen, input, quoted );
}

char *Auth_argv( int num )
{
	if(num >= 0 && num < auth_argc)
		return auth_argv[num];
	return NULL;
}

void Auth_decrypt_args( char *string )
{
	Auth_args(Auth_decrypt(string));
}

/*
int Auth_args_to_string( char *stringv, int stringc, char *type, char *cmd, char *headers, ... )
{
	va_list argptr;
	char *argv;
	int argc;
	char *s;
	int len;
	int a;

	if ( !stringv && !stringc ) return 0;

	// header

	len = strlen(type) + strlen(cmd) + strlen(headers) + 2;

	if( len >= stringc ) {
		Com_sprintf( stringv, stringc, AUTH_PREFIX" headers overrun max lenght (%i"), stringc);
		return -2;
	}

	Com_sprintf( stringv, stringc, "%s:%s %s", type, cmd, headers);

	s = stringv + strlen(stringv);

	// args

	argc = 0;
	a = 0;

	va_start( argptr, headers );

	while((argv = va_arg(argptr, char *)) != NULL)
	{
		#ifdef AUTH_CMD_LINE_TOOL
		// printf("%s\n", argv);
		#endif
		argc++;
		a = 0;
		if( argc >= AUTH_MAX_ARGS ) {
			Com_sprintf( stringv, stringc, AUTH_PREFIX" too many arguments (max %i"), AUTH_MAX_ARGS);
			return -2;
		}
		if( len + 3 + strlen(argv) > stringc-2 ) {
			Com_sprintf( stringv, stringc, AUTH_PREFIX" arg %i overruns max lenght (%i"), argc, stringc);
			return -3;
		}
		*s++ = ' ';
		*s++ = '"';
		while(a<strlen(argv)) {
			if( argv[a]==';' || argv[a]=='"' ) {
				Com_sprintf( stringv, stringc, AUTH_PREFIX" invalid char \"%c\" in arg %i", argv[a], argc);
				return -4;
			}
			if( argv[a]<32 || argv[a]>126 ) {
				Com_sprintf( stringv, stringc, AUTH_PREFIX" invalid ascii char #%i in arg %i", argv[a], argc);
				return -5;
			}
			*s++ = argv[a];
			a++;
		}
		*s++ = '"';
		len += a + 3;
	}

	va_end( argptr );

	if(len==0) {
		Com_sprintf( stringv, stringc, AUTH_PREFIX" args string is empty");
		return -4;
	}

	stringv[len] = '\0';
	return len;
}
*/

// command line tool only

#ifdef AUTH_CMD_LINE_TOOL

int main (int argc, char * const argv[])
{

	int command, i, j, a;
	char input[AUTH_MAX_STRING_CHARS];
	char output[AUTH_MAX_STRING_CHARS];
	command = 0;
	input[0] = '\0';
	output[0] = '\0';
	if( argc>1 ) {
		command = atoi(argv[1]);
		if(argc==2) {
			i = 2;
			sprintf( input, "%s", argv[2]);
		}
		else if(argc>2) {
			i = 2;
			while( i < argc ) {
				if(strlen(argv[i])+strlen(input)>AUTH_MAX_STRING_CHARS) break;
				j = a = 0;
				while( j < strlen(argv[i]) ) { if(argv[i][j]==' ') { a = 1; }; j++; }
				if( a==1 ) sprintf( input, "%s\"%s\" ", input, argv[i]);
				else sprintf( input, "%s%s ", input, argv[i]);
				i++;
			}
		}
	}
	sprintf( output, "%s", input);

	printf("-----------------------------\n");
	printf("FS AUTH TEST TOOL\n");
	printf("-----------------------------\n");
	printf("protocol: #%i \n", AUTH_PROTOCOL);
	printf("input: %s \n", input);
	printf("-----------------------------\n");

	if(command==0) {
		printf("Usage: 1 encrypt / 2 decrypt \n");
	} else if(command==1) {
		printf("Encrypt: \"%s\" \n=> \"%s\"\n", input, Auth_encrypt(output) );
	} else if(command==2) {
		printf("Decrypt: \"%s\" \n=> \"%s\"\n", input, Auth_decrypt(output) );
	}  else if(command==3) {
		Auth_decrypt_args(output);
		if( auth_argc<1 ) {
			printf("Args decrypt: no argument\n");
		} else {
			printf("Args decrypt:\n");
			i = 0;
			while( i < auth_argc ) {
				printf("Arg #%i: %s\n",i,auth_argv[i]);
				i++;
			}
		}
	}  else if(command==4) {
		Auth_args(output);
		if( auth_argc<1 ) {
			printf("Args: no argument\n");
		} else {
			printf("Args:\n");
			i = 0;
			while( i < auth_argc ) {
				printf("Arg #%i: %s\n",i,auth_argv[i]);
				i++;
			}
		}
	}  else if(command==5) {
		Auth_args(output);
		if( auth_argc<1 ) {
			printf("Args: no argument\n");
		} else {
			printf("Chars added:\n");
			output[0] = '\0';
			a = Auth_cat_int( output, sizeof(output), 100066, qfalse);
			printf("%i ",a);
			a = Auth_cat_int( output, sizeof(output), 100066, qtrue);
			printf("%i ",a);
			a = Auth_cat_str( output, sizeof(output), "test to see", qfalse);
			printf("%i ",a);
			a = Auth_cat_str( output, sizeof(output), "", qfalse);
			printf("%i ",a);
			a = Auth_cat_str( output, sizeof(output), "test to see", qtrue);
			printf("\nResult: %s\n",output);
		}
	}
	printf("-----------------------------\n");

    return 0;
}

#endif

#endif
