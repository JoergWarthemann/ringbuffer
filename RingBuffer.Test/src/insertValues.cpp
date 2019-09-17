#include "catch.hpp"
#include <array>
#include "RingBuffer.h"
#include <string>

TEST_CASE("Insert values into the ring buffer", "[insert values]")
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

    SECTION("Move single elements into the ring buffer destination.")
    {
        for (std::size_t i = 0; i < sourceSize; ++i)
        {
            // After moving an rvalue element to destination the element at index in source is empty. This results from ValueType being movable.
            destination.insert(std::move(source[i]));
                
            DYNAMIC_SECTION("  The " << i << ". element in the source is empty after having been forwarded as rvalue and the element count of destination increases.")
            {
                REQUIRE(source[i].empty());
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

    SECTION("Move an oversized block of elements into the ring buffer destination")
    {
        // We expect the ring buffer to efficiently move the last destinationSize elements only.
        destination.insert(std::move(source), sourceSize);

        // Now the destinationSize last elements of source will be in destination...
        for (std::size_t i = 0; i < destinationSize; ++i)
        {
            ValueType element = destination.copy(i);

            DYNAMIC_SECTION("  The " << i << ". element having been moved into destination contains correct data.")
            {
                REQUIRE(element == sourceControl[sourceSize - i - 1]);
            }
        }

        // ...while the other elements of source remain untouched.
        for (std::size_t i = 0, iEnd = sourceSize - destinationSize; i < iEnd; ++i)
        {
            DYNAMIC_SECTION("  The " << i << ". element remaining in source contains data.")
            {
                REQUIRE_FALSE(source[i].empty());
            }
        }
    }

    //WHEN("Move different sized blocks of elements into the ring buffer destination")
    //{
    //    std::array<ValueType, sourceSize> source1 = source;
    //    std::array<ValueType, sourceSize> source2 = source;
    //    std::array<ValueType, sourceSize> source3 = source;
    //
    //    const std::size_t size1 = 15, size2 = 10, size3 = 5;
    //    std::vector<ValueType> controlGroup;
    //    // Build up a control group that contains the expected elements in the right order at the destinationSize last positions in the ring buffer.
    //    std::copy(source1.begin(), source1.begin() + size1, std::back_inserter(controlGroup));
    //    std::copy(source2.begin(), source2.begin() + size2, std::back_inserter(controlGroup));
    //    std::copy(source3.begin(), source3.begin() + size3, std::back_inserter(controlGroup));
    //
    //    // Move the sources into the ring buffer destination.
    //    destination.insert(std::move(source1), size1);
    //    destination.insert(std::move(source2), size2);
    //    destination.insert(std::move(source3), size3);
    //
    //    auto controlTheCurrentIndex = [&sourceControl](std::size_t theIndex, std::array<ValueType, sourceSize>& theSourceArray, std::size_t theArraySize)
    //    {
    //        if (theIndex < theArraySize)
    //            THEN("the right element has been moved.")
    //            {
    //                REQUIRE(theSourceArray[theIndex].empty());
    //            }
    //        else
    //            THEN("the right element has not been moved.")
    //            {
    //                REQUIRE(sourceControl[theIndex] == theSourceArray[theIndex]);
    //            }
    //    };
    //
    //    // Check whether the right elements have been moved from source or have been left behind in source.
    //    for (std::size_t i = 0; i < sourceSize; ++i)
    //    {
    //        controlTheCurrentIndex(i, source1, size1);
    //        controlTheCurrentIndex(i, source2, size2);
    //        controlTheCurrentIndex(i, source3, size3);
    //    }
    //
    //    // Check whether the elements in destination match the elements from the local control group.
    //    auto currentlyExpected = controlGroup.end();
    //    for (std::size_t i = 0; i < destinationSize; ++i)
    //    {
    //        --currentlyExpected;
    //        auto copiedElement = destination.copy(i);
    //        auto exp = *currentlyExpected;
    //
    //        THEN("the right element has been moved.")
    //        {
    //            REQUIRE(*currentlyExpected == copiedElement);
    //        }
    //    }
    //}
}