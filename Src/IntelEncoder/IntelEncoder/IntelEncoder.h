#ifndef _IntelEncoder_H_
#define	_IntelEncoder_H_

#include "mfxvideo++.h"

#define MSDK_ALIGN16(value)                     (((value + 15) >> 4) << 4) // round up to a multiple of 16
#define MSDK_ALIGN32(value)                     (((value + 31) >> 5) << 5) // round up to a multiple of 32
#define MSDK_IGNORE_MFX_STS(P, X)               {if ((X) == (P)) {P = MFX_ERR_NONE;}}
#define Error_General							-1

typedef struct _IntelEncoderHandle {
	MFXVideoSession *pSession = NULL;
	MFXVideoENCODE *pEncoder = NULL;

	mfxU16 nEncSurfNum = 0;
	mfxU8 *pSurfaceBuffers = NULL;
	mfxFrameSurface1 **ppEncSurfaces = NULL;

	int width = 0;
	int height = 0;

	int buffer_size_kb = 0;

	void *pYUV_Buffer = NULL;
} IntelEncoderHandle;

typedef struct _IntelDecoderHandle {
	MFXVideoSession *pSession = NULL;
	MFXVideoDECODE *pDecoder = NULL;

	mfxVideoParam mfxVideoParams;

	mfxU16 nDecSurfNum = 0;
	mfxU8 *pSurfaceBuffers = NULL;
	mfxFrameSurface1 **ppDecSurfaces = NULL;

	mfxBitstream mfxBS;

	mfxFrameSurface1 *pOutSurface = NULL;

	mfxSyncPoint syncp;

	int width = 0;
	int height = 0;
	int CropW = 0;
	int CropH = 0;

	void *pYUV_Buffer = NULL;
} IntelDecoderHandle;

class BitStream {
private:
public:
	mfxBitstream mfxBS;
	mfxSyncPoint syncp;

	BitStream(int buffer_size_kb)
	{
		memset(&mfxBS, 0, sizeof(mfxBS));
		mfxBS.MaxLength = buffer_size_kb * 1024;
		mfxBS.Data = new mfxU8[mfxBS.MaxLength];
	}

	~BitStream()
	{
		delete[] mfxBS.Data;
	}
};

#endif /* _IntelEncoder_H_ */
