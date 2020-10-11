/*
  Copyright (c) 2020 Sogou, Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#ifndef __RPC_BUFFER_H__
#define __RPC_BUFFER_H__

#ifdef _WIN32
#include <workflow/PlatformSocket.h>
#else
#include <sys/uio.h>
#endif
#include <stddef.h>
#include <string.h>
#include <list>

namespace srpc
{

static constexpr int	BUFFER_PIECE_MIN_SIZE		= 2 * 1024;
static constexpr int	BUFFER_PIECE_MAX_SIZE		= 256 * 1024;

static constexpr int	BUFFER_MODE_COPY			= 0;
static constexpr int	BUFFER_MODE_NOCOPY			= 1;
static constexpr int	BUFFER_MODE_GIFT_NEW		= 2;
static constexpr int	BUFFER_MODE_GIFT_MALLOC		= 3;

static constexpr bool	BUFFER_FETCH_MOVE			= true;
static constexpr bool	BUFFER_FETCH_STAY			= false;

/**
 * @brief   Buffer Class
 * @details
 * - Thread Safety : NO
 * - All buffer should allocated by new char[...] or malloc
 * - Gather buffer piece by piece
 * - Get buffer one by one
 */
class RPCBuffer
{
public:
	/**
	 * @brief      Free all buffer and rewind every state to initialize.
	 * @note       NEVER fail
	 */
	void clear();

	/**
	 * @brief      Get all size of buffer
	 */
	size_t size() const;

	/**
	 * @brief      Cut current buffer at absolutely offset, current buffer keep first part, give second part to out buffer
	 * @param[in]  offset           where to cut
	 * @param[in]  out              points to out buffer
	 * @return     actual give how many bytes to out
	 * @note       this will cause current buffer rewind()
	 */
	size_t cut(size_t offset, RPCBuffer *out);

public:
	/**
	 * @brief      For write. Add one buffer allocated by RPCBuffer
	 * @param[in,out]  buf          a pointer to a buffer
	 * @param[in,out]  size         points to the expect size of buffer, return actual size
	 * @return     false if OOM, or your will get a buffer actual size <=expect-size
	 * @note       Ownership of this buffer remains with the stream
	 */
	bool acquire(void **buf, size_t *size);

	/**
	 * @brief      For write. Add one buffer allocated by RPCBuffer
	 * @param[in,out]  buf          a pointer to a buffer
	 * @return     0 if OOM, or the actual size of the buffer
	 * @note       Ownership of this buffer remains with the stream
	 */
	size_t acquire(void **buf);

	/**
	 * @brief      For write. Add one buffer
	 * @param[in]  buf              upstream name
	 * @param[in]  buflen           consistent-hash functional
	 * @param[in]  mode             BUFFER_MODE_XXX
	 * @return     false if OOM
	 * @note       BUFFER_MODE_NOCOPY      mean in deconstructor ignore this buffer
	 * @note       BUFFER_MODE_COPY        mean memcpy this buffer right here at once
	 * @note       BUFFER_MODE_GIFT_NEW    mean in deconstructor delete this buffer
	 * @note       BUFFER_MODE_GIFT_MALLOC mean in deconstructor free this buffer
	 */
	bool append(void *buf, size_t buflen, int mode);
	bool append(const void *buf, size_t buflen, int mode);

	/**
	 * @brief      For write. Backs up a number of bytes of last buffer
	 * @param[in]  count            how many bytes back up
	 * @return     the actual bytes backup
	 * @note       It is affect the buffer with both acquire and append
	 * @note       count should be less than or equal to the size of the last buffer
	 */
	size_t backup(size_t count);

	/**
	 * @brief      For write. Add one buffer use memcpy
	 * @param[in]  buf              buffer
	 * @param[in]  buflen           buffer size
	 * @return     false if OOM
	 */
	bool write(const void *buf, size_t buflen);

public:
	/**
	 * @brief      For workflow message encode.
	 * @param[in]  iov              iov vector
	 * @param[in]  count            iov vector count
	 * @return     use how many iov
	 * @retval     -1               when count <=0, and set errno EINVAL
	 */
	int encode(struct iovec *iov, int count);

	/**
	 * @brief      merge all buffer into one piece
	 * @param[out] iov              pointer and length of result
	 * @return     suceess or OOM
	 * @retval     0                success
	 * @retval     -1               OOM
	 */
	int merge_all(struct iovec& iov);

public:
	/**
	 * @brief      For read. Reset buffer position to head
	 * @note       NEVER fail
	 */
	void rewind();

	/**
	 * @brief      For read. Get one buffer, NO move offset
	 * @param[in,out]  buf          a pointer to a buffer
	 * @return     0 if no more data to read, or the forward size available
	 */
	size_t peek(const void **buf);

	/**
	 * @brief      For read. Get one buffer by except size, move offset
	 * @param[in,out]  buf          a pointer to a buffer
	 * @param[in,out]  size         except buffer size, and return actual size
	 * @return     false if OOM, or your will get a buffer actual size <=expect-size
	 */
	bool fetch(const void **buf, size_t *size);

	/**
	 * @brief      For read. Get one buffer, move offset
	 * @param[in,out]  buf          a pointer to a buffer
	 * @return     0 if no more data to read, or the forward size available
	 */
	size_t fetch(const void **buf);

