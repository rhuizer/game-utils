/*
 * rbi2rob
 * -------
 *
 * Decodes and disassembles RoboCom .rbi files.
 * See http://www.cyty.com/robocom/ for further info.
 *
 * Dedicated to Anne van Leeuwen.
 *
 *   -- Ronald Huizer / rhuizer@hexpedition.com (c) 2006
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <stdint.h>
#include <getopt.h>
#include <string.h>

/* prototypes */
void error(const char *, ...);
static unsigned char get_key_byte(char *, int);

/* type/length/value datatype handling */
enum tlv_type {
	TLV_INT		= 1,		TLV_STRING	= 5
};

enum rbi_opcode {
	DIE		= 0x02,		TRANS		= 0x03,
	MOVE		= 0x04,		TURN		= 0x05,
	JUMP		= 0x06,		SET		= 0x07,
	ADD		= 0x08,		SUB		= 0x09,
	BJUMP		= 0x0a,		COMP		= 0x0c,
	CREATE		= 0x0d,		SCAN		= 0x0e,

	NCOMP		= 0x0f,		LCOMP		= 0x10,
	GCOMP		= 0x11,		RANDOM		= 0x12,
	MIN		= 0x13,		MAX		= 0x14,
	RTRANS		= 0x15,		AJUMP		= 0x16,
	SLEEP		= 0x17,		INIT		= 0x18,
	SEIZE		= 0x19,		RESUME		= 0x1a,
	BREAK		= 0x1b,		QUIT		= 0x1c,
	MUL		= 0x1d,		DIV		= 0x1e,
	MOD		= 0x1f,		FARSCAN		= 0x20
};

#define OPCODE_MIN	DIE
#define OPCODE_MAX	FARSCAN

struct rbi_instruction {
	enum rbi_opcode		opcode;
	char			*quux;
	unsigned int		params;
} rinstr[] = {
	{ 0,			"RESERVED 0",		0 },
	{ 0,			"RESERVED 1",		0 },
	{ DIE,			"Die",			0 },
	{ TRANS,		"Trans",		2 },
	{ MOVE,			"Move",			0 },
	{ TURN,			"Turn",			1 },
	{ JUMP,			"Jump",			1 },
	{ SET,			"Set",			2 },
	{ ADD,			"Add",			2 },
	{ SUB,			"Sub",			2 },
	{ BJUMP,		"BJump",		2 },
	{ 0,			"RESERVED 3",		0 },
	{ COMP,			"Comp",			2 },
	{ CREATE,		"Create",		3 },
	{ SCAN,			"Scan",			1 },

	/* extended instruction set */
	{ NCOMP,		"NComp",		2 },
	{ LCOMP,		"LComp",		2 },
	{ GCOMP,		"GComp",		2 },
	{ RANDOM,		"Random",		3 },
	{ MIN,			"Min",			2 },
	{ MAX,			"Max",			2 },
	{ RTRANS,		"RTrans",		2 },
	{ AJUMP,		"AJump",		1 },
	{ SLEEP,		"Sleep",		1 },
	{ INIT,			"Init",			2 },
	{ SEIZE,		"Seize",		0 },
	{ RESUME,		"Resume",		0 },
	{ BREAK,		"Break",		0 },
	{ QUIT,			"Quit",			0 },
	{ MUL,			"Mul",			2 },
	{ DIV,			"Div",			2 },
	{ MOD,			"Mod",			2 },
	{ FARSCAN,		"Farscan",		3 }
};

/*  RBI global variables management
 */
enum rbi_var_type {
	LOCAL		= 0x03,		REMOTE		= 0x83
};

enum rbi_var_global {
	BANKS		= 0xe9,		MOBILE		= 0xea,
	INSTRSET	= 0xeb,		ACTIVE		= 0xec,
	FIELDS		= 0xed,		GENERATION	= 0xee,	
	TIME		= 0xef,		MYBOTS		= 0xf0,
	OTHERBOTS	= 0xf1,		PUB		= 0xf2,
	RANDNUM		= 0xf3,		INSTRPOS	= 0xf4,
	TASKS		= 0xf5,		ID		= 0xf6,
	TIMEOUT		= 0xf7,		MAXLIFETIME	= 0xf8,
	ELIMTRIGGER	= 0xf9,		/* undocumented */
	MAXTASKS	= 0xfb,		MAXGENERATION	= 0xfc,
	MAXMYBOTS	= 0xfd,		AGE		= 0xfe	
};

#define GLOBAL_MIN	BANKS
#define GLOBAL_MAX	AGE

