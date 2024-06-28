// license:All rights Reserved (for now, contact about licensing if you need it)
// copyright-holders:Jonathan Gevaryahu
// Reimplementation of the Don't Ask Computer Software/Softvoice 'reciter'/'translator' engine
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
#define RULES_TOTAL 27
#define RULES_PUNCT_DIGIT 26
#define RECITER_END_CHAR 0x1b

#define SUPPORT_CONS1M 1
#define SUPPORT_CONS1EI 1
#undef NRL_VOWEL
#undef ORIGINAL_BUGS
#define C64_ADDED_RULES 1
#undef C64_RULES_BUGS
#define APPLE_RULES_VARIANT 1

// verbose macros
#define e_printf(v, ...) \
	do { if (v) { fprintf(stderr, __VA_ARGS__); fflush(stderr); } } while (0)
#define o_printf(v, ...) \
	do { if (v) { fprintf(stdout, __VA_ARGS__); fflush(stdout); } } while (0)

// verbosity defines; V_DEBUG can be changed here to enable/disable debug messages
#define V_DEBUG (1)
#define V_ERR (1)
#define V_PARAM    (c.verbose & (1<<0))
#define V_PARSE    (c.verbose & (1<<1))
#define V_MAINLOOP (c.verbose & (1<<2))
#define V_SEARCH   (c.verbose & (1<<3))
#define V_SEARCH2  (c.verbose & (1<<4))
#define V_RULES    (c.verbose & (1<<5))
#define V_ERULES   (c.verbose & (1<<6))

// 'vector' structs for holding data

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
	e_printf(V_DEBUG,"vec_char32 capacity: %d, elements: %d\n", l->capacity, l->elements);
}

