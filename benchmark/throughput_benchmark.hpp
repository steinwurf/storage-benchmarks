// Copyright Steinwurf ApS 2011-2012.
// Distributed under the "STEINWURF RESEARCH LICENSE 1.0".
// See accompanying file LICENSE.rst or
// http://www.steinwurf.com/licensing

#pragma once

#include <ctime>
#include <cstdint>
#include <cstdio>

#include <gauge/gauge.hpp>


template<class Encoder, class Decoder, bool Relaxed = false>
struct throughput_benchmark : public gauge::time_benchmark
{
    void init()
    {
        m_factor = 1;
        gauge::time_benchmark::init();
    }

    void start()
    {
        m_encoded_symbols = 0;
        m_recovered_symbols = 0;
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

        // The number of bytes {en|de}coded
        uint64_t total_bytes = 0;

        if (type == "decoder")
        {
            total_bytes = m_recovered_symbols * symbol_size;
        }
        else if (type == "encoder")
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

    bool needs_warmup_iteration()
    {
        return false;
    }

    bool accept_measurement()
    {
        gauge::config_set cs = get_current_configuration();

        std::string type = cs.get_value<std::string>("type");

        if (type == "decoder")
        {
            // If we are benchmarking a decoder, we only accept
            // the measurement if the decoding was successful
            if (m_decoder->is_complete() == false)
            {
                return false;
            }
            // At this point, the output data should be equal to the input data
            assert(m_decoder->verify_data(m_encoder));
        }

        // Force a single iteration (repeated tests produce unstable results)
        return true;
    }

    std::string unit_text() const
    {
        return "MB/s";
    }

    void get_options(gauge::po::variables_map& options)
    {
        auto symbols = options["symbols"].as<std::vector<uint32_t> >();
        auto loss_rate = options["loss_rate"].as<std::vector<double> >();
        auto symbol_size = options["symbol_size"].as<std::vector<uint32_t> >();
        auto types = options["type"].as<std::vector<std::string> >();

        assert(symbols.size() > 0);
        assert(loss_rate.size() > 0);
        assert(symbol_size.size() > 0);
        assert(types.size() > 0);

        for (const auto& s : symbols)
        {
            for (const auto& r : loss_rate)
            {
                for (const auto& p : symbol_size)
                {
                    // Symbol size must be a multiple of 32
                    assert(p % 32 == 0);

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

        m_encoder = std::make_shared<Encoder>(
            symbols, symbol_size, erased_symbols);
        m_decoder = std::make_shared<Decoder>(
            symbols, symbol_size, erased_symbols);
    }

    void encode_payloads()
    {
        m_encoder->encode_all();
        m_encoded_symbols += m_encoder->payload_count();
    }

    void decode_payloads()
    {
        m_processed_symbols += m_decoder->decode_all(m_encoder);

        gauge::config_set cs = get_current_configuration();
        uint32_t erased_symbols = cs.get_value<uint32_t>("erased_symbols");

        if (m_decoder->is_complete())
        {
            m_recovered_symbols += erased_symbols;
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
    uint32_t m_recovered_symbols;

    /// The number of symbols processed by the decoder
    uint32_t m_processed_symbols;

    /// Multiplication factor for payload_count
    uint32_t m_factor;
};