struct rbi_global {
	enum rbi_var_global	type;
	char			*str;
	unsigned int		writeable;
} rglobal[] = {
	{ BANKS,		"Banks",		0 },
	{ MOBILE,		"Mobile",		0 },
	{ INSTRSET,		"InstrSet",		0 },
	{ ACTIVE,		"Active",		1 },
	{ FIELDS,		"Fields",		0 },
	{ GENERATION,		"Generation",		0 },
	{ TIME,			"Time",			0 },
	{ MYBOTS,		"MyBots",		0 },
	{ OTHERBOTS,		"OtherBots",		0 },
	{ PUB,			"Pub",			1 },
	{ RANDNUM,		"RandNum",		0 },
	{ INSTRPOS,		"InstrPos",		0 },
	{ TASKS,		"Tasks",		0 },
	{ ID,			"Id",			0 },
	{ TIMEOUT,		"Timeout",		0 },
	{ MAXLIFETIME,		"MaxLifetime",		0 },
	{ ELIMTRIGGER,		"ElimTrigger",		0 },
	{ 0,			"RESERVED",		0 },
	{ MAXTASKS,		"MaxTasks",		0 },
	{ MAXGENERATION,	"MaxGeneration",	0 },
	{ MAXMYBOTS,		"MaxMyBots",		0 },
	{ AGE,			"Age",			0 }
};

#define tlv_check(type)							\
		if ( (type) <= TLV_INT || type > TLV_STRING + 4)	\
			error("Invalid TLV encoded entry. Aborting.\n")

#define tlv_check_string(type)						\
	if ( (type) <= TLV_STRING)					\
		error("TLV entry is not a string. Aborting.\n")

#define tlv_check_int(type)						\
	if ( (type) <= TLV_INT || (type) > TLV_STRING)			\
		error("TLV entry is not an integer. Aborting\n")

inline int xfgetc(FILE *fp) {
	int val;
	
	if ( (val = fgetc(fp)) == EOF)
		error("Read error or short RBI file. Aborting.\n");

	return val;
}

inline uint16_t xfgetw(FILE *fp, char *name) {
	uint16_t	word;

	word = xfgetc(fp) ^ get_key_byte(name, 0);
	word |= (xfgetc(fp) ^ get_key_byte(name, 0)) << 8;

	return word;
}

inline int tlv_get_type(FILE *fp) {
	int type;

	type = xfgetc(fp);
	tlv_check(type);

	return type;
}

inline void *xmalloc(size_t s) {
	void *ret;

	if (s == 0 || s > 1024 * 1024 * 8)
		error("stupid malloc length, sod off.\n");

	if ( (ret = malloc(s)) == NULL)
		error("malloc(%zu) failed: %s\n", s, strerror(errno));

	return ret;
}

static unsigned int
tlv_get_int(FILE *fp) {
	int type;
	int length;
	unsigned int i;

	type = tlv_get_type(fp);
	tlv_check_int(type);

	for (i = length = 0; i < type - TLV_INT; i++)
		length += xfgetc(fp) << (i * 8);

	return length;
}

static char *
tlv_get_string(FILE *fp) {
	int type;
	int length;
	char *value;
	unsigned int i;

	type = tlv_get_type(fp);
	tlv_check_string(type);

	for (i = length = 0; i < type - TLV_STRING; i++)
		length += xfgetc(fp) << (i * 8);

	value = (char *) xmalloc(length + 2);

	if (fread(value, length, 1, fp) != 1)
		error("Read error or short RBI file. Aborting.\n");

	value[length] = 0;

	return value;
}

#define FL_DECODE	1
#define FL_HEX		2

unsigned int flags = FL_DECODE;

/* key to the rolling xor over robot name */
unsigned char key[] = {
	0x4a, 0x6b, 0x46, 0x5a, 0x46, 0x67, 0x33, 0x67,
	0x46, 0x24, 0x25, 0x66, 0x67, 0x66, 0x6a, 0x2f,
	0x28, 0x29, 0x68, 0x67, 0x64, 0x66, 0x6e, 0x68,
	0x00
};

static unsigned char get_key_byte(char *name, int reset) {
	static int i = -2;
	unsigned char val;

	if (reset != 0)
		return i = -2;
	
	if (i++ == -2)
		return 0;

	if (i >= strlen(name)) {
		i = -1;
		return 0;
	}
	
	val = name[i] ^ key[i % sizeof(key)];
	
	return val;
}

static inline void usage(const char *p) {
	fprintf(stderr, "Use as: %s [-h] <rbi file>\n", p != NULL ? p : "");
	fprintf(stderr, "  -d\tToggles decoding the encoded RBI. On by default\n");
	fprintf(stderr, "  -h\tMake hex dump; does not disassemble instructions\n");
}

