// ImageProcessing.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "ImageProcessing.h"

using namespace ImageProcessing;

PNG_HANDLE::PNG_HANDLE(const char *filename)
{
	WIN32_FIND_DATAA *p = new WIN32_FIND_DATAA;
	HANDLE h = FindFirstFileA(filename, p);
	if (h != INVALID_HANDLE_VALUE)
	{
		file = new fstream(filename, ios::in | ios::out | ios::binary);
		byte c[4];
		file->seekg(1);
		file->read(c, 3);
		c[3] = 0;
		if (strcmp(c, "PNG") == 0)
		{
			fileVerified = true;
			file->seekg(0);
		}
		else 
		{
			file->close();
			delete file;
			fileVerified = false;
		}
	}
	else {
		file = nullptr;
		fileVerified = false;
	}
	FindClose(h);

	headerAvailable = false;
	dataChunk = nullptr;

	delete p;
}

bool PNG_HANDLE::isVerified()
{
	return fileVerified;
}

RESULT PNG_HANDLE::readHeader()
{
	if (!fileVerified)
	{
		return (RESULT("NOFILE ERROR: PNG file not found / corrupted."));
	}

	file->seekg(8);
	uint32_t length = 0;
	
	file->read((char*)&length, 4);
	length = _byteswap_ulong(length);

	byte type[5];
	file->read(type, 4);
	type[4] = 0;

	if (strcmp(type, "IHDR") != 0)
	{
		fileVerified = false;
		file->close();
		file = nullptr;
		return (RESULT("PNG FILE CORRUPTED"));
	}
	file->read((char*)(&header), length);
	header.width = _byteswap_ulong(header.width);
	header.height = _byteswap_ulong(header.height);
	headerAvailable = true;
	chunkOffset = file->tellg();
	chunkOffset += 4;

	return (RESULT(SUCCESS, nullptr));
}

RESULT::RESULT(const char * e)
{
	type = FAIL;
	error = e;
}

RESULT::RESULT(RESULT_TYPE t, void *d)
{
	type = t;
	dataptr = d;
}

PNG_HANDLE::~PNG_HANDLE()
{
	if (file != nullptr)
		file->close();
	if (dataChunk != nullptr)
		delete dataChunk;
}

RESULT PNG_HANDLE::width()
{
	if (!headerAvailable)
		return (RESULT("No header details"));
	return (RESULT(SUCCESS, (void *)(&(header.width))));
}

RESULT PNG_HANDLE::height()
{
	if (!headerAvailable)
		return (RESULT("No header details"));
	return (RESULT(SUCCESS, (void *)(&(header.height))));
}

RESULT PNG_HANDLE::colorType()
{
	if (!headerAvailable)
		return (RESULT("No header details"));
	return (RESULT(SUCCESS, (void *)(&(header.colorType))));
}

RESULT PNG_HANDLE::bitDepth()
{
	if (!headerAvailable)
		return (RESULT("No header details"));
	return (RESULT(SUCCESS, (void *)(&(header.bitDepth))));
}

RESULT PNG_HANDLE::compressionMethod()
{
	if (!headerAvailable)
		return (RESULT("No header details"));
	return (RESULT(SUCCESS, (void *)(&(header.compressionMethod))));
}

RESULT PNG_HANDLE::filterMethod()
{
	if (!headerAvailable)
		return (RESULT("No header details"));
	return (RESULT(SUCCESS, (void *)(&(header.filterMethod))));
}

RESULT PNG_HANDLE::interlaceMethod()
{
	if (!headerAvailable)
		return (RESULT("No header details"));
	return (RESULT(SUCCESS, (void *)(&(header.interlaceMethod))));
}

