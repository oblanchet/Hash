/*
 *  Chocobo1/Hash
 *
 *   Copyright 2017 by Mike Tzou (Chocobo1)
 *     https://github.com/Chocobo1/Hash
 *
 *   Licensed under GNU General Public License 3 or later.
 *
 *  @license GPL3 <https://www.gnu.org/licenses/gpl-3.0-standalone.html>
 */

#ifndef CHOCOBO1_RIPEMD_160_H
#define CHOCOBO1_RIPEMD_160_H

#include "gsl/span"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>


namespace Chocobo1
{
	// Use these!!
	// RIPEMD_160();
}


namespace Chocobo1
{
// users should ignore things in this namespace
namespace RIPEMD_160_Hash
{
	class RIPEMD_160
	{
		// https://homes.esat.kuleuven.be/~bosselae/ripemd160.html

		public:
			template <typename T>
			using Span = gsl::span<T>;

			typedef uint8_t Byte;


			inline explicit RIPEMD_160();

			inline void reset();
			inline RIPEMD_160& finalize();  // after this, only `toString()`, `toVector()`, `reset()` are available

			inline std::string toString() const;
			inline std::vector<RIPEMD_160::Byte> toVector() const;

			inline RIPEMD_160& addData(const Span<const Byte> inData);
			inline RIPEMD_160& addData(const void *ptr, const long int length);

		private:
			inline void addDataImpl(const Span<const Byte> data);

			const unsigned int BLOCK_SIZE = 64;

			std::vector<Byte> m_buffer;
			uint64_t m_sizeCounter;

			uint32_t m_h[5];
	};


	// helpers
	template <typename T>
	class Loader
	{
		// this class workaround loading data from unaligned memory boundaries
		// also eliminate endianness issues
		public:
			explicit Loader(const void *ptr)
				: m_ptr(static_cast<const uint8_t *>(ptr))
			{
			}

			constexpr T operator[](const size_t idx) const
			{
				static_assert(std::is_same<T, uint32_t>::value, "");
				// handle specific endianness here
				const uint8_t *ptr = m_ptr + (sizeof(T) * idx);
				return  ( (static_cast<T>(*(ptr + 0)) <<  0)
						| (static_cast<T>(*(ptr + 1)) <<  8)
						| (static_cast<T>(*(ptr + 2)) << 16)
						| (static_cast<T>(*(ptr + 3)) << 24));
			}

		private:
			const uint8_t *m_ptr;
	};

	template <typename R, typename T>
	constexpr R ror(const T x, const unsigned int s)
	{
		static_assert(std::is_unsigned<R>::value, "");
		const R mask = -1;
		return ((x >> s) & mask);
	}

	template <typename T>
	constexpr T rotl(const T x, const unsigned int s)
	{
		static_assert(std::is_unsigned<T>::value, "");
		if (s == 0)
			return x;
		return ((x << s) | (x >> ((sizeof(T) * 8) - s)));
	}


	//
	RIPEMD_160::RIPEMD_160()
	{
		m_buffer.reserve(BLOCK_SIZE * 2);  // x2 for paddings
		reset();
	}

	void RIPEMD_160::reset()
	{
		m_buffer.clear();
		m_sizeCounter = 0;

		m_h[0] = 0x67452301;
		m_h[1] = 0xEFCDAB89;
		m_h[2] = 0x98BADCFE;
		m_h[3] = 0x10325476;
		m_h[4] = 0xC3D2E1F0;
	}

	RIPEMD_160& RIPEMD_160::finalize()
	{
		m_sizeCounter += m_buffer.size();

		// append 1 bit
		m_buffer.emplace_back(1 << 7);

		// append paddings
		const size_t len = BLOCK_SIZE - ((m_buffer.size() + 8) % BLOCK_SIZE);
		m_buffer.insert(m_buffer.end(), (len + 8), 0);

		// append size in bits
		const uint64_t sizeCounterBits = m_sizeCounter * 8;
		const uint32_t sizeCounterBitsL = ror<uint32_t>(sizeCounterBits, 0);
		const uint32_t sizeCounterBitsH = ror<uint32_t>(sizeCounterBits, 32);
		for (int i = 0; i < 4; ++i)
		{
			m_buffer[m_buffer.size() - 8 + i] = ror<Byte>(sizeCounterBitsL, (8 * i));
			m_buffer[m_buffer.size() - 4 + i] = ror<Byte>(sizeCounterBitsH, (8 * i));
		}

		addDataImpl(m_buffer);
		m_buffer.clear();

		return (*this);
	}

