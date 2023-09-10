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
	GLOBAL,
	MEMORY,
	CODE,
	EXPORT
};

int state = BEGIN;
int count_func;

#define STORE_NUM(check)  ({						\
			int r = 0;					\
			int shift = 0;					\
			while (1) {					\
				int byte = ADVANCE_(check);		\
				r |= (byte & 0x7f) << shift;	\
				if (!(byte & 0x80))			\
					break;				\
				shift += 7;				\
			}						\
			r;						\
		})

#define READ_NUM(MSG, check)  do {					\
		result = STORE_NUM(check);				\
		printf("%s: %d", MSG, result);				\
	} while(0)

#define ADVANCE_(check) ({						\
			if (max-- <= 0) return -1;			\
			if (check && section_len-- == 0) goto out_section; \
			*in++;})

#define ADVANCE() ADVANCE_(1)

#define PRINT_TYPE() do {			\
	int rt = ADVANCE();			\
	switch(rt) {				\
	case 0x7f:				\
		printf("i32");			\
		break;				\
	case 0x7e:				\
		printf("i64");			\
		break;				\
	default:				\
		BOUM("unknow type %x\n", rt);	\
	} } while (0)

int wasm_comment(char in[static 1], char out[static 1], int max)
{
	int result;
	int section_len = -1;
	int stuff_len = -1;

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
			printf("\tVERSION: %d\n", *itmp);
			in += 4;
			max -= 8;
			state = WHICH_SECTION;
		}
		break;
		case FUNCTIONS:
			for (int i = 0; i < stuff_len; ++i) {
				printf("  function %d signature index\n", STORE_NUM(1));
			}
			if (section_len) {
				BOUM("section too big of %d\n", section_len);
			}
			goto out_section;
		case TABLE:
			for (int i = 0; i < stuff_len; ++i) {
				int t = ADVANCE();
				if (t != 0x70) {
					BOUM("unknow table type %x\n", t);
				}
				printf("  funref(flag %x)", STORE_NUM(1));
				printf("[%d]", STORE_NUM(1));
				printf("(max :%d)\n", STORE_NUM(1));
			}
			if (section_len) {
				BOUM("section too big of %d\n", section_len);
			}
			goto out_section;
		case MEMORY:
			for (int i = 0; i < stuff_len; ++i) {
				printf("  memory(flag %x)", STORE_NUM(1));
				printf("[%d]", STORE_NUM(1));
				printf("(max :%d)\n", STORE_NUM(1));
			}
			if (section_len) {
				BOUM("section too big of %d\n", section_len);
			}
			goto out_section;
		case GLOBAL:
			for (int i = 0; i < stuff_len; ++i) {
				printf("  global(%d):", i);
				PRINT_TYPE();
				printf(" - (mut: %d)\n", ADVANCE());
				int const_t = ADVANCE();
				if (const_t == 0x41) {
					printf("\ti32.const: %d", STORE_NUM(1));
				} else {
					BOUM("const %x not handle\n", const_t);
				}
				if (ADVANCE() != 0x0b) {
					BOUM("expect end tok 0x0b\n");
				}
				printf("\n");
			}
			if (section_len) {
				BOUM("section too big of %d\n", section_len);
			}
			goto out_section;
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
					PRINT_TYPE();
				}
				nb = ADVANCE();
				printf(")\n\tnb result: %d (", nb);
				for (int i = 0; i < nb; ++i) {
					if (i)
						printf(", ");
					PRINT_TYPE();
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
				printf("SECTION TYPE ");
				state = TYPE;
				what = " nb types";
				break;
			case 03:
				printf("SECTION FUNCTIONS ");
				state = FUNCTIONS;
				what = " nb functions";
				break;
			case 04:
				printf("SECTION TABLE ");
				state = TABLE;
				what = " nb tables";
				break;
			case 05:
				printf("SECTION MEMORY ");
				state = MEMORY;
				what = " nb memories";
				break;
			case 06:
				printf("SECTION GLOBAL ");
				state = GLOBAL;
				what = " nb globals";
				break;
			case 07:
				printf("SECTION EXPORT ");
				state = EXPORT;
				what = " nb exports";
				break;
			case 10:
				printf("SECTION CODE ");
				state = CODE;
				what = " nb functtions";
				break;

			default:
				BOUM("unknow section !! %x\n", in[-1]);
			}
			--max;

			/* variadic uint for len decoding */
			READ_NUM("section len", 0);
			section_len = result;
			READ_NUM(what, 1);
			stuff_len = result;
			puts(":");
		}
			break;
		default:
			BOUM("in the unknow(cur state %d, cur stuff %x)\n",
			     state, *in);
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
