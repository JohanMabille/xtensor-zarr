/***************************************************************************
* Copyright (c) Wolf Vollprecht, Sylvain Corlay and Johan Mabille          *
* Copyright (c) QuantStack                                                 *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef XTENSOR_ZARR_CHUNKED_ARRAY_HPP
#define XTENSOR_ZARR_CHUNKED_ARRAY_HPP

#define HALF_ENABLE_F16C_INTRINSICS 0
#define HALF_ENABLE_CPP11_LONG_LONG 1
#define HALF_ENABLE_CPP11_STATIC_ASSERT 1
#define HALF_ENABLE_CPP11_CONSTEXPR 1
#define HALF_ENABLE_CPP11_NOEXCEPT 1
#define HALF_ENABLE_CPP11_USER_LITERALS 1
#define HALF_ENABLE_CPP11_THREAD_LOCAL 1
#define HALF_ENABLE_CPP11_TYPE_TRAITS 1
#define HALF_ENABLE_CPP11_CSTDINT 1
#define HALF_ENABLE_CPP11_CMATH 1
#define HALF_ENABLE_CPP11_CFENV 1
#define HALF_ENABLE_CPP11_HASH 1
#include "half_float/half.hpp"
#include "xtensor/xchunked_array.hpp"
#include "xtensor/xchunk_store_manager.hpp"
#include "xtensor/xfile_array.hpp"
#include "xtensor/zarray.hpp"
#include "xtensor-io/xio_binary.hpp"
#include "xtensor-io/xgzip.hpp"
#include "xzarr_common.hpp"

namespace xt
{
    template <class T, class io_handler>
    zarray get_zarray(std::vector<std::size_t>& shape, std::vector<std::size_t>& chunk_shape, const std::string& path, char separator, const nlohmann::json& attrs)
    {
        xchunked_array<xchunk_store_manager<xfile_array<T, io_handler>, xzarr_index_path>, xzarr_attrs> a(shape, chunk_shape);
        a.chunks().set_directory(path.c_str());
        a.chunks().get_index_path().set_separator(separator);
        a.set_attrs(attrs);
        return zarray(a);
    }

    template <class store_type, class T>
    zarray build_chunked_array_impl(const std::string& compressor, std::vector<std::size_t>& shape, std::vector<std::size_t>& chunk_shape, const std::string& path, char separator, const nlohmann::json& attrs)
    {
        if (compressor == "binary")
        {
            using io_handler = typename store_type::template io_handler<xio_binary_config>;
            return get_zarray<T, io_handler>(shape, chunk_shape, path, separator, attrs);
        }
        else if (compressor == "gzip")
        {
            using io_handler = typename store_type::template io_handler<xgzip_config>;
            return get_zarray<T, io_handler>(shape, chunk_shape, path, separator, attrs);
        }
        else
        {
            XTENSOR_THROW(std::runtime_error, "Unknown compressor: " + compressor);
        }
    }

    template <class store_type>
    class xchunked_array_factory
    {
    public:

        static zarray build(const std::string& compressor, const std::string& dtype, std::vector<std::size_t>& shape, std::vector<std::size_t>& chunk_shape, const std::string& path, char separator, const nlohmann::json& attrs)
        {
            auto fun = instance().m_builders.find(dtype);
            if (fun != instance().m_builders.end())
            {
                return (fun->second)(compressor, shape, chunk_shape, path, separator, attrs);
            }
            else
            {
                XTENSOR_THROW(std::runtime_error, "Unkown data type: " + dtype);
            }
        }

    private:

        using self_type = xchunked_array_factory;

        static self_type& instance()
        {
            static self_type instance;
            return instance;
        }

        xchunked_array_factory()
        {
            m_builders.insert(std::make_pair("bool", &build_chunked_array_impl<store_type, bool>));
            m_builders.insert(std::make_pair("i1", &build_chunked_array_impl<store_type, int8_t>));
            m_builders.insert(std::make_pair("u1", &build_chunked_array_impl<store_type, uint8_t>));

            // TODO: maybe get rid of endianness here and deal with it in storage?

            // little-endian
            // signed integer
            m_builders.insert(std::make_pair("<i2", &build_chunked_array_impl<store_type, int16_t>));
            m_builders.insert(std::make_pair("<i4", &build_chunked_array_impl<store_type, int32_t>));
            m_builders.insert(std::make_pair("<i8", &build_chunked_array_impl<store_type, int64_t>));
            // unsigned integer
            m_builders.insert(std::make_pair("<u2", &build_chunked_array_impl<store_type, uint16_t>));
            m_builders.insert(std::make_pair("<u4", &build_chunked_array_impl<store_type, uint32_t>));
            m_builders.insert(std::make_pair("<u8", &build_chunked_array_impl<store_type, uint64_t>));
            // float
            m_builders.insert(std::make_pair("<f2", &build_chunked_array_impl<store_type, half_float::half>));
            m_builders.insert(std::make_pair("<f4", &build_chunked_array_impl<store_type, float>));
            m_builders.insert(std::make_pair("<f8", &build_chunked_array_impl<store_type, double>));

            // big-endian
            // signed integer
            m_builders.insert(std::make_pair(">i2", &build_chunked_array_impl<store_type, int16_t>));
            m_builders.insert(std::make_pair(">i4", &build_chunked_array_impl<store_type, int32_t>));
            m_builders.insert(std::make_pair(">i8", &build_chunked_array_impl<store_type, int64_t>));
            // unsigned integer
            m_builders.insert(std::make_pair(">u2", &build_chunked_array_impl<store_type, uint16_t>));
            m_builders.insert(std::make_pair(">u4", &build_chunked_array_impl<store_type, uint32_t>));
            m_builders.insert(std::make_pair(">u8", &build_chunked_array_impl<store_type, uint64_t>));
            // float
            m_builders.insert(std::make_pair(">f2", &build_chunked_array_impl<store_type, half_float::half>));
            m_builders.insert(std::make_pair(">f4", &build_chunked_array_impl<store_type, float>));
            m_builders.insert(std::make_pair(">f8", &build_chunked_array_impl<store_type, double>));
        }

        std::map<std::string, zarray (*)(const std::string& compressor, std::vector<std::size_t>& shape, std::vector<std::size_t>& chunk_shape, const std::string& path, char separator, const nlohmann::json& attrs)> m_builders;
    };
}

#endif