	std::string RIPEMD_160::toString() const
	{
		std::string ret;
		const auto v = toVector();
		ret.reserve(2 * v.size());
		for (const auto &i : v)
		{
			char buf[3];
			snprintf(buf, sizeof(buf), "%02x", i);
			ret.append(buf);
		}

		return ret;
	}

	std::vector<RIPEMD_160::Byte> RIPEMD_160::toVector() const
	{
		const Span<const uint32_t> state(m_h);
		const int dataSize = sizeof(decltype(state)::value_type);

		std::vector<Byte> ret;
		ret.reserve(dataSize * state.size());
		for (const auto &i : state)
		{
			for (int j = 0; j < dataSize; ++j)
				ret.emplace_back(ror<Byte>(i, (j * 8)));
		}

		return ret;
	}

	RIPEMD_160& RIPEMD_160::addData(const Span<const Byte> inData)
	{
		Span<const Byte> data = inData;

		if (!m_buffer.empty())
		{
			const size_t len = std::min<size_t>((BLOCK_SIZE - m_buffer.size()), data.size());  // try fill to BLOCK_SIZE bytes
			m_buffer.insert(m_buffer.end(), data.begin(), data.begin() + len);

			if (m_buffer.size() < BLOCK_SIZE)  // still doesn't fill the buffer
				return (*this);

			addDataImpl(m_buffer);
			m_buffer.clear();

			data = data.subspan(len);
		}

		const size_t dataSize = data.size();
		if (dataSize < BLOCK_SIZE)
		{
			m_buffer = {data.begin(), data.end()};
			return (*this);
		}

		const size_t len = dataSize - (dataSize % BLOCK_SIZE);  // align on BLOCK_SIZE bytes
		addDataImpl(data.first(len));

		if (len < dataSize)  // didn't consume all data
			m_buffer = {data.begin() + len, data.end()};

		return (*this);
	}

	RIPEMD_160& RIPEMD_160::addData(const void *ptr, const long int length)
	{
		// gsl::span::index_type = long int
		return addData({reinterpret_cast<const Byte*>(ptr), length});
	}

