/*
  Hatari - floppy_stx.h

  This file is distributed under the GNU General Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/




typedef struct {
	/* Content of the STX sector block (16 bytes) */
	Uint32		DataOffset;				/* Offset of sector data in the track data */
	Uint16		BitPosition;				/* Position in bits from the start of the track */
								/* (this seems to be the position of the start of the ID field, */
								/* just after the IDAM, but it's not always precise) */
	Uint16		ReadTime;				/* in ms */

	Uint8		ID_Track;				/* Content of the Address Field */
	Uint8		ID_Head;
	Uint8		ID_Sector;
	Uint8		ID_Size;
	Uint16		ID_CRC;

	Uint8		FDC_Status;				/* FDC status and flags for this sector */
	Uint8		Reserved;				/* Unused, always 0 */

	/* Other internal variables */
	Uint16		SectorSize;				/* In bytes, depends on ID_Size */
	Uint8		*pData;					/* Bytes for this sector or null if RNF */
	Uint8		*pFuzzyData;				/* Fuzzy mask for this sector or null if no fuzzy bits */
	Uint8		*pTimingData;				/* Data for variable bit width or null */
} STX_SECTOR_STRUCT;

#define	STX_SECTOR_BLOCK_SIZE		( 4+2+2+1+1+1+1+2+1+1)	/* Size of the sector block in an STX file = 16 bytes */

/* NOTE : bits 3,4,5 have the same meaning as in the FDC's Status register */
#define	STX_SECTOR_FLAG_VARIABLE_TIME	(1<<0)			/* bit 0, if set, this sector has variable bit width */
#define	STX_SECTOR_FLAG_CRC		(1<<3)			/* bit 3, if set, there's a CRC error */
#define	STX_SECTOR_FLAG_RNF		(1<<4)			/* bit 4, if set, there's no sector data */
#define	STX_SECTOR_FLAG_RECORD_TYPE	(1<<5)			/* bit 5, if set, deleted data */
#define	STX_SECTOR_FLAG_FUZZY		(1<<7)			/* bit 7, if set, this sector has fuzzy bits */

#define	STX_SECTOR_READ_TIME_DEFAULT	16384			/* Default value if ReadTime==0 */


typedef struct {
	/* Content of the STX track block (16 bytes) */
	Uint32		BlockSize;				/* Number of bytes in this track block */
	Uint32		FuzzySize;				/* Number of bytes in fuzzy mask */
	Uint16		SectorsCount;				/* Number of sector blocks in this track */
	Uint16		Flags;					/* Flags for this track */
	Uint16		MFMSize;				/* Number of MFM bytes in this track */
	Uint8		TrackNumber;				/* bits 0-6 = track number   bit 7 = side */
	Uint8		RecordType;				/* Unused */

	/* Other internal variables */
	STX_SECTOR_STRUCT	*pSectorsStruct;		/* All the sectors struct for this track or null */

	Uint8			*pFuzzyData;			/* Fuzzy mask data for all the fuzzy sectors of the track */

	Uint8			*pTrackData;			/* Track data (after sectors data and fuzzy data) */
	Uint16			TrackImageSyncPosition;
	Uint16			TrackImageSize;			/* Number of bytes in pTrackImageData */
	Uint8			*pTrackImageData;		/* Optionnal data as returned by the read track command */

	Uint8			*pSectorsImageData;		/* Optionnal data for the sectors of this track */

	Uint8			*pTiming;
	Uint16			TimingFlags;			/* always '5' ? */
	Uint16			TimingSize;
	Uint8			*pTimingData;			/* Timing data for all the sectors of the track ; each timing */
								/* consists of 2 bytes per 16 FDC bytes */
} STX_TRACK_STRUCT;

#define	STX_TRACK_BLOCK_SIZE		( 4+4+2+2+2+1+1 )	/* Size of the track block in an STX file = 16 bytes */

#define	STX_TRACK_FLAG_SECTOR_BLOCK	(1<<0)			/* bit 0, if set, this track contains sector blocks */
#define	STX_TRACK_FLAG_TRACK_IMAGE	(1<<6)			/* bit 6, if set, this track contains a track image */
#define	STX_TRACK_FLAG_TRACK_IMAGE_SYNC	(1<<7)			/* bit 7, if set, the track image has a sync position */




typedef struct {
	/* Content of the STX header block (16 bytes) */
	char		FileID[ 4 ];				/* Should be "RSY\0" */
	Uint16		Version;				/* Only version 3 is supported */
	Uint16		ImagingTool;				/* 0x01 (Atari Tool) or 0xCC (Discovery Cartridge) */
	Uint16		Reserved_1;				/* Unused */
	Uint8		TracksCount;				/* Number of track blocks in this file */
	Uint8		Revision;				/* 0x00 (old Pasti file)   0x02 (new Pasti file) */
	Uint32		Reserved_2;				/* Unused */

	/* Other internal variables */
	STX_TRACK_STRUCT	*pTracksStruct;
} STX_MAIN_STRUCT;

#define	STX_MAIN_BLOCK_SIZE		( 4+2+2+2+1+1+4 )	/* Size of the header block in an STX file = 16 bytes */



extern void	STX_MemorySnapShot_Capture(bool bSave);
extern bool	STX_FileNameIsSTX(const char *pszFileName, bool bAllowGZ);
extern Uint8	*STX_ReadDisk(const char *pszFileName, long *pImageSize, int *pImageType);
extern bool	STX_WriteDisk(const char *pszFileName, Uint8 *pBuffer, int ImageSize);

extern bool	STX_Init ( void );
extern bool	STX_Insert ( int Drive , Uint8 *pImageBuffer , long ImageSize );
extern bool	STX_Eject ( int Drive );

extern void	STX_FreeStruct ( STX_MAIN_STRUCT *pStxMain );
extern STX_MAIN_STRUCT *STX_BuildStruct ( Uint8 *pFileBuffer , int Debug );


extern Uint32	FDC_GetCyclesPerRev_FdcCycles_STX ( Uint8 Drive , Uint8 Track , Uint8 Side );
extern int	FDC_NextSectorID_FdcCycles_STX ( Uint8 Drive , Uint8 Track , Uint8 Side );
extern Uint8	FDC_NextSectorID_TR_STX ( void );
extern Uint8	FDC_NextSectorID_SR_STX ( void );
extern Uint8	FDC_ReadSector_STX ( Uint8 Drive , Uint8 Track , Uint8 Sector , Uint8 Side , Uint8 *buf , int *pSectorSize );
extern Uint8	FDC_ReadAddress_STX ( Uint8 Drive , Uint8 Track , Uint8 Sector , Uint8 Side );
extern Uint8	FDC_ReadTrack_STX ( Uint8 Drive , Uint8 Track , Uint8 Side );