RESULT PNG_HANDLE::loadData()
{
	if (!headerAvailable)
		return (RESULT("No header data"));

	file->seekg(chunkOffset);

	switch (header.colorType)
	{
	case 1:
		//load palette first
	case 0:
		if (header.bitDepth == 1 || header.bitDepth == 2 || header.bitDepth == 4 || header.bitDepth == 8)
		{
			dataChunk = new chunkG_8[header.width * header.height];
			if (dataChunk == nullptr)
				return (RESULT("Not enough memory"));
			uint32_t size;
			chunkG_1 *currData = (chunkG_1*)dataChunk;

			while (true)
			{
				char chunkName[5];
				file->read((char*)&size, 4);
				size = _byteswap_ulong(size);
				file->read(chunkName, 4);
				chunkName[5] = 0;

				if (strcmp(chunkName, "IDAT") == 0)
				{
					//read data
					byte Byte;
					while (size--)
					{
						Byte = file->get();
						if (header.bitDepth == 1)
						{
							while (Byte)
							{
								currData->g = Byte & (1 << 7);
								Byte <<= 1;
								currData++;
							}
						}
						else if (header.bitDepth == 2)
						{
							while (Byte)
							{
								currData->g = Byte & (0b11000000);
								Byte <<= 2;
								currData++;
							}
						}
						else if (header.bitDepth == 4)
						{
							while (Byte)
							{
								currData->g = Byte & (0b11110000);
								Byte <<= 4;
								currData++;
							}
						}
						else
							(currData++)->g = Byte;
					}
					file->seekg(4, ios::cur);
					continue;
				}
				else if (strcmp(chunkName, "IEND") == 0)
				{
					break;
				}
				file->seekg(size + 4, ios::cur);
			}
			return (RESULT(CHUNKG_8, dataChunk));
		}
		else if (header.bitDepth == 16)
		{
			dataChunk = new chunkG_16[header.width * header.height];
			if (dataChunk == nullptr)
				return RESULT("Not enough memory");

			uint32_t size;
			chunkG_16 *currData = (chunkG_16*)dataChunk;

			while (true)
			{
				file->read((char*)&size, 4);
				size = _byteswap_ulong(size);
				char chunkName[5];
				file->read(chunkName, 4);
				chunkName[5] = 0;
				if (strcmp(chunkName, "IEND") == 0)
				{
					break;
				}
				else if (strcmp(chunkName, "IDAT") == 0)
				{
					file->read((char*)currData, size);
					currData += size / 16;
					file->seekg(4, ios::cur);
					continue;
				}
				file->seekg(size + 4, ios::cur);
			}

			return (RESULT(CHUNKG_16, dataChunk));
		}
		else
			return RESULT("Invalid/unsupported bitdepth");
		
	case 2:
		if (header.bitDepth == 8)
		{
			bool flag = true;
			dataChunk = new chunkRGB_8[header.width * header.height];
			if (dataChunk == nullptr)
			{
				return (RESULT("Not enough memory"));
			}
			uint32_t size = 0;
			char *currData = (char*)dataChunk;
			do
			{
				while (file->read((char*)&size, 4)) {
					size = _byteswap_ulong(size);
					char chunk[5];
					file->read(chunk, 4);
					chunk[4] = 0;
					if (strcmp(chunk, "IDAT") == 0)
						break;
					else if (strcmp(chunk, "IEND") == 0)
					{
						flag = false;
						break;
					}
					file->seekg(size + 4, ios::cur);
				}
				if (flag)
				{
					while (size--)	
						*(currData++) = file->get();

					file->seekg(4, ios::cur);
				}
			} while (flag);
			return (RESULT(CHUNKRGB_8, dataChunk));
		}
		else if (header.bitDepth == 16)
		{
			bool flag = true;
			dataChunk = new chunkRGB_16[header.width * header.height];
			if (dataChunk == nullptr)
			{
				return (RESULT("Not enough memory"));
			}
			uint32_t size = 0;
			char *currData = (char*)dataChunk;
			do
			{
				while (file->read((char*)&size, 4)) {
					size = _byteswap_ulong(size);
					char chunk[5];
					file->read(chunk, 4);
					chunk[4] = 0;
					if (strcmp(chunk, "IDAT") == 0)
						break;
					else if (strcmp(chunk, "IEND") == 0)
					{
						flag = false;
						break;
					}
					file->seekg(size + 4, ios::cur);
				}
				if (flag)
				{
					while (size--)
						*(currData++) = file->get();

					file->seekg(4, ios::cur);
				}
			} while (flag);
			return (RESULT(CHUNKRGB_16, dataChunk));
		}
		else
			return (RESULT("Invalid/unsupported bitdepth"));

	default:
		return RESULT("Invalid/unsupported color type");
	}
}