	void RIPEMD_160::addDataImpl(const Span<const Byte> data)
	{
		assert((data.size() % BLOCK_SIZE) == 0);

		m_sizeCounter += data.size();

		for (size_t i = 0, iend = static_cast<size_t>(data.size() / BLOCK_SIZE); i < iend; ++i)
		{
			const Loader<uint32_t> x(reinterpret_cast<const Byte *>(data.data() + (i * BLOCK_SIZE)));

			const auto f1 = [](uint32_t &x, uint32_t &y, uint32_t &z) -> uint32_t
			{
				return (x ^ y ^ z);
			};
			const auto f2 = [](uint32_t &x, uint32_t &y, uint32_t &z) -> uint32_t
			{
				return ((x & (y ^ z)) ^ z);  // alternative
			};
			const auto f3 = [](uint32_t &x, uint32_t &y, uint32_t &z) -> uint32_t
			{
				return ((x | (~y)) ^ z);
			};
			const auto f4 = [](uint32_t &x, uint32_t &y, uint32_t &z) -> uint32_t
			{
				return (((x ^ y) & z) ^ y);  // alternative
			};
			const auto f5 = [](uint32_t &x, uint32_t &y, uint32_t &z) -> uint32_t
			{
				return (x ^ (y | (~z)));
			};

			uint32_t a = m_h[0];
			uint32_t b = m_h[1];
			uint32_t c = m_h[2];
			uint32_t d = m_h[3];
			uint32_t e = m_h[4];
			const auto lineLeft = [x](uint32_t &a, uint32_t &b, uint32_t &c, uint32_t &d, uint32_t &e, const auto & f, const uint32_t k, const unsigned int r, const unsigned int s) -> void
			{
				a = rotl((a + f(b, c, d) + x[r] + k), s) + e;
				c = rotl(c, 10);
			};

			uint32_t aa = m_h[0];
			uint32_t bb = m_h[1];
			uint32_t cc = m_h[2];
			uint32_t dd = m_h[3];
			uint32_t ee = m_h[4];
			const auto &lineRight = lineLeft;

			lineLeft(a, b, c, d, e, f1, 0x00000000, 0, 11);
			lineLeft(e, a, b, c, d, f1, 0x00000000, 1, 14);
			lineRight(aa, bb, cc, dd, ee, f5, 0x50A28BE6, 5, 8);
			lineRight(ee, aa, bb, cc, dd, f5, 0x50A28BE6, 14, 9);
			lineLeft(d, e, a, b, c, f1, 0x00000000, 2, 15);
			lineLeft(c, d, e, a, b, f1, 0x00000000, 3, 12);
			lineRight(dd, ee, aa, bb, cc, f5, 0x50A28BE6, 7, 9);
			lineRight(cc, dd, ee, aa, bb, f5, 0x50A28BE6, 0, 11);
			lineLeft(b, c, d, e, a, f1, 0x00000000, 4, 5);
			lineLeft(a, b, c, d, e, f1, 0x00000000, 5, 8);
			lineRight(bb, cc, dd, ee, aa, f5, 0x50A28BE6, 9, 13);
			lineRight(aa, bb, cc, dd, ee, f5, 0x50A28BE6, 2, 15);
			lineLeft(e, a, b, c, d, f1, 0x00000000, 6, 7);
			lineLeft(d, e, a, b, c, f1, 0x00000000, 7, 9);
			lineRight(ee, aa, bb, cc, dd, f5, 0x50A28BE6, 11, 15);
			lineRight(dd, ee, aa, bb, cc, f5, 0x50A28BE6, 4, 5);
			lineLeft(c, d, e, a, b, f1, 0x00000000, 8, 11);
			lineLeft(b, c, d, e, a, f1, 0x00000000, 9, 13);
			lineRight(cc, dd, ee, aa, bb, f5, 0x50A28BE6, 13, 7);
			lineRight(bb, cc, dd, ee, aa, f5, 0x50A28BE6, 6, 7);
			lineLeft(a, b, c, d, e, f1, 0x00000000, 10, 14);
			lineLeft(e, a, b, c, d, f1, 0x00000000, 11, 15);
			lineRight(aa, bb, cc, dd, ee, f5, 0x50A28BE6, 15, 8);
			lineRight(ee, aa, bb, cc, dd, f5, 0x50A28BE6, 8, 11);
			lineLeft(d, e, a, b, c, f1, 0x00000000, 12, 6);
			lineLeft(c, d, e, a, b, f1, 0x00000000, 13, 7);
			lineRight(dd, ee, aa, bb, cc, f5, 0x50A28BE6, 1, 14);
			lineRight(cc, dd, ee, aa, bb, f5, 0x50A28BE6, 10, 14);
			lineLeft(b, c, d, e, a, f1, 0x00000000, 14, 9);
			lineLeft(a, b, c, d, e, f1, 0x00000000, 15, 8);
			lineRight(bb, cc, dd, ee, aa, f5, 0x50A28BE6, 3, 12);
			lineRight(aa, bb, cc, dd, ee, f5, 0x50A28BE6, 12, 6);
			lineLeft(e, a, b, c, d, f2, 0x5A827999, 7, 7);
			lineLeft(d, e, a, b, c, f2, 0x5A827999, 4, 6);
			lineRight(ee, aa, bb, cc, dd, f4, 0x5C4DD124, 6, 9);
			lineRight(dd, ee, aa, bb, cc, f4, 0x5C4DD124, 11, 13);
			lineLeft(c, d, e, a, b, f2, 0x5A827999, 13, 8);
			lineLeft(b, c, d, e, a, f2, 0x5A827999, 1, 13);
			lineRight(cc, dd, ee, aa, bb, f4, 0x5C4DD124, 3, 15);
			lineRight(bb, cc, dd, ee, aa, f4, 0x5C4DD124, 7, 7);
			lineLeft(a, b, c, d, e, f2, 0x5A827999, 10, 11);
			lineLeft(e, a, b, c, d, f2, 0x5A827999, 6, 9);
			lineRight(aa, bb, cc, dd, ee, f4, 0x5C4DD124, 0, 12);
			lineRight(ee, aa, bb, cc, dd, f4, 0x5C4DD124, 13, 8);
			lineLeft(d, e, a, b, c, f2, 0x5A827999, 15, 7);
			lineLeft(c, d, e, a, b, f2, 0x5A827999, 3, 15);
			lineRight(dd, ee, aa, bb, cc, f4, 0x5C4DD124, 5, 9);
			lineRight(cc, dd, ee, aa, bb, f4, 0x5C4DD124, 10, 11);
			lineLeft(b, c, d, e, a, f2, 0x5A827999, 12, 7);
			lineLeft(a, b, c, d, e, f2, 0x5A827999, 0, 12);
			lineRight(bb, cc, dd, ee, aa, f4, 0x5C4DD124, 14, 7);
			lineRight(aa, bb, cc, dd, ee, f4, 0x5C4DD124, 15, 7);
			lineLeft(e, a, b, c, d, f2, 0x5A827999, 9, 15);
			lineLeft(d, e, a, b, c, f2, 0x5A827999, 5, 9);
			lineRight(ee, aa, bb, cc, dd, f4, 0x5C4DD124, 8, 12);
			lineRight(dd, ee, aa, bb, cc, f4, 0x5C4DD124, 12, 7);
			lineLeft(c, d, e, a, b, f2, 0x5A827999, 2, 11);
			lineLeft(b, c, d, e, a, f2, 0x5A827999, 14, 7);
			lineRight(cc, dd, ee, aa, bb, f4, 0x5C4DD124, 4, 6);
			lineRight(bb, cc, dd, ee, aa, f4, 0x5C4DD124, 9, 15);
			lineLeft(a, b, c, d, e, f2, 0x5A827999, 11, 13);
			lineLeft(e, a, b, c, d, f2, 0x5A827999, 8, 12);
			lineRight(aa, bb, cc, dd, ee, f4, 0x5C4DD124, 1, 13);
			lineRight(ee, aa, bb, cc, dd, f4, 0x5C4DD124, 2, 11);
			lineLeft(d, e, a, b, c, f3, 0x6ED9EBA1, 3, 11);
			lineLeft(c, d, e, a, b, f3, 0x6ED9EBA1, 10, 13);
			lineRight(dd, ee, aa, bb, cc, f3, 0x6D703EF3, 15, 9);
			lineRight(cc, dd, ee, aa, bb, f3, 0x6D703EF3, 5, 7);
			lineLeft(b, c, d, e, a, f3, 0x6ED9EBA1, 14, 6);
			lineLeft(a, b, c, d, e, f3, 0x6ED9EBA1, 4, 7);
			lineRight(bb, cc, dd, ee, aa, f3, 0x6D703EF3, 1, 15);
			lineRight(aa, bb, cc, dd, ee, f3, 0x6D703EF3, 3, 11);
			lineLeft(e, a, b, c, d, f3, 0x6ED9EBA1, 9, 14);
			lineLeft(d, e, a, b, c, f3, 0x6ED9EBA1, 15, 9);
			lineRight(ee, aa, bb, cc, dd, f3, 0x6D703EF3, 7, 8);
			lineRight(dd, ee, aa, bb, cc, f3, 0x6D703EF3, 14, 6);
			lineLeft(c, d, e, a, b, f3, 0x6ED9EBA1, 8, 13);
			lineLeft(b, c, d, e, a, f3, 0x6ED9EBA1, 1, 15);
			lineRight(cc, dd, ee, aa, bb, f3, 0x6D703EF3, 6, 6);
			lineRight(bb, cc, dd, ee, aa, f3, 0x6D703EF3, 9, 14);
			lineLeft(a, b, c, d, e, f3, 0x6ED9EBA1, 2, 14);
			lineLeft(e, a, b, c, d, f3, 0x6ED9EBA1, 7, 8);
			lineRight(aa, bb, cc, dd, ee, f3, 0x6D703EF3, 11, 12);
			lineRight(ee, aa, bb, cc, dd, f3, 0x6D703EF3, 8, 13);
			lineLeft(d, e, a, b, c, f3, 0x6ED9EBA1, 0, 13);
			lineLeft(c, d, e, a, b, f3, 0x6ED9EBA1, 6, 6);
			lineRight(dd, ee, aa, bb, cc, f3, 0x6D703EF3, 12, 5);
			lineRight(cc, dd, ee, aa, bb, f3, 0x6D703EF3, 2, 14);
			lineLeft(b, c, d, e, a, f3, 0x6ED9EBA1, 13, 5);
			lineLeft(a, b, c, d, e, f3, 0x6ED9EBA1, 11, 12);
			lineRight(bb, cc, dd, ee, aa, f3, 0x6D703EF3, 10, 13);
			lineRight(aa, bb, cc, dd, ee, f3, 0x6D703EF3, 0, 13);
			lineLeft(e, a, b, c, d, f3, 0x6ED9EBA1, 5, 7);
			lineLeft(d, e, a, b, c, f3, 0x6ED9EBA1, 12, 5);
			lineRight(ee, aa, bb, cc, dd, f3, 0x6D703EF3, 4, 7);
			lineRight(dd, ee, aa, bb, cc, f3, 0x6D703EF3, 13, 5);
			lineLeft(c, d, e, a, b, f4, 0x8F1BBCDC, 1, 11);
			lineLeft(b, c, d, e, a, f4, 0x8F1BBCDC, 9, 12);
			lineRight(cc, dd, ee, aa, bb, f2, 0x7A6D76E9, 8, 15);
			lineRight(bb, cc, dd, ee, aa, f2, 0x7A6D76E9, 6, 5);
			lineLeft(a, b, c, d, e, f4, 0x8F1BBCDC, 11, 14);
			lineLeft(e, a, b, c, d, f4, 0x8F1BBCDC, 10, 15);
			lineRight(aa, bb, cc, dd, ee, f2, 0x7A6D76E9, 4, 8);
			lineRight(ee, aa, bb, cc, dd, f2, 0x7A6D76E9, 1, 11);
			lineLeft(d, e, a, b, c, f4, 0x8F1BBCDC, 0, 14);
			lineLeft(c, d, e, a, b, f4, 0x8F1BBCDC, 8, 15);
			lineRight(dd, ee, aa, bb, cc, f2, 0x7A6D76E9, 3, 14);
			lineRight(cc, dd, ee, aa, bb, f2, 0x7A6D76E9, 11, 14);
			lineLeft(b, c, d, e, a, f4, 0x8F1BBCDC, 12, 9);
			lineLeft(a, b, c, d, e, f4, 0x8F1BBCDC, 4, 8);
			lineRight(bb, cc, dd, ee, aa, f2, 0x7A6D76E9, 15, 6);
			lineRight(aa, bb, cc, dd, ee, f2, 0x7A6D76E9, 0, 14);
			lineLeft(e, a, b, c, d, f4, 0x8F1BBCDC, 13, 9);
			lineLeft(d, e, a, b, c, f4, 0x8F1BBCDC, 3, 14);
			lineRight(ee, aa, bb, cc, dd, f2, 0x7A6D76E9, 5, 6);
			lineRight(dd, ee, aa, bb, cc, f2, 0x7A6D76E9, 12, 9);
			lineLeft(c, d, e, a, b, f4, 0x8F1BBCDC, 7, 5);
			lineLeft(b, c, d, e, a, f4, 0x8F1BBCDC, 15, 6);
			lineRight(cc, dd, ee, aa, bb, f2, 0x7A6D76E9, 2, 12);
			lineRight(bb, cc, dd, ee, aa, f2, 0x7A6D76E9, 13, 9);
			lineLeft(a, b, c, d, e, f4, 0x8F1BBCDC, 14, 8);
			lineLeft(e, a, b, c, d, f4, 0x8F1BBCDC, 5, 6);
			lineRight(aa, bb, cc, dd, ee, f2, 0x7A6D76E9, 9, 12);
			lineRight(ee, aa, bb, cc, dd, f2, 0x7A6D76E9, 7, 5);
			lineLeft(d, e, a, b, c, f4, 0x8F1BBCDC, 6, 5);
			lineLeft(c, d, e, a, b, f4, 0x8F1BBCDC, 2, 12);
			lineRight(dd, ee, aa, bb, cc, f2, 0x7A6D76E9, 10, 15);
			lineRight(cc, dd, ee, aa, bb, f2, 0x7A6D76E9, 14, 8);
			lineLeft(b, c, d, e, a, f5, 0xA953FD4E, 4, 9);
			lineLeft(a, b, c, d, e, f5, 0xA953FD4E, 0, 15);
			lineRight(bb, cc, dd, ee, aa, f1, 0x00000000, 12, 8);
			lineRight(aa, bb, cc, dd, ee, f1, 0x00000000, 15, 5);
			lineLeft(e, a, b, c, d, f5, 0xA953FD4E, 5, 5);
			lineLeft(d, e, a, b, c, f5, 0xA953FD4E, 9, 11);
			lineRight(ee, aa, bb, cc, dd, f1, 0x00000000, 10, 12);
			lineRight(dd, ee, aa, bb, cc, f1, 0x00000000, 4, 9);
			lineLeft(c, d, e, a, b, f5, 0xA953FD4E, 7, 6);
			lineLeft(b, c, d, e, a, f5, 0xA953FD4E, 12, 8);
			lineRight(cc, dd, ee, aa, bb, f1, 0x00000000, 1, 12);
			lineRight(bb, cc, dd, ee, aa, f1, 0x00000000, 5, 5);
			lineLeft(a, b, c, d, e, f5, 0xA953FD4E, 2, 13);
			lineLeft(e, a, b, c, d, f5, 0xA953FD4E, 10, 12);
			lineRight(aa, bb, cc, dd, ee, f1, 0x00000000, 8, 14);
			lineRight(ee, aa, bb, cc, dd, f1, 0x00000000, 7, 6);
			lineLeft(d, e, a, b, c, f5, 0xA953FD4E, 14, 5);
			lineLeft(c, d, e, a, b, f5, 0xA953FD4E, 1, 12);
			lineRight(dd, ee, aa, bb, cc, f1, 0x00000000, 6, 8);
			lineRight(cc, dd, ee, aa, bb, f1, 0x00000000, 2, 13);
			lineLeft(b, c, d, e, a, f5, 0xA953FD4E, 3, 13);
			lineLeft(a, b, c, d, e, f5, 0xA953FD4E, 8, 14);
			lineRight(bb, cc, dd, ee, aa, f1, 0x00000000, 13, 6);
			lineRight(aa, bb, cc, dd, ee, f1, 0x00000000, 14, 5);
			lineLeft(e, a, b, c, d, f5, 0xA953FD4E, 11, 11);
			lineLeft(d, e, a, b, c, f5, 0xA953FD4E, 6, 8);
			lineRight(ee, aa, bb, cc, dd, f1, 0x00000000, 0, 15);
			lineRight(dd, ee, aa, bb, cc, f1, 0x00000000, 3, 13);
			lineLeft(c, d, e, a, b, f5, 0xA953FD4E, 15, 5);
			lineLeft(b, c, d, e, a, f5, 0xA953FD4E, 13, 6);
			lineRight(cc, dd, ee, aa, bb, f1, 0x00000000, 9, 11);
			lineRight(bb, cc, dd, ee, aa, f1, 0x00000000, 11, 11);

			const uint32_t t = m_h[1] + c + dd;
			m_h[1] = m_h[2] + d + ee;
			m_h[2] = m_h[3] + e + aa;
			m_h[3] = m_h[4] + a + bb;
			m_h[4] = m_h[0] + b + cc;
			m_h[0] = t;
		}
	}
}
	using RIPEMD_160 = RIPEMD_160_Hash::RIPEMD_160;
}

#endif  // CHOCOBO1_RIPEMD_160_H