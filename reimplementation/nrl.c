// license:All rights Reserved (for now, contact about licensing if you need it)
// copyright-holders:Jonathan Gevaryahu
// Reimplementation of the Naval Research Laboratory's Text to Phoneme ruleset parser.
// Preliminary version using NRL ruleset, this is very incomplete.
// Copyright (C)2021-2024 Jonathan Gevaryahu
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <uchar.h>
#include <ctype.h>
#include <string.h>

// basic typedefs
typedef int8_t s8;
typedef uint8_t u8;
typedef int16_t s16;
typedef uint16_t u16;
typedef int32_t s32;
typedef uint32_t u32;
typedef int64_t s64;
typedef uint64_t u64;

// magic numbers
#define RULES_TOTAL 28
#define RULES_PUNCT 0
#define RULES_NUMBERS 27

// verbose macro
#define e_printf(v, ...) \
	do { if (v) { fprintf(stderr, __VA_ARGS__); fflush(stderr); } } while (0)

// verbosity defines; V_DEBUG can be changed here to enable/disable debug messages
#define V_DEBUG (1)
#define V_0 (c.verbose & (1<<0))
#define V_1 (c.verbose & (1<<1))

// 'global' struct
typedef struct s_cfg
{
	//const char* const letters;
	u32 verbose;
} s_cfg;

// 'vector' structs for holding data
/*
typedef struct vec_u8
{
	u32 elements; // number of elements in the vector, defaults to zero/empty
	u32 capacity; // amount of element-sized memory blocks currently allocated for the vector; i.e. capacity
	u8* data;
} vec_u8;*/

typedef struct vec_char32
{
	u32 elements; // number of elements in the vector, defaults to zero/empty
	u32 capacity; // amount of element-sized memory blocks currently allocated for the vector; i.e. capacity
	char32_t* data;
} vec_char32;

vec_char32* vec_char32_alloc(u32 init_len)
{
	// allocate and initialize the vector
	vec_char32 *r = malloc(sizeof(vec_char32));
	r->elements = 0;
	r->capacity = 0;
	// allocate the data pointer, and since the data is a direct type, this allocation contains the data itself
	r->data = malloc(init_len * sizeof(char32_t));
	// fill in the capacity; if malloc failed (or init_len was zero), capacity remains 0 and the data pointer is NULL
	if (r->data) r->capacity = init_len;
	return r;
}

void vec_char32_free(vec_char32* l)
{
	//free the structs that the data pointer points to, sequentially. (not necessary with this structure)
	//for (int i = 0; i < l->capacity; i++)
	//free(l->data[i]);
	// free the data pointer itself
	free(l->data);
	l->data = NULL;
	l->capacity = 0;
	l->elements = 0;
	// free the actual structure
	free(l);
}

void vec_char32_resize(vec_char32* l, u32 capacity)
{
	char32_t* new_data = realloc(l->data, sizeof(l->data[0]) * capacity);
	if (new_data) // make sure it actually allocated...
	{
		l->capacity = capacity; // update to the new capacity
		l->data = new_data; // update the stale pointer to the new data
	}
}

void vec_char32_append(vec_char32* l, char32_t a)
{
	// if current vector capacity is insufficient to have another element added to it, reallocate it to twice its current capacity
	if (l->elements == l->capacity)
	{
		u32 old_capacity = l->capacity;
		u64 new_capacity = l->capacity<<1;
		if ((new_capacity > ((u32)~0)) && (old_capacity <= ((u32)~0))) new_capacity = ((u32)~0); // if we would have exceeded a u32 but there's still headroom, realloc to max possible u32 size
		vec_char32_resize(l, new_capacity);
		if (l->capacity == old_capacity) return; // unable to resize properly, just bail out instead of doing bad things
	}
	// stick the new element on the end of the list and update the number of elements
	l->data[l->elements] = a;
	l->elements++;
}

void vec_char32_dbg_stats(vec_char32* l)
{
	e_printf(V_DEBUG,"DEBUG: vec_char32 capacity: %d, elements: %d\n", l->capacity, l->elements);
}

void vec_char32_dbg_print(vec_char32* l)
{
	e_printf(V_DEBUG,"DEBUG: vec_char32 contents: '");
	for (u32 i=0; i < l->elements; i++)
	{
		e_printf(V_DEBUG,"%c", (char)l->data[i]);
	}
	e_printf(V_DEBUG,"'\n");
}

