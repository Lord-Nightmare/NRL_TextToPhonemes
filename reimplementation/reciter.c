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
// There are at least three versions of the official RECITER ruleset:
// 1. The version used on the Atari 800 initial release (as well as the Apple2 "Software Version" AKA "Son of SAM" unofficial hack, which is of the Atari version).
#define RULES_ATARI 1
// 2. The version used on the C64 release (which inadvertently added a bunch of bugs, as well as a few new commodore-specific rules). Despite being released after the apple2 version below, this version does not have the improved rules from that version!
#define RULES_C64 2
// 3. The version used on the Apple2 release (which used the DAC card, AKA the "Hardware Version").
#define RULES_APPLE 3
// And there are at least six additional rulesets used by the 68000 versions, post-SAM:
// 4. The version used in the alpha of "MacTalk"
#define RULES_MACTALK 4
// 5. The version used in the beta/demo release (1.0?) of MacInTalk
#define RULES_MACINTALKDEMO 5
// 6. The version used in the final release (1.2 and 1.3) of MacInTalk
#define RULES_MACINTALK 6
// 7. The version used in the first v34 release of translator.device
#define RULES_TRANSLATOR 7
// 8. The version used in the later v37+ releases of translator.device
// 9. The version used in SVTTS (v1) on Windows 3.1.
// 10. The version used in SVTTS (v2) on Windows 3.1.
// 11. The version used in SVTTS (v3) on Windows 3.1.
// 12. The version used in SVTTS (v4) on Windows 3.1.
// 13. The version used in TLTTS on Windows 95
// 14. The version used in FruityLoops on Windows XP
// 15. The version used in FL-Studio on Windows 7
// There's also at least one bug caused by a transcription error from the IEEE paper in one rule.

#define RULES_VERSION RULES_MACINTALK

// this will add the commodore specific rules that might not appear in some rule versions anyway.
#define C64_ADDED_RULES 1
// this will add the bugs from the commodore specific ruleset if rule version 3 is requested
#undef C64_RULES_BUGS
// this will add the bugs from the apple specific ruleset if rule version 2 is requested
#undef APPLE_RULES_BUGS
// this will fix the IEEE transcription error (missing the first '^' in "^[OU]^L=AH5") in versions which had the error
#define FIX_IEEE_ERROR 1

