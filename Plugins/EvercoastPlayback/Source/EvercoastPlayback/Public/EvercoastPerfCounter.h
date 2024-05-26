#pragma once

#include <string>
#include <deque>
#include <mutex>
#include "CoreMinimal.h"


class EVERCOASTPLAYBACK_API EvercoastPerfCounter
{
public:
	// Counter starts with a name, and within a time span that it will automatically accumulate the 
	// latest sample and dispose the outdated samples based on timestamps
	EvercoastPerfCounter(const std::string& name, double timeSpan);
	~EvercoastPerfCounter() = default;

	// Begin of thread-safe functions
	void AddSample();
	void AddSampleAsInt64(int64_t measuredData);
	void AddSampleAsDouble(double measuredData);

	size_t GetSampleCount(); // trim happens so no const

	double GetSampleAccumulatedDouble(); // trim happens so no const
	int64_t GetSampleAccumulatedInt64();

	double GetSampleAverageInt64OnCount();
	double GetSampleAverageInt64OnDuration();
	double GetSampleAverageDoubleOnCount();
	double GetSampleAverageDoubleOnDuration();
	void SetTimespan(double newTimespan);
	void Reset();
	// End of thread-safe functions

	const std::string& Name() const;
	
	struct DataType
	{
		union {
			double measuredDouble;
			int64_t measuredInt64;
		} data;
		double timestamp;
	};

private:

	void TrimToTimespan();

	std::string _name;
	mutable std::mutex _mutex;
	std::deque<DataType> _measurements;
	double _duration;
	
};