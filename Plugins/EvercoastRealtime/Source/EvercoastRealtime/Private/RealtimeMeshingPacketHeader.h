#pragma once

#pragma pack(push)
#pragma pack(1)
struct RealtimePacketHeader
{
	uint16_t headerType;
	// offset 2
	uint16_t headerVersion;
	// offset 4
	uint32_t u32streamType;

	bool IsValid() const
	{
		return headerType == 0 || // old format
            headerType == 1 || // voxel
			headerType == 32768; // mesh
	}
};

struct RealtimeMeshingPacketHeaderV1
{
	// offset 0 // NEVER GOT 32768???
	uint16_t headerType = 32768; //stay out of the way of existing / voxel codecs
	// offset 2
	uint16_t headerVersion = 1;
	// offset 4
	uint32_t u32streamType = 0;

	// offset 8
	int64_t frameNumber = 0;
	// offset 16
	uint32_t absoluteOffsetToCortoData = 0;
	// offset 20
	uint32_t cortoDataLength = 0;
	// offset 24
	uint32_t absoluteOffsetToWepPData = 0;
	// offset 28
	uint32_t wepPDataLength = 0;
};
#pragma pack(pop)