RESULT::operator bool() {
	return type;
}

int ImageProcessing::chunkG_8::compare(chunk *a)
{
	return g - ((chunkG_8*)a)->g;
}

int chunkG_16::compare(chunk *a)
{
	return g - ((chunkG_16*)a)->g;
}

int chunkI_1::compare(chunk *a)
{
	return i - ((chunkI_1*)a)->i;
}

int chunkGA_8::compare(chunk *a)
{
	chunkGA_8*p = (chunkGA_8*)a;
	return !(g == p->g && this->a == p->a);
}

int chunkGA_16::compare(chunk *a)
{
	chunkGA_16*p = (chunkGA_16*)a;
	return !(g == p->g && this->a == p->a);
}

int chunkRGB_8::compare(chunk *a)
{
	chunkRGB_8*p = (chunkRGB_8*)a;
	return !(g == p->g && r == p->r && b == p->b);
}

int chunkRGB_16::compare(chunk *a)
{
	chunkRGB_16*p = (chunkRGB_16*)a;
	return !(g == p->g && r == p->r && b == p->b);
}

int chunkRGBA_8::compare(chunk *a)
{
	chunkRGBA_8*p = (chunkRGBA_8*)a;
	return !(g == p->g && r == p->r && b == p->b && this->a == p->a);
}

int chunkRGBA_16::compare(chunk *a)
{
	chunkRGBA_16*p = (chunkRGBA_16*)a;
	return !(g == p->g && r == p->r && b == p->b && this->a == p->a);
}

chunkG_8::chunkG_8(uint8_t g)
{
	this->g = g;
}

chunkG_8::chunkG_8()
{
	g = 0;
}

chunkG_16::chunkG_16(uint16_t a)
{
	g = a;
}

chunkG_16::chunkG_16()
{
	g = 0;
}

chunkI_1::chunkI_1(uint8_t a)
{
	i = a;
}

chunkGA_8::chunkGA_8()
{
	g = a = 0;
}

chunkGA_8::chunkGA_8(uint8_t x, uint8_t y)
{
	g = x;
	a = y;
}

chunkGA_16::chunkGA_16()
{
	g = a = 0;
}

chunkGA_16::chunkGA_16(uint16_t x, uint16_t y)
{
	g = x;
	a = y;
}

chunkRGB_8::chunkRGB_8()
{
	r = g = b = 0;
}

chunkRGB_8::chunkRGB_8(uint8_t a, uint8_t b, uint8_t c)
{
	r = a;
	g = b;
	this->b = c;
}

chunkRGB_16::chunkRGB_16()
{
	r = g = b = 0;
}

chunkRGB_16::chunkRGB_16(uint16_t a, uint16_t b, uint16_t c)
{
	r = a;
	g = b;
	this->b = c;
}

chunkRGBA_8::chunkRGBA_8()
{
	r = g = b = a = 0;
}

chunkRGBA_8::chunkRGBA_8(uint8_t x, uint8_t y, uint8_t z, uint8_t w)
{
	r = x;
	g = y;
	b = z;
	a = w;
}

chunkRGBA_16::chunkRGBA_16()
{
	r = g = b = a = 0;
}

chunkRGBA_16::chunkRGBA_16(uint16_t x, uint16_t y, uint16_t z, uint16_t w)
{
	r = x;
	g = y;
	b = z;
	a = w;
}

point::point()
{
	x = y = 0;
}

point::point(int a, int b)
{
	x = a;
	y = b;
}