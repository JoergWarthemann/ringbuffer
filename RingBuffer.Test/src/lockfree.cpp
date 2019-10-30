#include "catch.hpp"
#include <algorithm>
#include <array>
//#include <iostream>
#include "RingBuffer.h"
#include <string>
#include <thread>
#include <vector>

static const std::size_t sourceSize = 128;
using ValueType = typename unsigned int;
using SourceArrayType = typename std::array<ValueType, sourceSize>;

bool findSubsequence(const std::vector<ValueType>& needle, const SourceArrayType& haystack)
{
    auto position{ haystack.begin() };
    return ((position = std::search(position, haystack.end(), needle.begin(), needle.end())) != haystack.end());
}

TEST_CASE("Concurrent access from 2 threads", "[lock-free]")
{
    static const std::size_t destinationSize = 20;
    using DestinationArrayType = typename Buffer::RingBuffer<ValueType>;

    std::vector<std::vector<ValueType>> consumedArrays;

    SourceArrayType source
    {
          1,   2,   3,   4,   5,   6,   7,   8,
          9,  10,  11,  12,  13,  14,  15,  16,
         17,  18,  19,  20,  21,  22,  23,  24,
         25,  26,  27,  28,  29,  30,  31,  32,
         33,  34,  35,  36,  37,  38,  39,  40,
         41,  42,  43,  44,  45,  46,  47,  48,
         49,  50,  51,  52,  53,  54,  55,  56,
         57,  58,  59,  60,  61,  62,  63,  64,
         65,  66,  67,  68,  69,  70,  71,  72,
         73,  74,  75,  76,  77,  78,  79,  80,
         81,  82,  83,  84,  85,  86,  87,  88,
         89,  90,  91,  92,  93,  94,  95,  96,
         97,  98,  99, 100, 101, 102, 103, 104,
        105, 106, 107, 108, 109, 110, 111, 112,
        113, 114, 115, 116, 117, 118, 119, 120,
        121, 122, 123, 124, 125, 126, 127, 128
    };
    SourceArrayType sourceControl = source;
    DestinationArrayType destination(destinationSize);

    REQUIRE(source.size() == sourceSize);

    SECTION("Adding and reading items concurrently on two different threads")
    {
        std::thread producer([&source, &destination]()
            {
                const std::size_t writeLaps = 16;
                const std::size_t samplesPerLap = sourceSize / writeLaps;

                for (auto writeLap = 0; writeLap < writeLaps; ++writeLap)
                {
                    std::array<ValueType, samplesPerLap> insertSamples;
                    std::copy_n(source.begin() + writeLap * samplesPerLap, samplesPerLap, insertSamples.begin());

                    destination.insert(insertSamples, samplesPerLap);

                    //std::cout << "\n";
                    //for (const auto& sample : insertSamples)
                    //    std::cout << " w" << sample;

                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            });

        std::thread consumer([&destination, &consumedArrays]()
            {
                const std::size_t readLaps = 16;
                const std::size_t samplesPerLap = sourceSize / readLaps;

                for (auto readLap = 0; readLap < readLaps; ++readLap)
                {
                    std::array<ValueType, samplesPerLap> readSamples;
                    auto count = destination.copy(readSamples.data(), samplesPerLap);

                    //std::cout << "\n";
                    //for (auto i = 0; i < count; ++i)
                    //    std::cout << " r" << readSamples[i];

                    if (count > 0)
                        consumedArrays.push_back(std::vector<ValueType>(readSamples.begin(), readSamples.end()));

                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
            });

        producer.join();
        consumer.join();

        REQUIRE(consumedArrays.size() > 0);

        for (auto consumed = consumedArrays.begin(); consumed != consumedArrays.end(); ++consumed)
        {
            auto found = findSubsequence(*consumed, source);

            DYNAMIC_SECTION("  The consumed array needs to exist in the source array.")
            {
                REQUIRE(found);
            }
        }
	}
}