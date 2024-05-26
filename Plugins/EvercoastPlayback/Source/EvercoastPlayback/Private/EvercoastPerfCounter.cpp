#include "EvercoastPerfCounter.h"
#include <thread>

#define MEASUREMENTS_MAX_COUNT (16384)

EvercoastPerfCounter::EvercoastPerfCounter(const std::string& name, double timeSpan) :
	_name(name), _duration(timeSpan)
{
}

const std::string& EvercoastPerfCounter::Name() const
{
	return _name;
}

void EvercoastPerfCounter::AddSample()
{
	std::lock_guard<std::mutex> guard(_mutex);

	DataType dataType{ .timestamp = FPlatformTime::Seconds() };
	dataType.data.measuredInt64 = 1;
	_measurements.push_back(dataType);

	// do NOT exceed the hard limit
	while (_measurements.size() > MEASUREMENTS_MAX_COUNT)
	{
		_measurements.pop_front();
	}
}

void EvercoastPerfCounter::AddSampleAsInt64(int64_t measuredData)
{
	std::lock_guard<std::mutex> guard(_mutex);

	DataType dataType{ .timestamp = FPlatformTime::Seconds() };
	dataType.data.measuredInt64 = measuredData;

	_measurements.push_back(dataType);

	// do NOT exceed the hard limit
	while (_measurements.size() > MEASUREMENTS_MAX_COUNT)
	{
		_measurements.pop_front();
	}
}
void EvercoastPerfCounter::AddSampleAsDouble(double measuredData)
{
	std::lock_guard<std::mutex> guard(_mutex);

	DataType dataType{ .timestamp = FPlatformTime::Seconds() };
	dataType.data.measuredDouble = measuredData;
	_measurements.push_back(dataType);

	// do NOT exceed the hard limit
	while (_measurements.size() > MEASUREMENTS_MAX_COUNT)
	{
		_measurements.pop_front();
	}
}

void EvercoastPerfCounter::TrimToTimespan()
{
	// Trim to the timespan
	double currTime = FPlatformTime::Seconds();
	while (!_measurements.empty() && currTime - _measurements[0].timestamp > _duration)
	{
		_measurements.pop_front();
	}
}

size_t EvercoastPerfCounter::GetSampleCount()
{
	std::lock_guard<std::mutex> guard(_mutex);
	
	TrimToTimespan();
	return _measurements.size();
}

double EvercoastPerfCounter::GetSampleAccumulatedDouble()
{
	std::lock_guard<std::mutex> guard(_mutex);
	TrimToTimespan();

	double accumulated = 0.0;
	for (auto it = _measurements.begin(); it != _measurements.end(); ++it)
	{
		accumulated += (*it).data.measuredDouble;
	}

	return accumulated;
}


int64_t EvercoastPerfCounter::GetSampleAccumulatedInt64()
{
	std::lock_guard<std::mutex> guard(_mutex);
	TrimToTimespan();

	int64_t accumulated = 0;
	for (auto it = _measurements.begin(); it != _measurements.end(); ++it)
	{
		accumulated += (*it).data.measuredInt64;
	}

	return accumulated;
}


// NOTE: this assume the measured data is in unit(1)
double EvercoastPerfCounter::GetSampleAverageInt64OnCount()
{
	size_t count = GetSampleCount();
	int64_t accum = GetSampleAccumulatedInt64();
	{
		if (count == 0)
			return -1.0;


		return (double)accum / count;
	}
	
}

double EvercoastPerfCounter::GetSampleAverageDoubleOnCount()
{
	size_t count = GetSampleCount();
	double accum = GetSampleAccumulatedDouble();
	{
		if (count == 0)
			return -1.0;


		return accum / count;
	}

}


double EvercoastPerfCounter::GetSampleAverageInt64OnDuration()
{
	size_t count = GetSampleCount();
	int64_t accum = GetSampleAccumulatedInt64();
	{
		if (count == 0)
			return -1.0;


		std::lock_guard<std::mutex> guard(_mutex);
		return (double)accum / _duration;
	}
}


double EvercoastPerfCounter::GetSampleAverageDoubleOnDuration()
{
	size_t count = GetSampleCount();
	double accum = GetSampleAccumulatedDouble();
	{
		if (count == 0)
			return -1.0;


		return accum / _duration;
	}
}


void EvercoastPerfCounter::SetTimespan(double newTimespan)
{
	std::lock_guard<std::mutex> guard(_mutex);
	_duration = newTimespan;
}

void EvercoastPerfCounter::Reset()
{
	_measurements.clear();
}