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
#include <lib_common/of_openfec_api.h>
}

#include "../throughput_benchmark.hpp"


struct openfec_rs_encoder
{
    openfec_rs_encoder(uint32_t symbols, uint32_t symbol_size) :
        m_symbols(symbols), m_symbol_size(symbol_size)
    {
        k = m_symbols;
        m = m_symbols;
        m_block_size = m_symbols * m_symbol_size;
        m_payload_count = m_symbols;

        int i;
        int vector_count = k + m;

        // Resize data pointer vectors
        m_symbol_table.resize(vector_count);
        m_data.resize(vector_count);
        for (i = 0; i < vector_count; ++i)
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
        for (i = k; i < m; i++)
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
        for (int esi = k; esi < k + m; esi++)
        {
            if (of_build_repair_symbol(ses, (void**)&m_symbol_table[0], esi))
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

    // OpenFEC arguments
    //
    // orig_symb: table of all symbols (source+repair) in sequential order
    std::vector<char*> m_symbol_table;

    /// Storage for encoded symbols
    std::vector<std::vector<uint8_t>> m_data;
};


struct openfec_rs_decoder
{
    openfec_rs_decoder(uint32_t symbols, uint32_t symbol_size) :
        m_symbols(symbols), m_symbol_size(symbol_size)
    {
        k = m_symbols;
        m = 2 * m_symbols;

        m_block_size = m_symbols * m_symbol_size;
        m_decoding_result = -1;
    }

    ~openfec_rs_decoder()
    {
    }

    void decode_all(std::shared_ptr<openfec_rs_encoder> encoder)
    {
        uint32_t payload_count = encoder->payload_count();
        assert(payload_count == (uint32_t)m);
    }

    bool verify_data(std::shared_ptr<openfec_rs_encoder> encoder)
    {
        assert(m_block_size == encoder->block_size());

//         for (int i = 0; i < k; i++)
//         {
//             if (memcmp(m_buffs[i], encoder->m_buffs[src_err_list[i]],
//                 m_symbol_size))
//             {
//                 return false;
//             }
//         }

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
    types.push_back("encoder");
    //types.push_back("decoder");

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

BENCHMARK_F(openfec_rs_throughput, OpenFEC, ReedSolomon, 1)
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
