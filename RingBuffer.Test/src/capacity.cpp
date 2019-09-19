#include "catch.hpp"
#include <array>
#include "RingBuffer.h"
#include <string>

TEST_CASE("Capacity of the ring buffer and element count", "[capacity and count]")
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

	WHEN("More elements than the ring buffers capacity are being inserted")
    {
		destination.insert(source, sourceSize);

		THEN("  the size of the ring buffer is correct.")
        {
            REQUIRE(destination.currentSize() == destinationSize);
        }
	}

	WHEN("The ring buffer is getting resetted")
    {
		destination.insert(source, sourceSize);
		destination.reset();
		
		THEN("  the size of the ring buffer is 0.")
        {
            REQUIRE(destination.currentSize() == 0);
        }
	}

    WHEN("A block of elements is copied into the ring buffer destination and then the ring buffer is resized")
    {
        // First we insert too many elements into the ring buffer.
        destination.insert(source, sourceSize);

        // Then we resize the ring buffer
        destination.reset(sourceSize * 2);

        THEN("  the ring buffer capacity has the right size.")
        {
            REQUIRE(sourceSize * 2 == destination.capacity());
        }

        AND_THEN("  the size of the ring buffer is correct.")
        {
            REQUIRE(destination.currentSize() == 0);
        }

        // Then we twice insert the source.
        destination.insert(source, sourceSize);
        destination.insert(source, sourceSize);

        for (std::size_t i = 0; i < sourceSize; ++i)
        {
            auto copiedElement = destination.copy(i);

            DYNAMIC_SECTION("  The " << i << ". destination element contains the expected data.")
            {
                REQUIRE(sourceControl[sourceSize - i % sourceSize - 1] == copiedElement);
            }
        }
    }

    WHEN("A block of elements is copied into the ring buffer destination")
    {
        destination.insert(source, sourceSize);

        THEN("  the ring buffer size does not exceed the source size.")
        {
            REQUIRE_FALSE(sourceSize == destination.currentSize());
        }

        AND_THEN("  the ring buffer capacity and its current size are equal.")
        {
            REQUIRE(sourceSize - destination.capacity() == sourceSize - destination.currentSize());
        }
    }
}