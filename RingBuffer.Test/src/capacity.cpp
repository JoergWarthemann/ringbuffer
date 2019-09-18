#include "catch.hpp"
#include <array>
#include "RingBuffer.h"
#include <string>

TEST_CASE("Capacity of the ring buffer", "[capacity]")
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

	WHEN("The elements are getting inserted into the ring buffer")
    {
		destination.insert(source, sourceSize);

		THEN("the size of the ring buffer is correct.")
        {
            REQUIRE(destination.currentSize() == destinationSize);
        }
	}

	WHEN("The ring buffer is getting resetted")
    {
		destination.insert(source, sourceSize);
		destination.reset();
		
		THEN("the size of the ring buffer is 0.")
        {
            REQUIRE(destination.currentSize() == 0);
        }
	}
}