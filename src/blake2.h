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

#ifndef CHOCOBO1_BLAKE2_H
#define CHOCOBO1_BLAKE2_H

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
	// Blake2();
}


namespace Chocobo1
{
// users should ignore things in this namespace
namespace Blake2_Hash
{
	class Blake2
	{
		// https://blake2.net/

		public:
			template <typename T>
			using Span = gsl::span<T>;

			typedef uint8_t Byte;


			inline explicit Blake2();

			inline void reset();
			inline Blake2& finalize();  // after this, only `toString()`, `toVector()`, `reset()` are available

			inline std::string toString() const;
			inline std::vector<Blake2::Byte> toVector() const;

			inline Blake2& addData(const Span<const Byte> inData);
			inline Blake2& addData(const void *ptr, const long int length);

		private:
			class Uint128
			{
				public:
					explicit Uint128()
						: m_lo(0), m_hi(0)
					{
					}

					Uint128& operator= (const uint64_t n)
					{
						this->m_lo = n;
						this->m_hi = 0;
						return (*this);
					}

					Uint128& operator* (const unsigned int n)
					{
						// only handle `*8` case
						assert(n == 8);

						const uint8_t msb = (m_lo >> 61) & 0xff;
						m_hi = (m_hi << 3) | msb;
						m_lo = m_lo << 3;

						return (*this);
					}

					Uint128& operator+= (const uint64_t n)
					{
						const uint64_t newLo = (m_lo + n);
						if (newLo < m_lo)
							++m_hi;
						m_lo = newLo;

						return (*this);
					}

					uint64_t low() const
					{
						return m_lo;
					}

					uint64_t high() const
					{
						return m_hi;
					}

				private:
					uint64_t m_lo;
					uint64_t m_hi;
			};

			inline void addDataImpl(const Span<const Byte> data, const bool isFinal, const uint64_t paddingLen = 0);

			const unsigned int BLOCK_SIZE = 128;

			std::vector<Byte> m_buffer;
			Uint128 m_sizeCounter;

			uint64_t m_h[8];
			const uint64_t m_initializationVector[8] =
			{
				0x6a09e667f3bcc908, 0xbb67ae8584caa73b, 0x3c6ef372fe94f82b, 0xa54ff53a5f1d36f1,
				0x510e527fade682d1, 0x9b05688c2b3e6c1f, 0x1f83d9abfb41bd6b, 0x5be0cd19137e2179
			};
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
				static_assert(std::is_same<T, uint64_t>::value, "");
				// handle specific endianness here
				const uint8_t *ptr = m_ptr + (sizeof(T) * idx);
				return  ( (static_cast<T>(*(ptr + 0)) <<  0)
						| (static_cast<T>(*(ptr + 1)) <<  8)
						| (static_cast<T>(*(ptr + 2)) << 16)
						| (static_cast<T>(*(ptr + 3)) << 24)
						| (static_cast<T>(*(ptr + 4)) << 32)
						| (static_cast<T>(*(ptr + 5)) << 40)
						| (static_cast<T>(*(ptr + 6)) << 48)
						| (static_cast<T>(*(ptr + 7)) << 56));
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
	constexpr T rotr(const T x, const unsigned int s)
	{
		static_assert(std::is_unsigned<T>::value, "");
		if (s == 0)
			return x;
		return ((x >> s) | (x << ((sizeof(T) * 8) - s)));
	}


	//
	Blake2::Blake2()
	{
		m_buffer.reserve(BLOCK_SIZE);
		reset();
	}

	void Blake2::reset()
	{
		m_buffer.clear();
		m_sizeCounter = 0;

		for (int i = 0; i < 8; ++i)
			m_h[i] = m_initializationVector[i];

		m_h[0] ^= (0x01010000 ^ (0 << 8) ^ 64);
	}