// ruleset struct to point to all the rulesets for each letter/punct/etc
typedef struct sym_ruleset
{
	u32 num_rules;
	//u32* const * ruleLen;
	const char* const * rule;
} sym_ruleset;

// 'vector' struct for holding a list of strings
/*typedef struct vec_strs
{
	u32 elements; // number of elements in the vector, defaults to zero/empty
	u32 capacity; // amount of element-sized memory blocks currently allocated for the vector; i.e. capacity
	vec_u8* data;
} vec_strs;*/

bool isIllegalPunct(char32_t in)
{
	switch(in)
	{
		// []\/
		case '[': case ']': case '\\': case '/':
			return true;
		default:
			return false;
	}
	return false;
}

bool isPunct(char32_t in)
{
	switch(in)
	{
		//  ,.?;:+*"$%&-<>!()=
		case ' ': case ',': case '.': case '?': case ';': case ':': case '+': case '*':
		case '"': case '$': case '%': case '&': case '-': case '<': case '>': case '!':
		case '(': case ')': case '=': case '\'':
			return true;
		default:
			return false;
	}
	return false;
}

bool isPunctNoSpace(char32_t in)
{
	if (in == ' ')
		return false;
	else return isPunct(in);
}

bool isVowel(char32_t in)
{
	switch(in)
	{
		// AEIOUY
		case 'A': case 'E': case 'I': case 'O': case 'U': case 'Y':
		case 'a': case 'e': case 'i': case 'o': case 'u': case 'y':
			return true;
		default:
			return false;
	}
	return false;
}

bool isConsonant(char32_t in)
{
	switch(in)
	{
		// BCDFGHJKLMNPQRSTVWXZ
		case 'B': case 'C': case 'D': case 'F': case 'G': case 'H': case 'J': case 'K': case 'L': case 'M': case 'N': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'V': case 'W': case 'X': case 'Z':
		case 'b': case 'c': case 'd': case 'f': case 'g': case 'h': case 'j': case 'k': case 'l': case 'm': case 'n': case 'p': case 'q': case 'r': case 's': case 't': case 'v': case 'w': case 'x': case 'z':
			return true;
		default:
			return false;
	}
	return false;
}

bool isVoiced(char32_t in)
{
	switch(in)
	{
		// BDVGJLMNRWZ
		case 'B': case 'D': case 'V': case 'G': case 'J': case 'L': case 'M': case 'N': case 'R': case 'W': case 'Z':
		case 'b': case 'd': case 'v': case 'g': case 'j': case 'l': case 'm': case 'n': case 'r': case 'w': case 'z':
			return true;
		default:
			return false;
	}
	return false;
}

bool isFront(char32_t in)
{
	switch(in)
	{
		// EIY
		case 'E': case 'I': case 'Y':
		case 'e': case 'i': case 'y':
			return true;
		default:
			return false;
	}
	return false;
}

// preprocess a vec_char32* list into another vec_char32* list starting at a given offset, return the final offset+1
u32 preprocess(vec_char32* in, vec_char32* out, u32 in_offset)
{
	// prepend a space to output
	vec_char32_append(out, ' ');
	// iterate over input
	u32 i;
	for (i = in_offset; i < in->elements; i++)
	{
		if (in->data[i] == '#') // early return: end marker
		{
			return i+1;
		}
		else if (isIllegalPunct(in->data[i]))
		{
			//e_printf(V_DEBUG,"got illegal punctuation of '%c'\n",in->data[i]);
			continue; // skip illegal punctuation characters
		}
		else if (isPunctNoSpace(in->data[i])) // special case for punctuation
		{
			//e_printf(V_DEBUG,"got non-space punctuation of '%c'\n",in->data[i]);
			vec_char32_append(out, ' ');
			vec_char32_append(out, in->data[i]);
			vec_char32_append(out, ' ');
		}
		else if (in->data[i] == ' ') // special case for space, make sure we do not append successive spaces
		{
			//e_printf(V_DEBUG,"got space punctuation of '%c'\n",in->data[i]);
			if ((out->elements > 0) && (out->data[out->elements-1] != ' '))
			{
				vec_char32_append(out, in->data[i]);
			}
		}
		else if (isalpha(in->data[i]) || isdigit(in->data[i]))
		{
			//e_printf(V_DEBUG,"got alphanumeric of '%c'\n",in->data[i]);
			vec_char32_append(out, toupper(in->data[i]));
		}
		else e_printf(V_DEBUG,"Unknown character 0x%x in input stream\n", in->data[i]);
	}
	// reached end of input, i.e. an implicit '#'
	return i; // we incremented past the end of input, so just return i
}

