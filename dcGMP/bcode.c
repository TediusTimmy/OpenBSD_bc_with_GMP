/*	$OpenBSD: bcode.c,v 1.62 2017/12/29 08:16:55 otto Exp $	*/

/*
 * Copyright (c) 2003, Otto Moerbeek <otto@drijf.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <err.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

/* #define	DEBUGGING */

#define MAX_ARRAY_INDEX		2048
#define READSTACK_SIZE		8

#define NO_ELSE			-2	/* -1 is EOF */
#define REG_ARRAY_SIZE_SMALL	(UCHAR_MAX + 1)
#define REG_ARRAY_SIZE_BIG	(UCHAR_MAX + 1 + USHRT_MAX + 1)

struct bmachine {
	struct stack		stack;
	u_int			scale;
	u_int			obase;
	u_int			ibase;
	size_t			readsp;
	bool			extended_regs;
	size_t			reg_array_size;
	struct stack		*reg;
	volatile sig_atomic_t	interrupted;
	struct source		*readstack;
	size_t			readstack_sz;
};

static struct bmachine	bmachine;
static void sighandler(int);

static __inline int	readch(void);
static __inline void	unreadch(void);
static __inline char	*readline(void);
static __inline void	src_free(void);

static __inline u_int	max(u_int, u_int);
static u_long		get_ulong(struct number *);

static __inline void	push_number(struct number *);
static __inline void	push_string(char *);
static __inline void	push(struct value *);
static __inline struct value *tos(void);
static __inline struct number	*pop_number(void);
static __inline char	*pop_string(void);
static void		clear_stack(void);
static void		print_tos(void);
static void		print_err(void);
static void		pop_print(void);
static void		pop_printn(void);
static void		print_stack(void);
static void		dup(void);
static void		swap(void);
static void		drop(void);

static void		get_scale(void);
static void		set_scale(void);
static void		get_obase(void);
static void		set_obase(void);
static void		get_ibase(void);
static void		set_ibase(void);
static void		stackdepth(void);
static void		push_scale(void);
static u_int		count_digits(const struct number *);
static void		num_digits(void);
static void		to_ascii(void);
static void		push_line(void);
static void		comment(void);
static void		badd(void);
static void		bsub(void);
static void		bmul(void);
static void		bdiv(void);
static void		bmod(void);
static void		bdivmod(void);
static void		bexp(void);
static void		bsqrt(void);
static void		not(void);
static void		equal_numbers(void);
static void		less_numbers(void);
static void		lesseq_numbers(void);
static void		equal(void);
static void		less(void);
static void		greater(void);
static void		not_compare(void);
static bool		compare_numbers(enum bcode_compare, struct number *,
			    struct number *);
static void		compare(enum bcode_compare);
static int		readreg(void);
static void		load(void);
static void		store(void);
static void		load_stack(void);
static void		store_stack(void);
static void		load_array(void);
static void		store_array(void);
static void		nop(void);
static void		quit(void);
static void		quitN(void);
static void		skipN(void);
static void		skip_until_mark(void);
static void		parse_number(void);
static void		unknown(void);
static void		eval_string(char *);
static void		eval_line(void);
static void		eval_tos(void);


typedef void		(*opcode_function)(void);

struct jump_entry {
	u_char		ch;
	opcode_function	f;
};


static opcode_function jump_table[UCHAR_MAX + 1];

static const struct jump_entry jump_table_data[] = {
	{ ' ',	nop		},
	{ '!',	not_compare	},
	{ '#',	comment		},
	{ '%',	bmod		},
	{ '(',	less_numbers	},
	{ '*',	bmul		},
	{ '+',	badd		},
	{ '-',	bsub		},
	{ '.',	parse_number	},
	{ '/',	bdiv		},
	{ '0',	parse_number	},
	{ '1',	parse_number	},
	{ '2',	parse_number	},
	{ '3',	parse_number	},
	{ '4',	parse_number	},
	{ '5',	parse_number	},
	{ '6',	parse_number	},
	{ '7',	parse_number	},
	{ '8',	parse_number	},
	{ '9',	parse_number	},
	{ ':',	store_array	},
	{ ';',	load_array	},
	{ '<',	less		},
	{ '=',	equal		},
	{ '>',	greater		},
	{ '?',	eval_line	},
	{ 'A',	parse_number	},
	{ 'B',	parse_number	},
	{ 'C',	parse_number	},
	{ 'D',	parse_number	},
	{ 'E',	parse_number	},
	{ 'F',	parse_number	},
	{ 'G',	equal_numbers	},
	{ 'I',	get_ibase	},
	{ 'J',	skipN		},
	{ 'K',	get_scale	},
	{ 'L',	load_stack	},
	{ 'M',	nop		},
	{ 'N',	not		},
	{ 'O',	get_obase	},
	{ 'P',	pop_print	},
	{ 'Q',	quitN		},
	{ 'R',	drop		},
	{ 'S',	store_stack	},
	{ 'X',	push_scale	},
	{ 'Z',	num_digits	},
	{ '[',	push_line	},
	{ '\f',	nop		},
	{ '\n',	nop		},
	{ '\r',	nop		},
	{ '\t',	nop		},
	{ '^',	bexp		},
	{ '_',	parse_number	},
	{ 'a',	to_ascii	},
	{ 'c',	clear_stack	},
	{ 'd',	dup		},
	{ 'e',	print_err	},
	{ 'f',	print_stack	},
	{ 'i',	set_ibase	},
	{ 'k',	set_scale	},
	{ 'l',	load		},
	{ 'n',	pop_printn	},
	{ 'o',	set_obase	},
	{ 'p',	print_tos	},
	{ 'q',	quit		},
	{ 'r',	swap		},
	{ 's',	store		},
	{ 'v',	bsqrt		},
	{ 'x',	eval_tos	},
	{ 'z',	stackdepth	},
	{ '{',	lesseq_numbers	},
	{ '~',	bdivmod		}
};

