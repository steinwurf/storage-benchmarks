// Copyright Steinwurf ApS 2011-2012.
// Distributed under the "STEINWURF RESEARCH LICENSE 1.0".
// See accompanying file LICENSE.rst or
// http://www.steinwurf.com/licensing

#include <ctime>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>  // for memset, memcmp

#include <vector>

#include <gauge/gauge.hpp>

extern "C"
{
#include "erasure_code.h"
#include "test.h"
}

#include "../throughput_benchmark.hpp"

#define TEST_SOURCES 250
#define MMAX TEST_SOURCES
#define KMAX TEST_SOURCES

typedef unsigned char u8;

struct isa_encoder
{
    isa_encoder(uint32_t symbols, uint32_t symbol_size) :
        m_symbols(symbols), m_symbol_size(symbol_size)
    {
        k = m_symbols;
        m = 2 * m_symbols;
        m_block_size = m_symbols * m_symbol_size;
        m_payload_count = m_symbols;

        // Symbol size must be a multiple of 64
        assert(m_symbol_size % 64 == 0);

        // Allocate the arrays
        int i, j;
        void* buf = 0;
        for (i = 0; i < m; i++)
        {
            if (posix_memalign(&buf, 64, m_symbol_size))
            {
                printf("alloc error: Fail\n");
            }
            m_buffs[i] = (uint8_t*)buf;
        }

        // Make random data
        for(i = 0; i < k; i++)
            for(j = 0; j < (int)m_symbol_size; j++)
                m_buffs[i][j] = rand();
    }

    ~isa_encoder()
    {
        for (int i = 0; i < m; i++)
        {
            free(m_buffs[i]);
        }
    }

    void encode_all()
    {
        assert(m_payload_count == (uint32_t)(m-k));

        gf_gen_rs_matrix(a, m, k);

        // Make parity vects
        ec_init_tables(k, m - k, &a[k * k], g_tbls);
        ec_encode_data_sse(m_symbol_size,
            k, m - k, g_tbls, m_buffs, &m_buffs[k]);
    }

    uint32_t block_size() { return m_block_size; }
    uint32_t symbol_size() { return m_symbol_size; }
    uint32_t payload_size() { return m_symbol_size; }
    uint32_t payload_count() { return m_payload_count; }

protected:

    friend class isa_decoder;

    uint8_t* m_buffs[TEST_SOURCES];
    uint8_t a[MMAX*KMAX];
    uint8_t g_tbls[KMAX*TEST_SOURCES*32];

    // Code parameters
    int k, m;

    // Number of symbols
    uint32_t m_symbols;
    // Size of k+m symbols
    uint32_t m_symbol_size;
    // Size of a full generation (k symbols)
    uint32_t m_block_size;
    // Number of generated payloads
    uint32_t m_payload_count;
};


struct isa_decoder
{
    isa_decoder(uint32_t symbols, uint32_t symbol_size) :
        m_symbols(symbols), m_symbol_size(symbol_size)
    {
        k = m_symbols;
        m = 2 * m_symbols;

        m_block_size = m_symbols * m_symbol_size;
        m_decoding_result = -1;

        // Allocate the arrays
        int i;
        void* buf = 0;
        for (i = 0; i < m; i++)
        {
            if (posix_memalign(&buf, 64, m_symbol_size))
            {
                printf("alloc error: Fail\n");
            }
            m_buffs[i] = (uint8_t*)buf;
        }

        // Simulate m-k erasures (erase all original symbols)
        // No original symbols used during decoding (worst case)
        memset(src_in_err, 0, TEST_SOURCES);

        int errors;
        for (i = 0, errors = 0; i < k && errors < m - k; i++)
        {
            int err = 1; //rand() % 2;
            src_in_err[i] = err;
            if (err) src_err_list[errors++] = i;
        }
        nerrs = errors;
        assert(nerrs == m - k);

        gf_gen_rs_matrix(a, m, k);
    }

    ~isa_decoder()
    {
        for (int i = 0; i < m; i++)
        {
            free(m_buffs[i]);
        }
    }