u32 getRuleNum(char32_t input)
{
	if (isdigit(input))
	{
		return RULES_NUMBERS;
	}
	else if (isalpha(input))
	{
		return input - 0x40;
	}
	else
	{
		return RULES_PUNCT;
	}
}

// returns the offset of the first instance of a character found in a string
// starting from the left. otherwise return -1.
s32 strnfind(const char *src, int c, size_t n)
{
	for (int i = 0; i < n; i++)
	{
		if (src[i] == c) return i;
	}
	return -1;
}

// symbols
/*
*       # = 1 OR MORE VOWELS
*       * = 1 OR MORE CONSONANTS
*       . = A VOICED CONSONANT
*       $ = SINGLE CONSONANT FOLLOWED BY AN 'I' OR 'E'
*       % = SUFFIX SUCH AS 'E','ES','ED','ER','ING','ELY'
*       & = A SIBILANT
*       @ = A CONSONANT AFTER WHICH LONG 'U' IS PRONOUNCED
*               AS IN 'RULE', NOT 'MULE' (a nonpalate)
*       ^ = A SINGLE CONSONANT
*       + = A FRONT VOWEL: 'E','I','Y'
*       : = 0 OR MORE CONSONANTS
*/
#define VOWEL1M '#'
#define CONS1M '*'
#define VOICED '.'
#define CONS1IE '$'
#define SUFFIX '%'
#define SIBIL '&'
#define NONPAL '@'
#define CONS1 '^'
#define FRONT '+'
#define CONS0M ':'

bool matchRule(const char const rule, const char32_t const input)
{

}

bool parseLeft(const char* const rule, const vec_char32* const input, const u32 rpinit, const u32 inpos)
{
	// rule[rulepos] points to the rule symbol being evaluated
	// input->data[inpos] points to the input symbol being evaluated
	s32 rulepos = rpinit;
	// the leftmost valid input character is technically inpos of 0
	while (rulepos > 0)
	{
		bool rulepass = false;
		switch(rule[rulepos])
		{
			case VOWEL1M: // 1 or more vowels
				if (isVowel(input->data[inpos]))
				{
					rulepass = true;
					// we got at least one vowel, call this function repeatedly in a loop
					// consuming more vowels from input-1 until either input-1 becomes -1 OR
					// we hit a non-vowel, then terminate and leave inpos at that failing position
				}
			case CONS1:
				break; // TODO
			
		}
	}
	return false;
}

bool parseRight()
{
	return false;
}

bool parseRule(const char* const rule, const vec_char32* const input, const u32 inpos)
{
	// find left end
	s32 left = strnfind(rule, '[', ruleLen);
	if (left != 0)
	{
		// match left part of rule
		// early out: if the rule is not ':' which is 'zero or more' of a symbol
		//   AND this is the leftmost character of the input string, just die immediately
		//   and fail the rule, otherwise continue
		if ((rule[left-1] != CONS0) && (inpos == 0))
		{
			return false;
		}
		// call parseLeft which recursively calls itself and returns true if the rule matches and false if it doesn't.
		if (!parseLeft(rule, input, left-1, inpos))
		{
			return false;
		}
	}
	e_printf(V_DEBUG,"Left half of rule %s matched input string %s at offset %d\n", rule, input, inpos);
	// find right end
	s32 right = strnfind(rule, ']', ruleLen);
	if (rule[right+1] != '=')
	{
		// call parseRight which recursively calls itself and returns true if the rule matches and false if it doesn't.
		if (!parseright(rule, input, right+1, inpos))
		{
			return false;
		}
	}
	e_printf(V_DEBUG,"Right half of rule %s matched input string %s at offset %d\n", rule, input, inpos);
	return true;
}

s32 applyRule(const char* const rule)
{
	u32 ruleLen = strlen(rule);
	// search through the rule string for the [
	s32 first = strnfind(rule, '[', ruleLen);
	if (first == -1) return -1; // early out for invalid rule
	// search through the rule string for the ]
	s32 last = strnfind(rule, ']', ruleLen);
	// return the number of characters between those two.
	return (last-first)-1;
}