#ifndef nitems
#define nitems(a)	(sizeof((a)) / sizeof((a)[0]))
#endif

/* ARGSUSED */
static void
sighandler(int ignored)
{
	(void) ignored;
	bmachine.interrupted = true;
}

void
init_bmachine(bool extended_registers)
{
	int i;

	bmachine.extended_regs = extended_registers;
	bmachine.reg_array_size = bmachine.extended_regs ?
	    REG_ARRAY_SIZE_BIG : REG_ARRAY_SIZE_SMALL;

	bmachine.reg = calloc(bmachine.reg_array_size,
	    sizeof(bmachine.reg[0]));
	if (bmachine.reg == NULL)
		err(1, NULL);

	for (i = 0; i < nitems(jump_table); i++)
		jump_table[i] = unknown;

	for (i = 0; i < nitems(jump_table_data); i++) {
		if ((unsigned int)jump_table_data[i].ch >= nitems(jump_table))
			errx(1, "opcode '%c' overflows jump table",
			    jump_table_data[i].ch);
		if (jump_table[jump_table_data[i].ch] != unknown)
			errx(1, "opcode '%c' already assigned",
			    jump_table_data[i].ch);
		jump_table[jump_table_data[i].ch] = jump_table_data[i].f;
	}

	stack_init(&bmachine.stack);

	for (i = 0; i < bmachine.reg_array_size; i++)
		stack_init(&bmachine.reg[i]);

	bmachine.readstack_sz = READSTACK_SIZE;
	bmachine.readstack = calloc(sizeof(struct source),
	    bmachine.readstack_sz);
	if (bmachine.readstack == NULL)
		err(1, NULL);
	bmachine.obase = bmachine.ibase = 10;
	(void)signal(SIGINT, sighandler);
}

u_int
bmachine_scale(void)
{
	return bmachine.scale;
}

/* Reset the things needed before processing a (new) file */
void
reset_bmachine(struct source *src)
{
	bmachine.readsp = 0;
	bmachine.readstack[0] = *src;
}

static __inline int
readch(void)
{
	struct source *src = &bmachine.readstack[bmachine.readsp];

	return src->vtable->readchar(src);
}

static __inline void
unreadch(void)
{
	struct source *src = &bmachine.readstack[bmachine.readsp];

	src->vtable->unreadchar(src);
}

static __inline char *
readline(void)
{
	struct source *src = &bmachine.readstack[bmachine.readsp];

	return src->vtable->readline(src);
}

static __inline void
src_free(void)
{
	struct source *src = &bmachine.readstack[bmachine.readsp];

	src->vtable->free(src);
}

#ifdef DEBUGGING
void
pn(const char *str, const struct number *n)
{
	char *p = BN_bn2dec(n->number);
	if (p == NULL)
		err(1, "BN_bn2dec failed");
	(void)fputs(str, stderr);
	(void)fprintf(stderr, " %s (%u)\n" , p, n->scale);
	OPENSSL_free(p);
}

void
pbn(const char *str, const mpz_t n)
{
	char *p = BN_bn2dec(n);
	if (p == NULL)
		err(1, "BN_bn2dec failed");
	(void)fputs(str, stderr);
	(void)fprintf(stderr, " %s\n", p);
	OPENSSL_free(p);
}

#endif

static __inline u_int
max(u_int a, u_int b)
{
	return a > b ? a : b;
}

static unsigned long factors[] = {
	0, 10, 100, 1000, 10000, 100000, 1000000, 10000000,
	100000000, 1000000000
};