	Blake2& Blake2::finalize()
	{
		// append paddings
		const size_t len = (BLOCK_SIZE - m_buffer.size());
		m_buffer.insert(m_buffer.end(), len, 0);

		addDataImpl(m_buffer, true, len);
		m_buffer.clear();

		return (*this);
	}

	std::string Blake2::toString() const
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

	std::vector<Blake2::Byte> Blake2::toVector() const
	{
		const Span<const uint64_t> state(m_h);
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

	Blake2& Blake2::addData(const Span<const Byte> inData)
	{
		if (inData.empty())
			return (*this);

		Span<const Byte> data = inData;

		if (m_buffer.size() == BLOCK_SIZE)
		{
			addDataImpl(m_buffer, false);
			m_buffer.clear();
		}
		else if (!m_buffer.empty())
		{
			// try fill to BLOCK_SIZE bytes
			const size_t len = std::min<size_t>((BLOCK_SIZE - m_buffer.size()), data.size());
			m_buffer.insert(m_buffer.end(), data.begin(), data.begin() + len);

			addData(data.subspan(len));
			return (*this);
		}

		const size_t dataSize = data.size();
		const size_t remainder = ((dataSize % BLOCK_SIZE) != 0) ? (dataSize % BLOCK_SIZE) : BLOCK_SIZE;
		const size_t len = dataSize - remainder;

		// process data in bulk
		addDataImpl(data.first(len), false);

		// put leftovers or last block in buffer
		m_buffer = {(data.end() - remainder), data.end()};

		return (*this);
	}

	Blake2& Blake2::addData(const void *ptr, const long int length)
	{
		// gsl::span::index_type = long int
		return addData({reinterpret_cast<const Byte*>(ptr), length});
	}

	void Blake2::addDataImpl(const Span<const Byte> data, const bool isFinal, const uint64_t paddingLen)
	{
		assert((data.size() % BLOCK_SIZE) == 0);

		for (size_t iter = 0, iend = static_cast<size_t>(data.size() / BLOCK_SIZE); iter < iend; ++iter)
		{
			const Loader<uint64_t> m(reinterpret_cast<const Byte *>(data.data() + (iter * BLOCK_SIZE)));

			m_sizeCounter += (BLOCK_SIZE - paddingLen);

			uint64_t v[16] =
			{
				m_h[0], m_h[1], m_h[2], m_h[3], m_h[4], m_h[5], m_h[6], m_h[7],
				m_initializationVector[0], m_initializationVector[1], m_initializationVector[2], m_initializationVector[3],
				m_initializationVector[4] ^ m_sizeCounter.low(),
				m_initializationVector[5] ^ m_sizeCounter.high(),
				isFinal ? ~m_initializationVector[6] : m_initializationVector[6],
				m_initializationVector[7]
			};

			#ifdef blakeMix
			#error "macro name clash"
			#else
			#define blakeMix(a, b, c, d, x, y) \
				a = (a + b + x); \
				d = rotr((d ^ a), 32); \
				c = (c + d); \
				b = rotr((b ^ c), 24); \
				a = (a + b + y); \
				d = rotr((d ^ a), 16); \
				c = (c + d); \
				b = rotr((b ^ c), 63);

			// had to unroll loop manually :(
			blakeMix(v[0], v[4], v[8], v[12], m[0], m[1]);
			blakeMix(v[1], v[5], v[9], v[13], m[2], m[3]);
			blakeMix(v[2], v[6], v[10], v[14], m[4], m[5]);
			blakeMix(v[3], v[7], v[11], v[15], m[6], m[7]);
			blakeMix(v[0], v[5], v[10], v[15], m[8], m[9]);
			blakeMix(v[1], v[6], v[11], v[12], m[10], m[11]);
			blakeMix(v[2], v[7], v[8], v[13], m[12], m[13]);
			blakeMix(v[3], v[4], v[9], v[14], m[14], m[15]);
			blakeMix(v[0], v[4], v[8], v[12], m[14], m[10]);
			blakeMix(v[1], v[5], v[9], v[13], m[4], m[8]);
			blakeMix(v[2], v[6], v[10], v[14], m[9], m[15]);
			blakeMix(v[3], v[7], v[11], v[15], m[13], m[6]);
			blakeMix(v[0], v[5], v[10], v[15], m[1], m[12]);
			blakeMix(v[1], v[6], v[11], v[12], m[0], m[2]);
			blakeMix(v[2], v[7], v[8], v[13], m[11], m[7]);
			blakeMix(v[3], v[4], v[9], v[14], m[5], m[3]);
			blakeMix(v[0], v[4], v[8], v[12], m[11], m[8]);
			blakeMix(v[1], v[5], v[9], v[13], m[12], m[0]);
			blakeMix(v[2], v[6], v[10], v[14], m[5], m[2]);
			blakeMix(v[3], v[7], v[11], v[15], m[15], m[13]);
			blakeMix(v[0], v[5], v[10], v[15], m[10], m[14]);
			blakeMix(v[1], v[6], v[11], v[12], m[3], m[6]);
			blakeMix(v[2], v[7], v[8], v[13], m[7], m[1]);
			blakeMix(v[3], v[4], v[9], v[14], m[9], m[4]);
			blakeMix(v[0], v[4], v[8], v[12], m[7], m[9]);
			blakeMix(v[1], v[5], v[9], v[13], m[3], m[1]);
			blakeMix(v[2], v[6], v[10], v[14], m[13], m[12]);
			blakeMix(v[3], v[7], v[11], v[15], m[11], m[14]);
			blakeMix(v[0], v[5], v[10], v[15], m[2], m[6]);
			blakeMix(v[1], v[6], v[11], v[12], m[5], m[10]);
			blakeMix(v[2], v[7], v[8], v[13], m[4], m[0]);
			blakeMix(v[3], v[4], v[9], v[14], m[15], m[8]);
			blakeMix(v[0], v[4], v[8], v[12], m[9], m[0]);
			blakeMix(v[1], v[5], v[9], v[13], m[5], m[7]);
			blakeMix(v[2], v[6], v[10], v[14], m[2], m[4]);
			blakeMix(v[3], v[7], v[11], v[15], m[10], m[15]);
			blakeMix(v[0], v[5], v[10], v[15], m[14], m[1]);
			blakeMix(v[1], v[6], v[11], v[12], m[11], m[12]);
			blakeMix(v[2], v[7], v[8], v[13], m[6], m[8]);
			blakeMix(v[3], v[4], v[9], v[14], m[3], m[13]);
			blakeMix(v[0], v[4], v[8], v[12], m[2], m[12]);
			blakeMix(v[1], v[5], v[9], v[13], m[6], m[10]);
			blakeMix(v[2], v[6], v[10], v[14], m[0], m[11]);
			blakeMix(v[3], v[7], v[11], v[15], m[8], m[3]);
			blakeMix(v[0], v[5], v[10], v[15], m[4], m[13]);
			blakeMix(v[1], v[6], v[11], v[12], m[7], m[5]);
			blakeMix(v[2], v[7], v[8], v[13], m[15], m[14]);
			blakeMix(v[3], v[4], v[9], v[14], m[1], m[9]);
			blakeMix(v[0], v[4], v[8], v[12], m[12], m[5]);
			blakeMix(v[1], v[5], v[9], v[13], m[1], m[15]);
			blakeMix(v[2], v[6], v[10], v[14], m[14], m[13]);
			blakeMix(v[3], v[7], v[11], v[15], m[4], m[10]);
			blakeMix(v[0], v[5], v[10], v[15], m[0], m[7]);
			blakeMix(v[1], v[6], v[11], v[12], m[6], m[3]);
			blakeMix(v[2], v[7], v[8], v[13], m[9], m[2]);
			blakeMix(v[3], v[4], v[9], v[14], m[8], m[11]);
			blakeMix(v[0], v[4], v[8], v[12], m[13], m[11]);
			blakeMix(v[1], v[5], v[9], v[13], m[7], m[14]);
			blakeMix(v[2], v[6], v[10], v[14], m[12], m[1]);
			blakeMix(v[3], v[7], v[11], v[15], m[3], m[9]);
			blakeMix(v[0], v[5], v[10], v[15], m[5], m[0]);
			blakeMix(v[1], v[6], v[11], v[12], m[15], m[4]);
			blakeMix(v[2], v[7], v[8], v[13], m[8], m[6]);
			blakeMix(v[3], v[4], v[9], v[14], m[2], m[10]);
			blakeMix(v[0], v[4], v[8], v[12], m[6], m[15]);
			blakeMix(v[1], v[5], v[9], v[13], m[14], m[9]);
			blakeMix(v[2], v[6], v[10], v[14], m[11], m[3]);
			blakeMix(v[3], v[7], v[11], v[15], m[0], m[8]);
			blakeMix(v[0], v[5], v[10], v[15], m[12], m[2]);
			blakeMix(v[1], v[6], v[11], v[12], m[13], m[7]);
			blakeMix(v[2], v[7], v[8], v[13], m[1], m[4]);
			blakeMix(v[3], v[4], v[9], v[14], m[10], m[5]);
			blakeMix(v[0], v[4], v[8], v[12], m[10], m[2]);
			blakeMix(v[1], v[5], v[9], v[13], m[8], m[4]);
			blakeMix(v[2], v[6], v[10], v[14], m[7], m[6]);
			blakeMix(v[3], v[7], v[11], v[15], m[1], m[5]);
			blakeMix(v[0], v[5], v[10], v[15], m[15], m[11]);
			blakeMix(v[1], v[6], v[11], v[12], m[9], m[14]);
			blakeMix(v[2], v[7], v[8], v[13], m[3], m[12]);
			blakeMix(v[3], v[4], v[9], v[14], m[13], m[0]);
			blakeMix(v[0], v[4], v[8], v[12], m[0], m[1]);
			blakeMix(v[1], v[5], v[9], v[13], m[2], m[3]);
			blakeMix(v[2], v[6], v[10], v[14], m[4], m[5]);
			blakeMix(v[3], v[7], v[11], v[15], m[6], m[7]);
			blakeMix(v[0], v[5], v[10], v[15], m[8], m[9]);
			blakeMix(v[1], v[6], v[11], v[12], m[10], m[11]);
			blakeMix(v[2], v[7], v[8], v[13], m[12], m[13]);
			blakeMix(v[3], v[4], v[9], v[14], m[14], m[15]);
			blakeMix(v[0], v[4], v[8], v[12], m[14], m[10]);
			blakeMix(v[1], v[5], v[9], v[13], m[4], m[8]);
			blakeMix(v[2], v[6], v[10], v[14], m[9], m[15]);
			blakeMix(v[3], v[7], v[11], v[15], m[13], m[6]);
			blakeMix(v[0], v[5], v[10], v[15], m[1], m[12]);
			blakeMix(v[1], v[6], v[11], v[12], m[0], m[2]);
			blakeMix(v[2], v[7], v[8], v[13], m[11], m[7]);
			blakeMix(v[3], v[4], v[9], v[14], m[5], m[3]);

			#undef blakeMix
			#endif

			m_h[0] ^= (v[0] ^ v[8]);
			m_h[1] ^= (v[1] ^ v[9]);
			m_h[2] ^= (v[2] ^ v[10]);
			m_h[3] ^= (v[3] ^ v[11]);
			m_h[4] ^= (v[4] ^ v[12]);
			m_h[5] ^= (v[5] ^ v[13]);
			m_h[6] ^= (v[6] ^ v[14]);
			m_h[7] ^= (v[7] ^ v[15]);
		}
	}
}
	using Blake2 = Blake2_Hash::Blake2;
}

#endif  // CHOCOBO1_BLAKE2_H