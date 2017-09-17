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

#ifndef CHOCOBO1_MD2_H
#define CHOCOBO1_MD2_H

#include "gsl/span"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>


namespace Chocobo1
{
	// Use these!!
	// MD2();
}


namespace Chocobo1
{
// users should ignore things in this namespace
namespace MD2_Hash
{
	class MD2
	{
		// https://tools.ietf.org/html/rfc1319

		public:
			template <typename T>
			using Span = gsl::span<T>;

			typedef uint8_t Byte;


			inline explicit MD2();

			inline void reset();
			inline MD2& finalize();  // after this, only `toString()`, `toVector()`, `reset()` are available

			inline std::string toString() const;
			inline std::vector<MD2::Byte> toVector() const;

			inline MD2& addData(const Span<const Byte> inData);
			inline MD2& addData(const void *ptr, const long int length);

		private:
			inline void addDataImpl(const Span<const Byte> data);

			const unsigned int BLOCK_SIZE = 16;

			std::vector<Byte> m_buffer;

			Byte m_x[48];
			Byte m_checksum[16];
			Byte m_checksumL;
	};


	//
	MD2::MD2()
	{
		m_buffer.reserve(BLOCK_SIZE * 2);  // x2 for paddings
		reset();
	}

	void MD2::reset()
	{
		m_buffer.clear();

		for (auto &i : m_x)
			i = 0;
		for (auto &i : m_checksum)
			i = 0;
		m_checksumL = 0;
	}

	MD2& MD2::finalize()
	{
		// append padding bytes
		const unsigned int len = BLOCK_SIZE - (m_buffer.size() % BLOCK_SIZE);
		m_buffer.insert(m_buffer.end(), len, len);
		addDataImpl(m_buffer);
		m_buffer.clear();

		// append checksum
		const std::vector<Byte> currentChecksum = {std::begin(m_checksum), std::end(m_checksum)};
		addDataImpl(currentChecksum);

		return (*this);
	}

	std::string MD2::toString() const
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

	std::vector<MD2::Byte> MD2::toVector() const
	{
		const auto iter = std::begin(m_x);
		return {iter, (iter + 16)};
	}

	MD2& MD2::addData(const Span<const Byte> inData)
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

	MD2& MD2::addData(const void *ptr, const long int length)
	{
		// gsl::span::index_type = long int
		return addData({reinterpret_cast<const Byte *>(ptr), length});
	}

	void MD2::addDataImpl(const Span<const Byte> data)
	{
		for (size_t i = 0, iend = static_cast<size_t>(data.size() / BLOCK_SIZE); i < iend; ++i)
		{
			const Span<const Byte> m(reinterpret_cast<const Byte *>(data.data() + (i * BLOCK_SIZE)), 16);

			// calculate checksum
			static const Byte piSubst[256] =
			{
				 41,  46,  67, 201, 162, 216, 124,   1,  61,  54,  84, 161, 236, 240,   6,  19,
				 98, 167,   5, 243, 192, 199, 115, 140, 152, 147,  43, 217, 188,  76, 130, 202,
				 30, 155,  87,  60, 253, 212, 224,  22, 103,  66, 111,  24, 138,  23, 229,  18,
				190,  78, 196, 214, 218, 158, 222,  73, 160, 251, 245, 142, 187,  47, 238, 122,
				169, 104, 121, 145,  21, 178,   7,  63, 148, 194,  16, 137,  11,  34,  95,  33,
				128, 127,  93, 154,  90, 144,  50,  39,  53,  62, 204, 231, 191, 247, 151,   3,
				255,  25,  48, 179,  72, 165, 181, 209, 215,  94, 146,  42, 172,  86, 170, 198,
				 79, 184,  56, 210, 150, 164, 125, 182, 118, 252, 107, 226, 156, 116,   4, 241,
				 69, 157, 112,  89, 100, 113, 135,  32, 134,  91, 207, 101, 230,  45, 168,   2,
				 27,  96,  37, 173, 174, 176, 185, 246,  28,  70,  97, 105,  52,  64, 126,  15,
				 85,  71, 163,  35, 221,  81, 175,  58, 195,  92, 249, 206, 186, 197, 234,  38,
				 44,  83,  13, 110, 133,  40, 132,   9, 211, 223, 205, 244,  65, 129,  77,  82,
				106, 220,  55, 200, 108, 193, 171, 250,  36, 225, 123,   8,  12, 189, 177,  74,
				120, 136, 149, 139, 227,  99, 232, 109, 233, 203, 213, 254,  59,   0,  29,  57,
				242, 239, 183,  14, 102,  88, 208, 228, 166, 119, 114, 248, 235, 117,  75,  10,
				 49,  68,  80, 180, 143, 237,  31,  26, 219, 153, 141,  51, 159,  17, 131,  20
			};
			for (int j = 0; j < 16; ++j)
			{
				m_checksum[j] ^= piSubst[m[j] ^ m_checksumL];
				m_checksumL = m_checksum[j];
			}

			// calculate hash
			for (int j = 0; j < 16; ++j)
			{
				m_x[j + 16] = m[j];
				m_x[j + 32] = m_x[j + 16] ^ m_x[j];
			}

			Byte t = 0;
			for (int j = 0; j < 18; ++j)
			{
				for (int k = 0; k < 48; ++k)
				{
					m_x[k] ^= piSubst[t];
					t = m_x[k];
				}
				t += j;
			}
		}
	}
}
	using MD2 = MD2_Hash::MD2;
}

#endif  // CHOCOBO1_MD2_H