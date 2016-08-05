/* A baseline tiff reader that converts a single layer RGB image to a Bilevel (1 bit) rasterised TIFF image. */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

	/* Some TIF spec data types */
#define IMAGE_WIDTH 		256
#define IMAGE_HEIGHT 		257
#define ROWS_PER_STRIP 		278
#define STRIP_OFFSETS 		273
#define STRIP_BYTE_COUNTS	279
#define COMPRESSION		259
#define	PHOTOMETRIC		262
#define SAMPLES_PER_PIXEL	277
#define BITS_PER_SAMPLE		258

	/* We only use unsigned types */
typedef unsigned char 	t_1byte;
typedef unsigned short 	t_2byte;
typedef unsigned int 	t_4byte;
typedef unsigned long 	t_8byte;

	/* 12 bytes total */
typedef struct _TifTag {
	t_2byte	TagId;		/* The tag identifier */
	t_2byte	DataType;	/* The scalar type of the data items */
	t_4byte	DataCount;	/* The number of items in the tag data */
	t_4byte	DataOffset;	/* The byte offset to the data items */
} TIFTAG;


typedef struct _TiffHeader {
	t_2byte byteOrder;
	t_2byte tiffID;
	t_4byte IFDOffset;	
} TiffHeader;

typedef struct _ImageInfo {
	int imageWidth;
	int imageHeight;
	int rowsPerStrip;
	int stripOffsets;
	int stripByteCounts;
	unsigned char samplesPerPixels;
	unsigned char bitsPerSample; // assuming all samples have the same bit depth
	unsigned char compression;
} ImageInfo;

typedef struct _RGBData {
	t_1byte *red;
	t_1byte *green;
	t_1byte *blue;
} RGBData;

union tifHeader_union {
		/* used to create an 8 byte header from an array of values */
	struct {
		t_1byte byteOrder[2];
		t_1byte tiffID[2];
		t_4byte offset;
	};
	t_8byte header;
};

union tiftag_union {
		/* used to convert an array of 12 chars into a 12 byte struct */ 
	TIFTAG tiftag;
	t_1byte alpha[12];
};

union int_union {
		/* used to convert an array of 4 chars into a 4 byte int */
	t_4byte num;
	t_1byte alpha[4];
};

union short_union {
		/* used to convert an array of 2 chars into a 2 byte short */
	t_2byte num;
	t_1byte alpha[2];
};

union doubleShort_union {
	struct {
		t_2byte shortTop;
		t_2byte shortBottom;
	};
	t_4byte mergedInt;
};

void die(const char *message);

void extract_int_from_4bytes(t_1byte bytes[], bool isLittleEndian, int start, t_4byte *number);
void extract_short_from_2bytes(t_1byte bytes[], bool isLittleEndian, int start, t_2byte *number);
void extract_tiftag_from_12bytes(t_1byte bytes[], bool isLittleEndian, int start, TIFTAG *tiftag);
int getDataSize(t_2byte dataType, t_4byte dataCount);

void simpleForwardDither(t_1byte *pixelArray, long pixelArraySize);
void floydSteinbergDither(t_1byte *pixelArray, long imageWidth, long imageHeight);

char clampInt(int n) {
    if(n < 0)
        return 0;
    else if(n > 255)
        return 255;
    return n;
}

