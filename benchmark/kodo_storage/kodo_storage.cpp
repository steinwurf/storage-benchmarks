// Copyright Steinwurf ApS 2011-2012.
// Distributed under the "STEINWURF RESEARCH LICENSE 1.0".
// See accompanying file LICENSE.rst or
// http://www.steinwurf.com/licensing

#include <ctime>
#include <cstdint>
#include <set>
#include <string>
#include <algorithm>

#include <boost/make_shared.hpp>

#include <gauge/gauge.hpp>
#include <gauge/console_printer.hpp>
#include <gauge/python_printer.hpp>
#include <gauge/csv_printer.hpp>
#include <gauge/json_printer.hpp>

#include <kodo/has_systematic_encoder.hpp>
#include <kodo/set_systematic_off.hpp>
#include <kodo/rlnc/full_vector_codes.hpp>
#include <kodo/rlnc/perpetual_codes.hpp>

#include <tables/table.hpp>

template<class Encoder, class Decoder, bool Relaxed = false>
struct storage_benchmark : public gauge::time_benchmark
{
    typedef typename Encoder::factory encoder_factory;
    typedef typename Encoder::factory::pointer encoder_ptr;

    typedef typename Decoder::factory decoder_factory;
    typedef typename Decoder::factory::pointer decoder_ptr;

    void init()
    {
        m_factor = 1;
        gauge::time_benchmark::init();
    }