	/**
	 * @brief      For read. Fill one buffer with memcpy, move offset
	 * @param[in]  buf              buffer wait to fill
	 * @param[in]  buflen           except buffer size
	 * @return     true if fill buffer exactly bytes, false if no more data to read
	 */
	bool read(void *buf, size_t buflen);

	/**
	 * @brief      For read. move offset, positive mean skip, negative mean backward
	 * @param[in]  offset           except move offset
	 * @return     actual move offset
	 * @note       If offset=0, do nothing at all
	 */
	long seek(long offset);

public:
	void set_piece_min_size(size_t size) { piece_min_size_ = size; }
	void set_piece_max_size(size_t size) { piece_max_size_ = size; }

	RPCBuffer() = default;
	~RPCBuffer();

	RPCBuffer(const RPCBuffer&) = delete;
	RPCBuffer(RPCBuffer&&) = delete;
	RPCBuffer& operator=(const RPCBuffer&) = delete;
	RPCBuffer& operator=(RPCBuffer&&) = delete;

private:
	struct buffer_t
	{
		void *buf;
		size_t buflen;
		bool is_nocopy;
		bool is_new;
	};

	void clear_list_buffer();
	size_t internal_fetch(const void **buf, bool move_or_stay);
	long read_skip(long offset);
	long read_back(long offset);

	std::list<buffer_t> buffer_list_;
	std::pair<std::list<buffer_t>::iterator, size_t> cur_;
	size_t size_ = 0;
	size_t piece_min_size_ = BUFFER_PIECE_MIN_SIZE;
	size_t piece_max_size_ = BUFFER_PIECE_MAX_SIZE;
	bool init_read_over_ = false;
	size_t last_piece_left_ = 0;
};

////////
// inl
inline size_t RPCBuffer::size() const { return size_; }

inline void RPCBuffer::clear_list_buffer()
{
	for (const auto& ele: buffer_list_)
	{
		if (!ele.is_nocopy)
		{
			if (ele.is_new)
				delete []((char *)ele.buf);
			else
				free(ele.buf);
		}
	}
}

inline void RPCBuffer::clear()
{
	clear_list_buffer();
	buffer_list_.clear();
	size_ = 0;
	piece_min_size_ = BUFFER_PIECE_MIN_SIZE;
	piece_max_size_ = BUFFER_PIECE_MAX_SIZE;
	init_read_over_ = false;
	last_piece_left_ = 0;
}

inline bool RPCBuffer::append(void *buf, size_t buflen, int mode)
{
	if (mode == BUFFER_MODE_COPY)
		return write(buf, buflen);

	buffer_t ele;
	void *left_buf;

	if (last_piece_left_ > 0)
	{
		const auto it = buffer_list_.rbegin();

		left_buf = (char *)it->buf + it->buflen;
	}

	ele.buflen = buflen;
	ele.buf = buf;
	ele.is_nocopy = (mode == BUFFER_MODE_NOCOPY);
	ele.is_new = (mode == BUFFER_MODE_GIFT_NEW);
	buffer_list_.emplace_back(std::move(ele));
	size_ += buflen;

	if (last_piece_left_ > 0)
	{
		ele.buflen = 0;
		ele.buf = left_buf;
		ele.is_nocopy = true;
		ele.is_new = false;
		buffer_list_.emplace_back(std::move(ele));
	}

	return true;
}

inline bool RPCBuffer::append(const void *buf, size_t buflen, int mode)
{
	if (mode == BUFFER_MODE_COPY)
		return write(buf, buflen);

	return append(const_cast<void *>(buf), buflen, mode);
}

inline size_t RPCBuffer::backup(size_t count)
{
	if (count == 0 || buffer_list_.empty())
		return 0;

	const auto it = buffer_list_.rbegin();
	size_t sz = 0;

	if (it->buflen > count)
	{
		sz = count;
		it->buflen -= count;
	}
	else
	{
		sz = it->buflen;
		it->buflen = 0;
	}

	last_piece_left_ += sz;
	size_ -= sz;
	return sz;
}

inline bool RPCBuffer::read(void *buf, size_t buflen)
{
	while (buflen > 0)
	{
		const void *p;
		size_t sz = buflen;

		if (!fetch(&p, &sz))
			return false;

		memcpy(buf, p, sz);
		buf = (char *)buf + sz;
		buflen -= sz;
	}

	return true;
}

inline bool RPCBuffer::write(const void *buf, size_t buflen)
{
	while (buflen > 0)
	{
		void *p;
		size_t sz = buflen;

		if (!acquire(&p, &sz))
			return false;

		memcpy(p, buf, sz);
		buf = (const char *)buf + sz;
		buflen -= sz;
	}

	return true;
}

inline void RPCBuffer::rewind()
{
	cur_.first = buffer_list_.begin();
	cur_.second = 0;
	init_read_over_ = true;
}

inline long RPCBuffer::seek(long offset)
{
	if (offset > 0)
		return read_skip(offset);
	else if (offset < 0)
		return read_back(offset);

	return 0;
}

inline size_t RPCBuffer::peek(const void **buf)
{
	return internal_fetch(buf, BUFFER_FETCH_STAY);
}

inline size_t RPCBuffer::fetch(const void **buf)
{
	return internal_fetch(buf, BUFFER_FETCH_MOVE);
}

inline RPCBuffer::~RPCBuffer()
{
	clear_list_buffer();
}

} // namespace srpc

#endif