// this will add an additional rule to fix the words 'juice', 'juicy', 'juicier' and 'sluice' etc where the i needs to be silent
#define NEW_RULE_UIC 1

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
				else if (rulechar == ' ') // space matches one non-letter
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
				else if (rulechar == '#') // # matches one vowel
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
				else if (rulechar == '.') // . matches one voiced consonant
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
				else if (rulechar == '&') // & matches one sibilant; note the special cases for CH and SH
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
				else if (rulechar == '@') // @ matches one unvoiced affricate aka nonpalate; note special cases for TH, CH, SH
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
				else if (rulechar == '^') // ^ matches one consonant
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
				else if (rulechar == '+') // + matches one front vowel: E, I or Y
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
#if (RULES_VERSION >= RULES_MACTALK)
				// Mactalk and later has two extra rule characters: ? (single digit) and _ (zero or more digits)
				else if (rulechar == '?') // ? matches one digit
				{
					if (isDigit(inpchar,c))
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
				else if (rulechar == '_') // _ matches zero or more digits; this test can't fail, but it can consume digits in the input
				{
					ruleoffset--;
					while ((inpos+(inpoffset-1) >= 0) && isDigit(inpchar,c))
					{
						inpoffset--;
						inpchar = input->data[inpos+inpoffset];
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
				else if (rulechar == ' ') // space matches one non-letter
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
				else if (rulechar == '.') // . matches one voiced consonant
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
				else if (rulechar == '&') // & matches one sibilant;
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
				else if (rulechar == '^') // ^ matches one consonant
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
#if (RULES_VERSION >= RULES_MACTALK)
				// Mactalk and later has two extra rule characters: ? (single digit) and _ (zero or more digits)
				else if (rulechar == '?') // ? matches any single digit
				{
					if (isDigit(inpchar,c))
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
				else if (rulechar == '_') // _ matches zero or more digits; this test can't fail, but it can consume digits in the input
				{
					ruleoffset++;
					while ((inpos+inpoffset+1 <= input->elements) && isDigit(inpchar,c))
					{
						inpoffset++;
						inpchar = input->data[inpos+inpoffset];
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
	while (((inptemp = input->data[++inpos])||(1)) && (inptemp != RECITER_END_CHAR) && (inpos < input->elements))
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
#if (RULES_VERSION >= RULES_TRANSLATOR)
			" [A.]=EH3Y. ",
#else
			" [A.]=EH4Y. ",
#endif
			"[A] =AH",
			" [ARE] =AAR",
#if (RULES_VERSION >= RULES_MACTALK)
			" [AND] =AEND",
			" [AS] =AEZ",
			" [AT] =AET",
			" [AN] =AEN",
#endif
#if (RULES_VERSION >= RULES_TRANSLATOR)
			" [AM] =AEM",
			" [AREN'T] =AA1RINT",
			" [ABOVE]=AHBAH3V",
			" [AROUND]=AHRAW3ND",
			"[A]DAP=AX",
			" [AVE.] =AE2VINUW",
#endif
			" [AR]O=AXR",
#if (RULES_VERSION >= RULES_TRANSLATOR)
			"[AR]#=EHR",
			" ^[AS]#=EYS",
#elif ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"[AR]#=EH4R ", // extra space
			" ^[AS]#=EY4S",
#else
			"[AR]#=EH4R",
			" ^[AS]#=EY4S",
#endif
			"[A]WA=AX",
#if (RULES_VERSION >= RULES_TRANSLATOR)
			"[AW]=AO",
			" :[ANY]=EH3NIY",
#elif ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"[AW]=AO5 ", // extra space
			" :[ANY]=EH4NIY",
#else
			"[AW]=AO5",
			" :[ANY]=EH4NIY",
#endif
			// WIP end point for translator
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"[A]^+#=EY5 ", // extra space
#else
			"[A]^+#=EY5",
#endif
			"#:[ALLY]=ULIY",
			" [AL]#=UL",
			"[AGAIN]=AXGEH4N",
			"#:[AG]E=IHJ",
			"[A]^%=EY",
			"[A]^+:#=AE",
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			" :[A]^+ =EY4 ", // extra space
#else
			" :[A]^+ =EY4",
#endif
			" [ARR]=AXR",
			"[ARR]=AE4R",
#if (RULES_VERSION >= RULES_MACTALK)
			" :[AR] =AA5R",
			"[AR] =ER",
#else
			" ^[AR] =AA5R",
#endif
			"[AR]=AA5R",
			"[AIR]=EH4R",
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"[AI]=EY4 ", // extra space
			"[AY]=EY5 ", // extra space
			"[AU]=AO4 ", // extra space
#else
			"[AI]=EY4",
			"[AY]=EY5",
			"[AU]=AO4",
#endif
			"#:[AL] =UL",
			"#:[ALS] =ULZ",
			"[ALK]=AO4K",
			"[AL]^=AOL",
			" :[ABLE]=EY4BUL",
			"[ABLE]=AXBUL",
			"[A]VO=EY4",
			"[ANG]+=EY4NJ",
#if (RULES_VERSION >= RULES_MACTALK)
			" [AMIGA]=AHMIY5GAH",
#endif
			"[ATARI]=AHTAA4RIY",
			"[A]TOM=AE",
			"[A]TTI=AE",
			" [AT] =AET",
			" [A]T=AH",
#if (RULES_VERSION >= RULES_MACTALK)
			"[A]A=", // remove double characters (if this is the first one in a chain...)
#endif
			"[A]=AE",
		};

		const char* const brule_eng[] =
		{
#if (RULES_VERSION >= RULES_MACTALK)
			" :[B]: = BIY4 ",
#else
			" [B] =BIY4",
#endif
			" [BE]^#=BIH",
			"[BEING]=BIY4IHNX",
			" [BOTH] =BOW4TH",
#if (RULES_VERSION >= RULES_MACTALK)
			" [BY] =BAY",
			" [BUT] =BAHT",
			" [BEEN] =BIHN",
#endif
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			" [BUS]#=BIH4Z ", // extra space
#else
			" [BUS]#=BIH4Z",
#endif
			"[BREAK]=BREY5K",
			"[BUIL]=BIH4L",
#if (RULES_VERSION >= RULES_MACTALK)
			"B[B]=", // remove double characters (if this is the second one in a chain...)
#endif
			"[B]=B",
		};

		const char* const crule_eng[] =
		{
#if (RULES_VERSION >= RULES_MACTALK)
			" :[C]: = SIY4 ",
#else
			" [C] =SIY4",
#endif
			" [CH]^=K",
			"^E[CH]=K",
			"[CHA]R#=KEH5",
			"[CH]=CH",
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			" S[CI]#=SAY4 ", // extra space
#else
			" S[CI]#=SAY4",
#endif
			"[CI]A=SH",
			"[CI]O=SH",
			"[CI]EN=SH",
			"[CITY]=SIHTIY",
			"[C]+=S",
			"[CK]=K",
#if ((RULES_VERSION == RULES_C64) || defined(C64_ADDED_RULES))
			"[COMMODORE]=KAA4MAHDOHR",
#endif
#if (RULES_VERSION >= RULES_MACTALK)
			"[COM]%=KAHM",
#else
			"[COM]=KAHM",
#endif
			"[CUIT]=KIHT",
#if (RULES_VERSION >= RULES_MACTALK)
			"[CREA]^+=KRIYEY4",
			"[CC]=CH",
#else
			"[CREA]=KRIYEY",
#endif
			"[C]=K",
		};

		const char* const drule_eng[] =
		{
#if (RULES_VERSION >= RULES_MACTALK)
			" :[D]: = DIY4 ",
#else
			" [D] =DIY4",
#endif
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
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"#[DU]^#=JAX ", // extra space
#else
			"#[DU]^#=JAX",
#endif
#if (RULES_VERSION >= RULES_MACTALK)
			"D[D]=", // remove double characters (if this is the second one in a chain...)
#endif
			"[D]=D",
		};

		const char* const erule_eng[] =
		{
#if (RULES_VERSION >= RULES_MACTALK)
			" [E] = IY4 ",
#else
			" [E] =IYIY4",
#endif
			"#:[E] =",
			"':^[E] =",
			" :[E] =IY",
			"#[ED] =D",
			"#:[E]D =",
			"[EV]ER=EH4V",
#if (RULES_VERSION >= RULES_MACTALK) // these added rules seem like they could be done better using the & suffix rule symbol, but probably can't be...
			"#:[ERED] =ERD",
			"#:[ERING]=ERIHNX",
			"#:[EN] =EHN",
			"#:[ENED] =EHND",
			"#:[ENESS] =NEHS",
#endif
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"[E]^%=IY4 ", // extra space
#else
			"[E]^%=IY4",
#endif
			"[ERI]#=IY4RIY",
			"[ERI]=EH4RIH",
			"#:[ER]#=ER",
			"[ERROR]=EH4ROHR",
#if (RULES_VERSION >= RULES_MACTALK)
			"[ERAS]E=IHREY5S",
#elif ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"[ERASE]=IHREY5S ", // extra space
#else
			"[ERASE]=IHREY5S",
#endif
			"[ER]#=EHR",
#if (RULES_VERSION >= RULES_MACTALK)
			"#:[ER] =ER",
			"#:[ERS] =ERZ",
#endif
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
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"[EE]=IY4 ", // extra space
#else
			"[EE]=IY4",
#endif
			"[EARN]=ER5N",
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			" [EAR]^=ER5 ", // extra space
#else
			" [EAR]^=ER5",
#endif
			"[EAD]=EHD",
			"#:[EA] =IYAX",
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"[EA]SU=EH5 ", // extra space
			"[EA]=IY5 ", // extra space
			"[EIGH]=EY4 ", // extra space
			"[EI]=IY4 ", // extra space
			" [EYE]=AY4 ", // extra space
#else
			"[EA]SU=EH5",
			"[EA]=IY5",
			"[EIGH]=EY4",
			"[EI]=IY4",
			" [EYE]=AY4",
#endif
			"[EY]=IY",
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"[EU]=YUW5 ", // extra space
#else
			"[EU]=YUW5",
#endif
#if (RULES_VERSION >= RULES_MACTALK)
			"[EQUAL]=IY5KWUL",
#else
			"[EQUAL]=IY4KWUL",
#endif
			"[E]=EH",
		};

		const char* const frule_eng[] =
		{
#if (RULES_VERSION >= RULES_MACTALK)
			" :[F]: = EH4F ",
			" [FOR] =FOHR",
			" [FROM] =FRAHM",
#else
			" [F] =EH4F",
#endif
			"[FUL]=FUHL",
			"[FRIEND]=FREH5ND",
			"[FATHER]=FAA4DHER",
			"[F]F=", // remove double characters (if this is the first one in a chain...)
			"[F]=F",
		};

		const char* const grule_eng[] =
		{
#if (RULES_VERSION >= RULES_MACTALK)
			" :[G]: = JIY4 ",
#else
			" [G] =JIY4",
#endif
			"[GIV]=GIH5V",
			" [G]I^=G",
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"[GE]T=GEH5 ", // extra space
			"SU[GGES]=GJEH4S ", // extra space
#else
			"[GE]T=GEH5",
			"SU[GGES]=GJEH4S",
#endif
			"[GG]=G", // compress double characters to one (if there are two together)
			" B#[G]=G",
			"[G]+=J",
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"[GREAT]=GREY4T ", // extra space
#else
			"[GREAT]=GREY4T",
#endif
			"[GON]E=GAO5N",
			"#[GH]=",
			" [GN]=N",
			"[G]=G",
		};

		const char* const hrule_eng[] =
		{
#if (RULES_VERSION >= RULES_MACTALK)
			" :[H]: = EY4CH ",
			" [HAV]=/HAEV",
			" [HAS] =/HAEZ",
			" [HAD] =/HAED",
#else
			" [H] =EY4CH",
			" [HAV]=/HAE6V",
#endif
			" [HERE]=/HIYR",
			" [HOUR]=AW5ER",
			"[HOW]=/HAW",
			"[H]#=/H",
			"[H]=",
		};

		const char* const irule_eng[] =
		{
#if (RULES_VERSION >= RULES_MACTALK)
			" [IN] =IHN",
			" [IBM] =AY5BIYEH5M",
			" [IN]=IH4N",
			"#:[I]NG=IH",
			" [IS] =IHZ",
			" [IF] =IHF",
			" [INTO] =IH3NTUW",
#else
			" [IN]=IHN",
#endif
#if (RULES_VERSION >= RULES_MACTALK)
			" [I] = AY4 ",
#elif ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			" [I] =AY4 ", // extra space; unlike the others, this one might not be a bug
#else
			" [I] =AY4",
#endif
			"[I] =AY",
			"[IN]D=AY5N",
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"SEM[I]=IY ", // extra space
#else
			"SEM[I]=IY",
#endif
			" ANT[I]=AY",
			"[IER]=IYER",
			"#:R[IED] =IYD",
			"[IED] =AY5D",
			"[IEN]=IYEHN",
			"[IE]T=AY4EH",
			"[I']=AY5",
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			" :[I]^%=AY5 ", // extra space
#else
			" :[I]^%=AY5",
#endif
#if (RULES_VERSION >= RULES_MACTALK)
			" :[I]%=AY5",
#else
			" :[IE] =AY4",
#endif
			"[I]%=IY",
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"[IE]=IY4 ", // extra space
#else
			"[IE]=IY4",
#endif
			" [IDEA]=AYDIY5AH",
			"[I]^+:#=IH",
#if (RULES_VERSION >= RULES_MACTALK)
			"#:[I]^AL=IH",
#endif
			"[IR]#=AYR",
			"[IZ]%=AYZ",
#if (RULES_VERSION >= RULES_MACTALK)
			"[IS]%=AY4Z",
			"[I]D%=AY4",
			"#:[ITY] =IHTIY",
#else
			"[IS]%=AYZ",
#endif
			"I^[I]^#=IH",
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"+^[I]^+=AY ", // extra space
#else
			"+^[I]^+=AY",
#endif
			"#:^[I]^+=IH",
#ifdef NEW_RULE_UIC
			"^U[I]C=", // (. or ^ ?) removes the silent I in the words "JUICE" and "SLUICE"
#endif
			"[I]^+=AY",
			"[IR]=ER",
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"[IGH]=AY4 ", // extra space
			"[ILD]=AY5LD ", // extra space
#else
			"[IGH]=AY4",
			"[ILD]=AY5LD",
#endif
			" [IGN]=IHGN",
			"[IGN] =AY4N",
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"[IGN]^=AY4N ", // extra space
#else
			"[IGN]^=AY4N",
#endif
			"[IGN]%=AY4N",
#if (RULES_VERSION >= RULES_MACTALK)
			"#:[IC] = IHK",
			"[ICRO]=AY5KROW",
#else
			"[ICRO]=AY4KROH",
#endif
			"[IQUE]=IY4K",
			"[I]=IH",
		};

		const char* const jrule_eng[] =
		{
#if (RULES_VERSION >= RULES_MACTALK)
			" :[J]: = JEY4 ",
			"J[J]=", // remove double characters
#else
			" [J] =JEY4",
#endif
			"[J]=J",
		};

		const char* const krule_eng[] =
		{
#if (RULES_VERSION >= RULES_MACTALK)
			" :[K]: = KEY4 ",
#else
			" [K] =KEY4",
#endif
			" [K]N=",
#if (RULES_VERSION >= RULES_MACTALK)
			"K[K]=", // remove double characters (if this is the second one in a chain...)
#endif
			"[K]=K",
		};

		const char* const lrule_eng[] =
		{
#if (RULES_VERSION >= RULES_MACTALK)
			" :[L]: = EH4L ",
#else
			" [L] =EH4L",
#endif
			"[LO]C#=LOW",
			"L[L]=", // remove double characters (if this is the second one in a chain...)
			"#:^[L]%=UL",
			"[LEAD]=LIYD",
			" [LAUGH]=LAE4F",
			"[L]=L",
		};

		const char* const mrule_eng[] =
		{
#if (RULES_VERSION >= RULES_MACTALK)
			" :[M]: = EH4M ",
#else
			" [M] =EH4M",
#endif
			" [MR.] =MIH4STER",
			" [MS.]=MIH5Z",
			" [MRS.] =MIH4SIXZ",
			"[MOV]=MUW4V",
			"[MACHIN]=MAHSHIY5N",
			"M[M]=", // remove double characters (if this is the second one in a chain...)
			"[M]=M",
		};

		const char* const nrule_eng[] =
		{
#if (RULES_VERSION >= RULES_MACTALK)
			" :[N]: = EH4N ",
#else
			" [N] =EH4N",
#endif
			"E[NG]+=NJ",
			"[NG]R=NXG",
			"[NG]#=NXG",
			"[NGL]%=NXGUL",
			"[NG]=NX",
			"[NK]=NXK",
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			" [NOW] =NAW4 ", // extra space
#else
			" [NOW] =NAW4",
#endif
			"N[N]=", // remove double characters (if this is the second one in a chain...)
			"[NON]E=NAH4N",
			"[N]=N",
		};

		const char* const orule_eng[] =
		{
#if (RULES_VERSION >= RULES_MACTALK)
			" [O] = OW4 ",
#else
			" [O] =OH4W",
#endif
			"[OF] =AHV",
#if (RULES_VERSION >= RULES_MACTALK)
			" [ON] =AAN",
#endif
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			" [OH] =OW5 ", // extra space
#else
			" [OH] =OW5",
#endif
			"[OROUGH]=ER4OW",
#if (RULES_VERSION >= RULES_MACTALK)
			" [OR] =OHR",
			"[OR] =ER",
#else
			"#:[OR] =ER", // ?
#endif
			"#:[ORS] =ERZ",
#if (RULES_VERSION >= RULES_MACTALK)
			"[OR]=OHR",
#else
			"[OR]=AOR",
#endif
			" [ONE]=WAHN",
			"#[ONE] =WAHN",
			"[OW]=OW",
			" [OVER]=OW5VER",
			"PR[O]V=UW4",
			"[OV]=AH4V",
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"[O]^%=OW5 ", // extra space
#else
			"[O]^%=OW5",
#endif
#if (RULES_VERSION >= RULES_MACTALK)
			"[O]^EN=OW4",
#else
			"[O]^EN=OW",
#endif
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"[O]^I#=OW5 ", // extra space
#else
			"[O]^I#=OW5",
#endif
			"[OL]D=OW4L",
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"[OUGHT]=AO5T ", // extra space
#else
			"[OUGHT]=AO5T",
#endif
			"[OUGH]=AH5F",
			" [OU]=AW",
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"H[OU]S#=AW4 ", // extra space
#else
			"H[OU]S#=AW4",
#endif
			"[OUS]=AXS",
			"[OUR]=OHR",
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"[OULD]=UH5D ", // extra space
#else
			"[OULD]=UH5D",
#endif
			// this next rule is supposed to be "^[OU]^L=AH5" but this rule was missing the leading ^ in the IEEE version of the NRL paper due to a printing error (but is correct in the original NRL paper), and the error seems to have propagated to all official versions of RECITER as well. It is fixed in MACTALK and later.
#if ((RULES_VERSION >= RULES_MACTALK) || defined(FIX_IEEE_ERROR))
			"^[OU]^L=AH5",
#elif ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"[OU]^L=AH5 ", // extra space, and mistake, see above
#else
			"[OU]^L=AH5", // mistake, see above
#endif
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"[OUP]=UW5P ", // extra space
#else
			"[OUP]=UW5P",
#endif
			"[OU]=AW",
			"[OY]=OY",
			"[OING]=OW4IHNX",
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"[OI]=OY5 ", // extra space
#else
			"[OI]=OY5",
#endif
#if (RULES_VERSION >= RULES_MACTALK)
			"[OOR]=UH5R",
#else
			"[OOR]=OH5R", // unclear whether this is a mistake or not?
#endif
			"[OOK]=UH5K",
			"F[OOD]=UW5D",
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"L[OOD]=AH5D ", // extra space
#else
			"L[OOD]=AH5D",
#endif
			"M[OOD]=UW5D",
			"[OOD]=UH5D",
			"F[OOT]=UH5T",
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"[OO]=UW5 ", // extra space
#else
			"[OO]=UW5",
#endif
			"[O']=OH",
			"[O]E=OW",
			"[O] =OW",
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"[OA]=OW4 ", // extra space
#else
			"[OA]=OW4",
#endif
			" [ONLY]=OW4NLIY",
			" [ONCE]=WAH4NS",
			"[ON'T]=OW4NT",
			"C[O]N=AA",
			"[O]NG=AO",
			" :^[O]N=AH",
			"I[ON]=UN",
#if ((RULES_VERSION == RULES_C64) && defined(C64_RULES_BUGS))
			"#:[ON]=UN", // missing a space before =
#else
			"#:[ON] =UN",
#endif
			"#^[ON]=UN",
#if (RULES_VERSION >= RULES_MACTALK)
			"FR[O]ST=AO4",
			"L[O]ST=AO4",
			"C[O]ST=AO4",
			"[O]ST%=OW4",
			"[O]ST =OW5",
#elif ((RULES_VERSION == RULES_C64) && defined(C64_RULES_BUGS))
			"[O]ST=OW", // missing a space before =
#else
			"[O]ST =OW",
#endif
			"[OF]^=AO4F",
			"[OTHER]=AH5DHER",
			"R[O]B=RAA",
#if (RULES_VERSION >= RULES_APPLE)
			"PR[O]:#=ROW5", // This seems like an intentional change to make "PRODOS" pronounced slightly better
#else
			"^R[O]:#=OW5",
#endif
			"[OSS] =AO5S",
			"#:^[OM]=AHM",
			"[O]=AA",
		};

		const char* const prule_eng[] =
		{
#if (RULES_VERSION >= RULES_MACTALK)
			" :[P]: = PIY4 ",
#else
			" [P] =PIY4",
#endif
			"[PH]=F",
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"[PEOPL]=PIY5PUL ", // extra space
			"[POW]=PAW4 ", // extra space
#else
			"[PEOPL]=PIY5PUL",
			"[POW]=PAW4",
#endif
			"[PUT] =PUHT",
			"[P]P=", // remove double characters (if this is the first one in a chain...)
#if ((RULES_VERSION == RULES_C64) && defined(C64_RULES_BUGS))
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
#if (RULES_VERSION >= RULES_MACTALK)
			" :[Q]: = KYUW4 ",
#else
			" [Q] =KYUW4",
#endif
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"[QUAR]=KWOH5R ", // extra space
#else
			"[QUAR]=KWOH5R",
#endif
			"[QU]=KW",
			"[Q]=K",
		};

		const char* const rrule_eng[] =
		{
#if (RULES_VERSION >= RULES_MACTALK)
			" :[R]: = AA4R ",
#elif (RULES_VERSION == RULES_APPLE)
			" [R] =AA4R", // the apple version and onward has a different stress marker for this rule
#else
			" [R] =AA5R",
#endif
			" [RE]^#=RIY",
			"[R]R=", // remove double characters (if this is the first one in a chain...)
			"[R]=R",
		};

		const char* const srule_eng[] =
		{
#if (RULES_VERSION >= RULES_MACTALK)
			" :[S]: = EH4S ",
			" [SO]=SOW",
#else
			" [S] =EH4S",
#endif
			"[SH]=SH",
			"#[SION]=ZHUN",
			"[SOME]=SAHM",
			"#[SUR]#=ZHER",
			"[SUR]#=SHER",
			"#[SU]#=ZHUW",
			"#[SSU]#=SHUW",
#if ((RULES_VERSION == RULES_C64) && defined(C64_RULES_BUGS))
			// missing a space before =
			"#[SED]=ZD",
#else
			"#[SED] =ZD",
#endif
			"#[S]#=Z",
			"[SAID]=SEHD",
			"^[SION]=SHUN",
			"[S]S=", // remove double characters (if this is the first one in a chain...)
			".[S] =Z",
			"#:.E[S] =Z",
#if (RULES_VERSION >= RULES_MACTALK)
			"#:^##[S] =Z",
#endif
			"#:^#[S] =S",
			"U[S] =S",
			" :#[S] =Z",
			"##[S] =Z",
			" [SCH]=SK",
			"[S]C+=",
			"#[SM]=ZUM",
#if ((RULES_VERSION == RULES_C64) && defined(C64_RULES_BUGS))
			"#[SN]'=ZUM", // this is a copy-paste error, and screws up the pronunciation of the words "doesn't", "wasn't", etc.
#else
			"#[SN]'=ZUN",
#endif
			"[STLE]=SUL",
			"[S]=S",
		};

		const char* const trule_eng[] =
		{
#if (RULES_VERSION >= RULES_MACTALK)
			" :[T]: = TIY4 ",
#else
			" [T] =TIY4",
#endif
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
#if ((RULES_VERSION == RULES_C64) && defined(C64_RULES_BUGS))
			" [THEM] =DHAEN", // this is a copy-paste error, and screws up the pronunciation of the word "them"
#else
			" [THEM] =DHEHM",
#endif
			"[THESE] =DHIYZ",
			" [THEN]=DHEHN",
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"[THROUGH]=THRUW4 ", // extra space
#else
			"[THROUGH]=THRUW4",
#endif
			"[THOSE]=DHOHZ",
			"[THOUGH] =DHOW",
			"[TODAY]=TUXDEY",
			"[TOMO]RROW=TUMAA5",
			"[TO]TAL=TOW5",
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			" [THUS]=DHAH4S ", // extra space
#else
			" [THUS]=DHAH4S",
#endif
			"[TH]=TH",
#if ((RULES_VERSION == RULES_C64) && defined(C64_RULES_BUGS))
			"#:[TED]=TIXD", // missing a space before =
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
#if (RULES_VERSION >= RULES_APPLE)
			// the apple version and later of this rule drops the space before the =, and adds another rule below specific to starting with 'F' for the word '[s]often'
			"&[T]EN=",
			"F[T]EN=",
#else
			"&[T]EN =",
#endif
#if (RULES_VERSION >= RULES_MACTALK)
			"[T]T=", // remove double characters (if this is the first one in a chain...)
#endif
			"[T]=T",
		};

		const char* const urule_eng[] =
		{
#if (RULES_VERSION >= RULES_MACTALK)
			" [U] = YUW4 ",
#else
			" [U] =YUW4",
#endif
			" [UN]I=YUWN",
			" [UN]=AHN",
#if (RULES_VERSION >= RULES_MACTALK)
			" [UPON]=AXPAA3N",
#else
			" [UPON]=AXPAON",
#endif
			"@[UR]#=UH4R",
			"[UR]#=YUH4R",
			"[UR]=ER",
			"[U]^ =AH",
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"[U]^^=AH5 ", // extra space
			"[UY]=AY5 ", // extra space
#else
			"[U]^^=AH5",
			"[UY]=AY5",
#endif
			" G[U]#=",
			"G[U]%=",
			"G[U]#=W",
			"#N[U]=YUW",
			"@[U]=UW",
#if (RULES_VERSION >= RULES_MACTALK)
			"U[U]=", // note that mactalk actually has a broken rule here of "U[U]" with no equals after it. this should remove the second 'U' in a series of multiple 'U's
			" [USA] =YUW5EHSEY2",
#endif
			"[U]=YUW",
		};

		const char* const vrule_eng[] =
		{
#if (RULES_VERSION >= RULES_MACTALK)
			" :[V]: = VIY4 ",
#else
			" [V] =VIY4",
#endif
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"[VIEW]=VYUW5 ", // extra space
#else
			"[VIEW]=VYUW5",
#endif
#if (RULES_VERSION >= RULES_MACTALK)
			"V[V]=", // note that mactalk actually has a broken rule here of "V[V]" with no equals after it. this should remove the second 'V' in a series of multiple 'V's
#endif
			"[V]=V",
		};

		const char* const wrule_eng[] =
		{
#if (RULES_VERSION >= RULES_MACTALK)
			" :[W]: = DAH4BULYUW ",
#else
			" [W] =DAH4BULYUW",
#endif
			" [WERE]=WER",
			"[WA]SH=WAA",
			"[WA]ST=WEY",
			"[WA]S=WAH",
			"[WA]T=WAA",
			"[WHERE]=WHEHR",
			"[WHAT]=WHAHT",
			"[WHOL]=/HOWL",
			"[WHO]=/HUW",
#if (RULES_VERSION >= RULES_MACTALK)
			"[WH]=W",
#else
			"[WH]=WH",
#endif
			"[WAR]#=WEHR",
#if (RULES_VERSION >= RULES_MACTALK)
			"[WAR]=WAO5R",
#else
			"[WAR]=WAOR",
#endif
			"[WOR]^=WER",
			"[WR]=R",
#if (RULES_VERSION >= RULES_MACTALK)
			"[WOM]A=WUH5M",
			"[WOM]E=WIH5M",
#else
			"[WOM]A=WUHM",
			"[WOM]E=WIHM",
#endif
			"[WEA]R=WEH",
			"[WANT]=WAA5NT",
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"ANS[WER]=ER ", // extra space
#else
			"ANS[WER]=ER",
#endif
#if (RULES_VERSION >= RULES_MACTALK)
			"W[W]=", // remove double characters (if this is the second one in a chain...)
#endif
			"[W]=W",
		};

		const char* const xrule_eng[] =
		{
#if (RULES_VERSION >= RULES_MACINTALK)
			"?[X]?= BAY ",
			"? [X] ?=BAY",
#endif
#if (RULES_VERSION >= RULES_MACTALK)
			" :[X]: = EH4KS ",
#elif ((RULES_VERSION == RULES_C64) && defined(C64_RULES_BUGS))
			" [X] =EH4KR", // this is a typo and should be EH4KS to pronounce the letter properly
#else
			" [X] =EH4KS",
#endif
#if ((RULES_VERSION >= RULES_MACTALK) && (RULES_VERSION <= RULES_MACINTALKDEMO))
			// Note in mactalk and macintalk 1.0(demo), the two 'by' rules appear here, but cannot EVER work since they get caught by the EH4KS rule above.
			// This was fixed in macintalk 1.2 and onward by moving the rules above the EH4KS case.
			"?[X]?= BAY ", // these rules cannot ever be hit due to a bug
			"? [X] ?=BAY", // these rules cannot ever be hit due to a bug
#endif
			" [X]=Z",
#if (RULES_VERSION >= RULES_MACTALK)
			"X[X]=", // remove double characters (if this is the second one in a chain...)
#endif
			"[X]=KS",
		};

		const char* const yrule_eng[] =
		{
#if (RULES_VERSION >= RULES_MACTALK)
			" [Y] = WAY4 ",
#else
			" [Y] =WAY4",
#endif
			"[YOUNG]=YAHNX",
			" [YOUR]=YOHR",
			" [YOU]=YUW",
			" [YES]=YEHS",
			" [Y]=Y",
			"F[Y]=AY",
#if ((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS))
			"PS[YCH]=AYK ", // extra space
#else
			"PS[YCH]=AYK",
#endif
#if ((RULES_VERSION == RULES_C64) && defined(C64_RULES_BUGS))
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
#if (RULES_VERSION >= RULES_MACTALK)
			"Y[Y]=", // remove double characters (if this is the second one in a chain...)
#endif
			"[Y]=IH",
		};

		const char* const zrule_eng[] =
		{
#if (RULES_VERSION >= RULES_MACTALK)
			" :[Z]: = ZIY4 ",
			"Z[Z]=", // remove double characters (if this is the second one in a chain...)
#else
			" [Z] =ZIY4",
#endif
			"[Z]=Z",
		};

		const char* const punct_num_rule_eng[] =
		{
#if (RULES_VERSION < RULES_MACTALK)
			"[A]=", // this is a broken rule and can never be hit, may have been left from debugging, and only exists in the 6502 versions
#endif
#if (RULES_VERIONS >= RULES_MACTALK)
			"[ ]= ",
			"[...]= THRUW ",
			"[.]?= POY5NT ",
			"[.] =.",
			"[.]= ",
#endif
			"[!]=.",
#if (((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS)) || (RULES_VERIONS >= RULES_MACTALK))
			"[\"] =-AH5NKWOWT- ", // extra space... which became a feature later
#else
			"[\"] =-AH5NKWOWT-",
#endif
			"[\"]=KWOW4T-",
#if (RULES_VERIONS >= RULES_MACTALK)
			"[#]= NAH4MBER ",
			"C['S]=S",
			"G['S]=Z",
			"&['S]=IHZ",
			".['S]=Z",
			"#:&E['S]=IHZ",
			"#:.E['S]=Z",
			"#:^E['S]=S",
			"#['S]=Z",
			"['S]=S",
			"['T]=T",
			"['LL]=L",
			"['D]=D",
			"['M]=M",
			"[$]= DAA4LER ",
			"[%]= PERSEH4NT ",
			"[&]= AEND ",
#else
			"[#]= NAH4MBER",
			"[$]= DAA4LER",
			"[%]= PERSEH4NT",
			"[&]= AEND",
#endif
			"[']=",
#if (RULES_VERIONS >= RULES_MACTALK)
			"[*]= AE4STERIHSK ",
			"[+]= PLAH4S ",
#else
			"[*]= AE4STERIHSK",
			"[+]= PLAH4S",
#endif
			"[,]=,",
			" [-] =-",
			"[-]=",
#if (RULES_VERSION >= RULES_MACTALK)
			"[/]= SLAE4SH ",
#else
			"[.]= POYNT",
			"[/]= SLAE4SH",
#endif

			// *** begin digit rules; these are in their own table in mactalk and later
#if (RULES_VERIONS >= RULES_MACTALK)
			"[0]= ZIY4ROW ",
#else
			"[0]= ZIY4ROW",
#endif
			" [1ST]=FER4ST",
			" [10TH]=TEH4NTH",
#if (RULES_VERIONS >= RULES_MACTALK)
			" [10] = TEH4N ",
			"[1]= WAH4N ",
#else
			"[1]= WAH4N",
#endif
			" [2ND]=SEH4KUND",
#if (RULES_VERIONS >= RULES_MACTALK)
			"[2]= TUW4 ",
#else
			"[2]= TUW4",
#endif
			" [3RD]=THER4D",
#if (RULES_VERIONS >= RULES_MACTALK)
			"[3]= THRIY4 ",
			"[4]= FOH4R ",
#else
			"[3]= THRIY4",
			"[4]= FOH4R",
#endif
			" [5TH]=FIH4FTH",
#if (RULES_VERIONS >= RULES_MACTALK)
			"[5]= FAY4V ",
#else
			"[5]= FAY4V",
#endif
#if ((RULES_VERSION == RULES_C64) || defined(C64_ADDED_RULES))
			" [64] =SIH4KSTIY FOHR",
#endif
#if (RULES_VERIONS >= RULES_MACTALK)
			"[6]= SIH4KS ",
			"[7]= SEH4VUN ",
#else
			"[6]= SIH4KS",
			"[7]= SEH4VUN",
#endif
			" [8TH]=EY4TH",
#if (RULES_VERIONS >= RULES_MACTALK)
			"[8]= EY4T ",
			"[9]= NAY4N ",
#else
			"[8]= EY4T",
			"[9]= NAY4N",
#endif
			// *** end digit rules

#if (((RULES_VERSION == RULES_APPLE) && defined(APPLE_RULES_BUGS)) || (RULES_VERIONS >= RULES_MACTALK))
			"[:]=. ", // extra space... which became a feature later
#else
			"[:]=.",
#endif
			"[;]=.",
#if (RULES_VERIONS >= RULES_MACTALK)
			"[<]= LEH4S DHAEN ",
			"[=]= IY4KWULZ ",
			"[>]= GREY4TER DHAEN ",
#else
			"[<]= LEH4S DHAEN",
			"[=]= IY4KWULZ",
			"[>]= GREY4TER DHAEN",
#endif
			"[?]=.",
#if (RULES_VERIONS >= RULES_MACTALK)
			"[@]= AE6T ",
			"[(]=,",
			"[)]=,",
			"[^]= KAE4RIHT ",
			"[~]=NAA4T ",
			"[\]= ",
			"[[]= ",
			"[{]= ",
			"[}]= ",
			"[|]=OH4R ",
			"[_]= ",
			"[`]= ",
			"[]= ",
#else
			"[@]= AE6T",
			"[^]= KAE4RIXT",
#endif
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
