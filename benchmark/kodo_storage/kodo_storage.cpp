// Copyright Steinwurf ApS 2011-2012.
// Distributed under the "STEINWURF RESEARCH LICENSE 1.0".
// See accompanying file LICENSE.rst or
// http://www.steinwurf.com/licensing

#include <ctime>
#include <cstdint>
#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

#include <gauge/gauge.hpp>
#include <gauge/console_printer.hpp>
#include <gauge/python_printer.hpp>
#include <gauge/csv_printer.hpp>
#include <gauge/json_printer.hpp>
#include <tables/table.hpp>

#include <fifi/api/field.hpp>

#include <kodo_rlnc/coders.hpp>
#include <kodo_reed_solomon/codes.hpp>

/// Tag to turn on block coding in the benchmark
struct block_coding_on{};

/// Tag to turn off block coding in the benchmark
struct block_coding_off{};

/// Tag to activate relaxed mode in the benchmark
struct relaxed{};

template
<
    fifi::api::field Field,
    class Encoder,
    class Decoder,
    class Feature = block_coding_off
>
struct storage_benchmark : public gauge::time_benchmark
{
    using encoder_factory = typename Encoder::factory;
    using encoder_ptr = typename Encoder::factory::pointer;

    using decoder_factory = typename Decoder::factory;
    using decoder_ptr = typename Decoder::factory::pointer;

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

        if (std::is_same<Feature, relaxed>::value)
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
                if (std::is_same<Feature, relaxed>::value)
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

        m_decoder_factory = std::make_shared<decoder_factory>(
            Field, symbols, symbol_size);

        m_encoder_factory = std::make_shared<encoder_factory>(
            Field, symbols, symbol_size);

        setup_factories();

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

        m_encoder->set_const_symbols(storage::storage(m_data_in));

        m_decoder->set_mutable_symbols(storage::storage(m_data_out));

        // Prepare storage for the encoded payloads
        uint32_t payload_count = erased_symbols * m_factor;
        assert(payload_count > 0);

        // Allocate contiguous payload buffer and store payload pointers
        uint32_t payload_size = m_encoder->payload_size();
        m_payload_buffer.resize(payload_count * payload_size);
        m_payloads.resize(payload_count);

        for (uint32_t i = 0; i < payload_count; ++i)
        {
            m_payloads[i] = &m_payload_buffer[i * payload_size];
        }
    }

    virtual void setup_factories()
    {
    }

    virtual void configure_encoder()
    {
    }

    void encode_payloads()
    {
        configure_encoder();
        m_encoder->set_const_symbols(storage::storage(m_data_in));

        // We switch any systematic operations off, because we are only
        // interested in producing coded symbols
        if (m_encoder->has_systematic_mode())
            m_encoder->set_systematic_off();

        uint32_t payload_count = (uint32_t) m_payloads.size();

        if (std::is_same<Feature, block_coding_on>::value &&
            m_encoder->has_write_payloads())
        {
            m_encoder->write_payloads(m_payloads.data(), payload_count);
        }
        else
        {
            for (uint32_t i = 0; i < payload_count; ++i)
            {
                m_encoder->write_payload(m_payloads[i]);
            }
        }
    }

    void decode_payloads()
    {
        uint32_t payload_count = (uint32_t) m_payloads.size();

        if (std::is_same<Feature, block_coding_on>::value &&
            m_decoder->has_read_payloads())
        {
            m_decoder->read_payloads(m_payloads.data(), payload_count);

            m_processed_symbols += payload_count;
        }
        else
        {
            for (uint32_t i = 0; i < payload_count; ++i)
            {
                m_decoder->read_payload(m_payloads[i]);

                m_processed_symbols++;

                if (m_decoder->is_complete())
                {
                    return;
                }
            }
        }
    }

    /// Run the encoder
    void run_encode()
    {
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

        // The clock is running
        RUN
        {
            // We have to make sure the decoder is in a "clean" state
            // i.e. no symbols already decoded.
            m_decoder->initialize(*m_decoder_factory);

            m_decoder->set_mutable_symbols(storage::storage(m_data_out));

            // Set the existing original symbols
            for (uint32_t i = 0; i < symbols; ++i)
            {
                // Skip the erased symbols
                if (erased.count(i) == 0)
                {
                    if (std::is_same<Feature, block_coding_on>::value &&
                        m_decoder->has_read_payloads())
                    {
                        // It is enough to mark the symbol as uncoded when
                        // using the block_decoder layer
                        m_decoder->set_symbol_uncoded(i);
                    }
                    else
                    {
                        // We need to update the decoding matrix with
                        // read_uncoded_symbol() if we use the single decoder
                        m_decoder->read_uncoded_symbol(
                            &m_data_out[i * symbol_size], i);
                    }
                }
            }

            // Decode the payloads
            decode_payloads();
        }
    }

    void test_body() final
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

    /// Contiguous buffer for coded payloads
    std::vector<uint8_t> m_payload_buffer;

    /// Pointers to each payload in the payload buffer
    std::vector<uint8_t*> m_payloads;

    /// Multiplication factor for payload_count
    uint32_t m_factor;
};


