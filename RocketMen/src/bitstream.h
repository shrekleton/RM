
#pragma once

class BitStream
{
public:
	
	// Write data replacing the current buffer
	virtual void writeBuffer(char* data, size_t length) = 0;

	// Write data appending to the current buffer
	virtual void writeData(char* data, size_t length) = 0;

	virtual void writeBits(bool value, size_t numBits = 1) = 0;
	virtual void writeBytes(char value, size_t numBytes = 1) = 0;
	virtual void writeFloat(float value) = 0;
	virtual void writeInt(int value) = 0;
	virtual void writeBool(bool value) = 0;


	virtual void ReadBytes(char* output, size_t numBytes = 1) = 0;
	virtual void readBit(bool* output) = 0;
	virtual float readFloat() = 0;
	virtual int readInt() = 0;
	virtual bool readBool() = 0;

	virtual const size_t getLength() const = 0;
	virtual const char* getBuffer() const = 0;

	virtual ~BitStream() {}
	static BitStream* create();
};


