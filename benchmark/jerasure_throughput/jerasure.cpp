// Copyright Steinwurf ApS 2011-2012.
// Distributed under the "STEINWURF RESEARCH LICENSE 1.0".
// See accompanying file LICENSE.rst or
// http://www.steinwurf.com/licensing

#include <ctime>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <set>

#include <gauge/gauge.hpp>

extern "C"
{
#include <gf_rand.h>
#include <jerasure.h>
#include <reed_sol.h>
}

#include "../throughput_benchmark.hpp"

struct reed_sol_van_encoder
{
    reed_sol_van_encoder(
        uint32_t symbols, uint32_t symbol_size, uint32_t encoded_symbols) :
        m_symbols(symbols), m_symbol_size(symbol_size), matrix(0)
    {
        k = m_symbols;
        m = encoded_symbols;
        w = 8;
        m_block_size = m_symbols * m_symbol_size;

        // Resize data and coding pointer vectors
        data.resize(k);
        coding.resize(m);

        // Prepare the data to be encoded
        m_data_in.resize(m_block_size);

        for (uint8_t &e : m_data_in)
        {
            e = rand() % 256;
        }

        set_symbols(&m_data_in[0], m_data_in.size());

        // Prepare storage to the encoded payloads
        m_payload_count = encoded_symbols;

        m_payloads.resize(m_payload_count);
        for (uint32_t i = 0; i < m_payload_count; ++i)
        {
            m_payloads[i].resize(m_symbol_size);
        }

        matrix = reed_sol_vandermonde_coding_matrix(k, m, w);
    }

    ~reed_sol_van_encoder()
    {
        // matrix was allocated with malloc by jerasure: deallocate with free!
        if (matrix) { free(matrix); matrix = 0; }
    }

    void set_symbols(uint8_t* ptr, uint32_t size)
    {
        // Set pointers to point to the input symbols
        for (int i = 0; i < k; i++)
        {
            assert((i+1) * m_symbol_size <= size);
            data[i] = (char*)ptr + i * m_symbol_size;
        }
    }

    void encode_all()
    {
        assert(matrix != 0);
        assert(m_payload_count == (uint32_t)m);

        for (uint32_t i = 0; i < m_payload_count; ++i)
        {
            coding[i] = (char*)&(m_payloads[i][0]);
        }

        jerasure_matrix_encode(k, m, w, matrix, &data[0], &coding[0],
                               m_symbol_size);
    }

    uint32_t block_size() { return m_block_size; }
    uint32_t symbol_size() { return m_symbol_size; }
    uint32_t payload_size() { return m_symbol_size; }
    uint32_t payload_count() { return m_payload_count; }

protected:

    friend class reed_sol_van_decoder;

    /// The input data
    std::vector<uint8_t> m_data_in;

    /// Storage for encoded symbols
    std::vector<std::vector<uint8_t>> m_payloads;

    // Code parameters
    int k, m, w;

    // Number of symbols
    uint32_t m_symbols;
    // Size of k+m symbols
    uint32_t m_symbol_size;
    // Size of a full generation (k symbols)
    uint32_t m_block_size;
    // Number of generated payloads
    uint32_t m_payload_count;

    // Jerasure arguments
    std::vector<char*> data;
    std::vector<char*> coding;
    int* matrix;
};


struct reed_sol_van_decoder
{
    reed_sol_van_decoder(
        uint32_t symbols, uint32_t symbol_size, uint32_t encoded_symbols) :
        m_symbols(symbols), m_symbol_size(symbol_size), matrix(0)
    {
        k = m_symbols;
        m = encoded_symbols;
        w = 8;
        m_block_size = m_symbols * m_symbol_size;
        m_decoding_result = -1;
        uint32_t payload_count = encoded_symbols;

        // Resize data and coding pointer vectors
        data.resize(k);
        coding.resize(m);

        // Simulate m erasures (erase some original symbols)
        // The symbols will be restored by processing the encoded symbols

        while (erased.size() < payload_count)
        {
            uint32_t random_symbol = rand() % k;
            auto ret = erased.insert(random_symbol);
            // Skip this symbol if it was already included in the erased set
            if (ret.second==false) continue;
        }

        // Fill the erasure list
        erasures.resize(m + 1);
        int errors = 0;
        for (const uint32_t& e : erased)
        {
            erasures[errors++] = e;
        }
        // Terminate erasures vector with a -1 value
        erasures[m] = -1;

        m_data_out.resize(m_block_size);

        matrix = reed_sol_vandermonde_coding_matrix(k, m, w);
    }

