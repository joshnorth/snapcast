#ifndef CHUNK_H
#define CHUNK_H

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <streambuf>
#include <vector>
#include "common/sampleFormat.h"


typedef std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::milliseconds> time_point_ms;
using namespace std;


template<typename CharT, typename TraitsT = std::char_traits<CharT> >
class vectorwrapbuf : public std::basic_streambuf<CharT, TraitsT> {
public:
    vectorwrapbuf(std::vector<CharT> &vec) {
        this->setg(vec.data(), vec.data(), vec.data() + vec.size());
    }
};



enum message_type
{
	header = 0,
	payload = 1
};



struct BaseMessage
{
	BaseMessage()
	{
	}

	BaseMessage(message_type type_)
	{
		type = type_;
	};

	virtual ~BaseMessage()
	{
	}

	virtual void read(istream& stream)
	{
		stream.read(reinterpret_cast<char*>(&type), sizeof(uint16_t));
		stream.read(reinterpret_cast<char*>(&size), sizeof(uint32_t));
	}

	virtual void readVec(std::vector<char>& stream)
	{
		vectorwrapbuf<char> databuf(stream);
		std::istream is(&databuf);
		read(is);
	}

	virtual void serialize(ostream& stream)
	{
		stream.write(reinterpret_cast<char*>(&type), sizeof(uint16_t));
		size = getSize();
		stream.write(reinterpret_cast<char*>(&size), sizeof(uint32_t));
		doserialize(stream);
	}

	virtual uint32_t getSize()
	{
		return sizeof(uint16_t) + sizeof(uint32_t);
	};

	uint16_t type;
	uint32_t size;
protected:
	virtual void doserialize(ostream& stream)
	{
	};
};



class WireChunk : public BaseMessage 
{
public:
	WireChunk(size_t size = 0) : BaseMessage(message_type::payload), payloadSize(size)
	{
		payload = (char*)malloc(size);
	}

	virtual ~WireChunk()
	{
		free(payload);
	}

	virtual void read(istream& stream)
	{
		stream.read(reinterpret_cast<char *>(&tv_sec), sizeof(int32_t));
		stream.read(reinterpret_cast<char *>(&tv_usec), sizeof(int32_t));
		stream.read(reinterpret_cast<char *>(&payloadSize), sizeof(uint32_t));
		payload = (char*)realloc(payload, payloadSize);
		stream.read(payload, payloadSize);
	}

	virtual uint32_t getSize()
	{
		return sizeof(int32_t) + sizeof(int32_t) + sizeof(uint32_t) + payloadSize;
	}

	int32_t tv_sec;
	int32_t tv_usec;
	uint32_t payloadSize;
	char* payload;

protected:
	virtual void doserialize(ostream& stream)
	{
		stream.write(reinterpret_cast<char *>(&tv_sec), sizeof(int32_t));
		stream.write(reinterpret_cast<char *>(&tv_usec), sizeof(int32_t));
		stream.write(reinterpret_cast<char *>(&payloadSize), sizeof(uint32_t));
		stream.write(payload, payloadSize);
	}
};



class HeaderMessage : public BaseMessage 
{
public:
	HeaderMessage(size_t size = 0) : BaseMessage(message_type::header), payloadSize(size)
	{
		payload = (char*)malloc(size);
	}

	virtual ~HeaderMessage()
	{
		free(payload);
	}

	virtual void read(istream& stream)
	{
		stream.read(reinterpret_cast<char *>(&payloadSize), sizeof(uint32_t));
		payload = (char*)realloc(payload, payloadSize);
		stream.read(payload, payloadSize);
	}

	virtual uint32_t getSize()
	{
		return sizeof(uint32_t) + payloadSize;
	}

	uint32_t payloadSize;
	char* payload;

protected:
	virtual void doserialize(ostream& stream)
	{
		stream.write(reinterpret_cast<char *>(&payloadSize), sizeof(uint32_t));
		stream.write(payload, payloadSize);
	}
};



class PcmChunk : public WireChunk
{
public:
	PcmChunk(const SampleFormat& sampleFormat, size_t ms);
	~PcmChunk();

	int readFrames(void* outputBuffer, size_t frameCount);
	bool isEndOfChunk() const;

	inline time_point_ms timePoint() const
	{
		time_point_ms tp;
		std::chrono::milliseconds::rep relativeIdxTp = ((double)idx / ((double)format.rate/1000.));
		return 
			tp + 
			std::chrono::seconds(tv_sec) + 
			std::chrono::milliseconds(tv_usec / 1000) + 
			std::chrono::milliseconds(relativeIdxTp);
	}

	template<typename T>
	inline T getAge() const
	{
		return getAge<T>(timePoint());
	}

	inline long getAge() const
	{
		return getAge<std::chrono::milliseconds>().count();
	}

	inline static long getAge(const time_point_ms& time_point)
	{
		return getAge<std::chrono::milliseconds>(time_point).count();
	}

	template<typename T, typename U>
	static inline T getAge(const std::chrono::time_point<U>& time_point)
	{
		return std::chrono::duration_cast<T>(std::chrono::high_resolution_clock::now() - time_point);
	}

	int seek(int frames);
	double getDuration() const;
	double getDurationUs() const;
	double getTimeLeft() const;
	double getFrameCount() const;

	SampleFormat format;

private:
//	SampleFormat format_;
	uint32_t idx;
};



#endif