int main(int argc, char *argv[]) {

		/* check input filename */
	if(argc < 3) die("Please provide [FILENAME_IN] and [FILENAME_OUT].");



		/* print some byte lengts of types */
	printf("================\n");
	printf("  %lu bytes (unsigned char)  : t_1byte\n", sizeof(t_1byte));
	printf("  %lu bytes (unsigned short) : t_2byte\n", sizeof(t_2byte));
	printf("  %lu bytes (unsigned int)   : t_4byte\n", sizeof(t_4byte));
    	printf("  %lu bytes (unsigned long)  : t_8byte\n", sizeof(t_8byte));
	printf("================\n");


		/* get the filename */
	char filename_in[64];
	strncpy(filename_in, argv[1], 64);
	filename_in[64-1] = '\0'; // terminate string

	char filename_out[64];
	strncpy(filename_out ,argv[2], 64);
	filename_out[64-1] = '\0'; // terminate string





		/* create the file pointer */
	FILE *file_ptr;
	printf("Opening file %s\n", filename_in);

	





		/* try opening the file for reading */

	file_ptr = fopen(filename_in, "rb");
	if(file_ptr == NULL) die("File not found");
	printf("File \"%s\" open for reading.\n", filename_in);



	


		/* check first four bytes to see if it is a tiff file */
		/* 
			Tif file headers, first four bytes:
			0x49 0x49 0x2A 0x00 Little-Endian Intel
				or 
			0x4D 0x4D 0x00 0x2A Big-Endian Motorola
		*/






		/* read the file header */

	unsigned char fileheader[4];
	fread(fileheader, sizeof(fileheader[0]), 4, file_ptr);

	for(int i=0; i<=8; i++) {
		printf("0x%x : ", fileheader[i]);
	}
	printf("\n");

	bool is_a_tiff = true;
	
	printf("================\n");
	for(int i=0; i<4; i++) {
		unsigned char tif_big_fileheader[4] = {0x4D, 0x4D, 0x00, 0x2A};
		printf("  0x%x : 0x%x\n", fileheader[i], tif_big_fileheader[i]);
		if(fileheader[i] != tif_big_fileheader[i]) {
			is_a_tiff = false;
		}
	}

	if(is_a_tiff == false) {
		is_a_tiff = true;
		unsigned char tif_little_fileheader[4] = {0x49, 0x49, 0x2A, 0x00};
		for(int i=0; i<4; i++) {
			printf("  0x%x : 0x%x\n", fileheader[i], tif_little_fileheader[i]);
			if(fileheader[i] != tif_little_fileheader[i]) {
				is_a_tiff = false;
			}
		}
	}







		/* Check of it is a big-endiand or little-endian tif */
	
	bool isLittleEndian = true;
	if(fileheader[0] == 0x4D) isLittleEndian = false;






		/* Die if the file is not identified as a tif */

	if(is_a_tiff == true) {
		printf("  It's a %s tiff.\n", isLittleEndian ? "Little-Endian" : "Big-Endian");
	} else {
		die("File does not seem to be a tiff image.");
	}
	printf("================\n");







		/* ======================================= */
		/* From here, we presume the file is a tif */
		/* ======================================= */






		/* read the 4 byte IFDOffset */

	t_1byte IFDOffset[4];	
	fread(IFDOffset, sizeof(IFDOffset[0]), 4, file_ptr);
	printf("IFDOffset: 0x%x\n", IFDOffset[0]);
	printf("IFDOffset: 0x%x\n", IFDOffset[1]);
	printf("IFDOffset: 0x%x\n", IFDOffset[2]);
	printf("IFDOffset: 0x%x\n", IFDOffset[3]);


		/* Convert the 4 bytes to an int */

	t_4byte IFDOffset_val = 0;
	extract_int_from_4bytes(IFDOffset, isLittleEndian, 0, &IFDOffset_val);
	printf("IFDOffset_val: %d\n", IFDOffset_val);
	printf("\n");


		/* Move read pointer to first IFD, at byte n */
	
	fseek(file_ptr, IFDOffset_val, SEEK_SET);







		/* The Image File Directory */




		/* Read the 2 byte NumDirEntries */

	t_1byte numDirEntries[2];
	fread(numDirEntries, sizeof(numDirEntries[0]), 2, file_ptr);
	t_2byte numDirEntries_val = 0;
	extract_short_from_2bytes(numDirEntries, isLittleEndian, 0, &numDirEntries_val);
	printf("Number of directory entries: %d\n", numDirEntries_val);
	printf("\n");





		/* Read the [numDirEntries] tags into an array each tag is 12 bytes long */

	// array tag pointers
	TIFTAG tiftags[numDirEntries_val];
	int dataSizes[numDirEntries_val];
	
	for(int i=0; i<numDirEntries_val; i++) {
		t_1byte tiftagBytes[12];
		fread(tiftagBytes, sizeof(t_1byte), 12, file_ptr);
		extract_tiftag_from_12bytes(tiftagBytes, isLittleEndian, 0, &tiftags[i]);
		dataSizes[i] = getDataSize(tiftags[i].DataType, tiftags[i].DataCount);
	}

	printf("%s\n", isLittleEndian ? "Little endian" : "Big endian");

	for(int i=0; i<numDirEntries_val; i++) {
		if(dataSizes[i] > 4) {
			printf("  Tiftag: %*d | TagId: %*d | DataType %*d | DataCount: %*d | DataOffset: %*d -> | DataSize: %*d | \n", 2, i, 6, tiftags[i].TagId, 4, tiftags[i].DataType, 5, tiftags[i].DataCount, 8, tiftags[i].DataOffset, 6, dataSizes[i]);
		} else {
			printf("  Tiftag: %*d | TagId: %*d | DataType %*d | DataCount: %*d | DataValue:  %*d    | DataSize: %*d | \n", 2, i, 6, tiftags[i].TagId, 4, tiftags[i].DataType, 5, tiftags[i].DataCount, 8, tiftags[i].DataOffset, 6, dataSizes[i]);			
		}
	}
	printf("\n");


	



		/* Extract image info */

	ImageInfo imageInfo;

	for(int i=0; i<numDirEntries_val; i++) {
		if(tiftags[i].TagId == IMAGE_WIDTH) imageInfo.imageWidth = tiftags[i].DataOffset;
		if(tiftags[i].TagId == IMAGE_HEIGHT) imageInfo.imageHeight = tiftags[i].DataOffset;
		if(tiftags[i].TagId == ROWS_PER_STRIP) imageInfo.rowsPerStrip = tiftags[i].DataOffset;
		if(tiftags[i].TagId == STRIP_OFFSETS) imageInfo.stripOffsets = tiftags[i].DataOffset;		
		if(tiftags[i].TagId == STRIP_BYTE_COUNTS) imageInfo.stripByteCounts = tiftags[i].DataOffset;
		if(tiftags[i].TagId == SAMPLES_PER_PIXEL) imageInfo.samplesPerPixels = tiftags[i].DataOffset;
	}

	printf("Image name:.................%s\n", filename_in);
	printf("Byte order:.................%s\n", isLittleEndian ? "Little-Endian" : "Big-Endian");
	printf("Image width:................%d\n", imageInfo.imageWidth);
	printf("Image height:...............%d\n", imageInfo.imageHeight);
	printf("Samples per pixel:..........%d\n", imageInfo.samplesPerPixels);
	printf("Rows per strip:.............%d\n", imageInfo.rowsPerStrip);
	printf("Strip offsets:..............%d\n", imageInfo.stripOffsets);
	printf("Strip byte counts:..........%d\n", imageInfo.stripByteCounts);
	printf("Width * Height * Samples:...%d\n", imageInfo.imageWidth * imageInfo.imageHeight * imageInfo.samplesPerPixels);

	printf("\n");



		/* Read Next IFD Offset – a 4 byte value */
	t_4byte nextIFDOffset[1];
	fread(nextIFDOffset, sizeof(nextIFDOffset[0]), 1, file_ptr);
	printf("nextIFDOffset: %d\n", nextIFDOffset[0]);
	printf("\n");



		/* Read in the pixel data */
//	t_1byte red[imageInfo.imageWidth * imageInfo.imageHeight]; 
//	t_1byte green[imageInfo.imageWidth * imageInfo.imageHeight]; 
//	t_1byte blue[imageInfo.imageWidth * imageInfo.imageHeight];

	RGBData rgbData;
	long channelDataSize = imageInfo.imageWidth * imageInfo.imageHeight;
	rgbData.red = malloc(channelDataSize);
	if(!rgbData.red) die("Memory allocation error.");
	rgbData.green = malloc(channelDataSize);
	if(!rgbData.green) die("Memory allocation error.");
	rgbData.blue = malloc(channelDataSize);
	if(!rgbData.blue) die("Memory allocation error.");

		/* Move read pointer to RGB data */
	fseek(file_ptr, imageInfo.stripOffsets, SEEK_SET);
	// read pixel data
	long p = 0;
	t_1byte rgb_dump[3];
	while( p < channelDataSize ) {
		fread(rgb_dump, sizeof(t_1byte), 3, file_ptr);
		rgbData.red[p] = rgb_dump[0];
		rgbData.green[p] = rgb_dump[1];
		rgbData.blue[p] = rgb_dump[2];
		p++;
	}



		/* close file */
	fclose(file_ptr);



		/* We now have the raw rgb pixel data */

		/* Manipulate image pixels */


	// simple conversion to greyscale
	float fR, fG, fB, fGrey;
	t_1byte greyData[channelDataSize];
	for(long i=0; i<channelDataSize; i++) {
		fR = rgbData.red[i];
		fG = rgbData.green[i];
		fB = rgbData.blue[i];
		fGrey = sqrt((fR*fR+fG*fG+fB*fB)/3.0);
		greyData[i] = (t_1byte)(fGrey);
	}

	// dither image using Floyd Steinberg algorithm
	floydSteinbergDither(greyData, imageInfo.imageWidth, imageInfo.imageHeight);



		/* Write pixel data to raw image file */

	printf("Writing image to file: %s\n", filename_out);
	file_ptr = fopen(filename_out, "wb");
	fwrite(greyData, sizeof(t_1byte), channelDataSize, file_ptr);
	fclose(file_ptr);



		/* Create a bilevel tiff */
//	t_1byte byteOrderBuffer[2] = {0x49, 0x49};
//	short byteOrder = extract_short_from_2bytes();

//	TiffHeader biLevelTifHeader;
//	biLevelTifHeader.byteOrder = 

	union tifHeader_union tifHeader_union;
	tifHeader_union.byteOrder[0] = 0x49;
	tifHeader_union.byteOrder[1] = 0x49;
	tifHeader_union.tiffID[0] = 42;
	tifHeader_union.tiffID[1] = 0;
	tifHeader_union.offset = 0;

	printf("8 bit header: %lu\n", tifHeader_union.header);

	long headerValue = tifHeader_union.header;
	for(int i=0; i<8; i++){
		t_1byte b = ((unsigned char *)(&headerValue))[i];
		printf("%d 0x%x : ", b, b);
	}
	printf("\n");

		/* clean up */
	free(rgbData.red);
	free(rgbData.green);
	free(rgbData.blue);


	return 0;
}

