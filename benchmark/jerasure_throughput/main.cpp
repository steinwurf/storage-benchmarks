// Copyright Steinwurf ApS 2011-2012.
// Distributed under the "STEINWURF RESEARCH LICENSE 1.0".
// See accompanying file LICENSE.rst or
// http://www.steinwurf.com/licensing

#include <ctime>
#include <cstdint>

//#include <boost/make_shared.hpp>

#include <gauge/gauge.hpp>
#include <gauge/console_printer.hpp>
#include <gauge/python_printer.hpp>
#include <gauge/csv_printer.hpp>
#include <gauge/json_printer.hpp>

#include <tables/table.hpp>

extern "C"
{
#include <gf_rand.h>
#include <jerasure.h>
#include <reed_sol.h>
}

struct reed_sol_van_encoder
{
    reed_sol_van_encoder(uint32_t symbols, uint32_t symbol_size) :
        m_symbols(symbols), m_symbol_size(symbol_size)
    {
        k = m_symbols;
        m = m_symbols;
        w = 8;
        m_block_size = m_symbols * m_symbol_size;

        matrix = reed_sol_vandermonde_coding_matrix(k, m, w);

        // Allocate data and coding pointer arrays
        data = new char*[k];
        coding = new char*[m];
    }

    ~reed_sol_van_encoder()
    {
        if (data) delete[] data;
        if (coding) delete[] coding;
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

    void encode_all(std::vector<std::vector<uint8_t>>& payloads)
    {
        uint32_t payload_count = payloads.size();
        assert(payload_count == (uint32_t)m);

        for (uint32_t i = 0; i < payload_count; ++i)
        {
            coding[i] = (char*)&(payloads[i][0]);
        }

        jerasure_matrix_encode(k, m, w, matrix, data, coding, m_symbol_size);
    }

    uint32_t block_size() { return m_block_size; }
    uint32_t symbol_size() { return m_symbol_size; }
    uint32_t payload_size() { return m_symbol_size; }

protected:
    // Code parameters
    int k, m, w;

    // Number of symbols
    uint32_t m_symbols;
    // Size of k+m symbols
    uint32_t m_symbol_size;
    // Size of a full generation (k symbols)
    uint32_t m_block_size;

    /* Jerasure Arguments */
    char** data;
    char** coding;
    int* matrix;
};


struct reed_sol_van_decoder
{
    reed_sol_van_decoder(uint32_t symbols, uint32_t symbol_size) :
        m_symbols(symbols), m_symbol_size(symbol_size)
    {
        k = m_symbols;
        m = m_symbols;
        w = 8;
        m_block_size = m_symbols * m_symbol_size;

//         matrix = reed_sol_vandermonde_coding_matrix(k, m, w);
//
//         // Allocate data and coding pointer arrays
//         data = new char*[k];
//         coding = new char*[m];
    }

    uint32_t block_size() { return m_block_size; }
    uint32_t symbol_size() { return m_symbol_size; }
    uint32_t payload_size() { return m_symbol_size; }

protected:
    // Code parameters
    int k, m, w;

    // Number of symbols
    uint32_t m_symbols;
    // Size of k+m symbols
    uint32_t m_symbol_size;
    // Size of a full generation (k symbols)
    uint32_t m_block_size;

    /* Jerasure Arguments */
    char** data;
    char** coding;
    int* matrix;
};

/// A test block represents an encoder and decoder pair
template<class Encoder, class Decoder>
struct throughput_benchmark : public gauge::time_benchmark
{
    void init()
    {
        m_factor = 2;
        gauge::time_benchmark::init();
    }

    void start()
    {
        m_encoded_symbols = 0;
        m_decoded_symbols = 0;
        gauge::time_benchmark::start();
    }

    void stop()
    {
        gauge::time_benchmark::stop();
    }

    double measurement()
    {
        // Get the time spent per iteration
        double time = gauge::time_benchmark::measurement();

        gauge::config_set cs = get_current_configuration();
        std::string type = cs.get_value<std::string>("type");
        uint32_t symbol_size = cs.get_value<uint32_t>("symbol_size");

        // The number of bytes {en|de}coded
        uint64_t total_bytes = 0;

        if(type == "decoder")
        {
            total_bytes = m_decoded_symbols * symbol_size;
        }
        else if(type == "encoder")
        {
            total_bytes = m_encoded_symbols * symbol_size;
        }
        else
        {
            assert(0);
        }

        // The bytes per iteration
        uint64_t bytes =
            total_bytes / gauge::time_benchmark::iteration_count();

        return bytes / time; // MB/s for each iteration
    }

    void store_run(tables::table& results)
    {
        if(!results.has_column("throughput"))
            results.add_column("throughput");

        results.set_value("throughput", measurement());
    }

