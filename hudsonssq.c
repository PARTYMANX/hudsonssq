// hudson ssq decoder
// decodes type 9 step data, used by Mario Mix and the Hottest Party series, maybe more

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#define MEASURE_LENGTH 4096

typedef enum {
	CHUNK_TYPE_TEMPO = 0x01,
	CHUNK_TYPE_MUSH_STEPS = 0x09
} chunkType_t;

typedef struct chunk {
	struct chunk *next;
	uint16_t type;
	uint16_t param1;
	uint16_t param2;
	uint16_t param3;
} chunk_t;

typedef struct {
	chunk_t *next;
	uint16_t type;
	uint16_t tickRate;	// tick rate (in ticks per second)
	uint16_t entryCount;
	uint16_t param3;	// unused

	int32_t *offsets;	// note offset: musical offset (in 4096ths of a measure)
	int32_t *ticks;	// tick offset: temporal offset (in ticks)
} chunk_tempo_t;

typedef enum {
	P1_LEFT = 0x01,
	P1_DOWN = 0x02,
	P1_UP = 0x04,
	P1_RIGHT = 0x08,
	P2_LEFT = 0x10,
	P2_DOWN = 0x20,
	P2_UP = 0x40,
	P2_RIGHT = 0x80
} panelBits_t;

// mush data - chunk type 9.  like regular step data but with data appended to define extra arrow types, E.G. enemies in DDR Mario Mix's "Mush Mode"

typedef struct {
	int32_t offset;	// note offset (measures of 4096)
	uint8_t panels;	// bitmask of panels in step
	uint8_t type;	// determines extra data
	uint8_t mushData[8];	// determines mush data
} mushStep_t;

typedef struct {
	int32_t type;
	int32_t param1;
	int32_t param2;
} mushItem_t;

typedef struct {
	int32_t itemCount;
	mushItem_t *items;
} mushData_t;

typedef struct {
	chunk_t *next;
	uint16_t type;
	uint16_t difficulty;
	uint16_t stepCount;
	uint16_t param3;	// unused

	mushStep_t *steps;
	mushData_t mushData;

} chunk_mushSteps_t;

typedef struct {
	chunk_t *head;
} ssq_t;

int32_t read_i32(FILE* f) {
	int32_t result = 0;
	fread(&result, sizeof(int32_t), 1, f);	
	return result;
}

int16_t read_i16(FILE* f) {
	int16_t result = 0;
	fread(&result, sizeof(int16_t), 1, f);	
	return result;
}

uint8_t read_u8(FILE* f) {
	uint8_t result = 0;
	fread(&result, sizeof(uint8_t), 1, f);	
	return result;
}

chunk_tempo_t *readTempoChunk(FILE* f, int32_t size) {
	chunk_tempo_t *result = malloc(sizeof(chunk_tempo_t));

	result->next = NULL;
	result->type = CHUNK_TYPE_TEMPO;

	int32_t padding = (4 - (size % 4)) % 4;

	result->tickRate = read_i16(f);
	result->entryCount = read_i16(f);
	result->param3 = read_i16(f);

	result->offsets = malloc(sizeof(int32_t) * result->entryCount);
	result->ticks = malloc(sizeof(int32_t) * result->entryCount);

	for (int i = 0; i < result->entryCount; i++) {
		result->offsets[i] = read_i32(f);
	}	

	for (int i = 0; i < result->entryCount; i++) {
		result->ticks[i] = read_i32(f);
	}	

	fseek(f, padding, SEEK_CUR);

	return result;
}

char *getDifficultyName(int16_t difficulty) {
	switch(difficulty) {
		case 0x0114:
			return "Single Medium";
		case 0x0214:
			return "Single Hard";
		case 0x0314:
			return "Single Very Hard";
		case 0x0414:
			return "Single Easy";
		case 0x0614:
			return "Single Super Hard";
		case 0x0118:
			return "Double Medium";
		case 0x0218:
			return "Double Hard";
		case 0x0318:
			return "Double Very Hard";
		case 0x0418:
			return "Double Easy";
		case 0x0618:
			return "Double Super Hard";
		default:
			return "Unknown";
	}
}

