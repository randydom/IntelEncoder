// IntelEncoder.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include <WinBase.h>
#include <Windows.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "IntelEncoder.h"
#include "ipp.h"

#pragma comment(lib, "legacy_stdio_wide_specifiers.lib")
#pragma comment(lib, "legacy_stdio_definitions.lib")

using namespace std;

void trace(const char* format, ...)
{
	char buffer[4096];
	va_list vaList;
	va_start(vaList, format);
	sprintf(buffer, format, vaList);
	va_end(vaList);
	OutputDebugStringA(buffer);
}

int GetFreeSurfaceIndex(mfxFrameSurface1** pSurfacesPool, mfxU16 nPoolSize)
{
	mfxU16 i;

	if (pSurfacesPool)
		for (i = 0; i < nPoolSize; i++)
			if (0 == pSurfacesPool[i]->Data.Locked)
				return i;
	return MFX_ERR_NOT_FOUND;
}

IntelEncoderHandle *CreateEncoderHanlde()
{
	IntelEncoderHandle *pHandle = (IntelEncoderHandle *) malloc(sizeof(IntelEncoderHandle));
	pHandle->pSession = new MFXVideoSession();
	pHandle->pEncoder = NULL;
	pHandle->nEncSurfNum = 0;
	pHandle->pSurfaceBuffers = NULL;
	pHandle->ppEncSurfaces = NULL;
	pHandle->pYUV_Buffer = NULL;

	return pHandle;
}

void ReleaseEncoderHandle(IntelEncoderHandle *pHandle)
{
	if (pHandle == NULL) return;

	pHandle->pEncoder->Close();
	
	if (pHandle->pEncoder != NULL) delete pHandle->pEncoder;
	delete pHandle->pSession;

	for (int i = 0; i < pHandle->nEncSurfNum; i++) free(pHandle->ppEncSurfaces[i]);

	if (pHandle->ppEncSurfaces != NULL) free(pHandle->ppEncSurfaces);
	if (pHandle->pSurfaceBuffers != NULL) free(pHandle->pSurfaceBuffers);
	if (pHandle->pYUV_Buffer != NULL) free(pHandle->pYUV_Buffer);
	if (pHandle != NULL) free(pHandle);

	pHandle = NULL;
}