void
scale_number(mpz_t n, int s)
{
	int abs_scale;

	if (s == 0)
		return;

	abs_scale = s > 0 ? s : -s;

	if (abs_scale < nitems(factors)) {
		if (s > 0)
			mpz_mul_si(n, n, factors[abs_scale]);
		else
			(void)mpz_tdiv_q_ui(n, n, factors[abs_scale]);
	} else {
		mpz_t a;

		mpz_init(a);

		mpz_ui_pow_ui(a, 10, abs_scale);
		if (s > 0)
			mpz_mul(n, n, a);
		else
			mpz_tdiv_q(n, n, a);
		mpz_clear(a);
	}
}

void
split_number(const struct number *n, mpz_t i, mpz_t f)
{
	u_long rem;

	mpz_set(i, n->number);

	if (n->scale == 0) {
		if (f != NULL)
			mpz_set_ui(f, 0);
	} else if (n->scale < nitems(factors)) {
		rem = mpz_tdiv_q_ui(i, i, factors[n->scale]);
		if (f != NULL)
			mpz_set_ui(f, rem);
	} else {
		mpz_t a;

		mpz_init(a);

		mpz_ui_pow_ui(a, 10, n->scale);
		if (f != NULL)
			mpz_tdiv_qr(i, f, n->number, a);
		else
			mpz_tdiv_q(i, n->number, a);
		mpz_clear(a);
	}
}

void
normalize(struct number *n, u_int s)
{
	scale_number(n->number, s - n->scale);
	n->scale = s;
}

static u_long
get_ulong(struct number *n)
{
	normalize(n, 0);
	return mpz_get_ui(n->number);
}

void
negate(struct number *n)
{
	mpz_neg(n->number, n->number);
}

static __inline void
push_number(struct number *n)
{
	stack_pushnumber(&bmachine.stack, n);
}

static __inline void
push_string(char *string)
{
	stack_pushstring(&bmachine.stack, string);
}

static __inline void
push(struct value *v)
{
	stack_push(&bmachine.stack, v);
}

static __inline struct value *
tos(void)
{
	return stack_tos(&bmachine.stack);
}

static __inline struct value *
pop(void)
{
	return stack_pop(&bmachine.stack);
}

static __inline struct number *
pop_number(void)
{
	return stack_popnumber(&bmachine.stack);
}

static __inline char *
pop_string(void)
{
	return stack_popstring(&bmachine.stack);
}

static void
clear_stack(void)
{
	stack_clear(&bmachine.stack);
}

static void
print_stack(void)
{
	stack_print(stdout, &bmachine.stack, "", bmachine.obase);
}

static void
print_tos(void)
{
	struct value *value = tos();
	if (value != NULL) {
		print_value(stdout, value, "", bmachine.obase);
		(void)putchar('\n');
	} else
		warnx("stack empty");
}

static void
print_err(void)
{
	struct value *value = tos();
	if (value != NULL) {
		print_value(stderr, value, "", bmachine.obase);
		(void)putc('\n', stderr);
	} else
		warnx("stack empty");
}

static void
pop_print(void)
{
	struct value *value = pop();

	if (value != NULL) {
		switch (value->type) {
		case BCODE_NONE:
			break;
		case BCODE_NUMBER:
			normalize(value->u.num, 0);
			print_ascii(stdout, value->u.num);
			(void)fflush(stdout);
			break;
		case BCODE_STRING:
			(void)fputs(value->u.string, stdout);
			(void)fflush(stdout);
			break;
		}
		stack_free_value(value);
	}
}

static void
pop_printn(void)
{
	struct value *value = pop();

	if (value != NULL) {
		print_value(stdout, value, "", bmachine.obase);
		(void)fflush(stdout);
		stack_free_value(value);
	}
}

static void
dup(void)
{
	stack_dup(&bmachine.stack);
}

static void
swap(void)
{
	stack_swap(&bmachine.stack);
}

static void
drop(void)
{
	struct value *v = pop();
	if (v != NULL)
		stack_free_value(v);
}

static void
get_scale(void)
{
	struct number	*n;

	n = new_number();
	mpz_set_ui(n->number, bmachine.scale);
	push_number(n);
}

static void
set_scale(void)
{
	struct number	*n;
	u_long		scale;

	n = pop_number();
	if (n != NULL) {
		if (-1 == mpz_sgn(n->number))
			warnx("scale must be a nonnegative number");
		else {
			scale = get_ulong(n);
			if (scale != GMP_NUMB_MASK && scale <= UINT_MAX)
				bmachine.scale = (u_int)scale;
			else
				warnx("scale too large");
		}
		free_number(n);
	}
}

static void
get_obase(void)
{
	struct number	*n;

	n = new_number();
	mpz_set_ui(n->number, bmachine.obase);
	push_number(n);
}

