// license:License-shortname
// copyright-holders:copyright holder names, comma delimited
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

// verbose macros
#define v_printf(v, ...) \
	do { if (v) { fprintf(stderr, __VA_ARGS__); fflush(stderr); } } while (0)
#define eprintf(v, ...) \
	do { if (v) { fprintf(stdout, __VA_ARGS__); fflush(stdout); } } while (0)

// verbosity defines; V_DEBUG can be changed here to enable/disable debug messages
#define V_DEBUG (1)
#define V_ERR (1)
#define V_PARAM (c.verbose & (1<<0))
#define V_1 (c.verbose & (1<<1))

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
	v_printf(V_DEBUG,"DEBUG: vec_char32 capacity: %d, elements: %d\n", l->capacity, l->elements);
}

void vec_char32_dbg_print(vec_char32* l)
{
	v_printf(V_DEBUG,"DEBUG: vec_char32 contents: '");
	for (u32 i=0; i < l->elements; i++)
	{
		v_printf(V_DEBUG,"%c", (char)l->data[i]);
	}
	v_printf(V_DEBUG,"'\n");
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

// Digits, 0-9
#define A_DIGIT 0x01
// Punctuation characters that do not end a word "!\"#$%\'*+,-./0123456789:;<=>?@^"
// Note the punctuation characters that DO end a word or otherwise have special handling are " ()[\]_"
#define A_PUNCT 0x02
// Unvoiced Affricate, aka Non-Palate or 'NONPAL' "DJLNRSTZ"
#define A_UAFF 0x04
// Voiced consonants "BDGJLMNRVWZ"
#define A_VOICED 0x08
// Sibilants "CGJSXZ"
#define A_SIBIL 0x10
// Consonants "BCDFGHJKLMNPQRSTVWXZ"
#define A_CONS 0x20
// Vowels "AEIOUY"
#define A_VOWEL 0x40
// Letters "ABCDEFGHIJKLMNOPQRSTUVWXYZ'" - any character with this flag has a rule attached to it, this includes the apostrophe
#define A_LETTER 0x80

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

// preprocess: add a leading space, and turn all characters from lowercase into capital letters. return the length of the string.
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

#define LPAREN '['
#define RPAREN ']'

s32 processRule(const sym_ruleset const ruleset, const vec_char32* const input, const u32 inpos, vec_char32* output, s_cfg c)
{
	// iterate through the rules
	u32 i = 0;
	for (i = 0; i < ruleset.num_rules; i++)
	{
		v_printf(V_DEBUG, "found a rule %s\n", ruleset.rule[i]);
		// part 1: check the exact match section of the rule, between the parentheses
		// (and get the indexes to the two parentheses and the equals symbol, which will be used in parts 2 and 3)
		s32 lparen_idx = -1;
		s32 rparen_idx = -1;
		s32 equals_idx = -1;
		/* slow but safe... */
		u32 rulelen = strlen(ruleset.rule[i]);
		lparen_idx = strnfind(ruleset.rule[i], LPAREN, rulelen);
		rparen_idx = strnfind(ruleset.rule[i], RPAREN, rulelen);
		equals_idx = strnfind(ruleset.rule[i], '=', rulelen);
		v_printf(V_DEBUG, "  safe: left paren found at %d, right paren found at %d, equals found at %d\n", lparen_idx, rparen_idx, equals_idx);

		/* faster but less safe... will run off the end of the rule string if a rule has no equals sign and doesn't end with a NULL '/0'
		 (which should never happen) */
		lparen_idx = -1;
		rparen_idx = -1;
		equals_idx = -1;
		/*for (rparen_idx = 0; ruleset.rule[i][rparen_idx] != RPAREN; rparen_idx++)
		{
			if (ruleset.rule[i][rparen_idx] == LPAREN)
				lparen_idx = rparen_idx;
		}*/
		for (equals_idx = 0; ((ruleset.rule[i][equals_idx] != '=') && (ruleset.rule[i][equals_idx] != '\0')); equals_idx++)
		{
			if (ruleset.rule[i][equals_idx] == LPAREN)
				lparen_idx = equals_idx;
			if (ruleset.rule[i][equals_idx] == RPAREN)
				rparen_idx = equals_idx;
		}
		v_printf(V_DEBUG, "unsafe: left paren found at %d, right paren found at %d, equals found at %d\n", lparen_idx, rparen_idx, equals_idx);
		int n = (rparen_idx - 1) - (lparen_idx + 1); // number of letters in exact match part of the rule
		
		// part1: compare exact match; basically a slightly customized 'strncmp()'
		{
			int offset = 0; // offset within rule of exact match
			while ( n && input->data[inpos+offset] && (input->data[inpos+offset] == ruleset.rule[i][lparen_idx+offset]) )
			{
				offset++;
				n--;
			}
			v_printf(V_DEBUG, "attempted strncmp of rule resulted in %d\n",n);
			if (n != 0) continue; // mismatch, go to next rule.
			// if we got here, the fixed part of the rule matched.
			v_printf(V_DEBUG, "rule %s matched the input string, at rule offset %d\n", ruleset.rule[i], lparen_idx+1);
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
				v_printf(V_DEBUG, "rulechar is %c(%02x) at ruleoffset %d, inpchar is %c(%02x) at inpoffset %d\n", rulechar, rulechar, lparen_idx+ruleoffset, inpchar, inpchar, inpos+inpoffset);
				if (isLetter(rulechar, c)) // letter in rule matches that letter exactly, only.
				{
					// it's a letter, directly compare it to the input character
					if (rulechar == inpchar)
					{
						// we have a match, back the offsets off each by 1
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
						// we have a match, back the offsets off each by 1
						ruleoffset--;
						inpoffset--;
					}
					else
					{
						// mismatch
						fail = true;
					}
				}
				else if (rulechar == '#') // # matches any vowel
				{
					if (isVowel(inpchar,c))
					{
						// we have a match, back the offsets off each by 1
						ruleoffset--;
						inpoffset--;
					}
					else
					{
						// mismatch
						fail = true;
					}
				}
				else if (rulechar == '.') // . matches any voiced consonant
				{
					if (isVoiced(inpchar,c))
					{
						// we have a match, back the offsets off each by 1
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
						// we have a match, back the offsets off each by 1
						ruleoffset--;
						inpoffset--;
					}
					else if (inpchar == 'H') // could be CH or SH!
					{
						// the beginning of the input array is ALWAYS a space, so since we saw an 'H' 
						// we can't be at offset less than 1 here, so it is always safe to decrement
						inpoffset--;
						inpchar = input->data[inpos+inpoffset]; // load another char...
						if ((inpchar == 'C') || (inpchar == 'S'))
						{
							// we have a match, back the offsets off each by 1
							ruleoffset--;
							inpoffset--;
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
				else if (rulechar == '@') // @ matches any unvoiced affricative aka nonpalate; note special cases for TH, CH, SH
				{
					if (isUaff(inpchar,c))
					{
						// we have a match, back the offsets off each by 1
						ruleoffset--;
						inpoffset--;
					}
					else if (inpchar == 'H') // could be TH, CH or SH!; NOTE: the original reciter has a bug here and these tests ALWAYS fail!
					{
#ifdef ORIGINAL_BUGS
						fail = true
#else
						// the beginning of the input array is ALWAYS a space, so since we saw an 'H' 
						// we can't be at offset less than 1 here, so it is always safe to decrement
						inpoffset--;
						inpchar = input->data[inpos+inpoffset]; // load another char...
						if ((inpchar == 'T') || (inpchar == 'C') || (inpchar == 'S'))
						{
							// we have a match, back the offsets off each by 1
							ruleoffset--;
							inpoffset--;
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
						// we have a match, back the offsets off each by 1
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
						// we have a match, back the offsets off each by 1
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
					while ((inpos+inpoffset >= 0) && isCons(inpchar,c))
					{
						inpoffset--;
						inpchar = input->data[inpos+inpoffset];
					}
					ruleoffset--;
				}
				else
				{
					v_printf(V_ERR, "got an invalid rule character of '%c'(0x%02x), exiting!\n", rulechar, rulechar);
					exit(1);
				}
			}
			if (fail) continue; // mismatch, move on to the next rule.
			/// TODO
		}
		
		// part3: match the rule suffix
		{
			/// TODO
		}
		
		// if we got this far, dump the rule right hand side past the = sign to output, then
		// consume the number of characters between the parentheses by returning inpos + that number
		{
			// crude strcpy
			while (ruleset.rule[i][++equals_idx] != '\0')
			{
				vec_char32_append(output, ruleset.rule[i][equals_idx]);
			}
			return inpos+n;
		}
	}
	// did we break out with a valid rule?
	if (i == ruleset.num_rules)
	{
		v_printf(V_ERR, "unable to find any matching rule, exiting!\n");
		exit(1);
	}
	return inpos; // TODO
}

void processPhrase(const sym_ruleset* const ruleset, const vec_char32* const input, vec_char32* output, s_cfg c)
{
	v_printf(V_DEBUG, "processPhrase called, phrase has %d elements\n", input->elements);
	s32 inpos = -1;
	char32_t inptemp;
	while (((inptemp = input->data[++inpos])||(1)) && (inptemp != RECITER_END_CHAR)) // was (curpos < input->elements)
	{
		v_printf(V_DEBUG, "position is now %d (%c)\n", inpos, input->data[inpos]);
		if (input->data[inpos] == '.') // is this character a period?
		{
			v_printf(V_DEBUG, "character is a period...\n");
			if (isDigit(input->data[++inpos], c)) // is the character after the period a digit?
			{
				v_printf(V_DEBUG, " followed by a digit...\n");
				u8 inptemp_features = c.ascii_features[inptemp&0x7f]; // save features from initial character
				if (isPunct(inptemp, c)) // if the initial character was punctuation
				{
					v_printf(V_DEBUG, " and the character before the period was a punctuation symbol!\n");
					// look up PUNCT_DIGIT rules
					inpos = processRule(ruleset[RULES_PUNCT_DIGIT], input, inpos, output, c);
					// THIS CASE IS FINISHED
				}
				else
				{
					v_printf(V_DEBUG, " but the character before the period was not a punctuation symbol.\n");
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
							v_printf(V_ERR, "found a character that isn't punct/digit, nor letter, nor null, bail out!\n");
							exit(1);
							// THIS CASE IS FINISHED
						}
					}
				}
			}
			else
			{
				v_printf(V_DEBUG, " but not followed by a digit, so treat it as a pause.\n");
				vec_char32_append(output, '.'); // add a period to the output word.
				// THIS CASE IS FINISHED
			}
		}
		else
		{
			v_printf(V_DEBUG, "character is not a period...");
			u8 inptemp_features = c.ascii_features[inptemp&0x7f]; // save features from initial character
			if (isPunct(inptemp, c)) // if the initial character was punctuation
			{
				v_printf(V_DEBUG, " and the initial character was a punctuation symbol!\n");
				// look up PUNCT_DIGIT rules
				inpos = processRule(ruleset[RULES_PUNCT_DIGIT], input, inpos, output, c);
				// THIS CASE IS FINISHED
			}
			else
			{
				v_printf(V_DEBUG, " but the initial charater was not a punctuation symbol.\n");
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
						v_printf(V_ERR, "found a character that isn't punct/digit, nor letter, nor null, bail out!\n");
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
		1, // verbose (was 0)
	};

	//{
		const char* const arule_eng[] =
		{
			"[A] =/AX/",
			"[ARE] =/AA R/",
			"[AR]O=/AX R/",
			"[AR]#=/EH R/",
			"^[AS]#=/EY S/",
			"[A]WA=/AX/",
			"[AW]=/AO/",
			":[ANY]=/EH N IY/",
			"[A]^+#=/EY/",
			"#:[ALLY]=/AX L IY/",
			"[AL]#=/AX L/",
			"[AGAIN]=/AX G EH N/",
			"#:[AG]E=/IH JH/",
			"[A]^+:#=/AE/",
			":[A]^+ =/EY/",
			"[A]^%=/EY/",
			"[ARR]=/AX R/",
			"[ARR]=/AE R/",
			":[AR] =/AA R/",
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
			":[ABLE]=/EY B AX L/",
			"[ABLE]=/AX B AX L/",
			"[ANG]+=/EY N JH/",
			"[A]=/AE/",
		};

		const char* const brule_eng[] =
		{
			"[BE]^#=/B IH/",
			"[BEING]=/B IY IH NX/",
			"[BOTH] =/B OW TH/",
			"[BUS]#=/B IH Z/",
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

		const char* const punct_num_rule_eng[] =
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
				if (paramidx == (argc-0)) { v_printf(V_ERR,"E* Too few arguments for -v parameter!\n"); usage(); exit(1); }
				if (!sscanf(argv[paramidx], "%d", &c.verbose)) { v_printf(V_ERR,"E* Unable to parse argument for -v parameter!\n"); usage(); exit(1); }
				paramidx++;
				break;
			case '\0':
				// end of string for parameter, go to next param
				paramidx++;
				break;
			default:
				{ v_printf(V_ERR,"E* Invalid option!\n"); usage(); exit(1); }
				break;
		}
	}
	v_printf(V_PARAM,"D* Parameters: verbose: %d\n", c.verbose);


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
	vec_char32* d_raw = vec_char32_alloc(4);

	// read contents of dataArray into vec_char32
	for (u32 i = 0; i < len; i++)
	{
		vec_char32_append(d_raw,dataArray[i]);
	}
	v_printf(V_DEBUG,"Input phrase stats are:\n");
	vec_char32_dbg_stats(d_raw);
	vec_char32_dbg_print(d_raw);

	// free the input array
	free(dataArray);
	dataArray = NULL;

	// allocate another vector for preprocessing
	vec_char32* d_in = vec_char32_alloc(4);
	// preprocess d_raw into d_in
	preProcess(d_raw, d_in, c);
	vec_char32_free(d_raw);

	v_printf(V_DEBUG,"Preprocessing done, stats are now:\n");
	vec_char32_dbg_stats(d_in);
	vec_char32_dbg_print(d_in);

	// do stuff with preprocessed phrase here, i.e. the rest of the owl
	// allocate another vector for output
	vec_char32* d_out = vec_char32_alloc(4);
	processPhrase(ruleset, d_in, d_out, c);
	vec_char32_free(d_in);
	v_printf(V_DEBUG,"Processing done, stats are now:\n");
	vec_char32_dbg_stats(d_out);
	vec_char32_dbg_print(d_out);

	vec_char32_free(d_out);

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