/// A test block represents an encoder and decoder pair
template
<
    kodo_rlnc::coding_vector_format CodingVectorFormat,
    fifi::api::field Field,
    class Encoder,
    class Decoder,
    class Feature = block_coding_off
>
struct rlnc_storage_benchmark : public
    storage_benchmark<Field, Encoder, Decoder, Feature>
{
public:

    using Super = storage_benchmark<Field, Encoder, Decoder, Feature>;

    using Super::m_encoder_factory;
    using Super::m_decoder_factory;

public:

    virtual void setup_factories()
    {
        // Set the selected coding vector format on the factories
        m_encoder_factory->set_coding_vector_format(CodingVectorFormat);
        m_decoder_factory->set_coding_vector_format(CodingVectorFormat);
    }
};

/// A test block represents an encoder and decoder pair
template
<
    kodo_rlnc::coding_vector_format CodingVectorFormat,
    fifi::api::field Field,
    class Encoder,
    class Decoder,
    class Feature = block_coding_off
>
struct sparse_rlnc_storage_benchmark : public
    rlnc_storage_benchmark<CodingVectorFormat,Field,Encoder,Decoder,Feature>
{
public:

    using Super =
        rlnc_storage_benchmark<CodingVectorFormat,Field,Encoder,Decoder,Feature>;

    using Super::m_encoder;

public:

    void get_options(gauge::po::variables_map& options)
    {
        auto symbols = options["symbols"].as<std::vector<uint32_t> >();
        auto loss_rate = options["loss_rate"].as<std::vector<double> >();
        auto symbol_size = options["symbol_size"].as<std::vector<uint32_t> >();
        auto types = options["type"].as<std::vector<std::string> >();
        auto density = options["density"].as<std::vector<float> >();

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

                            cs.set_value<float>("density", d);

                            Super::add_configuration(cs);
                        }
                    }
                }
            }
        }
    }

    virtual void configure_encoder()
    {
        Super::configure_encoder();

        gauge::config_set cs = Super::get_current_configuration();
        float density = cs.get_value<float>("density");
        m_encoder->set_density(density);
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

    std::vector<float> density;
    density.push_back(0.5f);

    auto default_density =
        gauge::po::value<std::vector<float> >()->default_value(
            density, "")->multitoken();

    options.add_options()
        ("density", default_density, "Set the density of the sparse codes");

    gauge::runner::instance().register_options(options);
}

//------------------------------------------------------------------
// FullRLNC
//------------------------------------------------------------------

using setup_rlnc_throughput8 = rlnc_storage_benchmark<
    kodo_rlnc::coding_vector_format::full_vector,
    fifi::api::field::binary8,
    kodo_rlnc::encoder,
    kodo_rlnc::decoder>;

BENCHMARK_F(setup_rlnc_throughput8, FullRLNC, Binary8, 5);

using setup_block_rlnc_throughput8 = rlnc_storage_benchmark<
    kodo_rlnc::coding_vector_format::full_vector,
    fifi::api::field::binary8,
    kodo_rlnc::encoder,
    kodo_rlnc::decoder,
    block_coding_on>;

BENCHMARK_F(setup_block_rlnc_throughput8, BlockFullRLNC, Binary8, 5);

//------------------------------------------------------------------
// SparseFullRLNC
//------------------------------------------------------------------

using setup_sparse_rlnc_throughput8 = sparse_rlnc_storage_benchmark<
    kodo_rlnc::coding_vector_format::full_vector,
    fifi::api::field::binary8,
    kodo_rlnc::encoder,
    kodo_rlnc::decoder,
    relaxed>;

BENCHMARK_F(setup_sparse_rlnc_throughput8, SparseFullRLNC, Binary8, 5);

//------------------------------------------------------------------
// Reed Solomon
//------------------------------------------------------------------

using setup_reed_solomon_throughput = storage_benchmark<
    fifi::api::field::binary8,
    kodo_reed_solomon::encoder,
    kodo_reed_solomon::decoder>;

BENCHMARK_F(setup_reed_solomon_throughput, ReedSolomon, Binary8, 5);

using setup_block_reed_solomon_throughput = storage_benchmark<
    fifi::api::field::binary8,
    kodo_reed_solomon::encoder,
    kodo_reed_solomon::decoder,
    block_coding_on>;

BENCHMARK_F(setup_block_reed_solomon_throughput, BlockReedSolomon, Binary8, 5);

int main(int argc, const char* argv[])
{
    srand(static_cast<uint32_t>(time(0)));

    gauge::runner::add_default_printers();
    gauge::runner::run_benchmarks(argc, argv);

    return 0;
}
