#include "catch.hpp"
#include <array>
#include "RingBuffer.h"
#include <string>

TEST_CASE("Get values from the ring buffer", "[get values]")
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

    SECTION("Get elements from the ring buffer. The count of elements is bigger than the ring buffers capacity.")
    {
        destination.insert(source, sourceSize);

        // Check whether the elements in destination match the elements from the control group.
        // Also walk over the buffer size which causes giving back elements in ring like manner.
        for (std::size_t i = 0, iEnd = destinationSize * 2; i < iEnd; ++i)
        {
            ValueType element = destination.copy(i);
            ValueType expected = sourceControl[sourceSize - (i % destinationSize) - 1];

            DYNAMIC_SECTION("  The " << i << ". ring element contains the expected data.")
            {
                REQUIRE(element == expected);
            }
        }
    }

    SECTION("Get a block of elements from the ring buffer. The block is bigger than the ring buffers capacity.")
    {
        destination.insert(source, sourceSize);
        const std::size_t blockCount = 4;
        std::size_t blockSize = static_cast<std::size_t>(round(sourceSize / blockCount));

        for (std::size_t i = 1; i <= blockCount; ++i)
        {
            std::array<ValueType, destinationSize> output;

            std::size_t currentBlockSize = i * blockSize;
            destination.copyBlock(&output[0], currentBlockSize);

            for (std::size_t j = 0, jEnd = ((currentBlockSize <= destinationSize) ? currentBlockSize : destinationSize); j < jEnd; ++j)
            {
                DYNAMIC_SECTION("  The " << j << ". element of the copied block contains the expected data.")
                {
                    REQUIRE(sourceControl[sourceSize - j - 1] == output[jEnd - j - 1]);
                }
            }
        }
    }

    SECTION("Copy various blocks of elements into the ring buffer. Get blocks of elements from the ring buffer and check their data integrity.")
    {
        const std::size_t size1 = 5, size2 = 10, size3 = 15;

        std::vector<ValueType> controlGroup;
        // Build up a control group that contains the expected elements in the right order at the destinationSize last positions.
        std::copy(source.begin(), source.begin() + size1, std::back_inserter(controlGroup));
        std::copy(source.begin(), source.begin() + size2, std::back_inserter(controlGroup));
        std::copy(source.begin(), source.begin() + size3, std::back_inserter(controlGroup));
        std::copy(source.begin(), source.begin() + size1, std::back_inserter(controlGroup));
        std::copy(source.begin(), source.begin() + size1, std::back_inserter(controlGroup));
        std::copy(source.begin(), source.begin() + size2, std::back_inserter(controlGroup));

        // Insert blocks of elements into the ring buffer.
        destination.insert(source, size1);
        destination.insert(source, size2);
        destination.insert(source, size3);
        destination.insert(source, size1);
        destination.insert(source, size1);
        destination.insert(source, size2);

        std::array<std::string, destinationSize> output;
        destination.copyBlock(&output[0], destinationSize);

        for (std::size_t i = 0; i < destinationSize; ++i)
        {
            DYNAMIC_SECTION("  The " << i << ". element of the copied block contains the expected data.")
            {
                REQUIRE(controlGroup[controlGroup.size() - destinationSize + i] == output[i]);
            }
        }
    }
}