    void decode_all(std::shared_ptr<isa_encoder> encoder)
    {
        uint32_t payload_count = encoder->payload_count();
        assert(payload_count == (uint32_t)(m - k));

        int i, j, r;
        // Construct b by removing error rows from a
        // a contains m rows and k columns
        for (i = 0, r = 0; i < k; i++, r++)
        {
            while (src_in_err[r]) r++;
            for (j = 0; j < k; j++)
                b[k * i + j] = a[k * r + j];
        }

        // Invert the b matrix into d
        if (gf_invert_matrix(b, d, k) < 0)
        {
            printf("BAD MATRIX\n");
            m_decoding_result = -1;
            return;
        }

        // Set data pointers to point to the encoder payloads
        for (i = 0, r = 0; i < k; i++, r++)
        {
            while (src_in_err[r]) r++;
            data[i] = encoder->m_buffs[r];
        }

        // Construct c by copying the erasure rows from the inverse matrix d
        for (i = 0; i < nerrs; i++)
        {
            for (j = 0; j < k; j++)
                c[k * i + j] = d[k * src_err_list[i] + j];
        }

        // Recover data
        ec_init_tables(k, nerrs, c, g_tbls);
        ec_encode_data_sse(m_symbol_size,
            k, nerrs, g_tbls, &data[0], &m_buffs[0]);
        m_decoding_result = 0;
    }

    bool verify_data(std::shared_ptr<isa_encoder> encoder)
    {
        assert(m_block_size == encoder->block_size());

        for (int i = 0; i < nerrs; i++)
        {
            if (memcmp(m_buffs[i], encoder->m_buffs[src_err_list[i]],
                m_symbol_size))
            {
                return false;
            }
        }

        return true;
    }

    bool is_complete() { return (m_decoding_result != -1); }

    uint32_t block_size() { return m_block_size; }
    uint32_t symbol_size() { return m_symbol_size; }
    uint32_t payload_size() { return m_symbol_size; }

protected:

    uint8_t* m_buffs[TEST_SOURCES];
    uint8_t a[MMAX*KMAX], b[MMAX*KMAX], c[MMAX*KMAX], d[MMAX*KMAX];
    uint8_t g_tbls[KMAX*TEST_SOURCES*32];
    uint8_t src_in_err[TEST_SOURCES];
    uint8_t src_err_list[TEST_SOURCES];
    uint8_t* data[TEST_SOURCES];

    // Code parameters
    int k, m;
    // Number of erasures
    int nerrs;

    // Number of symbols
    uint32_t m_symbols;
    // Size of k+m symbols
    uint32_t m_symbol_size;
    // Size of a full generation (k symbols)
    uint32_t m_block_size;

    int m_decoding_result;
};

BENCHMARK_OPTION(throughput_options)
{
    gauge::po::options_description options;

    std::vector<uint32_t> symbols;
    symbols.push_back(16);
    //     symbols.push_back(32);
    //     symbols.push_back(64);
    //     symbols.push_back(128);
    //     symbols.push_back(256);
    //     symbols.push_back(512);

    auto default_symbols =
        gauge::po::value<std::vector<uint32_t> >()->default_value(
        symbols, "")->multitoken();

    // Symbol size must be a multiple of 32
    std::vector<uint32_t> symbol_size;
    symbol_size.push_back(1000000);

    auto default_symbol_size =
        gauge::po::value<std::vector<uint32_t> >()->default_value(
        symbol_size, "")->multitoken();

    std::vector<std::string> types;
    //types.push_back("encoder");
    types.push_back("decoder");

    auto default_types =
        gauge::po::value<std::vector<std::string> >()->default_value(
        types, "")->multitoken();

    options.add_options()
        ("symbols", default_symbols, "Set the number of symbols");

    options.add_options()
        ("symbol_size", default_symbol_size, "Set the symbol size in bytes");

    options.add_options()
        ("type", default_types, "Set type [encoder|decoder]");

    gauge::runner::instance().register_options(options);
}

//------------------------------------------------------------------
// Reed-Solomon Vandermonde
//------------------------------------------------------------------

typedef throughput_benchmark<isa_encoder, isa_decoder>
    isa_throughput;

BENCHMARK_F(isa_throughput, ISA, ErasureCode, 10)
{
    run_benchmark();
}

int main(int argc, const char* argv[])
{
    srand(static_cast<uint32_t>(time(0)));

    gauge::runner::add_default_printers();
    gauge::runner::run_benchmarks(argc, argv);

    return 0;
}