void die(const char *message) {
	if(errno) {
		perror(message);
	} else {
		printf("ERROR: %s\n", message);
	}
	exit(errno);
}

void extract_int_from_4bytes(t_1byte bytes[], bool isLittleEndian, int start, t_4byte *number) {
	// create a 4 byte unsigned int from an array of four 1 byte chars.
	union int_union int_union;
	if(isLittleEndian) {
		int_union.alpha[0] = bytes[start+0];
		int_union.alpha[1] = bytes[start+1];
		int_union.alpha[2] = bytes[start+2];
		int_union.alpha[3] = bytes[start+3];
	}
	if(!isLittleEndian) {
		int_union.alpha[0] = bytes[start+3];
		int_union.alpha[1] = bytes[start+2];
		int_union.alpha[2] = bytes[start+1];
		int_union.alpha[3] = bytes[start+0];
	}

	// set the number pointer to the unions int
	*number = int_union.num;
}

void extract_short_from_2bytes(t_1byte bytes[], bool isLittleEndian, int start, t_2byte *number) {
	// create a 2 byte short int from an array of four 1 byte chars.
	union short_union short_union;
	if(isLittleEndian) {
		short_union.alpha[0] = bytes[start+0];
		short_union.alpha[1] = bytes[start+1];
	}
	if(!isLittleEndian) {
		short_union.alpha[0] = bytes[start+1];
		short_union.alpha[1] = bytes[start+0];
	}

	// set the number pointer to the unions int
	*number = short_union.num;
}