void error(const char *fmt, ...) {
	va_list	ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	
	exit(EXIT_FAILURE);
}

inline void opcode_print(int opcode) {
	if (opcode < OPCODE_MIN || opcode > OPCODE_MAX)
		error("Found an unknown opcode: %.2x! Aborting...\n", opcode);
	
	printf("%s", rinstr[opcode].quux);
}

static void handle_global(FILE *fp, char *name) {
	int	type;
	int	ident;

	ident = xfgetc(fp);	 /* IZZA MAGIX */
	type = xfgetc(fp) ^ get_key_byte(name, 0);

	if (ident < GLOBAL_MIN || ident > GLOBAL_MAX)
		error("Found an unknown global variable identifier: %.2x! Aborting...\n", ident);

	if (type != LOCAL && type != REMOTE)
		error("Global variable is neither local nor remote: %.2x! Aborting...\n", type);

	ident -= GLOBAL_MIN;
	
	printf("%c%s", type == REMOTE ? '%' : 
			rglobal[ident].writeable == 0 ? '$' : '#',
			rglobal[ident].str);
}

static void handle_addr(FILE *fp, char *name) {
	int byte;

	byte = xfgetc(fp) ^ get_key_byte(name, 0);

	if (byte > 0x7f) {		 /* arbitrary */
		ungetc(byte, fp);
		handle_global(fp, name);
	} else {
		printf("#%d", byte);
		xfgetc(fp) ^ get_key_byte(name, 0);
	}
}

static void instr_decode(FILE *fp, char *name) {
	int opcode;
	int addrmode;
	unsigned int i;

	opcode = xfgetc(fp) ^ get_key_byte(name, 0);
	opcode_print(opcode);
	printf("\t\t");

	addrmode = xfgetc(fp) ^ get_key_byte(name, 0);

	for (i = 0; i < rinstr[opcode].params; i++) {
		if (~addrmode & (1 << i) ) {
			handle_addr(fp, name);
			printf( (rinstr[opcode].params - 1) == i ? "" : ", ");
		} else {
			printf( (rinstr[opcode].params - 1) == i ? "%hi" : "%hi, ", (int16_t)xfgetw(fp, name) );
		}
	}

	while (3 - i++)
		xfgetw(fp, name);
	
	printf("\n");
		
}

static inline void print_instr(FILE *fp, char *name, int decode) {
	unsigned int i;

	if (decode != 0) {
		for (i = 0; i < 7; i++)
			printf("%.2x ", xfgetc(fp) ^ get_key_byte(name, 0));
		printf("%.2x\n", xfgetc(fp) ^ get_key_byte(name, 0));
	} else {
		for (i = 0; i < 7; i++)
			printf("%.2x ", xfgetc(fp));
		printf("%.2x\n", xfgetc(fp));
	}
}

int
main(int argc, char **argv) {
	FILE *fp;
	int option;
	char *name, *author, *country;
	unsigned int i, j, bcount, icount = 0;
	
	opterr = 0;
	while ( (option = getopt(argc, argv, "dh")) != -1) {
		switch (option) {
		case 'd':
			flags ^= FL_DECODE;
			break;
		case 'h':
			flags |= FL_HEX;
			break;
		}
	}

	if (optind >= argc) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	if ( (fp = fopen(argv[optind], "rb")) == NULL) {
		perror("fopen()");
		exit(EXIT_FAILURE);
	}

	printf("Name:\t\t%s\n", name = tlv_get_string(fp));
	printf("Author:\t\t%s\n", author = tlv_get_string(fp));
	printf("Country:\t%s\n", country = tlv_get_string(fp));

	printf("Number of Banks: %d\n\n", bcount = tlv_get_int(fp));

	/* haha, they concatenate #banks / 10 literally to the botname */
	if (bcount >= 10) {
		size_t l = strlen(name);

		name[l] = bcount / 10 + 0x30;
		name[l + 1] = 0;
	}
	
	for (i = 0; i < bcount; i++) {
		icount = tlv_get_int(fp);
		get_key_byte("blah reset", 1);

		if (flags & FL_HEX) {
			printf("Bank #%2u; Number of instructions: %d\n", i + 1, icount);
			printf("-----------------------------------\n");

			for (j = 0; j < icount; j++) {
				printf("#%.3u: ", j + 1);
				print_instr(fp, name, flags & FL_DECODE);
			}
		} else {
			printf("Bank %u\n\n", i + 1);
			for (j = 0; j < icount; j++)
				instr_decode(fp, name);
		}
		printf("\n\n");
	}

	free(name), free(author), free(country);
}