    bool accept_measurement()
    {
        gauge::config_set cs = get_current_configuration();

        std::string type = cs.get_value<std::string>("type");

//         if(type == "decoder")
//         {
//             // If we are benchmarking a decoder we only accept
//             // the measurement if the decoding was successful
//             if(!m_decoder->is_complete())
//             {
//                 // We did not generate enough payloads to decode successfully,
//                 // so we will generate more payloads for next run
//                 m_factor++;
//
//                 return false;
//             }
//         }

        return gauge::time_benchmark::accept_measurement();
    }

    std::string unit_text() const
    {
        return "MB/s";
    }

    void get_options(gauge::po::variables_map& options)
    {
        auto symbols = options["symbols"].as<std::vector<uint32_t> >();
        auto symbol_size = options["symbol_size"].as<std::vector<uint32_t> >();
        auto types = options["type"].as<std::vector<std::string> >();

        assert(symbols.size() > 0);
        assert(symbol_size.size() > 0);
        assert(types.size() > 0);

        for (uint32_t i = 0; i < symbols.size(); ++i)
        {
            for (uint32_t j = 0; j < symbol_size.size(); ++j)
            {
                // Symbol size must be a multiple of 32
                assert(symbol_size[j] % 32 == 0);
                for (uint32_t u = 0; u < types.size(); ++u)
                {
                    gauge::config_set cs;
                    cs.set_value<uint32_t>("symbols", symbols[i]);
                    cs.set_value<uint32_t>("symbol_size", symbol_size[j]);

                    cs.set_value<std::string>("type", types[u]);

                    add_configuration(cs);
                }
            }
        }
    }

    void setup()
    {
        gauge::config_set cs = get_current_configuration();

        uint32_t symbols = cs.get_value<uint32_t>("symbols");
        uint32_t symbol_size = cs.get_value<uint32_t>("symbol_size");

        m_encoder = std::make_shared<Encoder>(symbols, symbol_size);
        m_decoder = std::make_shared<Decoder>(symbols, symbol_size);

        // Prepare the data to be encoded
        m_encoded_data.resize(m_encoder->block_size());

        for (uint8_t &e : m_encoded_data)
        {
            e = rand() % 256;
        }

        m_encoder->set_symbols(&m_encoded_data[0], m_encoded_data.size());

        // Prepare storage to the encoded payloads
        uint32_t payload_count = symbols * m_factor;

        m_payloads.resize(payload_count);
        for (uint32_t i = 0; i < payload_count; ++i)
        {
            m_payloads[i].resize(m_encoder->payload_size());
        }

        m_temp_payload.resize(m_encoder->payload_size());
    }

    void encode_payloads()
    {
        m_encoder->set_symbols(&m_encoded_data[0], m_encoded_data.size());

        m_encoder->encode_all(m_payloads);
        m_encoded_symbols += m_payloads.size();
    }

    void decode_payloads()
    {
        uint32_t payload_count = m_payloads.size();

        for (uint32_t i = 0; i < payload_count; ++i)
        {
            std::copy(m_payloads[i].begin(),
                      m_payloads[i].end(),
                      m_temp_payload.begin());

//             m_decoder->decode(&m_temp_payload[0]);
//
//             ++m_decoded_symbols;
//
//             if(m_decoder->is_complete())
//             {
//                 return;
//             }
        }
    }

    /// Run the encoder
    void run_encode()
    {
        // The clock is running
        RUN
        {
            encode_payloads();
        }
    }

    /// Run the decoder
    void run_decode()
    {
        // Encode some data
        encode_payloads();

        // The clock is running
        RUN
        {
            // We have to make sure the decoder is in a "clean" state
            // i.e. no symbols already decoded.
            //m_decoder->initialize(*m_decoder_factory);

            // Decode the payloads
            decode_payloads();
        }
    }


    void run_benchmark()
    {
        gauge::config_set cs = get_current_configuration();

        std::string type = cs.get_value<std::string>("type");

        if (type == "encoder")
        {
            run_encode();
        }
        else if (type == "decoder")
        {
            run_decode();
        }
        else
        {
            assert(0);
        }
    }

protected:

    /// The encoder
    std::shared_ptr<Encoder> m_encoder;

    /// The decoder
    std::shared_ptr<Decoder> m_decoder;

    /// The number of symbols encoded
    uint32_t m_encoded_symbols;

    /// The number of symbols decoded
    uint32_t m_decoded_symbols;

    /// The data encoded
    std::vector<uint8_t> m_encoded_data;

    /// Temporary payload to not destroy the already encoded payloads
    /// when decoding
    std::vector<uint8_t> m_temp_payload;

    /// Storage for encoded symbols
    std::vector< std::vector<uint8_t> > m_payloads;

    /// Multiplication factor for payload_count
    uint32_t m_factor;
};


/// Using this macro we may specify options. For specifying options
/// we use the boost program options library. So you may additional
/// details on how to do it in the manual for that library.
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
// Reed-Solomon Vandermonde
//------------------------------------------------------------------

typedef throughput_benchmark<reed_sol_van_encoder, reed_sol_van_decoder>
    reed_sol_van_throughput;

BENCHMARK_F(reed_sol_van_throughput, Jerasure, ReedSolVan, 5)
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
