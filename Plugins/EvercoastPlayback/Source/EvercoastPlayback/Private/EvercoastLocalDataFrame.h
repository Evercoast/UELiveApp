#pragma once
#include <cstdint>

struct EvercoastLocalDataFrame
{
	EvercoastLocalDataFrame(double timestamp, int64_t frameIndex, const uint8_t* data, size_t data_size);
	virtual ~EvercoastLocalDataFrame();

	virtual void Invalidate();

	double m_timestamp;
	int64_t m_frameIndex;
	uint8_t* m_data;
	size_t m_dataSize;
};


struct ECMLocalDataFrame : public EvercoastLocalDataFrame
{
	ECMLocalDataFrame(double timestamp, int64_t frameIndex, const uint8_t* data, size_t data_size, bool requiresImage, bool isImageData);
	virtual ~ECMLocalDataFrame();

	void UpdateData(const uint8_t* data, size_t data_size, bool isImage);
	bool IsReady() const
	{
		if (m_requireImageData)
			return m_data && m_dataSize > 0 && m_imageData && m_imageDataSize > 0;
		else
			return m_data && m_dataSize > 0;
	}

	virtual void Invalidate() override;

	bool m_requireImageData;
	uint8_t* m_imageData;
	size_t m_imageDataSize;
};