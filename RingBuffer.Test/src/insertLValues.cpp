#include "catch.hpp"
#include <array>
#include "RingBuffer.h"
#include <string>

TEST_CASE("Insert lvalues into the ring buffer", "[insert lvalues]")
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

    SECTION("Copy single elements into the ring buffer destination.")
    {
        for (std::size_t i = 0; i < sourceSize; ++i)
        {
            // After copying an lvalue element to the destination the element at index in source is not empty.
            destination.insert(source[i]);

            DYNAMIC_SECTION("  The " << i << ". source element is not empty after having been copied and the element count of destination increases.")
            {
                REQUIRE_FALSE(source[i].empty());
                REQUIRE(destination.currentSize() == std::min(destination.capacity(), i + 1));
            }
        }
    }

    WHEN("Copy an oversized block of elements into the ring buffer destination")
    {
        destination.insert(source, sourceSize);

        THEN("the destination element count is not bigger than its size.")
        {
            REQUIRE(destination.currentSize() == destinationSize);
        }
    }

    SECTION("Copy an oversized block of elements into the ring buffer destination")
    {
        // We expect the ring buffer to efficiently copy the last destinationSize elements only.
        destination.insert(source, sourceSize);

        // Now the destinationSize last elements of source will be in destination...
        for (std::size_t i = 0; i < destinationSize; ++i)
        {
            ValueType element = destination.copy(i);

            DYNAMIC_SECTION("  The " << i << ". source element has been copied into destination and contains the expected data.")
            {
                REQUIRE(element == sourceControl[sourceSize - i - 1]);
            }
        }
    }

    SECTION("Copy different sized blocks of elements into the ring buffer destination")
    {
        std::array<ValueType, sourceSize> source1 = source;
        std::array<ValueType, sourceSize> source2 = source;
        std::array<ValueType, sourceSize> source3 = source;

        const std::size_t size1 = 15, size2 = 10, size3 = 5;
        std::vector<ValueType> controlGroup;
        // Build up a control group that contains the expected elements in the right order at the destinationSize last positions in the ring buffer.
        std::copy(source1.begin(), source1.begin() + size1, std::back_inserter(controlGroup));
        std::copy(source2.begin(), source2.begin() + size2, std::back_inserter(controlGroup));
        std::copy(source3.begin(), source3.begin() + size3, std::back_inserter(controlGroup));

        // Copy the sources into the ring buffer destination.
        destination.insert(source1, size1);
        destination.insert(source2, size2);
        destination.insert(source3, size3);

        // Check whether the elements in destination match the elements from the local control group.
        auto currentlyExpected = controlGroup.end();
        for (std::size_t i = 0; i < destinationSize; ++i)
        {
            --currentlyExpected;
            auto copiedElement = destination.copy(i);
            auto exp = *currentlyExpected;

            DYNAMIC_SECTION("  The " << i << ". destination element contains the expected data.")
            {
                REQUIRE(*currentlyExpected == copiedElement);
            }
        }
    }
}