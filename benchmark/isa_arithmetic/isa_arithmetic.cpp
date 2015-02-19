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

//#include <sak/aligned_allocator.hpp>

#include <gauge/gauge.hpp>

extern "C"
{
#include "erasure_code.h"
#include "test.h"
}


#define TEST_SOURCES 250
#define MMAX TEST_SOURCES
#define KMAX TEST_SOURCES

/// Benchmark fixture for the arithmetic benchmark
class arithmetic_setup : public gauge::time_benchmark
{
public:

    /// The value type of a field element
    typedef uint8_t value_type;

public:

    double measurement()
    {
        // Get the time spent per iteration
        double time = gauge::time_benchmark::measurement();

        gauge::config_set cs = get_current_configuration();

        uint32_t size = cs.get_value<uint32_t>("size");
        uint32_t vectors = cs.get_value<uint32_t>("vectors");

        // The number of bytes processed per iteration
        uint64_t bytes = size * vectors;

        return bytes / time; // MB/s for each iteration
    }

    void store_run(tables::table& results)
    {
        if(!results.has_column("throughput"))
            results.add_column("throughput");

        results.set_value("throughput", measurement());
    }

    std::string unit_text() const
    {
        return "MB/s";
    }

    void get_options(gauge::po::variables_map& options)
    {
        auto sizes = options["size"].as<std::vector<uint32_t>>();
        auto vectors = options["vectors"].as<std::vector<uint32_t>>();

        assert(sizes.size() > 0);
        assert(vectors.size() > 0);

        for (const auto& s : sizes)
        {
            for (const auto& v : vectors)
            {
                gauge::config_set cs;
                cs.set_value<uint32_t>("size", s);
                cs.set_value<uint32_t>("vectors", v);

                add_configuration(cs);
            }
        }
    }

    /// Prepares the data structures between each run
    void setup()
    {
        gauge::config_set cs = get_current_configuration();

        uint32_t size = cs.get_value<uint32_t>("size");
        uint32_t vectors = cs.get_value<uint32_t>("vectors");

        // Prepare the continuous data blocks
        m_data_one.resize(vectors * size);
        m_data_two.resize(vectors * size);

        for (uint32_t i = 0; i < size; ++i)
        {
            m_data_one[i] = rand() % 256;
            m_data_two[i] = rand() % 256;
        }

        // Prepare the symbol pointers
        m_symbols_one.resize(vectors);
        m_symbols_two.resize(vectors);

        for (uint32_t i = 0; i < vectors; ++i)
        {
            m_symbols_one[i] = &m_data_one[i * size];
            m_symbols_two[i] = &m_data_two[i * size];
        }

        // Prepare constants, for block encoding you need a coefficient per
        // source symbol to produce one output symbol. In our benchmark we
        // "simulate" as many source vectors as destination vectors, so we
        // need vectors*vectors constants.
        m_constants.resize(vectors * vectors);
        for(auto& c : m_constants)
        {
            c = rand() % 256;
        }
    }

    void test_body()
    {
        gauge::config_set cs = base::get_current_configuration();
        uint32_t size = cs.get_value<uint32_t>("size");
        uint32_t vectors = cs.get_value<uint32_t>("vectors");

        gf_gen_rs_matrix(a, vectors, vectors);

        // Make parity vects
        ec_init_tables(vectors, vectors, &a[vectors * vectors], g_tbls);

        RUN
        {
            for (uint32_t i = 0; i < vectors; ++i)
            {
                gf_vect_dot_prod(size, vectors, g_tbls,
                                 m_symbols_two.data(), m_symbols_one[i]);
            }
        }
    }

protected:

    //uint8_t* m_buffs[TEST_SOURCES];
    uint8_t a[MMAX*KMAX];
    uint8_t g_tbls[KMAX*TEST_SOURCES*32];


    /// Type of the aligned vector
    //typedef std::vector<value_type, sak::aligned_allocator<value_type>>
    typedef std::vector<value_type>
        aligned_vector;

    /// The first buffer of vectors
    std::vector<value_type*> m_symbols_one;

    /// The second buffer of vectors
    std::vector<value_type*> m_symbols_two;

    /// Buffer for constants
    std::vector<value_type> m_constants;

    /// Random data for the first continuous buffer
    aligned_vector m_data_one;

    /// Random data for the second continuous buffer
    aligned_vector m_data_two;
};

/// Using this macro we may specify options. For specifying options
/// we use the boost program options library. So you may additional
/// details on how to do it in the manual for that library.
BENCHMARK_OPTION(arithmetic_options)
{
    gauge::po::options_description options;

    options.add_options()
        ("size",gauge::po::value<std::vector<uint32_t>>()->default_value(
    {64,1600}, "")->multitoken(), "Set the size of a vector in bytes");

    options.add_options()
        ("vectors", gauge::po::value<std::vector<uint32_t>>()->
        default_value({16,64,256}, "")->multitoken(),
        "Set the number of vectors to perform the operations on");

    gauge::runner::instance().register_options(options);
}

//------------------------------------------------------------------
// ISA Erasure Code
//------------------------------------------------------------------

typedef throughput_benchmark<isa_encoder, isa_decoder>
    isa_throughput;

BENCHMARK_F_INLINE(isa_throughput, ISA, ErasureCode, 1)
{
    run_benchmark();
}


//------------------------------------------------------------------
// ISA Arithmetic
//------------------------------------------------------------------

BENCHMARK_F_INLINE(arithmetic_setup, ISA, dot_product1, 1)
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