extern "C" __declspec(dllexport) IntelEncoderHandle *OpenEncoder(int *pErrorCode, int width, int height, int frame_rate, int bit_rate, int gop)
{
	*pErrorCode = 0;

	IntelEncoderHandle *pHandle = CreateEncoderHanlde();

	pHandle->width  = width;
	pHandle->height = height;
	pHandle->pYUV_Buffer = malloc(width * height * 4);

	mfxIMPL impl = MFX_IMPL_AUTO_ANY;
	mfxVersion ver = { 0, 1 };

	pHandle->pSession = new MFXVideoSession();
	mfxStatus sts = pHandle->pSession->Init(impl, &ver);
	if (sts != MFX_ERR_NONE) {
		*pErrorCode = sts;
		goto error;
	}

	mfxVideoParam mfxEncParams;
	memset(&mfxEncParams, 0, sizeof(mfxEncParams));
	mfxEncParams.mfx.CodecId = MFX_CODEC_AVC;

	// MFX_TARGETUSAGE_BEST_SPEED;  //MFX_TARGETUSAGE_BALANCED  // MFX_TARGETUSAGE_BEST_QUALITY
	mfxEncParams.mfx.TargetUsage = MFX_TARGETUSAGE_BALANCED;

	if (bit_rate == 0) bit_rate = 1024;
	mfxEncParams.mfx.TargetKbps = bit_rate;

	mfxEncParams.mfx.RateControlMethod = MFX_RATECONTROL_CBR;
	mfxEncParams.mfx.FrameInfo.FrameRateExtN = frame_rate;
	mfxEncParams.mfx.FrameInfo.FrameRateExtD = 1;
	mfxEncParams.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
	mfxEncParams.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
	mfxEncParams.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
	mfxEncParams.mfx.FrameInfo.CropX = 0;
	mfxEncParams.mfx.FrameInfo.CropY = 0;
	mfxEncParams.mfx.FrameInfo.CropW = width;
	mfxEncParams.mfx.FrameInfo.CropH = height;

	// Width must be a multiple of 16 
	// Height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
	mfxEncParams.mfx.FrameInfo.Width = MSDK_ALIGN16(width);
	mfxEncParams.mfx.FrameInfo.Height = (MFX_PICSTRUCT_PROGRESSIVE == mfxEncParams.mfx.FrameInfo.PicStruct) ? MSDK_ALIGN16(height) : MSDK_ALIGN32(height);

	mfxEncParams.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

	pHandle->pEncoder = new MFXVideoENCODE(*pHandle->pSession);

	sts = pHandle->pEncoder->Query(&mfxEncParams, &mfxEncParams);
	if (sts != MFX_ERR_NONE) {
		*pErrorCode = sts;
		goto error;
	}

	mfxFrameAllocRequest EncRequest;
	memset(&EncRequest, 0, sizeof(EncRequest));
	sts = pHandle->pEncoder->QueryIOSurf(&mfxEncParams, &EncRequest);
	if (sts != MFX_ERR_NONE) {
		*pErrorCode = sts;
		goto error;
	}

	pHandle->nEncSurfNum = EncRequest.NumFrameSuggested;

	mfxU16 w = (mfxU16) MSDK_ALIGN32(EncRequest.Info.Width);
	mfxU16 h = (mfxU16) MSDK_ALIGN32(EncRequest.Info.Height);

	// NV12 format is a 12 bits per pixel format
	mfxU8  bitsPerPixel = 12; 

	// * 2 --> Add extra memory space for safty.
	mfxU32 surfaceSize = w * h * bitsPerPixel * 2 / 8;

	pHandle->pSurfaceBuffers = (mfxU8 *)new mfxU8[surfaceSize * pHandle->nEncSurfNum];

	pHandle->ppEncSurfaces = new mfxFrameSurface1*[pHandle->nEncSurfNum];
	for (int i = 0; i < pHandle->nEncSurfNum; i++) {
		pHandle->ppEncSurfaces[i] = new mfxFrameSurface1;
		memset(pHandle->ppEncSurfaces[i], 0, sizeof(mfxFrameSurface1));
		memcpy(&(pHandle->ppEncSurfaces[i]->Info), &(mfxEncParams.mfx.FrameInfo), sizeof(mfxFrameInfo));
		pHandle->ppEncSurfaces[i]->Data.Y = &pHandle->pSurfaceBuffers[surfaceSize * i];
		pHandle->ppEncSurfaces[i]->Data.U = pHandle->ppEncSurfaces[i]->Data.Y + w * h;
		pHandle->ppEncSurfaces[i]->Data.V = pHandle->ppEncSurfaces[i]->Data.U + 1;
		pHandle->ppEncSurfaces[i]->Data.Pitch = w;

		// In case simulating direct access to frames we initialize the allocated surfaces with default pattern
		// - For true benchmark comparisons to async workloads all surfaces must have the same data
		memset(pHandle->ppEncSurfaces[i]->Data.Y, 100, w * h);  // Y plane
		memset(pHandle->ppEncSurfaces[i]->Data.U, 50, (w * h) / 2);  // UV plane
	}

	sts = pHandle->pEncoder->Init(&mfxEncParams);
	MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
	if (sts != MFX_ERR_NONE) {
		*pErrorCode = sts;
		goto error;
	}

	mfxVideoParam par;
	memset(&par, 0, sizeof(par));
	sts = pHandle->pEncoder->GetVideoParam(&par);
	if (sts != MFX_ERR_NONE) {
		*pErrorCode = sts;
		goto error;
	}

	pHandle->buffer_size_kb = par.mfx.BufferSizeInKB;

	return pHandle;

error:
	ReleaseEncoderHandle(pHandle);
	return NULL;
}

extern "C" __declspec(dllexport) void CloseEncoder(IntelEncoderHandle *pHandle)
{
	ReleaseEncoderHandle(pHandle);
}

extern "C" __declspec(dllexport) int EncodeBitmap(IntelEncoderHandle *pHandle, void *pBitmap, BitStream **ppBitStream)
{
	int nEncSurfIdx = GetFreeSurfaceIndex(pHandle->ppEncSurfaces, pHandle->nEncSurfNum);
	if (MFX_ERR_NOT_FOUND == nEncSurfIdx) return 0;

	*ppBitStream = new BitStream(pHandle->buffer_size_kb);
	mfxBitstream *pBS = &((*ppBitStream)->mfxBS);
	mfxSyncPoint *pSync = &((*ppBitStream)->syncp);

	IppiSize src_size = { pHandle->width, pHandle->height };
	ippiRGBToYCbCr420_8u_C3P2R(
		(Ipp8u *) pBitmap, pHandle->width * 3,
		pHandle->ppEncSurfaces[nEncSurfIdx]->Data.Y, pHandle->ppEncSurfaces[nEncSurfIdx]->Data.Pitch,
		pHandle->ppEncSurfaces[nEncSurfIdx]->Data.UV, pHandle->ppEncSurfaces[nEncSurfIdx]->Data.Pitch,
		src_size
	);

	mfxStatus sts = pHandle->pEncoder->EncodeFrameAsync(NULL, pHandle->ppEncSurfaces[nEncSurfIdx], pBS, pSync);

	if (sts != MFX_ERR_NONE) {
		delete *ppBitStream;
		*ppBitStream = NULL;
		return 0;
	}

	return 1;
}