void extract_tiftag_from_12bytes(t_1byte bytes[], bool isLittleEndian, int start, TIFTAG *tiftag) {
	// create a 12 byte tiftag struct from an array of 12 chars
	union tiftag_union tiftag_union;

	// tagid - 2 bytes
	if(isLittleEndian) {
		tiftag_union.alpha[0] = bytes[start+0];
		tiftag_union.alpha[1] = bytes[start+1];
	} else {
		// switch order of bytes
		tiftag_union.alpha[0] = bytes[start+1];
		tiftag_union.alpha[1] = bytes[start+0];		
	}

	// datatype - 2 bytes
	if(isLittleEndian) {
		tiftag_union.alpha[2] = bytes[start+2];
		tiftag_union.alpha[3] = bytes[start+3];
	} else {
		// switch order of bytes
		tiftag_union.alpha[2] = bytes[start+3];
		tiftag_union.alpha[3] = bytes[start+2];		
	}



	// datacount - 4 bytes
	if(isLittleEndian) {
		tiftag_union.alpha[4] = bytes[start+4];
		tiftag_union.alpha[5] = bytes[start+5];
		tiftag_union.alpha[6] = bytes[start+6];
		tiftag_union.alpha[7] = bytes[start+7];	
	} else {
		// switch order of bytes
		tiftag_union.alpha[4] = bytes[start+7];
		tiftag_union.alpha[5] = bytes[start+6];
		tiftag_union.alpha[6] = bytes[start+5];
		tiftag_union.alpha[7] = bytes[start+4];
	}


		/* Dataoffset can be different data types. Calculate size to make the correct big-endian conversion. */

	// get data type
	t_1byte dataTypeBytes[2] = { bytes[start+2], bytes[start+3] };
	t_2byte dataType;
	extract_short_from_2bytes(dataTypeBytes, isLittleEndian, 0, &dataType);

	// get data count
	t_1byte dataCountBytes[4] = { bytes[start+4], bytes[start+5], bytes[start+6], bytes[start+7] };
	t_4byte dataCount;
	extract_int_from_4bytes(dataCountBytes, isLittleEndian, 0, &dataCount);

	// calculate data size
	int dataSize = getDataSize(dataType, dataCount);
	if(dataSize == -1) die("Unknown datatype in tiff file.");

	// dataoffset - 4 bytes
	if(isLittleEndian) {
		tiftag_union.alpha[ 8] = bytes[start+ 8];
		tiftag_union.alpha[ 9] = bytes[start+ 9];
		tiftag_union.alpha[10] = bytes[start+10];
		tiftag_union.alpha[11] = bytes[start+11];
	} else {
		if(dataSize >= 4) {
			// switch order of bytes
			tiftag_union.alpha[ 8] = bytes[start+11];
			tiftag_union.alpha[ 9] = bytes[start+10];
			tiftag_union.alpha[10] = bytes[start+ 9];
			tiftag_union.alpha[11] = bytes[start+ 8];
		} else if(dataSize == 2) {
			// move the last two bytes first and switch order
			tiftag_union.alpha[ 8] = bytes[start+ 9];
			tiftag_union.alpha[ 9] = bytes[start+ 8];
			tiftag_union.alpha[10] = 0;
			tiftag_union.alpha[11] = 0;
		} else if(dataSize == 1) {
			// move the last byte first
			tiftag_union.alpha[ 8] = bytes[start+ 8];
			tiftag_union.alpha[ 9] = 0;
			tiftag_union.alpha[10] = 0;
			tiftag_union.alpha[11] = 0;
		}
	}	
	
	// fill the tiftag struct with bytes
	*tiftag = tiftag_union.tiftag;
}


