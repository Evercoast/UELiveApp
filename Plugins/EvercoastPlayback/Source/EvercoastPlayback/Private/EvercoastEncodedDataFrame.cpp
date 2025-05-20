#include "EvercoastEncodedDataFrame.h"
#include "CoreMinimal.h"

EvercoastEncodedDataFrame::EvercoastEncodedDataFrame(double timestamp, int64_t frameIndex, const uint8_t* data, size_t data_size) :
	m_timestamp(timestamp), m_frameIndex(frameIndex), m_data(nullptr), m_dataSize(0)
{
	m_dataSize = data_size;
	if (data_size > 0)
	{
		m_data = new uint8_t[data_size];
		FMemory::Memcpy(m_data, data, data_size);
	}
}

EvercoastEncodedDataFrame::~EvercoastEncodedDataFrame()
{
	Invalidate();
}

void EvercoastEncodedDataFrame::Invalidate()
{
	m_frameIndex = -1;
	m_timestamp = -1.0;
	m_dataSize = 0;
	if (m_data)
	{
		delete[] m_data;
		m_data = nullptr;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ECMEncodedDataFrame::ECMEncodedDataFrame(double timestamp, int64_t frameIndex, const uint8_t* data, size_t data_size, bool requiresImage, bool isImageData) :
	EvercoastEncodedDataFrame(timestamp, frameIndex, nullptr, 0), m_requireImageData(requiresImage), m_imageData(nullptr), m_imageDataSize(0)
{
	UpdateData(data, data_size, isImageData);
}

ECMEncodedDataFrame::~ECMEncodedDataFrame()
{
	Invalidate();
}

void ECMEncodedDataFrame::UpdateData(const uint8_t* data, size_t data_size, bool isImage)
{
	if (!isImage)
	{
		//check(m_data == nullptr && m_dataSize == 0);
		if (m_data)
		{
			delete[] m_data;
			m_data = nullptr;
		}
		m_dataSize = data_size;
		m_data = new uint8_t[data_size];
		FMemory::Memcpy(m_data, data, data_size);
	}
	else
	{
		check(m_requireImageData);
		//check(m_imageData == nullptr && m_imageDataSize == 0);
		if (m_imageData)
		{
			delete[] m_imageData;
			m_imageData = nullptr;
		}
		m_imageDataSize = data_size;
		m_imageData = new uint8_t[data_size];
		FMemory::Memcpy(m_imageData, data, data_size);
	}

}

void ECMEncodedDataFrame::Invalidate()
{
	EvercoastEncodedDataFrame::Invalidate();
	m_imageDataSize = 0;
	if (m_imageData)
	{
		delete[] m_imageData;
		m_imageData = nullptr;
	}
}