extern "C" __declspec(dllexport) int SyncEncodeResult(IntelEncoderHandle *pHandle, BitStream *pBitStream, void *pBuffer, int *pBufferSize, int wait)
{
	mfxStatus sts = pHandle->pSession->SyncOperation(pBitStream->syncp, wait);

	if (sts != MFX_ERR_NONE) {
		if (sts != MFX_WRN_DEVICE_BUSY) delete pBitStream;
		return sts;
	}

	void *pTemp = pBitStream->mfxBS.Data + pBitStream->mfxBS.DataOffset;
	memcpy(pBuffer, pTemp, pBitStream->mfxBS.DataLength);

	*pBufferSize = (int) pBitStream->mfxBS.DataLength;

	delete pBitStream;

	return sts;
}

IntelDecoderHandle *CreateDecoderHanlde()
{
	IntelDecoderHandle *pHandle = (IntelDecoderHandle *) malloc(sizeof(IntelDecoderHandle));
	pHandle->pSession = new MFXVideoSession();
	pHandle->nDecSurfNum = 0;
	pHandle->pDecoder = NULL;
	pHandle->mfxBS.Data = NULL;
	pHandle->pSurfaceBuffers = NULL;
	pHandle->ppDecSurfaces = NULL;
	pHandle->pYUV_Buffer = NULL;

	return pHandle;
}

void ReleaseDecoderHandle(IntelDecoderHandle *pHandle)
{
	if (pHandle == NULL) return;

	if (pHandle->pDecoder != NULL) pHandle->pDecoder->Close();

	if(pHandle->mfxBS.Data != NULL) delete pHandle->mfxBS.Data;

	for (int i = 0; i < pHandle->nDecSurfNum; i++) delete pHandle->ppDecSurfaces[i];

	if (pHandle->ppDecSurfaces != NULL) free(pHandle->ppDecSurfaces);
	if (pHandle->pSurfaceBuffers != NULL) free(pHandle->pSurfaceBuffers);
	if (pHandle->pYUV_Buffer != NULL) free(pHandle->pYUV_Buffer);
	if (pHandle != NULL) free(pHandle);

	pHandle = NULL;
}


extern "C" __declspec(dllexport) IntelDecoderHandle *OpenDecoder(int *pErrorCode)
{
	*pErrorCode = 0;

	IntelDecoderHandle *pHandle = CreateDecoderHanlde();

	mfxIMPL impl = MFX_IMPL_AUTO_ANY;
	mfxVersion ver = { 0, 1 };

	pHandle->pSession = new MFXVideoSession();
	mfxStatus sts = pHandle->pSession->Init(impl, &ver);
	if (sts != MFX_ERR_NONE) {
		*pErrorCode = sts;
		goto error;
	}

	pHandle->pDecoder = new	MFXVideoDECODE(*pHandle->pSession);

	memset(&pHandle->mfxVideoParams, 0, sizeof(pHandle->mfxVideoParams));
	pHandle->mfxVideoParams.mfx.CodecId = MFX_CODEC_AVC;
	pHandle->mfxVideoParams.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
	pHandle->mfxVideoParams.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

	memset(&pHandle->mfxBS, 0, sizeof(pHandle->mfxBS));
	pHandle->mfxBS.MaxLength = 1024 * 1024;
	pHandle->mfxBS.Data = new mfxU8[pHandle->mfxBS.MaxLength];

	return pHandle;

error:
	ReleaseDecoderHandle(pHandle);
	return NULL;
}

void ReadBitStreamData(mfxBitstream *pBS, void *pBuffer, int sizeOfBuffer)
{
	memmove(pBS->Data, pBS->Data + pBS->DataOffset, pBS->DataLength);
	pBS->DataOffset = 0;

	memcpy(pBS->Data + pBS->DataLength, pBuffer, sizeOfBuffer);

	pBS->DataLength += sizeOfBuffer;
}