s32 processLetter(const sym_ruleset* const ruleset, const vec_char32* const input, const u32 inpos)
{
	// find ruleset for this letter/punct/etc
	u32 rulenum = getRuleNum(input->data[inpos]);
	// iterate over every one of these rules and halt on the first match
	for (u32 i = 0; i < ruleset[rulenum].num_rules; i++)
	{
		e_printf(V_DEBUG, "found a rule %s\n", ruleset[rulenum].rule[i]);
		if (applyRule(ruleset[rulenum].rule[i]) < 0)
		{
			e_printf(V_DEBUG,"ERROR: encountered an invalid rule for character at position %d (%c)!\n", inpos, input->data[inpos]);
			exit(1);
		}
		//e_printf(V_DEBUG, "this rule would consume %d characters\n", applyRule(ruleset[rulenum].rule[i]));
		if (parseRule(ruleset[rulenum].rule[i], input, inpos))
		{
			return applyRule(ruleset[rulenum].rule[i]);
		}
	}
	return 0;
}

void processPhrase(const sym_ruleset* const ruleset, const vec_char32* const input)
{
	u32 curpos = 0;
	e_printf(V_DEBUG, "processPhrase called, phrase has %d elements\n", input->elements);
	while (curpos <= input->elements)
	{
		e_printf(V_DEBUG, "position is now %d (%c)\n", curpos, input->data[curpos]);
		u32 oldpos = curpos;
		curpos += processLetter(ruleset, input, curpos);
		if (curpos - oldpos == 0)
		{
			e_printf(V_DEBUG,"WARNING: unable to match any rule for position %d (%c)!\n", curpos, input->data[curpos]);
			curpos++;
		}
	}
}

void usage()
{
	printf("Usage: executablename parameters\n");
	printf("Brief explanation of function of executablename\n");
	printf("\n");
}

#define NUM_PARAMETERS 1

