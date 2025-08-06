#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#define IN_BUF_LEN (1024 * 64)

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
				r |= (byte & 0x7f) << shift;		\
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
	case 0x7d:				\
		printf("f32");			\
		break;				\
	case 0x7c:				\
		printf("f64");			\
		break;				\
	default:				\
		BOUM("unknow type %x\n", rt);	\
	} } while (0)


#define output(...)							\
	({								\
		printf("%x:", in - start);				\
		printf(__VA_ARGS__);					\
	})

int wasm_comment(unsigned char in[static 1], int max)
{
	unsigned char *start = in;
	int result;
	int section_len = -1;
	int stuff_len = -1;
	int count_bytes = 0;

	while (1) {
		switch (state) {
		case BEGIN:
		{
			unsigned char *beg = in;
			int *itmp;
			if (max < 9) {
				BOUM("file too small\n");
			}
			if (*in++ == 0 && *in++ == 0x61
			    && *in++ == 0x73 && *in++ == 0x6d) {
				output("0061 736d ; WASM_BINARY_MAGIC\n");
			} else {
				BOUM("invalide magic number at %zu\n", in  -beg);
			}
			itmp = (void *)in;
			output("\tVERSION: %d\n", *itmp);
			in += 4;
			max -= 8;
			state = WHICH_SECTION;
		}
		break;
		case FUNCTIONS:
			for (int i = 0; i < stuff_len; ++i) {
				output("  function %d signature index: %d\n", i, STORE_NUM(1));
			}
			if (section_len) {
				BOUM("functions section too big of %d\n", section_len);
			}
			goto out_section;
		case TABLE:
			for (int i = 0; i < stuff_len; ++i) {
				int t = ADVANCE();
				if (t != 0x70) {
					BOUM("unknow table type %x\n", t);
				}
				output("  funref(flag %x)", STORE_NUM(1));
				output("[%x]", STORE_NUM(1));
				output("(max :%x)\n", STORE_NUM(1));
			}
			if (section_len) {
				BOUM("table section too big of %d\n", section_len);
			}
			goto out_section;
		case MEMORY:
			for (int i = 0; i < stuff_len; ++i) {
				output("  memory(flag %x)", STORE_NUM(1));
				output("[%x]", STORE_NUM(1));
				output("(max :%x)\n", STORE_NUM(1));
			}
			if (section_len) {
				BOUM("mem section too big of %d\n", section_len);
			}
			goto out_section;
		case CODE:
		{
			int nb_parsed = 0;
		parse_fn_body: // yes I use a goto, just so I have less indentation :p
			int f_sz = STORE_NUM(1);
			int loc_dec_cnt = STORE_NUM(1);
			int cnt_ident = 1;
			output("func body(%d): (size %d), (loc decl count: %d) {\n",
			       nb_parsed, f_sz, loc_dec_cnt);
			if (loc_dec_cnt) {
				output("(local type cnt: %d ", STORE_NUM(1));
				PRINT_TYPE();
				printf(")\n");
			}
			while (1)
			{
				int instruction = ADVANCE();
				switch (instruction) {
				case 2:
				case 3:
				{
					output("\t%s: -> ", instruction == 2 ? "block" : "loop");
					++cnt_ident;
					int type = ADVANCE();
					if (type == 0x40) {
						output("() {\n");
					} else {
						BOUM("%x type not handle\n", type);
					}
				}
				continue;
				case 0x0c:
				{
					output("\tbr -> %d\n", STORE_NUM(1));
				}
				continue;
				case 0x0d:
				{
					output("\tbr_if -> %d\n", STORE_NUM(1));
				}
				continue;
				case 0x0e:
				{
					int nb = STORE_NUM(1);
					output("\tbr_table (%d) ->", nb);
					for (int i = 0; i < nb; ++i) {
						printf(" %d", STORE_NUM(1));
					}
					putchar('\n');
				}
				continue;
				case 0x10:
				{
					int fidx = STORE_NUM(1);
					output("\tcall func: %d\n", fidx);
				}
				continue;
				case 0x11:
				{
					int sidx = STORE_NUM(1);
					int tidx = STORE_NUM(1);
					output("\tindirect call func: sig %d -tbl  %d\n",
					       sidx, tidx);
				}
				continue;
				case 0x1a:
					output("drop\n");
					continue;
				case 0x24:
				{
					int fidx = STORE_NUM(1);
					output("\tglobal.set: %d\n", fidx);
				}
				continue;
				case 0x23:
				{
					int fidx = STORE_NUM(1);
					output("\tglobal.get: %d\n", fidx);
				}
				continue;
				case 0x20:
				{
					int fidx = STORE_NUM(1);
					output("\tlocal.get: %d\n", fidx);
				}
				continue;
				case 0x21:
				{
					int fidx = STORE_NUM(1);
					output("\tlocal.set: %d\n", fidx);
				}
				continue;
				case 0x22:
				{
					int fidx = STORE_NUM(1);
					output("\tlocal.tee: %d\n", fidx);
				}
				continue;
				case 0x28:
				{
					int alignement = STORE_NUM(1);
					int offset = STORE_NUM(1);
					output("\ti32.load: align: %d offset %d\n",
					       alignement, offset);
				}
				continue;
				case 0x36:
				{
					int alignement = STORE_NUM(1);
					int offset = STORE_NUM(1);
					output("\ti32.store: align: %d offset %d\n",
					       alignement, offset);
				}
				continue;
				case 0x37:
				{
					int alignement = STORE_NUM(1);
					int offset = STORE_NUM(1);
					output("\ti64.store: align: %d offset %d\n",
					       alignement, offset);
				}
				continue;
				case 0x41:
				{
					int fidx = STORE_NUM(1);
					output("\ti32.const: %d\n", fidx);
				}
				continue;
				case 0x42:
				{
					int fidx = STORE_NUM(1);
					output("\ti64.const: %d\n", fidx);
				}
				continue;
				case 0x45:
				{
					output("\ti32.eqz\n");
				}
				continue;
				case 0x46:
				{
					output("\ti32.eq\n");
				}
				continue;

				case 0x48:
				{
					output("\ti32.lt_s\n");
				}
				continue;
				case 0x4a:
				{
					output("\ti32.gt_s\n");
				}
				continue;
				case 0x4e:
				{
					output("\ti32.ge_s\n");
				}
				continue;
				case 0x6d:
				{
					output("\ti32.div_s\n");
				}
				continue;
				case 0x6a:
				{
					output("\ti32.add\n");
				}
				continue;
				case 0x6b:
				{
					output("\ti32.sub\n");
				}
				continue;
				case 0x6c:
				{
					output("\ti32.mul\n");
				}
				continue;
				case 0x71:
				{
					output("\ti32.and\n");
				}
				continue;
				case 0x72:
				{
					output("\ti32.or\n");
				}
				continue;
				case 0xac:
				{
					output("\ti64.extend_i32_s\n");
				}
				continue;
				case 0xf:
				{
					output("\treturn\n");
				}
				continue;
				case 0x0b:
					--cnt_ident;
					output("%s}\n", cnt_ident ? "\t" : "");
					if (!cnt_ident)
						break;
					continue;
				default:
					BOUM("unknow instruction %x\n", instruction);
				}
				break;
			}
			if (++nb_parsed < stuff_len)
				goto parse_fn_body;
		}
		if (section_len) {
			BOUM("section too big of %d\n", section_len);
		}
		goto out_section;
		case EXPORT:
			for (int i = 0; i < stuff_len; ++i) {
				output("  export(%d): ", i);
				/* read str */
				int len = STORE_NUM(1);
				if (max < len)
					return -1;
				section_len -= len;
				max -= len;
				char *str = malloc(len + 1);
				for (int i = 0; i < len; ++i)
					str[i] = *in++;
				str[len] = 0;
				output("'%s' ", str);
				free(str);
				/* end read str */
				int kind = STORE_NUM(1);
				switch (kind) {
				case 3:
					output("of unknow idx: %d", STORE_NUM(1));
					break;
				case 2:
					output("of mem idx: %d", STORE_NUM(1));
					break;
				case 0:
					output("of func idx: %d", STORE_NUM(1));
					break;
				case 1:
					output("of table idx: %d", STORE_NUM(1));
					break;
				default:
					BOUM("unknow kind %x\n", kind);
				}
				output("\n");
			}
			if (section_len) {
				BOUM("section too big of %d\n", section_len);
			}
			goto out_section;
		case GLOBAL:
			for (int i = 0; i < stuff_len; ++i) {
				output("  global(%d):", i);
				PRINT_TYPE();
				output(" - (mut: %d)\n", ADVANCE());
				int const_t = ADVANCE();
				if (const_t == 0x41) {
					output("\ti32.const: %d", STORE_NUM(1));
				} else {
					BOUM("const %x not handle\n", const_t);
				}
				if (ADVANCE() != 0x0b) {
					BOUM("expect end tok 0x0b\n");
				}
				output("\n");
			}
			if (section_len) {
				BOUM("section too big of %d\n", section_len);
			}
			goto out_section;
		case TYPE:
		{
			int t = ADVANCE();
			if (t == 0x60) {
				output("func(%d)\n", count_func++);
				int nb = ADVANCE();
				output("\tnb params (%d): (", nb);
				for (int i = 0; i < nb; ++i) {
					if (i)
						output(", ");
					PRINT_TYPE();
				}
				nb = ADVANCE();
				output(")\n\tnb result: %d (", nb);
				for (int i = 0; i < nb; ++i) {
					if (i)
						output(", ");
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
				output("SECTION TYPE ");
				state = TYPE;
				what = " nb types";
				break;
			case 03:
				output("SECTION FUNCTIONS ");
				state = FUNCTIONS;
				what = " nb functions";
				break;
			case 04:
				output("SECTION TABLE ");
				state = TABLE;
				what = " nb tables";
				break;
			case 05:
				output("SECTION MEMORY ");
				state = MEMORY;
				what = " nb memories";
				break;
			case 06:
				output("SECTION GLOBAL ");
				state = GLOBAL;
				what = " nb globals";
				break;
			case 07:
				output("SECTION EXPORT ");
				state = EXPORT;
				what = " nb exports";
				break;
			case 10:
				output("SECTION CODE ");
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
	unsigned char in[IN_BUF_LEN];
	ssize_t rret;

	while ((rret = read(0, in, IN_BUF_LEN - 1))) {
		in[rret] = 0;
		wasm_comment(in, rret);
	}
	fflush(stdout);

}