    ~reed_sol_van_decoder()
    {
        // matrix was allocated with malloc by jerasure: deallocate with free!
        if (matrix) { free(matrix); matrix = 0; }
    }

    uint32_t decode_all(std::shared_ptr<reed_sol_van_encoder> encoder)
    {
        assert(matrix != 0);
        uint32_t payload_count = encoder->m_payloads.size();
        uint32_t data_size = m_data_out.size();
        assert(payload_count == (uint32_t)m);

        // Set data pointers to point to the output symbols
        for (int i = 0; i < k; i++)
        {
            assert((i+1) * m_symbol_size <= data_size);
            // Use original symbols that were not erased
            if (erased.count(i) == 0)
                data[i] = encoder->data[i];
            else
                data[i] = (char*)&m_data_out[i * m_symbol_size];
        }

        for (uint32_t i = 0; i < payload_count; ++i)
        {
            coding[i] = (char*)&(encoder->m_payloads[i][0]);
        }

        m_decoding_result =
            jerasure_matrix_decode(k, m, w, matrix, 1, &erasures[0], &data[0],
                                   &coding[0], m_symbol_size);

        return payload_count;
    }

    bool verify_data(std::shared_ptr<reed_sol_van_encoder> encoder)
    {
        assert(m_data_out.size() == encoder->m_data_in.size());

        // We only verify the erased symbols
        for (const uint32_t& e : erased)
        {
            if (memcmp(data[e], encoder->data[e], m_symbol_size))
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

    /// The output data
    std::vector<uint8_t> m_data_out;

    // Code parameters
    int k, m, w;

    // Number of symbols
    uint32_t m_symbols;
    // Size of k+m symbols
    uint32_t m_symbol_size;
    // Size of a full generation (k symbols)
    uint32_t m_block_size;

    // Jerasure arguments
    std::vector<char*> data;
    std::vector<char*> coding;
    int* matrix;
    std::set<uint32_t> erased;
    std::vector<int> erasures;
    int m_decoding_result;
};

BENCHMARK_OPTION(throughput_options)
{
    gauge::po::options_description options;

    std::vector<uint32_t> symbols;
    symbols.push_back(8);
    symbols.push_back(16);
    symbols.push_back(32);
    symbols.push_back(64);
    symbols.push_back(128);

    auto default_symbols =
        gauge::po::value<std::vector<uint32_t> >()->default_value(
            symbols, "")->multitoken();

    std::vector<double> loss_rate;
    loss_rate.push_back(0.05);
    loss_rate.push_back(0.1);
    loss_rate.push_back(0.15);
    loss_rate.push_back(0.2);
    loss_rate.push_back(0.25);
    loss_rate.push_back(0.3);

    auto default_loss_rate =
        gauge::po::value<std::vector<double>>()->default_value(
            loss_rate, "")->multitoken();

    // Symbol size must be a multiple of 32
    std::vector<uint32_t> symbol_size;
    symbol_size.push_back(32000);
    symbol_size.push_back(64000);
    symbol_size.push_back(128000);
    symbol_size.push_back(256000);
    symbol_size.push_back(512000);
    symbol_size.push_back(1024000);

    auto default_symbol_size =
        gauge::po::value<std::vector<uint32_t> >()->default_value(
            symbol_size, "")->multitoken();

    std::vector<std::string> types;
    types.push_back("encoder");
    types.push_back("decoder");

    auto default_types =
        gauge::po::value<std::vector<std::string> >()->default_value(
            types, "")->multitoken();

    options.add_options()
        ("symbols", default_symbols, "Set the number of symbols");

    options.add_options()
        ("loss_rate", default_loss_rate, "Set the ratio of repair symbols");

    options.add_options()
        ("symbol_size", default_symbol_size, "Set the symbol size in bytes");

    options.add_options()
        ("type", default_types, "Set type [encoder|decoder]");

    gauge::runner::instance().register_options(options);
}

//------------------------------------------------------------------
// Reed-Solomon Vandermonde
//------------------------------------------------------------------

typedef throughput_benchmark<reed_sol_van_encoder, reed_sol_van_decoder>
    reed_sol_van_throughput;

BENCHMARK_F(reed_sol_van_throughput, Jerasure, ReedSolVan, 1)
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