static void
set_obase(void)
{
	struct number	*n;
	u_long		base;

	n = pop_number();
	if (n != NULL) {
		base = get_ulong(n);
		if (base != GMP_NUMB_MASK && base > 1 && base <= UINT_MAX)
			bmachine.obase = (u_int)base;
		else
			warnx("output base must be a number greater than 1");
		free_number(n);
	}
}

static void
get_ibase(void)
{
	struct number *n;

	n = new_number();
	mpz_set_ui(n->number, bmachine.ibase);
	push_number(n);
}

static void
set_ibase(void)
{
	struct number	*n;
	u_long		base;

	n = pop_number();
	if (n != NULL) {
		base = get_ulong(n);
		if (base != GMP_NUMB_MASK && 2 <= base && base <= 16)
			bmachine.ibase = (u_int)base;
		else
			warnx("input base must be a number between 2 and 16 "
			    "(inclusive)");
		free_number(n);
	}
}

static void
stackdepth(void)
{
	size_t i;
	struct number *n;

	i = stack_size(&bmachine.stack);
	n = new_number();
	mpz_set_ui(n->number, i);
	push_number(n);
}

static void
push_scale(void)
{
	struct value	*value;
	u_int		scale = 0;
	struct number	*n;

	value = pop();
	if (value != NULL) {
		switch (value->type) {
		case BCODE_NONE:
			return;
		case BCODE_NUMBER:
			scale = value->u.num->scale;
			break;
		case BCODE_STRING:
			break;
		}
		stack_free_value(value);
		n = new_number();
		mpz_set_ui(n->number, scale);
		push_number(n);
	}
}

static u_int
count_digits(const struct number *n)
{
	mpz_t		int_part, a;
	uint		d;
	const uint64_t	c = 1292913986; /* floor(2^32 * log_10(2)) */
	int		bits;

	if (0 == mpz_sgn(n->number))
		return n->scale;

	mpz_init(int_part);

	split_number(n, int_part, NULL);
	bits = mpz_sizeinbase(int_part, 2);

	if (bits == 0)
		d = 0;
	else {
		/*
		 * Estimate number of decimal digits based on number of bits.
		 * Divide 2^32 factor out by shifting
		 */
		d = (c * bits) >> 32;

		/* If close to a possible rounding error fix if needed */
		if (d != (c * (bits - 1)) >> 32) {
			mpz_init(a);

			mpz_ui_pow_ui(a, 10, d);

			if (mpz_cmpabs(int_part, a) >= 0)
				d++;

			mpz_clear(a);
		} else
			d++;
	}

	mpz_clear(int_part);

	return d + n->scale;
}

static void
num_digits(void)
{
	struct value	*value;
	size_t		digits;
	struct number	*n = NULL;

	value = pop();
	if (value != NULL) {
		switch (value->type) {
		case BCODE_NONE:
			return;
		case BCODE_NUMBER:
			digits = count_digits(value->u.num);
			n = new_number();
			mpz_set_ui(n->number, digits);
			break;
		case BCODE_STRING:
			digits = strlen(value->u.string);
			n = new_number();
			mpz_set_ui(n->number, digits);
			break;
		}
		stack_free_value(value);
		push_number(n);
	}
}

static void
to_ascii(void)
{
	char		str[2];
	struct value	*value;
	struct number	*n;

	value = pop();
	if (value != NULL) {
		str[1] = '\0';
		switch (value->type) {
		case BCODE_NONE:
			return;
		case BCODE_NUMBER:
			n = value->u.num;
			normalize(n, 0);
			str[0] = (char)mpz_get_ui(n->number);
			break;
		case BCODE_STRING:
			str[0] = value->u.string[0];
			break;
		}
		stack_free_value(value);
		push_string(bstrdup(str));
	}
}

static int
readreg(void)
{
	int idx, ch1, ch2;

	idx = readch();
	if (idx == 0xff && bmachine.extended_regs) {
		ch1 = readch();
		ch2 = readch();
		if (ch1 == EOF || ch2 == EOF) {
			warnx("unexpected eof");
			idx = -1;
		} else
			idx = (ch1 << 8) + ch2 + UCHAR_MAX + 1;
	}
	if (idx < 0 || idx >= bmachine.reg_array_size) {
		warnx("internal error: reg num = %d", idx);
		idx = -1;
	}
	return idx;
}

static void
load(void)
{
	int		idx;
	struct value	*v, copy;
	struct number	*n;

	idx = readreg();
	if (idx >= 0) {
		v = stack_tos(&bmachine.reg[idx]);
		if (v == NULL) {
			n = new_number(); /* Always initialized to zero. */
			push_number(n);
		} else
			push(stack_dup_value(v, &copy));
	}
}

