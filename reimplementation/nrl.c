// license:License-shortname
// copyright-holders:copyright holder names, comma delimited
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

// basic typedefs
typedef int8_t s8;
typedef uint8_t u8;
typedef int16_t s16;
typedef uint16_t u16;
typedef int32_t s32;
typedef uint32_t u32;
typedef int64_t s64;
typedef uint64_t u64;

// 'global' struct
typedef struct s_cfg
{
	//const char* const letters;
	u32 verbose;
} s_cfg;

// 'vector' structs for holding data
typedef struct vec_u8
{
	u32 elements; // number of elements in the vector, defaults to zero/empty
	u32 capacity; // amount of element-sized memory blocks currently allocated for the vector; i.e. capacity
	u8* data;
} vec_u8;

typedef struct vec_u32
{
	u32 elements; // number of elements in the vector, defaults to zero/empty
	u32 capacity; // amount of element-sized memory blocks currently allocated for the vector; i.e. capacity
	u32* data;
} vec_u32;

// 'vector' struct for holding a list of strings
typedef struct vec_strs
{
	u32 elements; // number of elements in the vector, defaults to zero/empty
	u32 capacity; // amount of element-sized memory blocks currently allocated for the vector; i.e. capacity
	vec_u8* data;
} vec_strs;

// put the nrl array as a long list of strings here, as a global
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


void parseRule(u8* input, u8* rule, u8* output)
{
	
	// if the rule doesn't match...
	return false;
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
	fprintf(stdout,"trying to print size of arule array, should be 33\n");
	fprintf(stdout,"sizeof(arule_eng): %d\n", sizeof(arule_eng));
	fprintf(stdout,"(&arule_eng)[1] - arule_eng: %d\n", (&arule_eng)[1] - arule_eng);
	fprintf(stdout,"sizeof(arule_eng)/sizeof(*arule_eng): %d\n", sizeof(arule_eng)/sizeof(*arule_eng));
	fflush(stdout);

//	fclose(out); 
//	free(dataArray);
//	dataArray = NULL;
	return 0;
}