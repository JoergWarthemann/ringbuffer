#include "catch.hpp"
#include <array>
#include "RingBuffer.h"
#include <string>

SCENARIO("Insert values into the ring buffer", "[insert values]")
{
    GIVEN("A source buffer with some items.")
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
        SourceArrayType sourceControl = source;
        DestinationArrayType destination(destinationSize);

        REQUIRE(source.size() == sourceSize);

        WHEN("Move single elements into the ring buffer destination")
        {
            for (std::size_t i = 0; i < sourceSize; ++i)
            {
                // After moving an rvalue element to destination the element at index in source is empty. This results from ValueType being movable.
                destination.insert(std::move(source[i]));
                
                THEN("the element in the source is empty after having been forwarded as rvalue.")
                {
                    REQUIRE(source[i].empty());
                }
                AND_THEN("the element count of destination increases.")
                {
                    REQUIRE(destination.currentSize() == std::min(destination.capacity(), i + 1));
                }
            }
        }

        WHEN("Move an oversized block of elements into the ring buffer destination")
        {
            destination.insert(std::move(source), sourceSize);

            THEN("the destination has the right element count.")
            {
                REQUIRE(destination.currentSize() == destinationSize);
            }
        }

        WHEN("Move an oversized block of elements into the ring buffer destination")
        {
            // We expect the ring buffer to efficiently move the last destinationSize elements only.
            destination.insert(std::move(source), sourceSize);

            // Now the destinationSize last elements of source will be in destination...
            for (std::size_t i = 0; i < destinationSize; ++i)
            {
                ValueType element = destination.copy(i);

                THEN("the element having been moved into destination contains correct data.")
                {
                    REQUIRE(element == sourceControl[sourceSize - i - 1]);
                }
            }

            // ...while the other elements of source remain untouched.
            for (std::size_t i = 0, iEnd = sourceSize - destinationSize; i < iEnd; ++i)
            {
                AND_THEN("the element not having been moved contains data.")
                {
                    REQUIRE_FALSE(source[i].empty());
                }
            }
        }
    }
}