// NOTE: Mario Mix only	- Hottest Party 1 (possibly the rest) use similar behaving data
char *getEnemyName(int32_t enemy) {
	switch(enemy) {
		case 0x00000001:
			return "Goomba";	// regular step, but looks like a goomba (Hottest Party uses this to encode a wiimote shake).  decodes to regular step
		case 0x00000002:
			return "Koopa Troopa";	// two steps, one for the koopa, one for the shell.  decodes to two steps, spaced as quarter notes
		case 0x00000003:
			return "Bob-Omb and Podoboo";	// step
		case 0x00000004:
			return "Cheep Cheep";	// step
		case 0x00000005:
			return "Spiny";	// mine
		case 0x00000006:
			return "Blooper";	// step
		case 0x00000007:
			return "Hammer";	// step
		case 0x00000008:
			return "Coin Switch";	// step
		case 0x00000009:
			return "Boo";	// step
		case 0x0000000a:
			return "Bob-Omb and Podoboo";	// step
		case 0x0000000b:
			return "Cheep Cheep (Rearrange)";	// step (moves between columns)
		case 0x0000000c:
			return "Coin Switch";	// step
		case 0x0000000d:
			return "Freezie";	// step
		case 0x0000000e:
			return "Ice Spiny";	// mine
		case 0x0000000f:
			return "Bullet Bill";	// step
		case 0x00000010:
			return "Rocket Part";	// step
		default:
			return "Unknown";
	}
}

// get the number of bits set in a step
int getArrowCount(uint8_t step) {
	int result = 0;

	while (step) {
		if (step & 0x01) {
			result++;
		}
		step = step >> 1;
	}

	return result;
}

#define STEP_DEBUG
#define ENEMY_DEBUG

// linked list of extra (only freeze) data.  unused in mario mix
struct extraList {
	int32_t count;
	int32_t size;
	int32_t *indices;
};

struct extraList extraListInit() {
	struct extraList result;

	result.count = 0;
	result.size = 16;
	result.indices = malloc(sizeof(int32_t) * result.size);

	return result;
}

void extraListAppend(struct extraList *list, int32_t value) {
	if (list->count >= list->size) {
		list->size *= 2;
		list->indices = realloc(list->indices, sizeof(int32_t) * list->size);
	}

	list->indices[list->count] = value;
	list->count++;
}

chunk_mushSteps_t *readMushStepsChunk(FILE* f, int32_t size) {
	chunk_mushSteps_t *result = malloc(sizeof(chunk_mushSteps_t));

	result->next = NULL;
	result->type = CHUNK_TYPE_MUSH_STEPS;

	int32_t padding = (4 - (size % 4)) % 4;

	result->difficulty = read_i16(f);
	result->stepCount = read_i16(f);
	result->param3 = read_i16(f);

	result->steps = malloc(sizeof(mushStep_t) * result->stepCount);

	int32_t postDataOffset = read_i32(f);	// offset of mush/extra data

	for (int i = 0; i < result->stepCount; i++) {
		result->steps[i].offset = read_i32(f);
	}	

	int32_t stepMushBytes = 0;
	int32_t mushItemCount = 0;
	int32_t numExtras = 0;
	struct extraList extraList = extraListInit();

	for (int i = 0; i < result->stepCount; i++) {
		result->steps[i].panels = read_u8(f);
		result->steps[i].type = 0x00;

		int arrowCount = getArrowCount(result->steps[i].panels);
		for (int j = 0; j < arrowCount; j++) {
			stepMushBytes++;
			result->steps[i].mushData[j] = read_u8(f);
			if (result->steps[i].mushData[j]) {
				mushItemCount++;
			}
		}

		if (arrowCount == 0) {
			numExtras++;
			extraListAppend(&extraList, i);
		}
	}	

	// skip unaligned data
	int32_t stepDataLen = (result->stepCount * 4) + (result->stepCount + stepMushBytes);
	int32_t stepDataPadding = (4 - (stepDataLen % 4)) % 4;

	fseek(f, stepDataPadding, SEEK_CUR);

	// mush mode enemy metadata
	if (postDataOffset != 0) {
		int32_t postDataMagic = read_i32(f);	// 0x54455845 - mush magic number
		int32_t endPostDataOffset = read_i32(f);	// offset to end of mush and extra data
	}

	// step mush data
	if (mushItemCount > 0) {
		// following may be missing if there is no mush data but there is freeze data
		int32_t mushMagic = read_i32(f);	// 0x52414843 - step mush magic number
		int32_t endItemOffset = read_i32(f);	// offset to end of mush item data

		result->mushData.itemCount = (endItemOffset - 0x00000008) / 0x0000000C;	// cheating a bit here;  getting the item type count via the end offset
		result->mushData.items = malloc(sizeof(mushItem_t) * result->mushData.itemCount);

		for (int i = 0; i < result->mushData.itemCount; i++) {
			result->mushData.items[i].type = read_i32(f);
			result->mushData.items[i].param1 = read_i32(f);
			result->mushData.items[i].param2 = read_i32(f);
		}
	} else {
		result->mushData.itemCount = 0;
		result->mushData.items = NULL;
	}

	// extra data header
	if (numExtras > 0) {
		int32_t extraMagic = read_i32(f);	// 0x5a455246 - extra mush magic number
		int32_t endExtraOffset = read_i32(f);	// offset to end of freeze data
	}

	// next data is DWORD aligned, but mush data is all DWORD aligned anyway so we don't need to skip any bytes
	
	int32_t extraMushBytes = 0;

	// freeze data
	for (int i = 0; i < numExtras; i++) {
		int32_t idx = extraList.indices[i];

		result->steps[idx].panels = read_u8(f);
		result->steps[idx].type = read_u8(f);

		int arrowCount = getArrowCount(result->steps[idx].panels);
		for (int j = 0; j < arrowCount; j++) {
			extraMushBytes++;
			result->steps[idx].mushData[j] = read_u8(f);
		}
	}

	free(extraList.indices);

	int32_t extraDataLen = (numExtras * 2) + (extraMushBytes);
	int32_t extraDataPadding = (4 - (extraDataLen % 4)) % 4;

	fseek(f, extraDataPadding, SEEK_CUR);

	fseek(f, padding, SEEK_CUR);

	return result;
}

