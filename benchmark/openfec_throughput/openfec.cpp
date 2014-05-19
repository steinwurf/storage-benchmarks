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
#include <set>

#include <gauge/gauge.hpp>

extern "C"
{
#include <lib_common/of_openfec_api.h>
}

#include "../throughput_benchmark.hpp"


struct openfec_rs_encoder
{
    openfec_rs_encoder(uint32_t symbols, uint32_t symbol_size) :
        m_symbols(symbols), m_symbol_size(symbol_size)
    {
        k = m_symbols;
        m = m_symbols / 2;
        m_block_size = m_symbols * m_symbol_size;
        m_payload_count = m;

        int i;
        int vector_count = k + m;

        // Resize data vectors
        m_symbol_table.resize(vector_count);
        m_data.resize(vector_count);
        for (i = 0; i < vector_count; i++)
        {
            m_data[i].resize(m_symbol_size);
        }

        // Set pointers to point to the input symbols
        for (i = 0; i < k; i++)
        {
            // Fill source symbols with random data
            for (uint8_t &e : m_data[i])
            {
                e = rand() % 256;
            }
            m_symbol_table[i] = (char*)&(m_data[i][0]);
        }

        // Set pointers to point to the repair symbol buffers
        for (i = k; i < k + m; i++)
        {
            m_symbol_table[i] = (char*)&(m_data[i][0]);
        }
    }

    ~openfec_rs_encoder()
    {
    }

    void encode_all()
    {
        assert(m_payload_count == (uint32_t)m);

        of_session_t* ses;
        of_codec_id_t codec_id = OF_CODEC_REED_SOLOMON_GF_2_8_STABLE;
        of_codec_type_t codec_type = OF_ENCODER;

        // Create the codec instance and initialize it accordingly
        if (of_create_codec_instance(&ses, codec_id, codec_type,
            of_verbosity))
        {
            printf("of_create_codec_instance() failed\n");
        }

        of_rs_parameters_t params;
        params.nb_source_symbols = k;
        params.nb_repair_symbols = m;
        params.encoding_symbol_length = m_symbol_size;
        if (of_set_fec_parameters(ses, (of_parameters_t*)&params))
        {
            printf("of_set_fec_parameters() failed\n");
        }

        // Generate repair symbols
        for (int i = k; i < k + m; i++)
        {
            if (of_build_repair_symbol(ses, (void**)&m_symbol_table[0], i))
            {
                printf("of_build_repair_symbol() failed\n");
            }
        }

        // Release the FEC codec instance.
        if (of_release_codec_instance(ses))
        {
            printf("of_release_codec_instance() failed\n");
        }
    }

    uint32_t block_size() { return m_block_size; }
    uint32_t symbol_size() { return m_symbol_size; }
    uint32_t payload_size() { return m_symbol_size; }
    uint32_t payload_count() { return m_payload_count; }

protected:

    friend class openfec_rs_decoder;

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

    // Table of all symbols (source+repair) in sequential order
    std::vector<char*> m_symbol_table;

    // Storage for source and repair symbols
    std::vector<std::vector<uint8_t>> m_data;
};


struct openfec_rs_decoder
{
    openfec_rs_decoder(uint32_t symbols, uint32_t symbol_size) :
        m_symbols(symbols), m_symbol_size(symbol_size)
    {
        k = m_symbols;
        m = m_symbols / 2;

        m_block_size = m_symbols * m_symbol_size;
        m_decoding_result = -1;

        // Resize data vector to hold original symbols
        m_data.resize(m_symbols);
        for (uint32_t i = 0; i < m_symbols; i++)
        {
            m_data[i].resize(m_symbol_size);
        }

        // Simulate m erasures (erase some original symbols)
        // The symbols will be restored by processing the encoded symbols
        while (m_erased.size() < (uint32_t)m)
        {
            uint8_t random_symbol = rand() % k;
            auto ret = m_erased.insert(random_symbol);
            // Skip this symbol if it was already included in the erased set
            if (ret.second==false) continue;
        }
    }

    ~openfec_rs_decoder()
    {
    }

    static void* allocate_source_symbol(void* context, uint32_t size,
        uint32_t esi)
    {
        openfec_rs_decoder* self = (openfec_rs_decoder*)context;
        assert(size == self->m_symbol_size);
        return (void*)&(self->m_data[esi][0]);
    }

    void decode_all(std::shared_ptr<openfec_rs_encoder> encoder)
    {
        int payload_count = (int)encoder->payload_count();
        assert(payload_count == m);

        of_session_t* ses;
        of_codec_id_t codec_id = OF_CODEC_REED_SOLOMON_GF_2_8_STABLE;
        of_codec_type_t codec_type = OF_DECODER;

        // Create the codec instance and initialize it accordingly
        if (of_create_codec_instance(&ses, codec_id, codec_type,
            of_verbosity))
        {
            printf("of_create_codec_instance() failed\n");
        }

        of_rs_parameters_t params;
        params.nb_source_symbols = k;
        params.nb_repair_symbols = m;
        params.encoding_symbol_length = m_symbol_size;
        if (of_set_fec_parameters(ses, (of_parameters_t*)&params))
        {
            printf("of_set_fec_parameters() failed\n");
        }

        // The decoder uses pre-allocated data buffers to avoid unnecessary
        // copying after decoding
        of_set_callback_functions(ses,
            allocate_source_symbol, NULL, (void*)this);

        // Process original and repair symbols
        for (int i = 0; i < k + m; i++)
        {
            // Skip the erased original symbols
            if (m_erased.count(i)) continue;
            if (of_decode_with_new_symbol(ses, &encoder->m_data[i][0], i) ==
                OF_STATUS_ERROR)
            {
                printf("of_decode_with_new_symbol() failed\n");
            }
        }

        if (of_is_decoding_complete(ses) == true)
            m_decoding_result = 0;

        // Release the FEC codec instance.
        if (of_release_codec_instance(ses))
        {
            printf("of_release_codec_instance() failed\n");
        }
    }

    bool verify_data(std::shared_ptr<openfec_rs_encoder> encoder)
    {
        assert(m_block_size == encoder->block_size());

        // We only verify the erased symbols
        for (const uint8_t& e : erased)
        {
            if (memcmp(&m_data[e][0], &(encoder->m_data[e][0]), m_symbol_size))
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

    // Code parameters
    int k, m;

    // Number of symbols
    uint32_t m_symbols;
    // Size of k+m symbols
    uint32_t m_symbol_size;
    // Size of a full generation (k symbols)
    uint32_t m_block_size;
    // Set of erased symbols
    std::set<uint8_t> m_erased;

    int m_decoding_result;

    // Storage for source symbols
    std::vector<std::vector<uint8_t>> m_data;
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
    types.push_back("encoder");
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
// OpenFEC Reed-Solomon codec
//------------------------------------------------------------------

typedef throughput_benchmark<openfec_rs_encoder, openfec_rs_decoder>
    openfec_rs_throughput;

BENCHMARK_F(openfec_rs_throughput, OpenFEC, ReedSolomon, 10)
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