int getDataSize(t_2byte dataType, t_4byte dataCount) {

	int dataSize = 0;

	switch(dataType) {
		case 1: // BYTE - 1 byte
			dataSize = dataCount * 1;
			break;
		
		case 2: // ASCII - 1 byte
			dataSize = dataCount * 1;
			break;
		
		case 3: // SHORT - 2 bytes
			dataSize = dataCount * 2;
			break;
		
		case 4: // LONG - 4 bytes
			dataSize = dataCount * 4;
			break;
		
		case 5: // RATIONAL - 8 byte
			dataSize = dataCount * 8;
			break;
		
		case 6: // SBYTE - 1 byte
			dataSize = dataCount * 1;
			break;
		
		case 7: // UNDEFINED - 1 byte
			dataSize = dataCount * 1;
			break;
		
		case 8: // SSHORT - 2 byte
			dataSize = dataCount * 2;
			break;
		
		case 9: // SLONG - 4 byte
			dataSize = dataCount * 4;
			break;
		
		case 10: // SRATIONAL - 8 byte
			dataSize = dataCount * (2 * 4);
			break;
		
		case 11: // FLOAT - 4 byte
			dataSize = dataCount * 4;
			break;
		
		case 12: // DOUBLE - 4 byte
			dataSize = dataCount * 8;
			break;

		default:
			dataSize = -1;
	}

	return dataSize;

}

void simpleForwardDither(t_1byte *pixelArray, long pixelArrayDataSize) {
	int errVal, inColor, outColor;
	for(int i=0;i<pixelArrayDataSize;i++) {
		inColor = pixelArray[i] + errVal;
		outColor = (inColor<128?0:255);
		errVal = inColor - outColor;
		pixelArray[i] = clampInt(outColor);
	}
}


void floydSteinbergDither(t_1byte *pixelArray, long imageWidth, long imageHeight) {
	for( int y=0; y<imageHeight; y++ ){
		for( int x=0; x<imageWidth; x++ ) {
			long currentPos = y * imageWidth + x;
			int inColor = pixelArray[currentPos];
			int realColor = ( inColor<128 ? 0:255 );
			int error = inColor - realColor;
			pixelArray[currentPos] = (t_1byte)realColor;
			int colorWithError;
			
			// do boundary checks and distribute the errors
			if(x<imageWidth-1) { 	
				colorWithError = pixelArray[currentPos+1] + ((error*7)>>4);
				pixelArray[currentPos+1] = clampInt(colorWithError);
			}
			
			if(y+1==imageHeight) continue;
			
			if(x>0) {		
				colorWithError = pixelArray[currentPos+imageWidth-1] + ((error*3)>>4);
				pixelArray[currentPos+imageWidth-1] = clampInt(colorWithError);
			}
			colorWithError = pixelArray[currentPos+imageWidth] + ((error*5)>>4);
			pixelArray[currentPos+imageWidth] = clampInt(colorWithError);

			if(x+1<imageWidth) {
				colorWithError = pixelArray[currentPos+imageWidth+1] + ((error*1)>>4);
				pixelArray[currentPos+imageWidth+1] = clampInt(colorWithError);
			}
		}
	}
}