static void
store(void)
{
	int		idx;
	struct value	*val;

	idx = readreg();
	if (idx >= 0) {
		val = pop();
		if (val == NULL) {
			return;
		}
		stack_set_tos(&bmachine.reg[idx], val);
	}
}

static void
load_stack(void)
{
	int		idx;
	struct stack	*stack;
	struct value	*value;

	idx = readreg();
	if (idx >= 0) {
		stack = &bmachine.reg[idx];
		value = NULL;
		if (stack_size(stack) > 0) {
			value = stack_pop(stack);
		}
		if (value != NULL)
			push(value);
		else
			warnx("stack register '%c' (0%o) is empty",
			    idx, idx);
	}
}

static void
store_stack(void)
{
	int		idx;
	struct value	*value;

	idx = readreg();
	if (idx >= 0) {
		value = pop();
		if (value == NULL)
			return;
		stack_push(&bmachine.reg[idx], value);
	}
}

static void
load_array(void)
{
	int			reg;
	struct number		*inumber, *n;
	u_long			idx;
	struct stack		*stack;
	struct value		*v, copy;

	reg = readreg();
	if (reg >= 0) {
		inumber = pop_number();
		if (inumber == NULL)
			return;
		idx = get_ulong(inumber);
		if (-1 == mpz_sgn(inumber->number))
			warnx("negative idx");
		else if (idx == GMP_NUMB_MASK || idx > MAX_ARRAY_INDEX)
			warnx("idx too big");
		else {
			stack = &bmachine.reg[reg];
			v = frame_retrieve(stack, idx);
			if (v == NULL || v->type == BCODE_NONE) {
				n = new_number(); /* Always initialized to zero. */
				push_number(n);
			} else
				push(stack_dup_value(v, &copy));
		}
		free_number(inumber);
	}
}

static void
store_array(void)
{
	int			reg;
	struct number		*inumber;
	u_long			idx;
	struct value		*value;
	struct stack		*stack;

	reg = readreg();
	if (reg >= 0) {
		inumber = pop_number();
		if (inumber == NULL)
			return;
		value = pop();
		if (value == NULL) {
			free_number(inumber);
			return;
		}
		idx = get_ulong(inumber);
		if (-1 == mpz_sgn(inumber->number)) {
			warnx("negative idx");
			stack_free_value(value);
		} else if (idx == GMP_NUMB_MASK || idx > MAX_ARRAY_INDEX) {
			warnx("idx too big");
			stack_free_value(value);
		} else {
			stack = &bmachine.reg[reg];
			frame_assign(stack, idx, value);
		}
		free_number(inumber);
	}
}

static void
push_line(void)
{
	push_string(read_string(&bmachine.readstack[bmachine.readsp]));
}

static void
comment(void)
{
	free(readline());
}

static void
badd(void)
{
	struct number	*a, *b;

	a = pop_number();
	if (a == NULL)
		return;
	b = pop_number();
	if (b == NULL) {
		push_number(a);
		return;
	}

	if (b->scale > a->scale)
		normalize(a, b->scale);
	else if (a->scale > b->scale)
		normalize(b, a->scale);
	mpz_add(b->number, a->number, b->number);
	free_number(a);
	push_number(b);
}

static void
bsub(void)
{
	struct number	*a, *b;

	a = pop_number();
	if (a == NULL)
		return;
	b = pop_number();
	if (b == NULL) {
		push_number(a);
		return;
	}

	if (b->scale > a->scale)
		normalize(a, b->scale);
	else if (a->scale > b->scale)
		normalize(b, a->scale);
	mpz_sub(b->number, b->number, a->number);
	free_number(a);
	push_number(b);
}

void
bmul_number(struct number *r, struct number *a, struct number *b, u_int scale)
{
	/* Create copies of the scales, since r might be equal to a or b */
	u_int ascale = a->scale;
	u_int bscale = b->scale;
	u_int rscale = ascale + bscale;

	mpz_mul(r->number, a->number, b->number);

	r->scale = rscale;
	if (rscale > bmachine.scale && rscale > ascale && rscale > bscale)
		normalize(r, max(scale, max(ascale, bscale)));
}

static void
bmul(void)
{
	struct number	*a, *b;

	a = pop_number();
	if (a == NULL)
		return;
	b = pop_number();
	if (b == NULL) {
		push_number(a);
		return;
	}

	bmul_number(b, a, b, bmachine.scale);
	free_number(a);
	push_number(b);
}

static void
bdiv(void)
{
	struct number	*a, *b;
	struct number	*r;
	u_int		scale;

	a = pop_number();
	if (a == NULL)
		return;
	b = pop_number();
	if (b == NULL) {
		push_number(a);
		return;
	}

	r = new_number();
	r->scale = bmachine.scale;
	scale = max(a->scale, b->scale);

	if (0 == mpz_sgn(a->number))
		warnx("divide by zero");
	else {
		normalize(a, scale);
		normalize(b, scale + r->scale);

		mpz_tdiv_q(r->number, b->number, a->number);
	}
	push_number(r);
	free_number(a);
	free_number(b);
}