extern "C" __declspec(dllexport) char PrepareDecoder(IntelDecoderHandle *pHandle, void *pBuffer, int size_of_buffer, int *pErrorCode, int *pWidth, int *pHeight)
{
	*pErrorCode = 0;

	ReadBitStreamData(&pHandle->mfxBS, pBuffer, size_of_buffer);

	mfxStatus sts = pHandle->pDecoder->DecodeHeader(&pHandle->mfxBS, &pHandle->mfxVideoParams);
	MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
	if (sts != MFX_ERR_NONE) {
		*pErrorCode = sts;
		return false;
	}

	mfxFrameAllocRequest Request;
	memset(&Request, 0, sizeof(Request));
	sts = pHandle->pDecoder->QueryIOSurf(&pHandle->mfxVideoParams, &Request);
	MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
	if (sts != MFX_ERR_NONE) {
		*pErrorCode = sts;
		return false;
	}

	pHandle->nDecSurfNum = Request.NumFrameSuggested;

	pHandle->width = Request.Info.Width;
	pHandle->height = Request.Info.Height;

	pHandle->CropW = Request.Info.CropW;
	pHandle->CropH = Request.Info.CropH;

	int w = (mfxU16) MSDK_ALIGN32(Request.Info.Width);
	int h = (mfxU16) MSDK_ALIGN32(Request.Info.Height);

	*pWidth = w; // Request.Info.Width;
	*pHeight = h; // Request.Info.Height;

	pHandle->pYUV_Buffer = malloc(w * h * 4);

	mfxU8  bitsPerPixel = 12;
	mfxU32 surfaceSize = w * h * bitsPerPixel / 8;

	pHandle->pSurfaceBuffers = (mfxU8 *) new mfxU8[surfaceSize * pHandle->nDecSurfNum];

	pHandle->ppDecSurfaces = new mfxFrameSurface1*[pHandle->nDecSurfNum];
	for (int i = 0; i < pHandle->nDecSurfNum; i++)
	{
		pHandle->ppDecSurfaces[i] = new mfxFrameSurface1;
		memset(pHandle->ppDecSurfaces[i], 0, sizeof(mfxFrameSurface1));
		memcpy(&(pHandle->ppDecSurfaces[i]->Info), &(pHandle->mfxVideoParams.mfx.FrameInfo), sizeof(mfxFrameInfo));
		pHandle->ppDecSurfaces[i]->Data.Y = &pHandle->pSurfaceBuffers[surfaceSize * i];
		pHandle->ppDecSurfaces[i]->Data.U = pHandle->ppDecSurfaces[i]->Data.Y + w * h;
		pHandle->ppDecSurfaces[i]->Data.V = pHandle->ppDecSurfaces[i]->Data.U + 1;
		pHandle->ppDecSurfaces[i]->Data.Pitch = w;
	}

	sts = pHandle->pDecoder->Init(&pHandle->mfxVideoParams);
	MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
	if (sts != MFX_ERR_NONE) {
		*pErrorCode = sts;
		return false;
	}

	return true;
}

extern "C" __declspec(dllexport) void CloseDecoder(IntelDecoderHandle *pHandle)
{
	ReleaseDecoderHandle(pHandle);
}

extern "C" __declspec(dllexport) int DecodeStreamData(IntelDecoderHandle *pHandle, void *pBuffer, int size_of_buffer)
{
	ReadBitStreamData(&pHandle->mfxBS, pBuffer, size_of_buffer);

	int nDecSurfIdx = GetFreeSurfaceIndex(pHandle->ppDecSurfaces, pHandle->nDecSurfNum);

	mfxStatus sts = pHandle->pDecoder->DecodeFrameAsync(
		&pHandle->mfxBS, pHandle->ppDecSurfaces[nDecSurfIdx], &pHandle->pOutSurface,
		&pHandle->syncp
	);

	return sts;
}

extern "C" __declspec(dllexport) char GetBitmap(IntelDecoderHandle *pHandle, void *pBitmap, int wait)
{
	mfxStatus sts = pHandle->pSession->SyncOperation(pHandle->syncp, wait);

	if (sts != MFX_ERR_NONE) {
		trace("decodeBitmap - SyncOperation: %d", sts);
		return false;
	}

	Ipp8u *pSrc[3] = { pHandle->pOutSurface->Data.Y, pHandle->pOutSurface->Data.U, pHandle->pOutSurface->Data.V };
	IppiSize src_size = { pHandle->width, pHandle->height };

	src_size.width  = (mfxU16) MSDK_ALIGN32(pHandle->width);
	src_size.height = (mfxU16) MSDK_ALIGN32(pHandle->height);

	ippiYCbCr420ToRGB_8u_P2C3R(
		pHandle->pOutSurface->Data.Y, src_size.width,
		pHandle->pOutSurface->Data.CbCr, src_size.width,
		(Ipp8u *) pBitmap, src_size.width * 3,
		src_size
	);

	return true;
}
