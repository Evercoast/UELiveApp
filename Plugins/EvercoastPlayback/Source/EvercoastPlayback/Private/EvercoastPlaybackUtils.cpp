#include "EvercoastPlaybackUtils.h"

int64_t GetFrameIndex(double timestamp, double frameRate)
{
	//double interval = 1.0 / frameRate;
	//int64_t index = (int64_t)(timestamp / interval + 0.5);
	int64_t index = (int64_t)(timestamp * frameRate + 0.5);
	return index;
}