    void start()
    {
        m_processed_symbols = 0;
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
        uint32_t erased_symbols = cs.get_value<uint32_t>("erased_symbols");

        // The number of bytes {en|de}coded
        uint64_t total_bytes = 0;

        if (type == "decoder")
        {
            total_bytes = erased_symbols * symbol_size;
        }
        else if (type == "encoder")
        {
            uint32_t payload_count = (uint32_t)m_payloads.size();
            total_bytes = payload_count * symbol_size;
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
        if (!results.has_column("goodput"))
            results.add_column("goodput");

        results.set_value("goodput", measurement());

        if (Relaxed)
        {
            gauge::config_set cs = get_current_configuration();
            std::string type = cs.get_value<std::string>("type");

            if (type == "decoder")
            {
                if (!results.has_column("extra_symbols"))
                    results.add_column("extra_symbols");

                uint32_t erased_symbols =
                    cs.get_value<uint32_t>("erased_symbols");
                uint32_t extra_symbols = m_processed_symbols - erased_symbols;
                results.set_value("extra_symbols", extra_symbols);
            }
        }
    }

    bool accept_measurement()
    {
        gauge::config_set cs = get_current_configuration();

        std::string type = cs.get_value<std::string>("type");

        if (type == "decoder")
        {
            // If we are benchmarking a decoder we only accept
            // the measurement if the decoding was successful
            if (!m_decoder->is_complete())
            {
                // We did not generate enough payloads to decode successfully,
                // so we will generate more payloads for next run
                if (Relaxed == true)
                    ++m_factor;

                return false;
            }

            // At this point, the output data should be equal to the input data
            assert(m_data_out == m_data_in);
        }

        // Force only one iteration
        return true;
    }

    std::string unit_text() const
    {
        return "MB/s";
    }

    void get_options(gauge::po::variables_map& options)
    {
        auto symbols = options["symbols"].as<std::vector<uint32_t>>();
        auto loss_rate = options["loss_rate"].as<std::vector<double>>();
        auto symbol_size = options["symbol_size"].as<std::vector<uint32_t>>();
        auto types = options["type"].as<std::vector<std::string>>();

        assert(symbols.size() > 0);
        assert(loss_rate.size() > 0);
        assert(symbol_size.size() > 0);
        assert(types.size() > 0);

        for (const auto& s : symbols)
        {
            for (const auto& p : symbol_size)
            {
                for (const auto& r : loss_rate)
                {
                    for (const auto& t : types)
                    {
                        gauge::config_set cs;
                        cs.set_value<uint32_t>("symbols", s);
                        cs.set_value<uint32_t>("symbol_size", p);
                        cs.set_value<double>("loss_rate", r);
                        cs.set_value<std::string>("type", t);

                        uint32_t erased = (uint32_t)std::ceil(s * r);
                        cs.set_value<uint32_t>("erased_symbols", erased);

                        add_configuration(cs);
                    }
                }
            }
        }
    }

    void setup()
    {
        gauge::config_set cs = get_current_configuration();

        uint32_t symbols = cs.get_value<uint32_t>("symbols");
        uint32_t symbol_size = cs.get_value<uint32_t>("symbol_size");
        uint32_t erased_symbols = cs.get_value<uint32_t>("erased_symbols");

        // Make the factories fit perfectly otherwise there seems to
        // be problems with memory access i.e. when using a factory
        // with max symbols 1024 with a symbols 16
        m_decoder_factory = std::make_shared<decoder_factory>(
            symbols, symbol_size);

        m_encoder_factory = std::make_shared<encoder_factory>(
            symbols, symbol_size);

        m_decoder_factory->set_symbols(symbols);
        m_decoder_factory->set_symbol_size(symbol_size);

        m_encoder_factory->set_symbols(symbols);
        m_encoder_factory->set_symbol_size(symbol_size);

        m_encoder = m_encoder_factory->build();
        m_decoder = m_decoder_factory->build();

        // Prepare the data buffers
        m_data_in.resize(m_encoder->block_size());
        m_data_out.resize(m_encoder->block_size());
        std::fill_n(m_data_out.begin(), m_data_out.size(), 0);

        for (uint8_t &e : m_data_in)
        {
            e = rand() % 256;
        }

        m_encoder->set_symbols(sak::storage(m_data_in));

        m_decoder->set_symbols(sak::storage(m_data_out));

        // Prepare storage for the encoded payloads
        uint32_t payload_count = erased_symbols * m_factor;
        assert(payload_count > 0);

        m_payloads.resize(payload_count);
        for (uint32_t i = 0; i < payload_count; ++i)
        {
            m_payloads[i].resize(m_encoder->payload_size());
        }
    }

    void encode_payloads()
    {
        m_encoder->set_symbols(sak::storage(m_data_in));

        // We switch any systematic operations off, because we are only
        // interested in producing coded symbols
        if (kodo::has_systematic_encoder<Encoder>::value)
            kodo::set_systematic_off(m_encoder);

        uint32_t payload_count = (uint32_t)m_payloads.size();

        for (uint32_t i = 0; i < payload_count; ++i)
        {
            std::vector<uint8_t> &payload = m_payloads[i];
            m_encoder->encode(&payload[0]);
        }
    }

    void decode_payloads()
    {
        uint32_t payload_count = (uint32_t)m_payloads.size();

        for (uint32_t i = 0; i < payload_count; ++i)
        {
            m_decoder->decode(&m_payloads[i][0]);

            m_processed_symbols++;

            if (m_decoder->is_complete())
            {
                return;
            }
        }
    }

    /// Run the encoder
    void run_encode()
    {
        gauge::config_set cs = get_current_configuration();

        uint32_t symbols = cs.get_value<uint32_t>("symbols");
        uint32_t symbol_size = cs.get_value<uint32_t>("symbol_size");

        m_encoder_factory->set_symbols(symbols);
        m_encoder_factory->set_symbol_size(symbol_size);

        // The clock is running
        RUN
        {
            // We have to make sure the encoder is in a "clean" state
            m_encoder->initialize(*m_encoder_factory);

            encode_payloads();
        }
    }

    /// Run the decoder
    void run_decode()
    {
        // Encode some data
        encode_payloads();

        gauge::config_set cs = get_current_configuration();
        uint32_t erased_symbols = cs.get_value<uint32_t>("erased_symbols");
        uint32_t symbols = cs.get_value<uint32_t>("symbols");
        uint32_t symbol_size = cs.get_value<uint32_t>("symbol_size");

        // Prepare the data buffer for the decoder
        std::copy(m_data_in.begin(), m_data_in.end(), m_data_out.begin());
        // Randomly delete original symbols that will be restored by processing
        // the encoded symbols
        std::set<uint32_t> erased;
        while (erased.size() < erased_symbols)
        {
            uint32_t random_symbol = rand() % symbols;
            auto ret = erased.insert(random_symbol);
            // Skip this symbol if it was already included in the erased set
            if (ret.second == false) continue;
            // Zero the symbol
            std::fill_n(m_data_out.begin() + random_symbol * symbol_size,
                        symbol_size, 0);
        }

        m_decoder_factory->set_symbols(symbols);
        m_decoder_factory->set_symbol_size(symbol_size);

        // The clock is running
        RUN
        {
            // We have to make sure the decoder is in a "clean" state
            // i.e. no symbols already decoded.
            m_decoder->initialize(*m_decoder_factory);

            m_decoder->set_symbols(sak::storage(m_data_out));

            // Set the existing original symbols
            for (uint32_t i = 0; i < symbols; ++i)
            {
                // Skip the erased symbols
                if (erased.count(i) == 0)
                    m_decoder->decode_symbol(&m_data_out[i * symbol_size], i);
            }

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

    /// The decoder factory
    std::shared_ptr<decoder_factory> m_decoder_factory;

    /// The encoder factory
    std::shared_ptr<encoder_factory> m_encoder_factory;

    /// The encoder to use
    encoder_ptr m_encoder;

    /// The decoder to use
    decoder_ptr m_decoder;

    /// The number of symbols processed by the decoder
    uint32_t m_processed_symbols;

    /// The input data
    std::vector<uint8_t> m_data_in;

    /// The output data
    std::vector<uint8_t> m_data_out;

    /// Storage for encoded symbols
    std::vector< std::vector<uint8_t> > m_payloads;

    /// Multiplication factor for payload_count
    uint32_t m_factor;
};


/// A test block represents an encoder and decoder pair
template<class Encoder, class Decoder, bool Relaxed = false>
struct sparse_storage_benchmark :
    public storage_benchmark<Encoder,Decoder,Relaxed>
{
public:

    /// The type of the base benchmark
    typedef storage_benchmark<Encoder,Decoder,Relaxed> Super;

    /// We need access to the encoder built to adjust the average number of
    /// nonzero symbols
    using Super::m_encoder;

public:

    void get_options(gauge::po::variables_map& options)
    {
        auto symbols = options["symbols"].as<std::vector<uint32_t> >();
        auto loss_rate = options["loss_rate"].as<std::vector<double> >();
        auto symbol_size = options["symbol_size"].as<std::vector<uint32_t> >();
        auto types = options["type"].as<std::vector<std::string> >();
        auto density = options["density"].as<std::vector<double> >();

        assert(symbols.size() > 0);
        assert(loss_rate.size() > 0);
        assert(symbol_size.size() > 0);
        assert(types.size() > 0);
        assert(density.size() > 0);

        for (const auto& s : symbols)
        {
            for (const auto& r : loss_rate)
            {
                for (const auto& p : symbol_size)
                {
                    for (const auto& t : types)
                    {
                        for (const auto& d: density)
                        {
                            gauge::config_set cs;
                            cs.set_value<uint32_t>("symbols", s);
                            cs.set_value<uint32_t>("symbol_size", p);
                            cs.set_value<double>("loss_rate", r);
                            cs.set_value<std::string>("type", t);

                            uint32_t erased = (uint32_t)std::ceil(s * r);
                            cs.set_value<uint32_t>("erased_symbols", erased);

                            cs.set_value<double>("density", d);

                            Super::add_configuration(cs);
                        }
                    }
                }
            }
        }
    }

    void setup()
    {
        Super::setup();

        gauge::config_set cs = Super::get_current_configuration();
        double symbols = cs.get_value<double>("density");
        m_encoder->set_density(symbols);
    }
};

/// A test block represents an encoder and decoder pair
template<class Encoder, class Decoder, bool Relaxed = false>
struct perpetual_storage_benchmark :
    public storage_benchmark<Encoder,Decoder,Relaxed>
{
public:

    /// The type of the base benchmark
    typedef storage_benchmark<Encoder,Decoder,Relaxed> Super;

    /// We need access to the encoder to adjust the perpetual width ratio
    using Super::m_encoder;

public:

    void get_options(gauge::po::variables_map& options)
    {
        auto symbols = options["symbols"].as<std::vector<uint32_t> >();
        auto loss_rate = options["loss_rate"].as<std::vector<double> >();
        auto symbol_size = options["symbol_size"].as<std::vector<uint32_t> >();
        auto types = options["type"].as<std::vector<std::string> >();
        auto width_ratio = options["width_ratio"].as<std::vector<double> >();

        assert(symbols.size() > 0);
        assert(loss_rate.size() > 0);
        assert(symbol_size.size() > 0);
        assert(types.size() > 0);
        assert(width_ratio.size() > 0);

        for (const auto& s : symbols)
        {
            for (const auto& r : loss_rate)
            {
                for (const auto& p : symbol_size)
                {
                    for (const auto& t : types)
                    {
                        for (const auto& w: width_ratio)
                        {
                            gauge::config_set cs;
                            cs.set_value<uint32_t>("symbols", s);
                            cs.set_value<uint32_t>("symbol_size", p);
                            cs.set_value<double>("loss_rate", r);
                            cs.set_value<std::string>("type", t);

                            uint32_t erased = (uint32_t)std::ceil(s * r);
                            cs.set_value<uint32_t>("erased_symbols", erased);

                            cs.set_value<double>("width_ratio", w);

                            Super::add_configuration(cs);
                        }
                    }
                }
            }
        }
    }

    void setup()
    {
        Super::setup();

        gauge::config_set cs = Super::get_current_configuration();
        double width_ratio = cs.get_value<double>("width_ratio");
        m_encoder->set_width_ratio(width_ratio);
    }
};


/// Using this macro we may specify options. For specifying options
/// we use the boost program options library. So you may additional
/// details on how to do it in the manual for that library.
BENCHMARK_OPTION(throughput_options)
{
    gauge::po::options_description options;

    std::vector<uint32_t> symbols;
    symbols.push_back(16);

    auto default_symbols =
        gauge::po::value<std::vector<uint32_t>>()->default_value(
            symbols, "")->multitoken();

    std::vector<double> loss_rate;
    loss_rate.push_back(0.5);

    auto default_loss_rate =
        gauge::po::value<std::vector<double>>()->default_value(
            loss_rate, "")->multitoken();

    std::vector<uint32_t> symbol_size;
    symbol_size.push_back(1000000);

    auto default_symbol_size =
        gauge::po::value<std::vector<uint32_t>>()->default_value(
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
        ("loss_rate", default_loss_rate, "Set the ratio of erased symbols");

    options.add_options()
        ("symbol_size", default_symbol_size, "Set the symbol size in bytes");

    options.add_options()
        ("type", default_types, "Set type [encoder|decoder]");

    gauge::runner::instance().register_options(options);
}

BENCHMARK_OPTION(sparse_density_options)
{
    gauge::po::options_description options;

    std::vector<double> density;
    density.push_back(0.5);

    auto default_density =
        gauge::po::value<std::vector<double> >()->default_value(
            density, "")->multitoken();

    options.add_options()
        ("density", default_density, "Set the density of the sparse codes");

    gauge::runner::instance().register_options(options);
}

BENCHMARK_OPTION(perpetual_options)
{
    gauge::po::options_description options;

    std::vector<double> width_ratio;
    width_ratio.push_back(0.5);

    auto default_width_ratio =
        gauge::po::value<std::vector<double> >()->default_value(
            width_ratio, "")->multitoken();

    options.add_options()
        ("width_ratio", default_width_ratio,
        "Set the width ratio for perpetual codes");

    gauge::runner::instance().register_options(options);
}

//------------------------------------------------------------------
// FullRLNC
//------------------------------------------------------------------

typedef storage_benchmark<
    kodo::rlnc::shallow_full_vector_encoder<fifi::binary8>,
    kodo::rlnc::shallow_full_vector_decoder<fifi::binary8>>
    setup_rlnc_throughput8;

BENCHMARK_F(setup_rlnc_throughput8, FullRLNC, Binary8, 1)
{
    run_benchmark();
}

//------------------------------------------------------------------
// SparseFullRLNC
//------------------------------------------------------------------

typedef sparse_storage_benchmark<
    kodo::rlnc::shallow_sparse_full_vector_encoder<fifi::binary8>,
    kodo::rlnc::shallow_full_vector_decoder<fifi::binary8>, true>
    setup_sparse_rlnc_throughput8;

BENCHMARK_F(setup_sparse_rlnc_throughput8, SparseFullRLNC, Binary8, 1)
{
    run_benchmark();
}

//------------------------------------------------------------------
// Shallow Perpetual RLNC
//------------------------------------------------------------------

typedef perpetual_storage_benchmark<
    kodo::rlnc::shallow_perpetual_encoder<fifi::binary8>,
    kodo::rlnc::shallow_perpetual_decoder<fifi::binary8>, true>
    setup_perpetual_throughput8;

BENCHMARK_F(setup_perpetual_throughput8, Perpetual, Binary8, 1)
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