static void
bmod(void)
{
	struct number	*a, *b;
	struct number	*r;
	u_int		scale;

	a = pop_number();
	if (a == NULL)
		return;
	b = pop_number();
	if (b == NULL) {
		push_number(a);
		return;
	}

	r = new_number();
	scale = max(a->scale, b->scale);
	r->scale = max(b->scale, a->scale + bmachine.scale);

	if (0 == mpz_sgn(a->number))
		warnx("remainder by zero");
	else {
		normalize(a, scale);
		normalize(b, scale + bmachine.scale);

		mpz_tdiv_r(r->number, b->number, a->number);
	}
	push_number(r);
	free_number(a);
	free_number(b);
}

static void
bdivmod(void)
{
	struct number	*a, *b;
	struct number	*rdiv, *rmod;
	u_int		scale;

	a = pop_number();
	if (a == NULL)
		return;
	b = pop_number();
	if (b == NULL) {
		push_number(a);
		return;
	}

	rdiv = new_number();
	rmod = new_number();
	rdiv->scale = bmachine.scale;
	rmod->scale = max(b->scale, a->scale + bmachine.scale);
	scale = max(a->scale, b->scale);

	if (0 == mpz_sgn(a->number))
		warnx("divide by zero");
	else {
		normalize(a, scale);
		normalize(b, scale + bmachine.scale);

		mpz_tdiv_qr(rdiv->number, rmod->number,
		    b->number, a->number);
	}
	push_number(rdiv);
	push_number(rmod);
	free_number(a);
	free_number(b);
}

static void
bexp(void)
{
	struct number	*a, *p;
	struct number	*r;
	bool		neg;
	u_int		rscale;

	p = pop_number();
	if (p == NULL)
		return;
	a = pop_number();
	if (a == NULL) {
		push_number(p);
		return;
	}

	if (p->scale != 0) {
		mpz_t i, f;
		mpz_init(i);
		mpz_init(f);
		split_number(p, i, f);
		if (0 != mpz_sgn(f))
			warnx("Runtime warning: non-zero fractional part "
			    "in exponent");
		mpz_set(p->number, i);
		mpz_clear(f);
		mpz_clear(i);
	}

	neg = mpz_sgn(p->number) < 0;
	if (neg) {
		negate(p);
		rscale = bmachine.scale;
	} else {
		/* Posix bc says min(a.scale * b, max(a.scale, scale)) */
		u_long	b;
		u_int	m;

		b = mpz_get_ui(p->number); /* BUG : b is expected to be ~0 if number > MAX_UINT */
		m = max(a->scale, bmachine.scale);
		rscale = a->scale * (u_int)b;
		if (rscale > m || (a->scale > 0 && (b == GMP_NUMB_MASK ||
		    b > UINT_MAX)))
			rscale = m;
	}

	if (0 == mpz_sgn(p->number)) {
		r = new_number();
		mpz_set_ui(r->number, 1U);
		normalize(r, rscale);
	} else {
		u_int ascale, mscale;

		ascale = a->scale;
		while (!mpz_tstbit(p->number, 0)) {
			ascale *= 2;
			bmul_number(a, a, a, ascale);
			mpz_tdiv_q_2exp(p->number, p->number, 1);
		}

		r = dup_number(a);
		mpz_tdiv_q_2exp(p->number, p->number, 1);

		mscale = ascale;
		while (0 != mpz_sgn(p->number)) {
			ascale *= 2;
			bmul_number(a, a, a, ascale);
			if (mpz_tstbit(p->number, 0)) {
				mscale += ascale;
				bmul_number(r, r, a, mscale);
			}
			mpz_tdiv_q_2exp(p->number, p->number, 1);
		}

		if (neg) {
			mpz_t	one;

			mpz_init_set_ui(one, 1);
			scale_number(one, r->scale + rscale);

			if (0 == mpz_sgn(r->number))
				warnx("divide by zero");
			else
				mpz_tdiv_q(r->number, one,
				    r->number);
			mpz_clear(one);
			r->scale = rscale;
		} else
			normalize(r, rscale);
	}
	push_number(r);
	free_number(a);
	free_number(p);
}