chunk_t *readUnknownChunk(FILE* f, int32_t size, int16_t type) {
	chunk_t *result = malloc(sizeof(chunk_t));

	result->next = NULL;
	result->type = type;

	int32_t padding = (4 - (size % 4)) % 4;

	result->param1 = read_i16(f);
	result->param2 = read_i16(f);
	result->param3 = read_i16(f);

	fseek(f, size - (12) + padding, SEEK_CUR);

	return result;
}

chunk_t *readChunk(FILE* f) {
	int32_t size = read_i32(f);
	if (size == 0) {
		return NULL;
	}

	chunk_t *result = NULL;

	int16_t type = read_i16(f);

	switch(type) {
		case 1:
			result = (chunk_t *)readTempoChunk(f, size);
			break;
		case 2:
			result = readUnknownChunk(f, size, 2);
			break;
		case 9:
			result = (chunk_t *)readMushStepsChunk(f, size);
			break;
		default:
			result = readUnknownChunk(f, size, type);
	}

	return result;
}

void printTempoChunk(chunk_tempo_t *chunk) {
	printf("TEMPO CHUNK: tick rate = %d ticks per second, entry count = %d, param3 = %d\n", chunk->tickRate, chunk->entryCount, chunk->param3);

	for (int i = 1; i < chunk->entryCount; i++) {
		int32_t dOffset = chunk->offsets[i] - chunk->offsets[i-1];
		int32_t dTick = chunk->ticks[i] - chunk->ticks[i-1];
		float tempo = ((float)dOffset / 4096.0f) / (((float)dTick / (float)chunk->tickRate) / 240.0f);

		printf("  TEMPO VALUE: offset = %d - %d, tempo = %f bpm\n", chunk->offsets[i-1], chunk->offsets[i], tempo);
	}
}

void printMushStepsChunk(chunk_mushSteps_t *chunk) {
	printf("MUSH STEP CHUNK: difficulty = %s (%04x), step count = %d, param3 = %d\n", getDifficultyName(chunk->difficulty), chunk->difficulty, chunk->stepCount, chunk->param3);
	
	for (int i = 0; i < chunk->stepCount; i++) {
		char steps[9] = "<V^><V^>";

		for (int j = 0; j < 8; j++) {
			if (!(chunk->steps[i].panels & 0x01 << j)) {
				steps[j] = ' ';
			}
		}	

		printf("  STEP %04d: OFFSET = %08d, TYPE = %02x, PANELS = %s", i + 1, chunk->steps[i].offset, chunk->steps[i].type, steps);

		int arrowCount = getArrowCount(chunk->steps[i].panels);
		for (int j = 0; j < arrowCount; j++) {
			
			if (chunk->steps[i].mushData[j]) {
				printf(", %s (%02x)", getEnemyName(chunk->mushData.items[chunk->steps[i].mushData[j] - 1].type), chunk->steps[i].mushData[j]);
			} else {
				printf(", None (%02x)", chunk->steps[i].mushData[j]);
			}
		}
		printf("\n");
	}

	for (int i = 0; i < chunk->mushData.itemCount; i++) {
		printf("  MUSH ITEM: %s (%08x), %08x, %08x\n", getEnemyName(chunk->mushData.items[i].type), chunk->mushData.items[i].type, chunk->mushData.items[i].param1, chunk->mushData.items[i].param2);
	}
}

void printUnknownChunk(chunk_t *chunk) {
	printf("UNKNOWN CHUNK: type = %04x, param1 = %04x, param2 = %d, param3 = %d\n", chunk->type, chunk->param1, chunk->param2, chunk->param3);
}

