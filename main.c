#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#define IN_BUF_LEN 1024
#define OUT_BUF_LEN IN_BUF_LEN * 2

#define BOUM(...) do {				\
		fprintf(stderr, __VA_ARGS__);	\
		return -1;			\
	} while (0);

enum {
	BEGIN = 0,
	WHICH_SECTION,
	TYPE,
	FUNCTIONS,
	TABLE,
	MEMORY,
	EXPORT
};

int state = BEGIN;
int count_func;

#define READ_NUM(MSG)  do {			\
	result = 0;				\
	int shift = 0;				\
	while (1) {				\
	int byte = *in++;			\
	max -= 1;				\
	result |= (byte & 0x7f) << shift;	\
	if (!(byte & 0x80))			\
		break;				\
	shift += 7;				\
	}					\
	printf("%s: %d\n", MSG, result);	\
	} while(0)


#define ADVANCE() ({							\
			if (--max < 0) return -1;			\
			if (--section_len == 0) goto out_section;	\
			*in++;})

int wasm_comment(char in[static 1], char out[static 1], int max)
{
	int result;
	int section_len = -1;

	printf("state: %d\n", state == BEGIN);
	while (1) {
		switch (state) {
		case BEGIN:
		{
			char *beg = in;
			int *itmp;
			if (max < 9) {
				BOUM("file too small\n");
			}
			if (*in++ == 0 && *in++ == 0x61
			    && *in++ == 0x73 && *in++ == 0x6d) {
				printf("0061 736d ; WASM_BINARY_MAGIC\n");
			} else {
				BOUM("invalide magic number at %zu\n", in  -beg);
			}
			itmp = (void *)in;
			printf("VERSION: %d\n", *itmp);
			in += 4;
			max -= 8;
			state = WHICH_SECTION;
		}
		break;
		case TYPE:
		{
			int t = ADVANCE();
			if (t == 0x60) {
				printf("func(%d)\n", count_func++);
				int nb = ADVANCE();
				printf("\tnb params (%d): (", nb);
				for (int i = 0; i < nb; ++i) {
					if (i)
						printf(", ");
					int pt = ADVANCE();
					switch(pt) {
					case 0x7f:
						printf("i32");
						break;
					case 0x7e:
						printf("i64");
						break;
					default:
						BOUM("unknow param type %x\n", pt);
					}
				}
				nb = ADVANCE();
				printf(")\n\tnb result: %d (", nb);
				for (int i = 0; i < nb; ++i) {
					if (i)
						printf(", ");
					int rt = ADVANCE();
					switch(rt) {
					case 0x7f:
						printf("i32");
						break;
					case 0x7e:
						printf("i64");
						break;
					default:
						BOUM("unknow return type %x\n", rt);
					}
				}
				puts(")");
			} else
				BOUM("unknow %x (section bytes left: %d)\n",
				     t, section_len);
			break;
		}
		case WHICH_SECTION:
		{
			const char *what = "???";
			switch (*in++) {
			case 01:
				printf("SECTION CODE:\n");
				state = TYPE;
				what = "nb types";
				break;
			case 03:
				printf("SECTION FUNCTIONS:\n");
				state = FUNCTIONS;
				what = "nb functions";
				break;
			case 04:
				printf("SECTION TABLE:\n");
				state = TABLE;
				what = "nb tables";
				break;
			case 05:
				printf("SECTION MEMORY:\n");
				state = MEMORY;
				what = "nb memories";
				break;
			case 07:
				printf("SECTION EXPORT:\n");
				state = EXPORT;
				what = "nb exports";
				break;
			default:
				BOUM("unknow section !!\n");
			}
			--max;

			/* variadic uint for len decoding */
			READ_NUM("section len");
			section_len = result;
			READ_NUM(what);
		}
			break;
		default:
			BOUM("in the unknow\n");
		}
		continue;
	out_section:
		state = WHICH_SECTION;
	}
}

int main(void)
{
	char in[IN_BUF_LEN];
	char out[OUT_BUF_LEN];
	ssize_t rret;

	while ((rret = read(0, in, IN_BUF_LEN - 1))) {
		in[rret] = 0;
		wasm_comment(in, out, OUT_BUF_LEN - rret);
		printf("%s", out);
	}
	fflush(stdout);

}