static void
bsqrt(void)
{
	struct number	*n;
	struct number	*r;
	mpz_t		x, y, t, o;
	u_int		scale, onecount;

	onecount = 0;
	n = pop_number();
	if (n == NULL)
		return;
	if (0 == mpz_sgn(n->number)) {
		r = new_number();
		push_number(r);
	} else if (-1 == mpz_sgn(n->number))
		warnx("square root of negative number");
	else {
		scale = max(bmachine.scale, n->scale);
		normalize(n, 2*scale);
		mpz_init_set_ui(o, 1U);
		mpz_init_set(x, n->number);
		mpz_tdiv_q_2exp(x, x, mpz_sizeinbase(x, 2)/2);
		mpz_init(y);
		do {
			mpz_tdiv_q(y, n->number, x);
			mpz_add(y, x, y);
			mpz_tdiv_q_2exp(y, y, 1);
			mpz_sub(x, y, x);
			mpz_set(t, x);
			mpz_set(x, y);
			mpz_set(y, t);
		} while ((0 != mpz_sgn(y)) && (onecount += (0 == mpz_cmp(y, o)) ? 1 : 0) < 2);
		mpz_sub(y, x, y);
		r = bmalloc(sizeof(*r));
		r->scale = scale;
		mpz_set(r->number, y);
		mpz_clear(x);
		mpz_clear(o);
		push_number(r);
	}

	free_number(n);
}

static void
not(void)
{
	struct number	*a;

	a = pop_number();
	if (a == NULL)
		return;
	a->scale = 0;
	mpz_set_ui(a->number, (0 != mpz_sgn(a->number)) ? 0 : 1);
	push_number(a);
}

static void
equal(void)
{
	compare(BCODE_EQUAL);
}

static void
equal_numbers(void)
{
	struct number *a, *b, *r;

	a = pop_number();
	if (a == NULL)
		return;
	b = pop_number();
	if (b == NULL) {
		push_number(a);
		return;
	}
	r = new_number();
	mpz_set_ui(r->number,
	    compare_numbers(BCODE_EQUAL, a, b) ? 1 : 0);
	push_number(r);
}

static void
less_numbers(void)
{
	struct number *a, *b, *r;

	a = pop_number();
	if (a == NULL)
		return;
	b = pop_number();
	if (b == NULL) {
		push_number(a);
		return;
	}
	r = new_number();
	mpz_set_ui(r->number,
	    compare_numbers(BCODE_LESS, a, b) ? 1 : 0);
	push_number(r);
}

static void
lesseq_numbers(void)
{
	struct number *a, *b, *r;

	a = pop_number();
	if (a == NULL)
		return;
	b = pop_number();
	if (b == NULL) {
		push_number(a);
		return;
	}
	r = new_number();
	mpz_set_ui(r->number,
	    compare_numbers(BCODE_NOT_GREATER, a, b) ? 1 : 0);
	push_number(r);
}

static void
less(void)
{
	compare(BCODE_LESS);
}

static void
not_compare(void)
{
	switch (readch()) {
	case '<':
		compare(BCODE_NOT_LESS);
		break;
	case '>':
		compare(BCODE_NOT_GREATER);
		break;
	case '=':
		compare(BCODE_NOT_EQUAL);
		break;
	default:
		unreadch();
		warnx("! command is deprecated");
		break;
	}
}

static void
greater(void)
{
	compare(BCODE_GREATER);
}

static bool
compare_numbers(enum bcode_compare type, struct number *a, struct number *b)
{
	u_int	scale;
	int	cmp;

	scale = max(a->scale, b->scale);

	if (scale > a->scale)
		normalize(a, scale);
	else if (scale > b->scale)
		normalize(b, scale);

	cmp = mpz_cmp(a->number, b->number);

	free_number(a);
	free_number(b);

	switch (type) {
	case BCODE_EQUAL:
		return cmp == 0;
	case BCODE_NOT_EQUAL:
		return cmp != 0;
	case BCODE_LESS:
		return cmp < 0;
	case BCODE_NOT_LESS:
		return cmp >= 0;
	case BCODE_GREATER:
		return cmp > 0;
	case BCODE_NOT_GREATER:
		return cmp <= 0;
	}
	return false;
}

static void
compare(enum bcode_compare type)
{
	int		idx, elseidx;
	struct number	*a, *b;
	bool		ok;
	struct value	*v;

	elseidx = NO_ELSE;
	idx = readreg();
	if (readch() == 'e')
		elseidx = readreg();
	else
		unreadch();

	a = pop_number();
	if (a == NULL)
		return;
	b = pop_number();
	if (b == NULL) {
		push_number(a);
		return;
	}

	ok = compare_numbers(type, a, b);

	if (!ok && elseidx != NO_ELSE)
		idx = elseidx;

	if (idx >= 0 && (ok || (!ok && elseidx != NO_ELSE))) {
		v = stack_tos(&bmachine.reg[idx]);
		if (v == NULL)
			warnx("register '%c' (0%o) is empty", idx, idx);
		else {
			switch(v->type) {
			case BCODE_NONE:
				warnx("register '%c' (0%o) is empty", idx, idx);
				break;
			case BCODE_NUMBER:
				warn("eval called with non-string argument");
				break;
			case BCODE_STRING:
				eval_string(bstrdup(v->u.string));
				break;
			}
		}
	}
}

