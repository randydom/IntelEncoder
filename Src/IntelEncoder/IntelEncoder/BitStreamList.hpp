#pragma once 

#include "mfxvideo++.h"

#define MAX_BITSTREAMLIST_SIZE 4

typedef struct _BitStream {
	mfxBitstream mfxBS;
	mfxSyncPoint syncp;
} BitStream;

class BitStreamList {
public:
	BitStreamList()
	{
	}

	~BitStreamList()
	{
		for (int i = 0; i < MAX_BITSTREAMLIST_SIZE; i++) {
			delete[] list_[i].mfxBS.Data;
		}
	}

	void init(int bufferSizeInKB)
	{
		index_ = 0;

		for (int i = 0; i < MAX_BITSTREAMLIST_SIZE; i++) {
			memset(&list_[i].mfxBS, 0, sizeof(list_[i].mfxBS));
			list_[i].mfxBS.MaxLength = bufferSizeInKB * 1024;
			list_[i].mfxBS.Data = new mfxU8[list_[i].mfxBS.MaxLength];
		}
	}

	BitStream *getNextBitStream() 
	{
		count_++;

		index_++;
		index_ = index_ % MAX_BITSTREAMLIST_SIZE;

		return &list_[index_];
	}

	void releaseCurrentBitStream()
	{
		count_--;
	}

	bool isFull()
	{
		return count_ >= MAX_BITSTREAMLIST_SIZE;
	}

private:
	int count_ = 0;
	int index_ = 0;
	BitStream list_[MAX_BITSTREAMLIST_SIZE];
};