void printChunk(chunk_t *chunk) {
	switch(chunk->type) {
		case 1:
			printTempoChunk((chunk_tempo_t *)chunk);
			break;
		case 2:
			printUnknownChunk(chunk);
			break;
		case 9:
			printMushStepsChunk((chunk_mushSteps_t *)chunk);
			break;
		default:
			printUnknownChunk(chunk);
	}
}

void printSsq(ssq_t *ssq) {
	chunk_t *node = ssq->head;
	while (node != NULL) {
		printChunk(node);
		node = node->next;
	}
}

// DECODING NOTES: 
// KOOPAS GET A NOTE ONE BEAT LATER
// GOOMBAS BECOME REGULAR STEPS
// SPINYS AND ICE SPINYS BECOME MINES
// HAMMERS BECOME REGULAR STEPS
// EVERYTHING ELSE IS A REGULAR STEP?

typedef struct {
	char *title;

	uint32_t offset;

	uint32_t tempoCount;
	char **tempos;

	uint32_t stopCount;
	char **stops;
} simfile_header_t;

typedef struct {
	uint8_t step[8];
} simfile_step_t;

typedef struct {
	char *style;
	char *difficulty;

	uint32_t stepCount;
	simfile_step_t *steps;
} simfile_chart_t;

typedef struct {
	simfile_header_t header;

	uint32_t chartCount;
	simfile_chart_t *charts;
} simfile_t;

void printSMHeader(FILE *f, chunk_tempo_t *tempoChunk) {
	fprintf(f, "#TITLE:(title);");
	fprintf(f, "#SUBTITLE:;");
	fprintf(f, "#ARTIST:;");
	fprintf(f, "#TITLETRANSLIT:;");
	fprintf(f, "#SUBTITLETRANSLIT:;");
	fprintf(f, "#ARTISTTRANSLIT:;");
	fprintf(f, "#CREDIT:;");
	fprintf(f, "#BANNER:;");
	fprintf(f, "#BACKGROUND:;");
	fprintf(f, "#LYRICSPATH:;");
	fprintf(f, "#CDTITLE:;");
	fprintf(f, "#MUSIC:;");
	fprintf(f, "#OFFSET:;");
	fprintf(f, "#SAMPLESTART:;");
	fprintf(f, "#SAMPLELENGTH:;");
	fprintf(f, "#SELECTABLE:YES;");
	fprintf(f, "#DISPLAYBPM:;");
	fprintf(f, "#BPMS:(measure)=(tempo),(measure)=(tempo);");
	fprintf(f, "#STOPS:(measure)=(length),;");
	fprintf(f, "#BGCHANGES:;");
}

char *getStyle(uint16_t difficulty) {
	switch(difficulty & 0x00FF) {
		case 0x0014:
			return "single";
		case 0x0018:
			return "double";
		default:
			// TODO: something else here to note the error
			return "double";
	}
}

char *getSMDifficulty(uint16_t difficulty) {
	switch(difficulty & 0xFF00) {
		case 0x0100:
			return "Easy";
		case 0x0200:
			return "Medium";
		case 0x0300:
			return "Hard";
		case 0x0400:
			return "Beginner";
		case 0x0600:
			return "Challenge";
		default:
			return "Edit";
	}
}

simfile_chart_t *convertMushSteps(chunk_mushSteps_t *chunk) {
	

	return NULL;
}

void printMushSteps(FILE *f, chunk_mushSteps_t *chunk) {
	fprintf(f, "#NOTES:\n");
	fprintf(f, "\tdance-%s:\n", getStyle(chunk->difficulty));
	fprintf(f, "\t:\n");	// author
	fprintf(f, "\t%s:\n", getSMDifficulty(chunk->difficulty));
	fprintf(f, "\t1:\n");	// level
	fprintf(f, "\t0.000,0.000,0.000,0.000,0.000:\n");	// groove radar

	// preprocess steps (bake freezes and mush data), expand steps to byte
	
	// write steps
}

// sm data is writter measure-by-measure, with a comma separating each.  the number of lines in a measure dictates its rhythm.  i.e. if there are 4 lines in a measure, it is made up of quarter notes.
// the end of a chart is signaled by a semicolon

// ssq freezes are written as: normal note to start, type 1 note to end.

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("Hudson SSQ Decoder\nUsage: %s [filename...]\n", argv[0]);
		return 0;
	}

	for (int i = 1; i < argc; i++) {
		FILE *f = fopen(argv[i], "rb");

		printf("%s\n", argv[i]);

		ssq_t *ssq = malloc(sizeof(ssq_t));
		ssq->head = NULL;

		chunk_t **next = &ssq->head;
		while(1) {
			*next = readChunk(f);
			if (!*next) {
				break;
			}

			next = &((*next)->next);
		};

		printSsq(ssq);

		printf("\n");
	}
}