static void
nop(void)
{
}

static void
quit(void)
{
	if (bmachine.readsp < 2)
		exit(0);
	src_free();
	bmachine.readsp--;
	src_free();
	bmachine.readsp--;
}

static void
quitN(void)
{
	struct number	*n;
	u_long		i;

	n = pop_number();
	if (n == NULL)
		return;
	i = get_ulong(n);
	free_number(n);
	if (i == GMP_NUMB_MASK || i == 0)
		warnx("Q command requires a number >= 1");
	else if (bmachine.readsp < i)
		warnx("Q command argument exceeded string execution depth");
	else {
		while (i-- > 0) {
			src_free();
			bmachine.readsp--;
		}
	}
}

static void
skipN(void)
{
	struct number	*n;
	u_long		i;

	n = pop_number();
	if (n == NULL)
		return;
	i = get_ulong(n);
	if (i == GMP_NUMB_MASK)
		warnx("J command requires a number >= 0");
	else if (i > 0 && bmachine.readsp < i)
		warnx("J command argument exceeded string execution depth");
	else {
		while (i-- > 0) {
			src_free();
			bmachine.readsp--;
		}
		skip_until_mark();
	}
}

static void
skip_until_mark(void)
{
	for (;;) {
		switch (readch()) {
		case 'M':
			return;
		case EOF:
			errx(1, "mark not found");
			return;
		case 'l':
		case 'L':
		case 's':
		case 'S':
		case ':':
		case ';':
		case '<':
		case '>':
		case '=':
			(void)readreg();
			if (readch() == 'e')
				(void)readreg();
			else
				unreadch();
			break;
		case '[':
			free(read_string(&bmachine.readstack[bmachine.readsp]));
			break;
		case '!':
			switch (readch()) {
				case '<':
				case '>':
				case '=':
					(void)readreg();
					if (readch() == 'e')
						(void)readreg();
					else
						unreadch();
					break;
				default:
					free(readline());
					break;
			}
			break;
		default:
			break;
		}
	}
}

static void
parse_number(void)
{
	unreadch();
	push_number(readnumber(&bmachine.readstack[bmachine.readsp],
	    bmachine.ibase));
}

static void
unknown(void)
{
	int ch = bmachine.readstack[bmachine.readsp].lastchar;
	warnx("%c (0%o) is unimplemented", ch, ch);
}

static void
eval_string(char *p)
{
	int ch;

	if (bmachine.readsp > 0) {
		/* Check for tail call. Do not recurse in that case. */
		ch = readch();
		if (ch == EOF) {
			src_free();
			src_setstring(&bmachine.readstack[bmachine.readsp], p);
			return;
		} else
			unreadch();
	}
	if (bmachine.readsp == bmachine.readstack_sz - 1) {
		size_t newsz = bmachine.readstack_sz * 2;
		struct source *stack;
		stack = reallocarray(bmachine.readstack, newsz,
		    sizeof(struct source));
		if (stack == NULL)
			err(1, "recursion too deep");
		bmachine.readstack_sz = newsz;
		bmachine.readstack = stack;
	}
	src_setstring(&bmachine.readstack[++bmachine.readsp], p);
}

static void
eval_line(void)
{
	/* Always read from stdin */
	struct source	in;
	char		*p;

	clearerr(stdin);
	src_setstream(&in, stdin);
	p = (*in.vtable->readline)(&in);
	eval_string(p);
}

static void
eval_tos(void)
{
	char *p;

	p = pop_string();
	if (p != NULL)
		eval_string(p);
}

void
eval(void)
{
	int	ch;

	for (;;) {
		ch = readch();
		if (ch == EOF) {
			if (bmachine.readsp == 0)
				return;
			src_free();
			bmachine.readsp--;
			continue;
		}
		if (bmachine.interrupted) {
			if (bmachine.readsp > 0) {
				src_free();
				bmachine.readsp--;
				continue;
			} else
				bmachine.interrupted = false;
		}
#ifdef DEBUGGING
		(void)fprintf(stderr, "# %c\n", ch);
		stack_print(stderr, &bmachine.stack, "* ",
		    bmachine.obase);
		(void)fprintf(stderr, "%zd =>\n", bmachine.readsp);
#endif

		if (0 <= ch && ch < nitems(jump_table))
			(*jump_table[ch])();
		else
			unknown();

#ifdef DEBUGGING
		stack_print(stderr, &bmachine.stack, "* ",
		    bmachine.obase);
		(void)fprintf(stderr, "%zd ==\n", bmachine.readsp);
#endif
	}
}
