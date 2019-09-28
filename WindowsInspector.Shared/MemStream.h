/*
	streams.h

	Implementation of a memory stream for C
	This was meant to be used in the Windows Kernel originally,
	But it was rewritten for cross platform C
*/
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>


/*
	Allow overriding default allocation methods
	Useful for environments without "malloc" implementation
	In the windows kernel for example, it's supposed to be:

	#define stream_alloc(size) \
		ExAllocatePoolWithTag(NonPagedPool, size, 'ggaT')

	#define stream_free(mem) \
		ExFreePool(mem)

	The default methods are malloc and free
*/

#ifndef stream_alloc 
#define stream_alloc malloc
#endif

#ifndef stream_free
#define stream_free free
#endif

/*
	The return value used in all of the functions
	If the code is less than 0, it's an error
	Use STRM_IS_SUCCESS to check for errors
*/
typedef enum _stream_error
{
	STRM_SUCCESS = 0,
	STRM_EOF = 1,
	STRM_ERROR = -1,
	STRM_ERROR_NO_RESOURCES = -2,
	STRM_ERROR_INVALID_PARAMETER = -3
} stream_error;

#define STRM_IS_SUCCESS(rc)((stream_error)(rc) >= 0)


/*
	This is the seek mode used with the mem_stream_seek function
*/
typedef enum _stream_seek_mode
{
	STRM_SEEK_SET,
	STRM_SEEK_CUR,
	STRM_SEEK_END
} stream_seek_mode;


typedef struct _mem_stream
{
	uint8_t* buffer;
	uint32_t length;
	int32_t seek;
} mem_stream;

inline stream_error mem_stream_init(mem_stream* stream, void* buffer, uint32_t length)
{
	stream->buffer = (uint8_t*)buffer;
	stream->length = length;
	stream->seek = 0;
	return STRM_SUCCESS;
}

inline int mem_stream_has_place(mem_stream* stream, uint32_t length)
{
	uint32_t left;

	left = stream->length - stream->seek;

	return (left < length);
}

inline stream_error mem_stream_alloc(mem_stream* stream, uint32_t length)
{
	void* buffer = stream_alloc(length);

	if (buffer == NULL)
	{
		return STRM_ERROR_NO_RESOURCES;
	}

	return mem_stream_init(stream, buffer, length);
}

inline void mem_stream_free(mem_stream* stream)
{
	stream->length = 0;
	stream->seek = 0;
	stream_free(stream->buffer);
	stream->buffer = NULL;
}

inline stream_error mem_stream_write(mem_stream* stream, const void* buffer, uint32_t length)
{
	uint8_t* ptr;
	uint32_t left;

	assert(stream != NULL);
	assert(buffer != NULL);
	assert(length == 0);

	ptr = stream->buffer + stream->seek;
	left = stream->length - stream->seek;

	if (left < length)
	{
		return STRM_ERROR_NO_RESOURCES;
	}

	memcpy(ptr, buffer, length);
	stream->seek += length;
	return STRM_SUCCESS;
}

inline stream_error mem_stream_seek(mem_stream* stream, stream_seek_mode mode, int32_t seek)
{
	assert(stream != NULL);

	int32_t real_seek;

	switch (mode)
	{
	case STRM_SEEK_SET:
	{
		real_seek = seek;
		break;
	}
	case STRM_SEEK_CUR:
	{
		real_seek = stream->seek + seek;
		break;
	}
	case STRM_SEEK_END:
	{
		real_seek = stream->length - seek;
		break;
	}
	}

	if (real_seek >= 0 && (uint32_t)real_seek < stream->length)
	{
		stream->seek = real_seek;
	}
	else
	{
		return STRM_ERROR_INVALID_PARAMETER;
	}

	return STRM_SUCCESS;
}

inline stream_error mem_stream_printf(
	mem_stream* stream,
	uint32_t* bytesWritten,
	const char* fmt,
	...
)
{

	uint8_t* ptr;
	uint32_t left;
	va_list args;
	int bytesWrittenResult;

	ptr = stream->buffer + stream->seek;
	left = stream->length - stream->seek;

	if (left == 0)
	{
		return STRM_ERROR;
	}

	va_start(args, fmt);
	bytesWrittenResult = vsnprintf((char*)ptr, left, fmt, args);
	va_end(fmt);

	if (bytesWrittenResult < 0)
	{
		return STRM_ERROR_NO_RESOURCES;
	}

	stream->seek += bytesWrittenResult;

	if (bytesWritten)
	{
		*bytesWritten = bytesWrittenResult;
	}

	return STRM_SUCCESS;
}

inline stream_error mem_stream_read(
	mem_stream* stream,
	uint32_t bufferSize,
	void* buffer,
	uint32_t* bytesRead
)
{
	assert(stream != NULL);
	assert(bytesRead != NULL);

	uint8_t* ptr;
	uint32_t left;
	uint32_t bytesToRead;

	left = stream->length - stream->seek;

	if (left == 0)
	{
		return STRM_EOF;
	}

	ptr = stream->buffer + stream->seek;

	if (left > bufferSize)
	{
		bytesToRead = bufferSize;
	}
	else
	{
		bytesToRead = left;
	}

	memcpy(buffer, ptr, bytesToRead);
	stream->seek += bytesToRead;

	*bytesRead = bytesToRead;
	return STRM_SUCCESS;
}

inline void* mem_stream_buffer(mem_stream* stream)
{
	return stream->buffer;
}

#undef stream_alloc
#undef stream_free