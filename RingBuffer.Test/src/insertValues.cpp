#include "catch.hpp"
#include <array>
#include "RingBuffer.h"
#include <string>

SCENARIO("Inserting values into the ring buffer", "[inserting values]")
{
    GIVEN("An input buffer with some items.")
    {
        static const std::size_t sourceSize = 20;
        static const std::size_t destinationSize = 19;
        using ValueType = typename std::string;
        using SourceArrayType = typename std::array<ValueType, sourceSize>;
        using DestinationArrayType = typename Buffer::RingBuffer<ValueType>;

        SourceArrayType source
        {
            "one", "two", "three", "four", "five",
            "six", "seven", "eight", "nine", "ten",
            "eleven", "twelve", "thirteen", "fourteen", "fivteen",
            "sixteen", "seventeen", "eighteen", "nineteen", "twenty"
        };
        DestinationArrayType destination(destinationSize);

        REQUIRE(source.size() == sourceSize);

        WHEN("We move elements rvalue by rvalue into the ring buffer")
        {
            for (std::size_t i = 0; i < sourceSize; ++i)
            {
                // After moving an rvalue element to destination the element at index in source is empty. This results from ValueType being movable.
                destination.insert(std::move(source[i]));
                
                THEN("the element in the source array is empty after forwarding the rvalue.")
                {
                    REQUIRE(source[i].empty());
                }
                AND_THEN("the ringbuffer element count increases.")
                {
                    REQUIRE(destination.currentSize() == std::min(destination.capacity(), i + 1));
                }
            }
        }

        WHEN("We move a block of elements into the ring buffer")
        {
            destination.insert(std::move(source), sourceSize);

            THEN("the ringbuffer has the right element count.")
            {
                REQUIRE(destination.currentSize() == destinationSize);
            }
        }
    }
}


SCENARIO("2")
{
    GIVEN("An input buffer with some items.")
    {
        static const std::size_t sourceSize = 20;
        static const std::size_t destinationSize = 19;
        using ValueType = typename std::string;
        using SourceArrayType = typename std::array<ValueType, sourceSize>;
        using DestinationArrayType = typename Buffer::RingBuffer<ValueType>;

        SourceArrayType source
        {
            "one", "two", "three", "four", "five",
            "six", "seven", "eight", "nine", "ten",
            "eleven", "twelve", "thirteen", "fourteen", "fivteen",
            "sixteen", "seventeen", "eighteen", "nineteen", "twenty"
        };
        DestinationArrayType destination(destinationSize);

        REQUIRE(source.size() == sourceSize);

        WHEN("We move a block of elements into the ring buffer")
        {
            destination.insert(std::move(source), sourceSize);

            THEN("the ringbuffer has the right element count.")
            {
                REQUIRE(destination.currentSize() == destinationSize);
            }
        }
    }
}