int main(int argc, char **argv)
{
	s_cfg c =
	{
		0, // verbose
	};

	//{
		const char* const punctrule_eng[] =
		{
			"[ ]'=/ /",
			"[ - ]=/ /",
			"[ ]=/< >/",
			"[-]=/<->/",
			". [' S]=/Z/",
			"#:.E [' S]=/Z/",
			"# [' S]=/Z/",
			"[' ]=/ /",
			"[,]=/<,>/",
			"[.]=/<.>/",
			"[?]=/<?>/",
		};

		const char* const arule_eng[] =
		{
			"[A] =/AX/",
			" [ARE] =/AA R/",
			" [AR]O=/AX R/",
			"[AR]#=/EH R/",
			" ^[AS]#=/EY S/",
			"[A]WA=/AX/",
			"[AW]=/AO/",
			" :[ANY]=/EH N IY/",
			"[A]^+#=/EY/",
			"#:[ALLY]=/AX L IY/",
			" [AL]#=/AX L/",
			"[AGAIN]=/AX G EH N/",
			"#:[AG]E=/IH JH/",
			"[A]^+:#=/AE/",
			" :[A]^+ =/EY/",
			"[A]^%=/EY/",
			" [ARR]=/AX R/",
			"[ARR]=/AE R/",
			" :[AR] =/AA R/",
			"[AR] =/ER/",
			"[AR]=/AA R/",
			"[AIR]=/EH R/",
			"[AI]=/EY/",
			"[AY]=/EY/",
			"[AU]=/AO/",
			"#:[AL] =/AX L/",
			"#:[ALS] =/AX L Z/",
			"[ALK]=/AO K/",
			"[AL]^=/AO L/",
			" :[ABLE]=/EY B AX L/",
			"[ABLE]=/AX B AX L/",
			"[ANG]+=/EY N JH/",
			"[A]=/AE/",
		};

		const char* const brule_eng[] =
		{
			" [BE]^#=/B IH/",
			"[BEING]=/B IY IH NX/",
			" [BOTH] =/B OW TH/",
			" [BUS]#=/B IH Z/",
			"[BUIL]=/B IH L/",
			"[B]=/B/",
		};

		const char* const crule_eng[] =
		{
			" [CH]^=/K/",
			"^E[CH]=/K/",
			"[CH]=/CH/",
			" S[CI]#=/S AY/",
			"[CI]A=/SH/",
			"[CI]O=/SH/",
			"[CI]EN=/SH/",
			"[C]+=/S/",
			"[CK]=/K/",
			"[COM]%=/K AH M/",
			"[C]=/K/",
		};

		const char* const drule_eng[] =
		{
			"#:[DED] =/D IH D/",
			".E[D] =/D/",
			"#^:E[D] =/T/",
			" [DE]^#=/D IH/",
			" [DO] =/D UW/",
			" [DOES]=/D AH Z/",
			" [DOING]=/D UW IH NX/",
			" [DOW]=/D AW/",
			"[DU]A=/JH UW/",
			"[D]=/D/",
		};

		const char* const erule_eng[] =
		{
			"#:[E] =/ /",
			"' ^:[E] =/ /",
			" :[E] =/IY/",
			"#[ED] =/D/",
			"#:[E]D =/ /",
			"[EV]ER=/EH V/",
			"[E]^%=/IY/",
			"[ERI]#=/IY R IY/",
			"[ERI]=/EH R IH/",
			"#:[ER]#=/ER/",
			"[ER]#=/EH R/",
			"[ER]=/ER/",
			" [EVEN]=/IY V EH N/",
			"#:[E]W=/ /",
			"@[EW]=/UW/",
			"[EW]=/Y UW/",
			"[E]O=/IY/",
			"#:&[ES] =/IH Z/",
			"#:[E]S =/ /",
			"#:[ELY] =/L IY/",
			"#:[EMENT]=/M EH N T/",
			"[EFUL]=/F UH L/",
			"[EE]=/IY/",
			"[EARN]=/ER N/",
			" [EAR]^=/ER/",
			"[EAD]=/EH D/",
			"#:[EA] =/IY AX/",
			"[EA]SU=/EH/",
			"[EA]=/IY/",
			"[EIGH]=/EY/",
			"[EI]=/IY/",
			" [EYE]=/AY/",
			"[EY]=/IY/",
			"[EU]=/Y UW/",
			"[E]=/EH/",
		};

		const char* const frule_eng[] =
		{
			"[FUL]=/F UH L/",
			"[F]=/F/",
		};

		const char* const grule_eng[] =
		{
			"[GIV]=/G IH V/",
			" [G]I^=/G/",
			"[GE]T=/G EH/",
			"SU[GGES]=/G JH EH S/",
			"[GG]=/G/",
			" B#[G]=/G/",
			"[G]+=/JH/",
			"[GREAT]=/G R EY T/",
			"#[GH]=/ /",
			"[G]=/G/",
		};

		const char* const hrule_eng[] =
		{
			" [HAV]=/HH AE V/",
			" [HERE]=/HH IY R/",
			" [HOUR]=/AW ER/",
			"[HOW]=/HH AW/",
			"[H]#=/HH/",
			"[H]=/ /",
		};

		const char* const irule_eng[] =
		{
			" [IN]=/IH N/",
			" [I] =/AY/",
			"[IN]D=/AY N/",
			"[IER]=/IY ER/",
			"#:R[IED] =/IY D/",
			"[IED] =/AY D/",
			"[IEN]=/IY EH N/",
			"[IE]T=/AY EH/",
			" :[I]%=/AY/",
			"[I]%=/IY/",
			"[IE]=/IY/",
			"[I]^+:#=/IH/",
			"[IR]#=/AY R/",
			"[IZ]%=/AY Z/",
			"[IS]%=/AY Z/",
			"[I]D%=/AY/",
			"+^[I]^+=/IH/",
			"[I]T%=/AY/",
			"#^:[I]^+=/IH/",
			"[I]^+=/AY/",
			"[IR]=/ER/",
			"[IGH]=/AY/",
			"[ILD]=/AY L D/",
			"[IGN] =/AY N/",
			"[IGN]^=/AY N/",
			"[IGN]%=/AY N/",
			"[IQUE]=/IY K/",
			"[I]=/IH/",
		};

		const char* const jrule_eng[] =
		{
			"[J]=/JH/",
		};

		const char* const krule_eng[] =
		{
			" [K]N=/ /",
			"[K]=/K/",
		};

		const char* const lrule_eng[] =
		{
			"[LO]C#=/L OW/",
			"L[L]=/ /",
			"#^:[L]%=/AX L/",
			"[LEAD]=/L IY D/",
			"[L]=/L/",
		};

		const char* const mrule_eng[] =
		{
			"[MOV]=/M UW V/",
			"[M]=/M/",
		};

		const char* const nrule_eng[] =
		{
			"E[NG]+=/N JH/",
			"[NG]R=/NX G/",
			"[NG]#=/NX G/",
			"[NGL]%=/NX G AX L/",
			"[NG]=/NX/",
			"[NK]=/NX K/",
			" [NOW] =/N AW/",
			"[N]=/N/",
		};

		const char* const orule_eng[] =
		{
			"[OF] =/AX V/",
			"[OROUGH]=/ER OW/",
			"#:[OR] =/ER/",
			"#:[ORS] =/ER Z/",
			"[OR]=/AO R/",
			" [ONE]=/W AH N/",
			"[OW]=/OW/",
			" [OVER]=/OW V ER/",
			"[OV]=/AH V/",
			"[O]^%=/OW/",
			"[O]^EN=/OW/",
			"[O]^I#=/OW/",
			"[OL]D=/OW L/",
			"[OUGHT]=/AO T/",
			"[OUGH]=/AH F/",
			" [OU]=/AW/",
			"H[OU]S#=/AW/",
			"[OUS]=/AX S/",
			"[OUR]=/AO R/",
			"[OULD]=/UH D/",
			"^[OU]^L=/AH/",
			"[OUP]=/UW P/",
			"[OU]=/AW/",
			"[OY]=/OY/",
			"[OING]=/OW IH NX/",
			"[OI]=/OY/",
			"[OOR]=/AO R/",
			"[OOK]=/UH K/",
			"[OOD]=/UH D/",
			"[OO]=/UW/",
			"[O]E=/OW/",
			"[O] =/OW/",
			"[OA]=/OW/",
			" [ONLY]=/OW N L IY/",
			" [ONCE]=/W AH N S/",
			"[ON ' T]=/OW N T/",
			"C[O]N=/AA/",
			"[O]NG=/AO/",
			" ^:[O]N=/AH/",
			"I[ON]=/AX N/",
			"#:[ON] =/AX N/",
			"#^[ON]=/AX N/",
			"[O]ST =/OW/",
			"[OF]^=/AO F/",
			"[OTHER]=/AH DH ER/",
			"[OSS] =/AO S/",
			"#^:[OM]=/AH M/",
			"[O]=/AA/",
		};

		const char* const prule_eng[] =
		{
			"[PH]=/F/",
			"[PEOP]=/P IY P/",
			"[POW]=/P AW/",
			"[PUT] =/P UH T/",
			"[P]=/P/",
		};

		const char* const qrule_eng[] =
		{
			"[QUAR]=/K W AO R/",
			"[QU]=/K W/",
			"[Q]=/K/",
		};

		const char* const rrule_eng[] =
		{
			" [RE]^#=/R IY/",
			"[R]=/R/",
		};

		const char* const srule_eng[] =
		{
			"[SH]=/SH/",
			"#[SION]=/ZH AX N/",
			"[SOME]=/S AH M/",
			"#[SUR]#=/ZH ER/",
			"[SUR]#=/SH ER/",
			"#[SU]#=/ZH UW/",
			"#[SSU]#=/SH UW/",
			"#[SED] =/Z D/",
			"#[S]#=/Z/",
			"[SAID]=/S EH D/",
			"^[SION]=/SH AX N/",
			"[S]S=/ /",
			".[S] =/Z/",
			"#:.E[S] =/Z/",
			"#^:##[S] =/Z/",
			"#^:#[S] =/S/",
			"U[S] =/S/",
			" :#[S] =/Z/",
			" [SCH]=/S K/",
			"[S]C+=/ /",
			"#[SM]=/Z M/",
			"#[SN] '=/Z AX N/",
			"[S]=/S/",
		};

		const char* const trule_eng[] =
		{
			" [THE] =/DH AX/",
			"[TO] =/T UW/",
			"[THAT] =/DH AE T/",
			" [THIS] =/DH IH S/",
			" [THEY]=/DH EY/",
			" [THERE]=/DH EH R/",
			"[THER]=/DH ER/",
			"[THEIR]=/DH EH R/",
			" [THAN] =/DH AE N/",
			" [THEM] =/DH EH M/",
			"[THESE] =/DH IY Z/",
			" [THEN]=/DH EH N/",
			"[THROUGH]=/TH R UW/",
			"[THOSE]=/DH OW Z/",
			"[THOUGH] =/DH OW/",
			" [THUS]=/DH AH S/",
			"[TH]=/TH/",
			"#:[TED] =/T IH D/",
			"S[TI]#N=/CH/",
			"[TI]O=/SH/",
			"[TI]A=/SH/",
			"[TIEN]=/SH AX N/",
			"[TUR]#=/CH ER/",
			"[TU]A=/CH UW/",
			" [TWO]=/T UW/",
			"[T]=/T/",
		};

		const char* const urule_eng[] =
		{
			" [UN]I=/Y UW N/",
			" [UN]=/AH N/",
			" [UPON]=/AX P AO N/",
			"@[UR]#=/UH R/",
			"[UR]#=/Y UH R/",
			"[UR]=/ER/",
			"[U]^ =/AH/",
			"[U]^^=/AH/",
			"[UY]=/AY/",
			" G[U]#=/ /",
			"G[U]%=/ /",
			"G[U]#=/W/",
			"#N[U]=/Y UW/",
			"@[U]=/UW/",
			"[U]=/Y UW/",
		};

		const char* const vrule_eng[] =
		{
			"[VIEW]=/V Y UW/",
			"[V]=/V/",
		};

		const char* const wrule_eng[] =
		{
			" [WERE]=/W ER/",
			"[WA]S=/W AA/",
			"[WA]T=/W AA/",
			"[WHERE]=/WH EH R/",
			"[WHAT]=/WH AA T/",
			"[WHOL]=/HH OW L/",
			"[WHO]=/HH UW/",
			"[WH]=/WH/",
			"[WAR]=/W AO R/",
			"[WOR]^=/W ER/",
			"[WR]=/R/",
			"[W]=/W/",
		};

		const char* const xrule_eng[] =
		{
			"[X]=/K S/",
		};

		const char* const yrule_eng[] =
		{
			"[YOUNG]=/Y AH NX/",
			" [YOU]=/Y UW/",
			" [YES]=/Y EH S/",
			" [Y]=/Y/",
			"#^:[Y] =/IY/",
			"#^:[Y]I=/IY/",
			" :[Y] =/AY/",
			" :[Y]#=/AY/",
			" :[Y]^+:#=/IH/",
			" :[Y]^#=/AY/",
			"[Y]=/IH/",
		};

		const char* const zrule_eng[] =
		{
			"[Z]=/Z/",
		};

		const char* const numberrule_eng[] =
		{
			"[0]=/Z IH R OW/",
			"[1]=/W AH N/",
			"[2]=/T UW/",
			"[3]=/TH R IY/",
			"[4]=/F OW R/",
			"[5]=/F AY V/",
			"[6]=/S IH K S/",
			"[7]=/S EH V AX N/",
			"[8]=/EY T/",
			"[9]=/N AY N/",
		};
		sym_ruleset ruleset[RULES_TOTAL] =
		{
			{ sizeof(punctrule_eng)/sizeof(*punctrule_eng), punctrule_eng },
			{ sizeof(arule_eng)/sizeof(*arule_eng), arule_eng },
			{ sizeof(brule_eng)/sizeof(*brule_eng), brule_eng },
			{ sizeof(crule_eng)/sizeof(*crule_eng), crule_eng },
			{ sizeof(drule_eng)/sizeof(*drule_eng), drule_eng },
			{ sizeof(erule_eng)/sizeof(*erule_eng), erule_eng },
			{ sizeof(frule_eng)/sizeof(*frule_eng), frule_eng },
			{ sizeof(grule_eng)/sizeof(*grule_eng), grule_eng },
			{ sizeof(hrule_eng)/sizeof(*hrule_eng), hrule_eng },
			{ sizeof(irule_eng)/sizeof(*irule_eng), irule_eng },
			{ sizeof(jrule_eng)/sizeof(*jrule_eng), jrule_eng },
			{ sizeof(krule_eng)/sizeof(*krule_eng), krule_eng },
			{ sizeof(lrule_eng)/sizeof(*lrule_eng), lrule_eng },
			{ sizeof(mrule_eng)/sizeof(*mrule_eng), mrule_eng },
			{ sizeof(nrule_eng)/sizeof(*nrule_eng), nrule_eng },
			{ sizeof(orule_eng)/sizeof(*orule_eng), orule_eng },
			{ sizeof(prule_eng)/sizeof(*prule_eng), prule_eng },
			{ sizeof(qrule_eng)/sizeof(*qrule_eng), qrule_eng },
			{ sizeof(rrule_eng)/sizeof(*rrule_eng), rrule_eng },
			{ sizeof(srule_eng)/sizeof(*srule_eng), srule_eng },
			{ sizeof(trule_eng)/sizeof(*trule_eng), trule_eng },
			{ sizeof(urule_eng)/sizeof(*urule_eng), urule_eng },
			{ sizeof(vrule_eng)/sizeof(*vrule_eng), vrule_eng },
			{ sizeof(wrule_eng)/sizeof(*wrule_eng), wrule_eng },
			{ sizeof(xrule_eng)/sizeof(*xrule_eng), xrule_eng },
			{ sizeof(yrule_eng)/sizeof(*yrule_eng), yrule_eng },
			{ sizeof(zrule_eng)/sizeof(*zrule_eng), zrule_eng },
			{ sizeof(numberrule_eng)/sizeof(*numberrule_eng), numberrule_eng },
		};
	//}
	if (argc != NUM_PARAMETERS+1)
	{
		fprintf(stderr,"E* Incorrect number of parameters!\n"); fflush(stderr);
		usage();
		return 1;
	}

// input file
	FILE *in = fopen(argv[1], "rb");
	if (!in)
	{
		fprintf(stderr,"E* Unable to open input file %s!\n", argv[1]); fflush(stderr);
		return 1;
	}

	fseek(in, 0, SEEK_END);
	uint32_t len = ftell(in);
	rewind(in); //fseek(in, 0, SEEK_SET);

	uint8_t *dataArray = (uint8_t *) malloc((len) * sizeof(uint8_t));
	if (dataArray == NULL)
	{
		fprintf(stderr,"E* Failure to allocate memory for array of size %d, aborting!\n", len); fflush(stderr);
		fclose(in);
		return 1;
	}

	{ // scope limiter for temp
		uint32_t temp = fread(dataArray, sizeof(uint8_t), len, in);
		fclose(in);
		if (temp != len)
		{
			fprintf(stderr,"E* Error reading in %d elements, only read in %d, aborting!\n", len, temp); fflush(stderr);
			free(dataArray);
			dataArray = NULL;
			return 1;
		}
		fprintf(stderr,"D* Successfully read in %d bytes\n", temp); fflush(stderr);
	}

/*
// prepare output file
	FILE *out = fopen(argv[2], "wb");
	if (!out)
	{
		fprintf(stderr,"E* Unable to open output file %s!\n", argv[2]);
		free(dataArray);
		dataArray = NULL;
		return 1;
	}
	fflush(stderr);
*/

	// actual program goes here

	// allocate a vector
	vec_char32* d_in = vec_char32_alloc(4);

	// read contents of dataArray into vec_char32
	for (u32 i = 0; i < len; i++)
	{
		vec_char32_append(d_in,dataArray[i]);
	}
	e_printf(V_DEBUG,"Input phrase stats are:\n");
	vec_char32_dbg_stats(d_in);
	vec_char32_dbg_print(d_in);

	// free the input array
	free(dataArray);
	dataArray = NULL;

	// we may have multiple phrases in the input file, so handle each one here sequentially.
	bool done = false;
	u32 phrase_offset = 0;
	while (!done)
	{
		// allocate another vector for preprocessing
		vec_char32* d_pre = vec_char32_alloc(4);
		// preprocess d_in into d_pre
		phrase_offset = preprocess(d_in, d_pre, phrase_offset);
		e_printf(V_DEBUG,"Preprocessing done, stats are now:\n");
		vec_char32_dbg_stats(d_pre);
		vec_char32_dbg_print(d_pre);
		e_printf(V_DEBUG,"Input phrase offset is now %d\n", phrase_offset);

		// do stuff with preprocessed phrase here
		// i.e. the rest of the owl
		processPhrase(ruleset, d_pre);

		vec_char32_free(d_pre);
		if (phrase_offset >= d_in->elements)
		{
			done = true;
		}
		// HACK: for now, just end after the first phrase; later we need to make sure CR/LF etc get nuked properly;
		done = true;
	}
	vec_char32_free(d_in);

	//fprintf(stdout,"trying to print size of arule array, should be 33\n");
	//fprintf(stdout,"sizeof(arule_eng): %d\n", sizeof(arule_eng));
	//fprintf(stdout,"(&arule_eng)[1] - arule_eng: %d\n", (&arule_eng)[1] - arule_eng);
	//fprintf(stdout,"sizeof(arule_eng)/sizeof(*arule_eng): %d\n", sizeof(arule_eng)/sizeof(*arule_eng));
	//fflush(stdout);
	//fprintf(stdout,"trying to print size of arule_test array, should be 33\n");
	//fprintf(stdout,"sizeof(&tst.arule_test): %d\n", sizeof(&tst.arule_test));
	//fprintf(stdout,"(&tst.arule_test)[1] - tst.arule_test: %d\n", (&tst.arule_test)[1] - tst.arule_test);
	//fprintf(stdout,"sizeof(tst.arule_test)/sizeof(*tst.arule_test): %d\n", sizeof(tst.arule_test)/sizeof(*tst.arule_test));
	//fprintf(stdout,"%d",ruleset[1].num_rules);
	//fflush(stdout);


//	fclose(out);

	return 0;
}