void vec_char32_dbg_print(vec_char32* l)
{
	e_printf(V_DEBUG,"vec_char32 contents: '");
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

// Digits, 0-9
#define A_DIGIT 0x01
// Punctuation characters that do not end a word "!\"#$%\'*+,-./0123456789:;<=>?@^"
// Note the punctuation characters that DO end a word or otherwise have special handling are " ()[\]_"
#define A_PUNCT 0x02
// @ Unvoiced Affricate, aka Non-Palate or 'NONPAL' "DJLNRSTZ" plus "CH", "SH", and "TH"
#define A_UAFF 0x04
// . Voiced consonants "BDGJLMNRVWZ"
#define A_VOICED 0x08
// & Sibilants "CGJSXZ" plus "CH" and "SH"
#define A_SIBIL 0x10
// *:^ Consonants "BCDFGHJKLMNPQRSTVWXZ"
#define A_CONS 0x20
// # Vowels "AEIOUY"
#define A_VOWEL 0x40
// Letters "ABCDEFGHIJKLMNOPQRSTUVWXYZ'" - any character with this flag has a rule attached to it, this includes the apostrophe
#define A_LETTER 0x80
// $ Consonant followed by I or E is handled in the code itself
// + Front vowel is handled in the code itself
// % Suffix is handled in the code itself

// 'global' struct
typedef struct s_cfg
{
	const u8 const ascii_features[0x80];
	//sym_ruleset rules[RULES_TOTAL];
	u32 verbose;
} s_cfg;

//NRL isIllegalPunct: "[]\/"
// probably SV equivalent is `return (ascii_features[in&0x7f]==0);`

bool isDigit(char32_t in, s_cfg c)
{
	return c.ascii_features[in&0x7f]&A_DIGIT;
}

bool isPunct(char32_t in, s_cfg c)
{
	// NRL: " ,.?;:+*"$%&-<>!()='"
	// SV: "!\"#$%\'*+,-./0123456789:;<=>?@^"
	return c.ascii_features[in&0x7f]&A_PUNCT;
}

//NRL isPunctNoSpace: return (isPunct(in)&& (in != ' '))

bool isUaff(char32_t in, s_cfg c)
{
	return c.ascii_features[in&0x7f]&A_UAFF;
}

bool isVoiced(char32_t in, s_cfg c)
{
	return c.ascii_features[in&0x7f]&A_VOICED;
}

bool isSibil(char32_t in, s_cfg c)
{
	return c.ascii_features[in&0x7f]&A_SIBIL;
}

bool isCons(char32_t in, s_cfg c)
{
	return c.ascii_features[in&0x7f]&A_CONS;
}

bool isVowel(char32_t in, s_cfg c)
{
	return c.ascii_features[in&0x7f]&A_VOWEL;
}

bool isLetter(char32_t in, s_cfg c)
{
	return c.ascii_features[in&0x7f]&A_LETTER;
}

bool isFront(char32_t in, s_cfg c)
{
	in = toupper(in);
	return ((in == 'E')||(in == 'I')||(in == 'Y'));
}

// preprocess: add a leading space, and turn all characters from lowercase into capital letters.
void preProcess(vec_char32* in, vec_char32* out, s_cfg c)
{
	// prepend a space to output
	vec_char32_append(out, ' ');
	// iterate over input
	for (u32 i = 0; i < in->elements; i++)
	{
		vec_char32_append(out, toupper(in->data[i]));
	}
	// reached end of input, add a terminating character (usually 0x1b, ESC)
	vec_char32_append(out, RECITER_END_CHAR);
}

u32 getRuleNum(char32_t input)
{
	if (isdigit(input))
	{
		return RULES_PUNCT_DIGIT;
	}
	else if (isalpha(input))
	{
		return input - 0x41;
	}
	else
	{
		return RULES_PUNCT_DIGIT;
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

// symbols from NRL paper:
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
// Note that reciter treats # as 'exactly one vowel', not 'one or more'
#define VOWEL1M '#'
#define CONS1M '*'
#define VOICED '.'
#define CONS1EI '$'
#define SUFFIX '%'
#define SIBIL '&'
#define NONPAL '@'
#define CONS1 '^'
#define FRONT '+'
#define CONS0M ':'

#define LPAREN '['
#define RPAREN ']'

s32 processRule(const sym_ruleset const ruleset, const vec_char32* const input, const u32 inpos, vec_char32* output, s_cfg c)
{
	// iterate through the rules
	u32 i = 0;
	for (i = 0; i < ruleset.num_rules; i++)
	{
		e_printf(V_SEARCH, "found a rule %s\n", ruleset.rule[i]);
		// part 1: check the exact match section of the rule, between the parentheses
		// (and get the indexes to the two parentheses and the equals symbol, which will be used in parts 2 and 3)
		s32 lparen_idx = -1;
		s32 rparen_idx = -1;
		s32 equals_idx = -1;
		//u32 rulelen = -1;
		/* slow but safe... */
		/*
		rulelen = strlen(ruleset.rule[i]);
		lparen_idx = strnfind(ruleset.rule[i], LPAREN, rulelen);
		rparen_idx = strnfind(ruleset.rule[i], RPAREN, rulelen);
		equals_idx = strnfind(ruleset.rule[i], '=', rulelen);
		e_printf(V_DEBUG, "  safe: left paren found at %d, right paren found at %d, equals found at %d, rulelen was %d\n", lparen_idx, rparen_idx, equals_idx, rulelen);
		*/
		/* faster but less safe... will run off the end of the rule string if a rule has no equals sign and doesn't end with a NULL '/0'
		 (which should never happen) */
		//lparen_idx = -1;
		//rparen_idx = -1;
		//equals_idx = -1;
		/*for (rparen_idx = 0; ruleset.rule[i][rparen_idx] != RPAREN; rparen_idx++)
		{
			if (ruleset.rule[i][rparen_idx] == LPAREN)
				lparen_idx = rparen_idx;
		}*/
		int j;
		for (j = 0; (ruleset.rule[i][j] != '\0'); j++)
		{
			if (ruleset.rule[i][j] == LPAREN)
				lparen_idx = j;
			if (ruleset.rule[i][j] == RPAREN)
				rparen_idx = j;
			if (ruleset.rule[i][j] == '=')
				equals_idx = j;
		}
		//rulelen = j;
		//e_printf(V_DEBUG, "unsafe: left paren found at %d, right paren found at %d, equals found at %d, rulelen was %d\n", lparen_idx, rparen_idx, equals_idx, j);
		int nbase = (rparen_idx - 1) - lparen_idx; // number of letters in exact match part of the rule
		//e_printf(V_DEBUG, "n calculated to be %d\n", n);

		// part1: compare exact match; basically a slightly customized 'strncmp()'
		{
			int n = nbase;
			int offset = 0; // offset within rule of exact match
			while ( n && (input->data[inpos+offset]) && (input->data[inpos+offset] == ruleset.rule[i][lparen_idx+1+offset]) )
			{
				//e_printf(V_DEBUG, "strncmp - attempting to match %c(%02x) to %c(%02x)\n",input->data[inpos+offset],input->data[inpos+offset],ruleset.rule[i][lparen_idx+1+offset],ruleset.rule[i][lparen_idx+1+offset] );
				offset++;
				n--;
			}
			/*
			for (; n > 0; n--)
			{
				if (!input->data[inpos+offset])
				{
					e_printf(V_DEBUG, "strncmp got null/end of string, bailing out!\n");
					break;
				}
				else
				{
					e_printf(V_DEBUG, "strncmp - attempting to match %c(%02x) to %c(%02x)\n",input->data[inpos+offset],input->data[inpos+offset],ruleset.rule[i][lparen_idx+1+offset],ruleset.rule[i][lparen_idx+1+offset] );
					if (input->data[inpos+offset] == ruleset.rule[i][lparen_idx+1+offset])
					{
						offset++;
					}
					else
					{
						break;
					}
				}
			}
			*/
			//e_printf(V_DEBUG, "attempted strncmp of rule resulted in %d\n",n);
			if (n != 0) continue; // mismatch, go to next rule.
			// if we got here, the fixed part of the rule matched.
			e_printf(V_SEARCH2, "rule %s matched the input string, at rule offset %d\n", ruleset.rule[i], lparen_idx+1);
		}

		// part2: match the rule prefix
		{
			bool fail = false;
			s32 ruleoffset = -1;
			s32 inpoffset = -1;
			int rulechar;
			int inpchar;
			while ((!fail)&&(lparen_idx+ruleoffset >= 0)&&(inpos+inpoffset >= 0))
			{
				rulechar = ruleset.rule[i][lparen_idx+ruleoffset];
				inpchar = input->data[inpos+inpoffset];
				e_printf(V_SEARCH2, "rulechar is %c(%02x) at ruleoffset %d, inpchar is %c(%02x) at inpoffset %d\n", rulechar, rulechar, lparen_idx+ruleoffset, inpchar, inpchar, inpos+inpoffset);
				if (isLetter(rulechar, c)) // letter in rule matches that letter exactly, only.
				{
					// it's a letter, directly compare it to the input character
					if (rulechar == inpchar)
					{
						// match
						ruleoffset--;
						inpoffset--;
					}
					else
					{
						// mismatch
						fail = true;
					}
				}
				else if (rulechar == ' ') // space matches any non-letter
				{
					if (!isLetter(inpchar,c))
					{
						// match
						ruleoffset--;
						inpoffset--;
					}
					else
					{
						// mismatch
						fail = true;
					}
				}
				// The NRL rules have '#' match 'one or more vowels', while the SV rules have '#' match 'exactly one vowel'
				// NRL rules also allow '##' to match 'two or more vowels' which requires look-ahead or a recursive parser
				// which does a DFS or BFS of every potentially matching character. We cheat and look ahead here.
#ifdef NRL_VOWEL
				else if (rulechar == '#') // # matches one or more vowels
				{
					// special check here for the case where the rule has '##' in it
					if ( (lparen_idx+(ruleoffset-1) >= 0) && ( ruleset.rule[i][lparen_idx+(ruleoffset-1)] == '#') ) // '##' case
					{
						e_printf(V_ERULES, "found a prefix rule with the problematic ## case\n");
						// check for two vowels, plus any more.
						if (isVowel(inpchar,c) && (inpos+(inpoffset-1) >= 0) && isVowel(input->data[inpos+(inpoffset-1)],c))
						{
							// match 2 vowels...
							ruleoffset -= 2;
							// yes, we recheck what we already just checked. this avoids a bad bug.
							while ((inpos+(inpoffset-1) >= 0) && isVowel(inpchar,c))
							{
								// match another...
								inpoffset--;
								inpchar = input->data[inpos+inpoffset];
							}
						}
						else
						{
							// mismatch
							fail = true;
						}
					}
					else // '#' case
					{
						if (isVowel(inpchar,c))
						{
							// match one...
							ruleoffset--;
							// yes, we recheck what we already just checked. this avoids a bad bug.
							while ((inpos+(inpoffset-1) >= 0) && isVowel(inpchar,c))
							{
								// match another...
								inpoffset--;
								inpchar = input->data[inpos+inpoffset];
							}
						}
						else
						{
							// mismatch
							fail = true;
						}
					}
				}
#else
				else if (rulechar == '#') // # matches any vowel
				{
					if (isVowel(inpchar,c))
					{
						// match
						ruleoffset--;
						inpoffset--;
					}
					else
					{
						// mismatch
						fail = true;
					}
				}
#endif
				else if (rulechar == '.') // . matches any voiced consonant
				{
					if (isVoiced(inpchar,c))
					{
						// match
						ruleoffset--;
						inpoffset--;
					}
					else
					{
						// mismatch
						fail = true;
					}
				}
				else if (rulechar == '&') // & matches any sibilant; note the special cases for CH and SH
				{
					if (isSibil(inpchar,c))
					{
						// match
						ruleoffset--;
						inpoffset--;
					}
					else if (inpchar == 'H') // could be CH or SH!
					{
						// the beginning of the input array is ALWAYS a space, so since we saw an 'H'
						// we can't be at offset less than 1 here, so it is always safe to index back one more character
						// unlike many other similar tests in the original reciter code, this one actually works.
						inpchar = input->data[inpos+(inpoffset-1)]; // load another char...
						if ((inpchar == 'C') || (inpchar == 'S'))
						{
							// match 2 characters
							ruleoffset--;
							inpoffset -= 2;
						}
						else
						{
							// mismatch
							fail = true;
						}
					}
					else
					{
						// mismatch
						fail = true;
					}
				}
				else if (rulechar == '@') // @ matches any unvoiced affricate aka nonpalate; note special cases for TH, CH, SH
				{
					if (isUaff(inpchar,c))
					{
						// match
						ruleoffset--;
						inpoffset--;
					}
					else if (inpchar == 'H') // could be TH, CH or SH!; NOTE: the original reciter has a bug here and these tests ALWAYS fail!
					{
#ifdef ORIGINAL_BUGS
						// The original code forgets to decrement the pointer and load another character, so it compares
						// the already loaded and compared 'H' against 'T', 'C', and 'S', which will always fail!
						fail = true;
#else
						// the beginning of the input array is ALWAYS a space, so since we saw an 'H'
						// we can't be at offset less than 1 here, so it is always safe to index back one more character
						inpchar = input->data[inpos+(inpoffset-1)]; // load another char...
						if ((inpchar == 'T') || (inpchar == 'C') || (inpchar == 'S'))
						{
							// match 2 characters
							ruleoffset--;
							inpoffset -= 2;
						}
						else
						{
							// mismatch
							fail = true;
						}
#endif
					}
					else
					{
						// mismatch
						fail = true;
					}
				}
				else if (rulechar == '^') // ^ matches any consonant
				{
					if (isCons(inpchar,c))
					{
						// match
						ruleoffset--;
						inpoffset--;
					}
					else
					{
						// mismatch
						fail = true;
					}
				}
				else if (rulechar == '+') // + matches any front vowel: E, I or Y
				{
					if (isFront(inpchar,c))
					{
						// match
						ruleoffset--;
						inpoffset--;
					}
					else
					{
						// mismatch
						fail = true;
					}
				}
				else if (rulechar == ':') // : matches zero or more consonants; this test can't fail, but it can consume consonants in the input
				{
					// one problem is the NRL rules often have '^' before ':' in the prefix (meaning 'one or more consonant'),
					// and if we parse the ':' first we end up consuming all the consonants, leaving none for the '^'.
					// we need to explicitly check for that here and leave a single consonant in the input if that's the case.
					// this does NOT cover the circumstance with '^^:'. no NRL rules contain that chain, and you should be using ':^' or ':^^' anyway!
					bool singleBeforeMulti = false;
					if ( (lparen_idx+(ruleoffset-1) >= 0) && ( ruleset.rule[i][lparen_idx+(ruleoffset-1)] == '^') ) // '^:' case
					{
						e_printf(V_ERULES, "found a prefix rule with the problematic ^: case\n");
						singleBeforeMulti = true;
					}
					bool matchedCons = false;
					ruleoffset--;
					while ((inpos+(inpoffset-1) >= 0) && isCons(inpchar,c))
					{
						matchedCons = true;
						// match
						inpoffset--;
						inpchar = input->data[inpos+inpoffset];
					}
					if (singleBeforeMulti && matchedCons)
					{
						// in this very specific case, we're courteous and leave one extra character on the input, so the ^ has something to eat
						inpoffset++;
					}
				}
				// The NRL parser allows a rule character of '*' which is 'one or more consonants'
				// but none of the rules in the set in the published papers actually use this symbol.
				// Perhaps the lost older rule sets did. We can support it anyway.
#ifdef SUPPORT_CONS1M
				else if (rulechar == '*') // * matches one or more consonants
				{
					if (isCons(inpchar,c))
					{
						// match one...
						ruleoffset--;
						// yes, we recheck what we already just checked. this avoids a bad bug.
						while ((inpos+(inpoffset-1) >= 0) && isCons(inpchar,c))
						{
							// match another...
							inpoffset--;
							inpchar = input->data[inpos+inpoffset];
						}
					}
					else
					{
						// mismatch
						fail = true;
					}
				}
#endif
				// The NRL parser allows a rule character of '$' which is 'a consonant followed by E or I'
				// but none of the rules in the set in the published papers actually use this symbol.
				// Perhaps the lost older rule sets did. We can support it anyway.
#ifdef SUPPORT_CONS1EI
				else if (rulechar == '$') // $ matches one consonant followed by 'I' or 'E'
				{
					if ( (inpchar == 'E') || (inpchar == 'I') ) // '^E' and '^I' cases
					{
						// the beginning of the input array is ALWAYS a space, so since we saw an 'E' or 'I'
						// we can't be at offset less than 1 here, so it is always safe to index back one more character
						inpchar = input->data[inpos+(inpoffset-1)]; // load another char...
						if (isCons(inpchar,c))
						{
							// match 2 characters
							ruleoffset--;
							inpoffset -= 2;
						}
						else
						{
							// mismatch
							fail = true;
						}
					}
					else
					{
						// mismatch
						fail = true;
					}
				}
#endif
				else
				{
					e_printf(V_ERR, "got an invalid rule character of '%c'(0x%02x), exiting!\n", rulechar, rulechar);
					exit(1);
				}
			}
			if (fail) continue; // mismatch, move on to the next rule.
		}

		// part3: match the rule suffix
		{
			bool fail = false;
			s32 ruleoffset = 1;
			s32 inpoffset = nbase;
			int rulechar;
			int inpchar;
			while ((!fail)&&(rparen_idx+ruleoffset < equals_idx)&&(inpos+inpoffset <= input->elements))
			{
				rulechar = ruleset.rule[i][rparen_idx+ruleoffset];
				inpchar = input->data[inpos+inpoffset];
				e_printf(V_SEARCH2, "rulechar is %c(%02x) at ruleoffset %d, inpchar is %c(%02x) at inpoffset %d\n", rulechar, rulechar, rparen_idx+ruleoffset, inpchar, inpchar, inpos+inpoffset);
				if (isLetter(rulechar, c)) // letter in rule matches that letter exactly, only.
				{
					// it's a letter, directly compare it to the input character
					if (rulechar == inpchar)
					{
						// match
						ruleoffset++;
						inpoffset++;
					}
					else
					{
						// mismatch
						fail = true;
					}
				}
				else if (rulechar == ' ') // space matches any non-letter
				{
					if (!isLetter(inpchar,c))
					{
						// match
						ruleoffset++;
						inpoffset++;
					}
					else
					{
						// mismatch
						fail = true;
					}
				}
				// The NRL rules have '#' match 'one or more vowels', while the SV rules have '#' match 'exactly one vowel'
				// NRL rules also allow '##' to match 'two or more vowels' which requires look-ahead or a recursive parser
				// which does a DFS or BFS of every potentially matching character. We cheat and look ahead here.
#ifdef NRL_VOWEL
				else if (rulechar == '#') // # matches one or more vowels
				{
					// special check here for the case where the rule has '##' in it
					if ( (rparen_idx+ruleoffset+1 < equals_idx) && (ruleset.rule[i][rparen_idx+ruleoffset+1] == '#') ) // '##' case
					{
						e_printf(V_ERULES, "found a suffix rule with the problematic ## case\n");
						// check for two vowels, plus any more.
						if (isVowel(inpchar,c) && (inpos+inpoffset+1 <= input->elements) && isVowel(input->data[inpos+inpoffset+1],c))
						{
							// match 2 vowels...
							ruleoffset += 2;
							// yes, we recheck what we already just checked. this avoids a bad bug.
							while ((inpos+inpoffset+1 <= input->elements) && isVowel(inpchar,c))
							{
								// match another...
								inpoffset++;
								inpchar = input->data[inpos+inpoffset];
							}
						}
						else
						{
							// mismatch
							fail = true;
						}
					}
					else // '#' case
					{
						if (isVowel(inpchar,c))
						{
							// match one...
							ruleoffset++;
							// yes, we recheck what we already just checked. this avoids a bad bug.
							while ((inpos+inpoffset+1 <= input->elements) && isVowel(inpchar,c))
							{
								// match another...
								inpoffset++;
								inpchar = input->data[inpos+inpoffset];
							}
						}
						else
						{
							// mismatch
							fail = true;
						}
					}
				}
#else
				else if (rulechar == '#') // # matches one vowel
				{
					if (isVowel(inpchar,c))
					{
						// match
						ruleoffset++;
						inpoffset++;
					}
					else
					{
						// mismatch
						fail = true;
					}
				}
#endif
				else if (rulechar == '.') // . matches any voiced consonant
				{
					if (isVoiced(inpchar,c))
					{
						// match
						ruleoffset++;
						inpoffset++;
					}
					else
					{
						// mismatch
						fail = true;
					}
				}
				else if (rulechar == '&') // & matches any sibilant;
				// note the special cases for CH and SH which must be tested FIRST since 'C' and 'S' are themselves sibilants!
				{
					// the original code is buggy here, probably improperly copy-pasted from the prefix check code.
#ifdef ORIGINAL_BUGS
					if (isSibil(inpchar,c))
					{
						// match
						ruleoffset++;
						inpoffset++;
					}
					else if ( (inpchar == 'H') && (inpos+inpoffset+1 <= input->elements)
						&& (input->data[inpos+inpoffset+1] == 'C') ) // bugged 'HC' case
					{
						// match 2 characters
						ruleoffset++;
						inpoffset += 2;
					}
					else if ( (inpchar == 'H') && (inpos+inpoffset+1 <= input->elements)
						&& (input->data[inpos+inpoffset+1] == 'S') ) // bugged 'HS' case
					{
						// match 2 characters
						ruleoffset++;
						inpoffset += 2;
					}
#else
					if ( (inpchar == 'C') && (inpos+inpoffset+1 <= input->elements)
						&& (input->data[inpos+inpoffset+1] == 'H') ) // 'CH' case
					{
						// match 2 characters
						ruleoffset++;
						inpoffset += 2;
					}
					else if ( (inpchar == 'S') && (inpos+inpoffset+1 <= input->elements)
						&& (input->data[inpos+inpoffset+1] == 'H') ) // 'SH' case
					{
						// match 2 characters
						ruleoffset++;
						inpoffset += 2;
					}
					else if (isSibil(inpchar,c))
					{
						// match
						ruleoffset++;
						inpoffset++;
					}
#endif
					else
					{
						// mismatch
						fail = true;
					}
				}
				else if (rulechar == '@') // @ matches any unvoiced affricate aka nonpalate
				// note the special cases for TH, CH, SH must be tested FIRST since T and S are themselves unvoiced affricates!
				{
#ifdef ORIGINAL_BUGS
					// the original code is EXTREMELY BUGGY here: not only would it incorrectly check for 'HT' 'HC' and 'HS', but like the prefix version it forgets to increment the pointer and read the next byte, so it checks for 'H', then immediately checks that the 'H' is equal to 'T', 'C' or 'S', which will always fail! It also checks for isUaff BEFORE it checks for the 2 letter versions, which means it would never match 'TH' or 'SH' as it would match the 1-letter 'T' and 'S' first anyway!
					if (isUaff(inpchar,c))
					{
						// match
						ruleoffset++;
						inpoffset++;
					}
					else if (inpchar == 'H')
					{
						fail = true;
						// technically the original code tries to check for 'HT' 'HC' 'HS' like the bugged '&' rule case above,
						// but it forgets to increment the inpoffset pointer so it always fails since it checks the input
						// 'H' against the constants 'T', 'C', and 'S' without advancing to the next input character.
					}
#else
					if ( (inpchar == 'T') && (inpos+inpoffset+1 <= input->elements)
						&& (input->data[inpos+inpoffset+1] == 'H') ) // 'TH' case
					{
						// match 2 characters
						ruleoffset++;
						inpoffset += 2;
					}
					else if ( (inpchar == 'C') && (inpos+inpoffset+1 <= input->elements)
						&& (input->data[inpos+inpoffset+1] == 'H') ) // 'CH' case
					{
						// match 2 characters
						ruleoffset++;
						inpoffset += 2;
					}
					else if ( (inpchar == 'S') && (inpos+inpoffset+1 <= input->elements)
						&& (input->data[inpos+inpoffset+1] == 'H') ) // 'SH' case
					{
						// match 2 characters
						ruleoffset++;
						inpoffset += 2;
					}
					else if (isUaff(inpchar,c))
					{
						// match 1 character
						ruleoffset++;
						inpoffset++;
					}
#endif
					else
					{
						// mismatch
						fail = true;
					}
				}
				else if (rulechar == '^') // ^ matches any consonant
				{
					if (isCons(inpchar,c))
					{
						// match
						ruleoffset++;
						inpoffset++;
					}
					else
					{
						// mismatch
						fail = true;
					}
				}
				else if (rulechar == '+') // + matches any front vowel: E, I or Y
				{
					if (isFront(inpchar,c))
					{
						// match
						ruleoffset++;
						inpoffset++;
					}
					else
					{
						// mismatch
						fail = true;
					}
				}
				else if (rulechar == ':') // : matches zero or more consonants; this test can't fail, but it can consume consonants in the input
				{
					ruleoffset++;
					while ((inpos+inpoffset+1 <= input->elements) && isCons(inpchar,c))
					{
						inpoffset++;
						inpchar = input->data[inpos+inpoffset];
					}
				}
				else if (rulechar == '%') // % matches 'E', 'ER', 'ES', 'ED', 'ELY', 'EFUL', and 'ING'
				{
					if (inpchar == 'E') // 'E', 'ER', 'ES', 'ED', 'ELY', 'EFUL'; if this check for 'E' passes, this test can't fail outright unless we run out of input
					{
						// match
						ruleoffset++;
						if (inpos+inpoffset+1 <= input->elements)
						{
							inpoffset++;
							inpchar = input->data[inpos+inpoffset]; // load another char...
							if ((inpchar == 'R')||(inpchar == 'S')||(inpchar == 'D')) // 'ER', 'ES', 'ED' cases
							{
								// match
								inpoffset++;
							}
							else if ( (inpchar == 'L') && (inpos+inpoffset+1 <= input->elements)
								&& (input->data[inpos+inpoffset+1] == 'Y')
								) // 'ELY' case
							{
								// match 2 characters
								inpoffset += 2;
							}
							else if ( (inpchar == 'F') && (inpos+inpoffset+2 <= input->elements)
								&& (input->data[inpos+inpoffset+1] == 'U')
								&& (input->data[inpos+inpoffset+2] == 'L')
								) // 'EFUL' case
							{
								// match 3 characters
								inpoffset += 3;
							}
						}
						// we either matched or fell through due to lack of input, which is ok although may not match original behavior
					}
					else if ( (inpchar == 'I') && (inpos+inpoffset+2 <= input->elements)
						&& (input->data[inpos+inpoffset+1] == 'N')
						&& (input->data[inpos+inpoffset+2] == 'G') ) // 'ING' case
					{
						// match 3 characters
						ruleoffset++;
						inpoffset += 3;
					}
					else
					{
						// mismatch
						fail = true;
					}
				}
				// The NRL parser allows a rule character of '*' which is 'one or more consonants'
				// but none of the rules in the set in the published papers actually use this symbol.
				// Perhaps the lost older rule sets did. We can support it anyway.
#ifdef SUPPORT_CONS1M
				else if (rulechar == '*') // * matches one or more consonants
				{
					if (isCons(inpchar,c))
					{
						// match one...
						ruleoffset++;
						// yes, we recheck what we already just checked. this avoids a bad bug.
						while ((inpos+inpoffset+1 <= input->elements) && isCons(inpchar,c)) // TODO: there is a probable bug here with a string like " BANG" were NG are consonants but this while loops fails leaving the G unconsumed.
						{
							// match another...
							inpoffset++;
							inpchar = input->data[inpos+inpoffset];
						}
					}
					else
					{
						// mismatch
						fail = true;
					}
				}
#endif
				// The NRL parser allows a rule character of '$' which is 'a consonant followed by E or I'
				// but none of the rules in the set in the published papers actually use this symbol.
				// Perhaps the lost older rule sets did. We can support it anyway.
#ifdef SUPPORT_CONS1EI
				else if (rulechar == '$') // $ matches one consonant followed by 'E' or 'I'
				{
					if ( isCons(inpchar,c) && (inpos+inpoffset+1 <= input->elements)
						&& ( (input->data[inpos+inpoffset+1] == 'E') // '^E' case
							|| (input->data[inpos+inpoffset+1] == 'I') // '^I' case
						)
					)
					{
						// match
						ruleoffset++;
						inpoffset += 2;
					}
					else
					{
						// mismatch
						fail = true;
					}
				}
#endif
				else
				{
					e_printf(V_ERR, "got an invalid rule character of '%c'(0x%02x), exiting!\n", rulechar, rulechar);
					exit(1);
				}
			}
			if (fail) continue; // mismatch, move on to the next rule.
		}

		// if we got this far, dump the rule right hand side past the = sign to output, then
		// consume the number of characters between the parentheses by returning inpos + that number
		{
			// crude strcpy
			e_printf(V_RULES, "%s\n",ruleset.rule[i]);
			while (ruleset.rule[i][++equals_idx] != '\0')
			{
				vec_char32_append(output, ruleset.rule[i][equals_idx]);
			}
			return inpos+(nbase-1); // we return nbase-1 since the processing loop increments inpos first thing it does
		}
	}
	// did we break out with a valid rule?
	if (i == ruleset.num_rules)
	{
		e_printf(V_ERR, "unable to find any matching rule, exiting!\n");
		exit(1);
	}
	// we should never get here.
	e_printf(V_ERR, "something very bad happened, exiting!\n");
	exit(1);
	// we should especially never get here.
	return inpos;
}

void processPhrase(const sym_ruleset* const ruleset, const vec_char32* const input, vec_char32* output, s_cfg c)
{
	e_printf(V_MAINLOOP, "processPhrase called, phrase has %d elements\n", input->elements);
	s32 inpos = -1;
	char32_t inptemp;
	while (((inptemp = input->data[++inpos])||(1)) && (inptemp != RECITER_END_CHAR)) // was (curpos < input->elements)
	{
		e_printf(V_MAINLOOP, "position is now %d (%c)\n", inpos, input->data[inpos]);
		if (input->data[inpos] == '.') // is this character a period?
		{
			e_printf(V_MAINLOOP, "character is a period...\n");
			if (isDigit(input->data[++inpos], c)) // is the character after the period a digit? // TODO: verify there isn't a bug here with consuming an extra input item 
			{
				e_printf(V_MAINLOOP, " followed by a digit...\n");
				u8 inptemp_features = c.ascii_features[inptemp&0x7f]; // save features from initial character
				if (isPunct(inptemp, c)) // if the initial character was punctuation
				{
					e_printf(V_MAINLOOP, " and the character before the period was a punctuation symbol!\n");
					// look up PUNCT_DIGIT rules
					inpos = processRule(ruleset[RULES_PUNCT_DIGIT], input, inpos, output, c);
					// THIS CASE IS FINISHED
				}
				else
				{
					e_printf(V_MAINLOOP, " but the character before the period was not a punctuation symbol.\n");
					if (!inptemp_features) // if the feature was set to \0, then completely ignore this character.
					{
						//TODO(optional): original code clobbers the input string character with a space as well
						vec_char32_append(output, ' '); // add a space to the output word.
						// THIS CASE IS FINISHED
					}
					else
					{
						if (inptemp_features&A_LETTER) // could be isLetter(inptemp);
						{
							inpos = processRule(ruleset[inptemp-0x41], input, inpos, output, c);
							// THIS CASE IS FINISHED
						}
						else
						{
							e_printf(V_ERR, "found a character that isn't punct/digit, nor letter, nor null, bail out!\n");
							exit(1);
							// THIS CASE IS FINISHED
						}
					}
				}
			}
			else
			{
				e_printf(V_MAINLOOP, " but not followed by a digit, so treat it as a pause.\n");
				vec_char32_append(output, '.'); // add a period to the output word.
				// THIS CASE IS FINISHED
			}
		}
		else
		{
			e_printf(V_MAINLOOP, "character is not a period...");
			u8 inptemp_features = c.ascii_features[inptemp&0x7f]; // save features from initial character
			if (isPunct(inptemp, c)) // if the initial character was punctuation
			{
				e_printf(V_MAINLOOP, " and the initial character was a punctuation symbol!\n");
				// look up PUNCT_DIGIT rules
				inpos = processRule(ruleset[RULES_PUNCT_DIGIT], input, inpos, output, c);
				// THIS CASE IS FINISHED
			}
			else
			{
				e_printf(V_MAINLOOP, " but the initial charater was not a punctuation symbol.\n");
				if (!inptemp_features) // if the feature was set to \0, then completely ignore this character.
				{
					//TODO(optional): original code clobbers the input string character with a space as well
					vec_char32_append(output, ' '); // add a space to the output word.
					// THIS CASE IS FINISHED
				}
				else
				{
					if (inptemp_features&A_LETTER) // could be isLetter(inptemp);
					{
						inpos = processRule(ruleset[inptemp-0x41], input, inpos, output, c);
						// THIS CASE IS FINISHED
					}
					else
					{
						e_printf(V_ERR, "found a character that isn't punct/digit, nor letter, nor null, bail out!\n");
						exit(1);
						// THIS CASE IS FINISHED
					}
				}
			}
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
		{ // ascii_features rules set
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // CTRL-@ thru CTRL-O
			0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // CTRL-P thru CTRL+_
			0, // SPACE
			A_PUNCT, // !
			A_PUNCT, // "
			A_PUNCT, // #
			A_PUNCT, // $
			A_PUNCT, // %
			A_PUNCT, // &
			A_PUNCT|A_LETTER, // '
			0, // (
			0, // )
			A_PUNCT, // *
			A_PUNCT, // +
			A_PUNCT, // ,
			A_PUNCT, // -
			A_PUNCT, // .
			A_PUNCT, // /
			A_DIGIT|A_PUNCT, // 0
			A_DIGIT|A_PUNCT, // 1
			A_DIGIT|A_PUNCT, // 2
			A_DIGIT|A_PUNCT, // 3
			A_DIGIT|A_PUNCT, // 4
			A_DIGIT|A_PUNCT, // 5
			A_DIGIT|A_PUNCT, // 6
			A_DIGIT|A_PUNCT, // 7
			A_DIGIT|A_PUNCT, // 8
			A_DIGIT|A_PUNCT, // 9
			A_PUNCT, // :
			A_PUNCT, // ;
			A_PUNCT, // <
			A_PUNCT, // =
			A_PUNCT, // >
			A_PUNCT, // ?
			A_PUNCT, // @
			A_LETTER|A_VOWEL, // A
			A_LETTER|A_CONS|A_VOICED, // B
			A_LETTER|A_CONS|A_SIBIL, // C
			A_LETTER|A_CONS|A_VOICED|A_UAFF, // D
			A_LETTER|A_VOWEL, // E
			A_LETTER|A_CONS, // F
			A_LETTER|A_CONS|A_SIBIL|A_VOICED, // G
			A_LETTER|A_CONS, // H
			A_LETTER|A_VOWEL, // I
			A_LETTER|A_CONS|A_SIBIL|A_VOICED|A_UAFF, // J
			A_LETTER|A_CONS, // K
			A_LETTER|A_CONS|A_VOICED|A_UAFF, // L
			A_LETTER|A_CONS|A_VOICED, // M
			A_LETTER|A_CONS|A_VOICED|A_UAFF, // N
			A_LETTER|A_VOWEL, // O
			A_LETTER|A_CONS, // P
			A_LETTER|A_CONS, // Q
			A_LETTER|A_CONS|A_VOICED|A_UAFF, // R
			A_LETTER|A_CONS|A_SIBIL|A_UAFF, // S
			A_LETTER|A_CONS|A_UAFF, // T
			A_LETTER|A_VOWEL, // U
			A_LETTER|A_CONS|A_VOICED, // V
			A_LETTER|A_CONS|A_VOICED, // W
			A_LETTER|A_CONS|A_SIBIL, // X
			A_LETTER|A_VOWEL, // Y
			A_LETTER|A_CONS|A_SIBIL|A_VOICED|A_UAFF, // Z
			0, // [
			0, // '\'
			0, // ]
			A_PUNCT, // ^
			0, // _
			/// Technically, we can do a check for 0x60-0x7f and mirror it to 0x40-0x5f,
			// but to make it so we can just do a simple validation of "is a valid ascii character <= 0x7f"
			// and protect from out of bounds accesses using &0x7f; we repeat the 0x60-0x7f part here
			A_PUNCT, // `
			A_LETTER|A_VOWEL, // a
			A_LETTER|A_CONS|A_VOICED, // b
			A_LETTER|A_CONS|A_SIBIL, // c
			A_LETTER|A_CONS|A_VOICED|A_UAFF, // d
			A_LETTER|A_VOWEL, // e
			A_LETTER|A_CONS, // f
			A_LETTER|A_CONS|A_SIBIL|A_VOICED, // g
			A_LETTER|A_CONS, // h
			A_LETTER|A_VOWEL, // i
			A_LETTER|A_CONS|A_SIBIL|A_VOICED|A_UAFF, // j
			A_LETTER|A_CONS, // k
			A_LETTER|A_CONS|A_VOICED|A_UAFF, // l
			A_LETTER|A_CONS|A_VOICED, // m
			A_LETTER|A_CONS|A_VOICED|A_UAFF, // n
			A_LETTER|A_VOWEL, // o
			A_LETTER|A_CONS, // p
			A_LETTER|A_CONS, // q
			A_LETTER|A_CONS|A_VOICED|A_UAFF, // r
			A_LETTER|A_CONS|A_SIBIL|A_UAFF, // s
			A_LETTER|A_CONS|A_UAFF, // t
			A_LETTER|A_VOWEL, // u
			A_LETTER|A_CONS|A_VOICED, // v
			A_LETTER|A_CONS|A_VOICED, // w
			A_LETTER|A_CONS|A_SIBIL, // x
			A_LETTER|A_VOWEL, // y
			A_LETTER|A_CONS|A_SIBIL|A_VOICED|A_UAFF, // z
			0, // {
			0, // |
			0, // }
			A_PUNCT, // ~
			0 // DEL
		},
		//NULL, // letter to sound rules
		32, // verbose (was 0)
	};

	//{
		const char* const arule_eng[] =
		{
			" [A.]=EH4Y. ",
			"[A] =AH",
			" [ARE] =AAR",
			" [AR]O=AXR",
			"[AR]#=EH4R",
			" ^[AS]#=EY4S",
			"[A]WA=AX",
			"[AW]=AO5",
			" :[ANY]=EH4NIY",
			"[A]^+#=EY5",
			"#:[ALLY]=ULIY",
			" [AL]#=UL",
			"[AGAIN]=AXGEH4N",
			"#:[AG]E=IHJ",
			"[A]^%=EY",
			"[A]^+:#=AE",
			" :[A]^+ =EY4",
			" [ARR]=AXR",
			"[ARR]=AE4R",
			" ^[AR] =AA5R",
			"[AR]=AA5R",
			"[AIR]=EH4R",
			"[AI]=EY4",
			"[AY]=EY5",
			"[AU]=AO4",
			"#:[AL] =UL",
			"#:[ALS] =ULZ",
			"[ALK]=AO4K",
			"[AL]^=AOL",
			" :[ABLE]=EY4BUL",
			"[ABLE]=AXBUL",
			"[A]VO=EY4",
			"[ANG]+=EY4NJ",
			"[ATARI]=AHTAA4RIY",
			"[A]TOM=AE",
			"[A]TTI=AE",
			" [AT] =AET",
			" [A]T=AH",
			"[A]=AE",
		};

		const char* const brule_eng[] =
		{
			" [B] =BIY4",
			" [BE]^#=BIH",
			"[BEING]=BIY4IHNX",
			" [BOTH] =BOW4TH",
			" [BUS]#=BIH4Z",
			"[BREAK]=BREY5K",
			"[BUIL]=BIH4L",
			"[B]=B",
		};

		const char* const crule_eng[] =
		{
			" [C] =SIY4",
			" [CH]^=K",
			"^E[CH]=K",
			"[CHA]R#=KEH5",
			"[CH]=CH",
			" S[CI]#=SAY4",
			"[CI]A=SH",
			"[CI]O=SH",
			"[CI]EN=SH",
			"[CITY]=SIHTIY",
			"[C]+=S",
			"[CK]=K",
#ifdef C64_ADDED_RULES
			"[COMMODORE]=KAA4MAHDOHR",
#endif
			"[COM]=KAHM",
			"[CUIT]=KIHT",
			"[CREA]=KRIYEY",
			"[C]=K",
		};

		const char* const drule_eng[] =
		{
			" [D] =DIY4",
			" [DR.] =DAA4KTER",
			"#:[DED] =DIHD",
			".E[D] =D",
			"#:^E[D] =T",
			" [DE]^#=DIH",
			" [DO] =DUW",
			" [DOES]=DAHZ",
			"[DONE] =DAH5N",
			"[DOING]=DUW4IHNX",
			" [DOW]=DAW",
			"#[DU]A=JUW",
			"#[DU]^#=JAX",
			"[D]=D",
		};

		const char* const erule_eng[] =
		{
			" [E] =IYIY4",
			"#:[E] =",
			"':^[E] =",
			" :[E] =IY",
			"#[ED] =D",
			"#:[E]D =",
			"[EV]ER=EH4V",
			"[E]^%=IY4",
			"[ERI]#=IY4RIY",
			"[ERI]=EH4RIH",
			"#:[ER]#=ER",
			"[ERROR]=EH4ROHR",
			"[ERASE]=IHREY5S",
			"[ER]#=EHR",
			"[ER]=ER",
			" [EVEN]=IYVEHN",
			"#:[E]W=",
			"@[EW]=UW",
			"[EW]=YUW",
			"[E]O=IY",
			"#:&[ES] =IHZ",
			"#:[E]S =",
			"#:[ELY] =LIY",
			"#:[EMENT]=MEHNT",
			"[EFUL]=FUHL",
			"[EE]=IY4",
			"[EARN]=ER5N",
			" [EAR]^=ER5",
			"[EAD]=EHD",
			"#:[EA] =IYAX",
			"[EA]SU=EH5",
			"[EA]=IY5",
			"[EIGH]=EY4",
			"[EI]=IY4",
			" [EYE]=AY4",
			"[EY]=IY",
			"[EU]=YUW5",
			"[EQUAL]=IY4KWUL",
			"[E]=EH",
		};

		const char* const frule_eng[] =
		{
			" [F] =EH4F",
			"[FUL]=FUHL",
			"[FRIEND]=FREH5ND",
			"[FATHER]=FAA4DHER",
			"[F]F=",
			"[F]=F",
		};

		const char* const grule_eng[] =
		{
			" [G] =JIY4",
			"[GIV]=GIH5V",
			" [G]I^=G",
			"[GE]T=GEH5",
			"SU[GGES]=GJEH4S",
			"[GG]=G",
			" B#[G]=G",
			"[G]+=J",
			"[GREAT]=GREY4T",
			"[GON]E=GAO5N",
			"#[GH]=",
			" [GN]=N",
			"[G]=G",
		};

		const char* const hrule_eng[] =
		{
			" [H] =EY4CH",
			" [HAV]=/HAE6V",
			" [HERE]=/HIYR",
			" [HOUR]=AW5ER",
			"[HOW]=/HAW",
			"[H]#=/H",
			"[H]=",
		};

		const char* const irule_eng[] =
		{
			" [IN]=IHN",
			" [I] =AY4",
			"[I] =AY",
			"[IN]D=AY5N",
			"SEM[I]=IY",
			" ANT[I]=AY",
			"[IER]=IYER",
			"#:R[IED] =IYD",
			"[IED] =AY5D",
			"[IEN]=IYEHN",
			"[IE]T=AY4EH",
			"[I']=AY5",
			" :[I]^%=AY5",
			" :[IE] =AY4",
			"[I]%=IY",
			"[IE]=IY4",
			" [IDEA]=AYDIY5AH",
			"[I]^+:#=IH",
			"[IR]#=AYR",
			"[IZ]%=AYZ",
			"[IS]%=AYZ",
			"I^[I]^#=IH",
			"+^[I]^+=AY",
			"#:^[I]^+=IH",
			"[I]^+=AY",
			"[IR]=ER",
			"[IGH]=AY4",
			"[ILD]=AY5LD",
			" [IGN]=IHGN",
			"[IGN] =AY4N",
			"[IGN]^=AY4N",
			"[IGN]%=AY4N",
			"[ICRO]=AY4KROH",
			"[IQUE]=IY4K",
			"[I]=IH",
		};

		const char* const jrule_eng[] =
		{
			" [J] =JEY4",
			"[J]=J",
		};

		const char* const krule_eng[] =
		{
			" [K] =KEY4",
			" [K]N=",
			"[K]=K",
		};

		const char* const lrule_eng[] =
		{
			" [L] =EH4L",
			"[LO]C#=LOW",
			"L[L]=",
			"#:^[L]%=UL",
			"[LEAD]=LIYD",
			" [LAUGH]=LAE4F",
			"[L]=L",
		};

		const char* const mrule_eng[] =
		{
			" [M] =EH4M",
			" [MR.] =MIH4STER",
			" [MS.]=MIH5Z",
			" [MRS.] =MIH4SIXZ",
			"[MOV]=MUW4V",
			"[MACHIN]=MAHSHIY5N",
			"M[M]=",
			"[M]=M",
		};

		const char* const nrule_eng[] =
		{
			" [N] =EH4N",
			"E[NG]+=NJ",
			"[NG]R=NXG",
			"[NG]#=NXG",
			"[NGL]%=NXGUL",
			"[NG]=NX",
			"[NK]=NXK",
			" [NOW] =NAW4",
			"N[N]=",
			"[NON]E=NAH4N",
			"[N]=N",
		};

		const char* const orule_eng[] =
		{
			" [O] =OH4W",
			"[OF] =AHV",
			" [OH] =OW5",
			"[OROUGH]=ER4OW",
			"#:[OR] =ER",
			"#:[ORS] =ERZ",
			"[OR]=AOR",
			" [ONE]=WAHN",
			"#[ONE] =WAHN",
			"[OW]=OW",
			" [OVER]=OW5VER",
			"PR[O]V=UW4",
			"[OV]=AH4V",
			"[O]^%=OW5",
			"[O]^EN=OW",
			"[O]^I#=OW5",
			"[OL]D=OW4L",
			"[OUGHT]=AO5T",
			"[OUGH]=AH5F",
			" [OU]=AW",
			"H[OU]S#=AW4",
			"[OUS]=AXS",
			"[OUR]=OHR",
			"[OULD]=UH5D",
			"[OU]^L=AH5",
			"[OUP]=UW5P",
			"[OU]=AW",
			"[OY]=OY",
			"[OING]=OW4IHNX",
			"[OI]=OY5",
			"[OOR]=OH5R",
			"[OOK]=UH5K",
			"F[OOD]=UW5D",
			"L[OOD]=AH5D",
			"M[OOD]=UW5D",
			"[OOD]=UH5D",
			"F[OOT]=UH5T",
			"[OO]=UW5",
			"[O']=OH",
			"[O]E=OW",
			"[O] =OW",
			"[OA]=OW4",
			" [ONLY]=OW4NLIY",
			" [ONCE]=WAH4NS",
			"[ON'T]=OW4NT",
			"C[O]N=AA",
			"[O]NG=AO",
			" :^[O]N=AH",
			"I[ON]=UN",
#ifdef C64_RULES_BUGS
			// missing a space before =
			"#:[ON]=UN",
#else
			"#:[ON] =UN",
#endif
			"#^[ON]=UN",
#ifdef C64_RULES_BUGS
			// missing a space before =
			"[O]ST=OW",
#else
			"[O]ST =OW",
#endif
			"[OF]^=AO4F",
			"[OTHER]=AH5DHER",
			"R[O]B=RAA",
#ifdef APPLE_RULES_VARIANT
			// is this to make "PRODOS" pronounced slightly better? or just a transcription mistake?
			"PR[O]:#=ROW5",
#else
			"^R[O]:#=OW5",
#endif
			"[OSS] =AO5S",
			"#:^[OM]=AHM",
			"[O]=AA",
		};

		const char* const prule_eng[] =
		{
			" [P] =PIY4",
			"[PH]=F",
			"[PEOPL]=PIY5PUL",
			"[POW]=PAW4",
			"[PUT] =PUHT",
			"[P]P=",
#ifdef C64_RULES_BUGS
			// missing the preceding space, to force these to only start at the beginning of a word
			"[P]S=",
			"[P]N=",
			"[PROF.]=PROHFEH4SER",
#else
			// the apple version has these two next rules in the opposite order, but it makes absolutely no difference as the two rules are mutually exclusive due to the leading space
			" [P]S=",
			" [P]N=",
			" [PROF.]=PROHFEH4SER",
#endif
			"[P]=P",
		};

		const char* const qrule_eng[] =
		{
			" [Q] =KYUW4",
			"[QUAR]=KWOH5R",
			"[QU]=KW",
			"[Q]=K",
		};

		const char* const rrule_eng[] =
		{
#ifdef APPLE_RULES_VARIANT
			// the apple port has a different stress marker for this rule
			" [R] =AA4R",
#else
			" [R] =AA5R",
#endif
			" [RE]^#=RIY",
			"[R]R=",
			"[R]=R",
		};

		const char* const srule_eng[] =
		{
			" [S] =EH4S",
			"[SH]=SH",
			"#[SION]=ZHUN",
			"[SOME]=SAHM",
			"#[SUR]#=ZHER",
			"[SUR]#=SHER",
			"#[SU]#=ZHUW",
			"#[SSU]#=SHUW",
#ifdef C64_RULES_BUGS
			// missing a space before =
			"#[SED]=ZD",
#else
			"#[SED] =ZD",
#endif
			"#[S]#=Z",
			"[SAID]=SEHD",
			"^[SION]=SHUN",
			"[S]S=",
			".[S] =Z",
			"#:.E[S] =Z",
			"#:^#[S] =S",
			"U[S] =S",
			" :#[S] =Z",
			"##[S] =Z",
			" [SCH]=SK",
			"[S]C+=",
			"#[SM]=ZUM",
#ifdef C64_RULES_BUGS
			// this is a copy-paste error, and screws up the pronunciation of the word "wasn't"
			"#[SN]'=ZUM",
#else
			"#[SN]'=ZUN",
#endif
			"[STLE]=SUL",
			"[S]=S",
		};

		const char* const trule_eng[] =
		{
			" [T] =TIY4",
			" [THE] #=DHIY",
			" [THE] =DHAX",
			"[TO] =TUX",
			" [THAT]=DHAET",
			" [THIS] =DHIHS",
			" [THEY]=DHEY",
			" [THERE]=DHEHR",
			"[THER]=DHER",
			"[THEIR]=DHEHR",
			" [THAN] =DHAEN",
#ifdef C64_RULES_BUGS
			// this is a copy-paste error, and screws up the pronunciation of the word "them"
			" [THEM] =DHAEN",
#else
			" [THEM] =DHEHM",
#endif
			"[THESE] =DHIYZ",
			" [THEN]=DHEHN",
			"[THROUGH]=THRUW4",
			"[THOSE]=DHOHZ",
			"[THOUGH] =DHOW",
			"[TODAY]=TUXDEY",
			"[TOMO]RROW=TUMAA5",
			"[TO]TAL=TOW5",
			" [THUS]=DHAH4S",
			"[TH]=TH",
#ifdef C64_RULES_BUGS
			// missing a space before =
			"#:[TED]=TIXD",
#else
			"#:[TED] =TIXD",
#endif
			"S[TI]#N=CH",
			"[TI]O=SH",
			"[TI]A=SH",
			"[TIEN]=SHUN",
			"[TUR]#=CHER",
			"[TU]A=CHUW",
			" [TWO]=TUW",
#ifdef APPLE_RULES_VARIANT
			// the apple version of this rule drops the space before the =, and adds another rule below specific to starting with 'F' for the word '[s]often'
			"&[T]EN=",
			"F[T]EN=",
#else
			"&[T]EN =",
#endif
			"[T]=T",
		};

		const char* const urule_eng[] =
		{
			" [U] =YUW4",
			" [UN]I=YUWN",
			" [UN]=AHN",
			" [UPON]=AXPAON",
			"@[UR]#=UH4R",
			"[UR]#=YUH4R",
			"[UR]=ER",
			"[U]^ =AH",
			"[U]^^=AH5",
			"[UY]=AY5",
			" G[U]#=",
			"G[U]%=",
			"G[U]#=W",
			"#N[U]=YUW",
			"@[U]=UW",
			"[U]=YUW",
		};

		const char* const vrule_eng[] =
		{
			" [V] =VIY4",
			"[VIEW]=VYUW5",
			"[V]=V",
		};

		const char* const wrule_eng[] =
		{
			" [W] =DAH4BULYUW",
			" [WERE]=WER",
			"[WA]SH=WAA",
			"[WA]ST=WEY",
			"[WA]S=WAH",
			"[WA]T=WAA",
			"[WHERE]=WHEHR",
			"[WHAT]=WHAHT",
			"[WHOL]=/HOWL",
			"[WHO]=/HUW",
			"[WH]=WH",
			"[WAR]#=WEHR",
			"[WAR]=WAOR",
			"[WOR]^=WER",
			"[WR]=R",
			"[WOM]A=WUHM",
			"[WOM]E=WIHM",
			"[WEA]R=WEH",
			"[WANT]=WAA5NT",
			"ANS[WER]=ER",
			"[W]=W",
		};

		const char* const xrule_eng[] =
		{
#ifdef C64_RULES_BUGS
			// this is a typo and should be EH4KS to pronounce the letter properly
			" [X] =EH4KR",
#else
			" [X] =EH4KS",
#endif
			" [X]=Z",
			"[X]=KS",
		};

		const char* const yrule_eng[] =
		{
			" [Y] =WAY4",
			"[YOUNG]=YAHNX",
			" [YOUR]=YOHR",
			" [YOU]=YUW",
			" [YES]=YEHS",
			" [Y]=Y",
			"F[Y]=AY",
			"PS[YCH]=AYK",
#ifdef C64_RULES_BUGS
			// missing a space before =
			"#:^[Y]=IY",
#else
			"#:^[Y] =IY",
#endif
			"#:^[Y]I=IY",
			" :[Y] =AY",
			" :[Y]#=AY",
			" :[Y]^+:#=IH",
			" :[Y]^#=AY",
			"[Y]=IH",
		};

		const char* const zrule_eng[] =
		{
			" [Z] =ZIY4",
			"[Z]=Z",
		};

		const char* const punct_num_rule_eng[] =
		{
			"[A]=",
			"[!]=.",
			"[\"] =-AH5NKWOWT-",
			"[\"]=KWOW4T-",
			"[#]= NAH4MBER",
			"[$]= DAA4LER",
			"[%]= PERSEH4NT",
			"[&]= AEND",
			"[']=",
			"[*]= AE4STERIHSK",
			"[+]= PLAH4S",
			"[,]=,",
			" [-] =-",
			"[-]=",
			"[.]= POYNT",
			"[/]= SLAE4SH",
			"[0]= ZIY4ROW",
			" [1ST]=FER4ST",
			" [10TH]=TEH4NTH",
			"[1]= WAH4N",
			" [2ND]=SEH4KUND",
			"[2]= TUW4",
			" [3RD]=THER4D",
			"[3]= THRIY4",
			"[4]= FOH4R",
			" [5TH]=FIH4FTH",
			"[5]= FAY4V",
#ifdef C64_ADDED_RULES
			" [64] =SIH4KSTIY FOHR",
#endif
			"[6]= SIH4KS",
			"[7]= SEH4VUN",
			" [8TH]=EY4TH",
			"[8]= EY4T",
			"[9]= NAY4N",
			"[:]=.",
			"[;]=.",
			"[<]= LEH4S DHAEN",
			"[=]= IY4KWULZ",
			"[>]= GREY4TER DHAEN",
			"[?]=.",
			"[@]= AE6T",
			"[^]= KAE4RIXT",
		};
		sym_ruleset ruleset[RULES_TOTAL] =
		{
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
			{ sizeof(punct_num_rule_eng)/sizeof(*punct_num_rule_eng), punct_num_rule_eng },
		};
	//}

	// handle optional parameters
	u32 paramidx = 2;
	while (paramidx <= (argc-1))
	{
		switch (*(argv[paramidx]++))
		{
			case '-':
				// skip this character.
				break;
			case 'v':
				paramidx++;
				if (paramidx == (argc-0)) { e_printf(V_ERR,"E* Too few arguments for -v parameter!\n"); usage(); exit(1); }
				if (!sscanf(argv[paramidx], "%d", &c.verbose)) { e_printf(V_ERR,"E* Unable to parse argument for -v parameter!\n"); usage(); exit(1); }
				paramidx++;
				break;
			case '\0':
				// end of string for parameter, go to next param
				paramidx++;
				break;
			default:
				{ e_printf(V_ERR,"E* Invalid option!\n"); usage(); exit(1); }
				break;
		}
	}
	e_printf(V_PARAM,"D* Parameters: verbose: %d\n", c.verbose);


	if (argc < 2)
	{
		fprintf(stderr,"E* Too few parameters!\n"); fflush(stderr);
		usage();
		return 1;
	}

// input file
	FILE *in = fopen(argv[1], "rb");
	if (!in)
	{
		e_printf(V_ERR,"E* Unable to open input file %s!\n", argv[1]);
		return 1;
	}

	fseek(in, 0, SEEK_END);
	uint32_t len = ftell(in);
	rewind(in); //fseek(in, 0, SEEK_SET);

	uint8_t *dataArray = (uint8_t *) malloc((len) * sizeof(uint8_t));
	if (dataArray == NULL)
	{
		e_printf(V_ERR,"E* Failure to allocate memory for array of size %d, aborting!\n", len);
		fclose(in);
		return 1;
	}

	{ // scope limiter for temp
		uint32_t temp = fread(dataArray, sizeof(uint8_t), len, in);
		fclose(in);
		if (temp != len)
		{
			e_printf(V_ERR,"E* Error reading in %d elements, only read in %d, aborting!\n", len, temp);
			free(dataArray);
			dataArray = NULL;
			return 1;
		}
		e_printf(V_PARSE,"D* Successfully read in %d bytes\n", temp);
	}

	// actual program goes here

	// allocate a vector
	vec_char32* d_raw = vec_char32_alloc(4);

	// read contents of dataArray into vec_char32
	for (u32 i = 0; i < len; i++)
	{
		vec_char32_append(d_raw,dataArray[i]);
	}
	//e_printf(V_DEBUG,"Input phrase stats are:\n");
	//vec_char32_dbg_stats(d_raw);
	//vec_char32_dbg_print(d_raw);

	// free the input array
	free(dataArray);
	dataArray = NULL;

	// allocate another vector for preprocessing
	vec_char32* d_in = vec_char32_alloc(4);
	// preprocess d_raw into d_in
	preProcess(d_raw, d_in, c);
	vec_char32_free(d_raw);

	//e_printf(V_DEBUG,"Preprocessing done, stats are now:\n");
	//vec_char32_dbg_stats(d_in);
	vec_char32_dbg_print(d_in);

	// do stuff with preprocessed phrase here, i.e. the rest of the owl
	// allocate another vector for output
	vec_char32* d_out = vec_char32_alloc(4);
	processPhrase(ruleset, d_in, d_out, c);
	vec_char32_free(d_in);
	//e_printf(V_DEBUG,"Processing done, stats are now:\n");
	//vec_char32_dbg_stats(d_out);
	vec_char32_dbg_print(d_out);

	vec_char32_free(d_out);

	return 0;
}
