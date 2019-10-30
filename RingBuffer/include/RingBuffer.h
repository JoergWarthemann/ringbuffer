#pragma once

#include <algorithm>
#include <assert.h>
#include <atomic>
#include <functional>
#include <memory>
#include <type_traits>

namespace Buffer
{
/** Does not cast into move iterator. Simply return the passed iterator.
	Iterator ... The type of iterator. std::is_lvalue_reference<Type&&>::value returns true.
	\param iterator ... The iterator.
	\param std::true_type ... Is always std::true_type.
	\return auto ... Iterator.
*/
template <typename Iterator>
Iterator make_forward_iterator(Iterator iterator, std::true_type)
{
	return iterator;
}

/** Casts iterator into a move iterator.
	Iterator ... The type of iterator. std::is_lvalue_reference<Type&&>::value returns false.
	\param iterator ... The iterator.
	\param std::false_type ... Is always std::false_type.
	\return auto ... The moveable version of iterator.
*/
template <typename Iterator>
auto make_forward_iterator(Iterator iterator, std::false_type)
{
	return std::make_move_iterator(iterator);
}

/** Uses SFINAE to cast an iterator into a move iterator when the forwarded type is moveable.
	Type ... The type that is to be iterated.
	Iterator ... The type of iterator. std::is_lvalue_reference<Type&&>{} is true or false type.
	\param iterator ... The iterator.
	\return auto ... Iterator or iterator casted to be moveable.
*/
template <typename Type, typename Iterator>
auto make_forward_iterator(Iterator iterator)
{
	return make_forward_iterator<Iterator>(iterator, std::is_lvalue_reference<Type&&>{});
}


/** Iterates through an array and calls a function on the element together with an incremental index.
	Iterator ... The type of iterator.
	Function ... The type of function that is to be called on iterated elements.
	\param first ... A start iterator.
	\param last ... An end iterator.
	\param initial ... An initial value that gets incremented inside.
	\param func ... The function object that is to be called on the element.
	\return Function ... The function object.
*/
template <typename Iterator, typename Function>
Function enumerate(Iterator first, Iterator last, typename std::iterator_traits<Iterator>::difference_type initial, Function func)
{
	for (; first != last; ++first, ++initial)
		func(initial, std::forward<typename std::iterator_traits<Iterator>::value_type>(*first));
	return func;
}


/** This defines a ring buffer. When reaching the buffer end while inserting new elements it will overwrite the oldest elements.
	Uses aligned storage for performance gain.
	Element ... The type of elements that are getting stored.
*/
template<typename Element>
class RingBuffer
{
	// Create a type which optimally aligns Element for internal use.
    typedef typename std::aligned_storage<sizeof(Element), alignof(Element)>::type ElementStorageType;

    std::function<void(ElementStorageType*)> customizedStorageDeleter_ = [this](ElementStorageType* storageToGetDeleted)
    {
        const std::size_t tmpCapacity = capacity_.load(std::memory_order_relaxed);
        const std::size_t tmpCurrentSize = currentSize_.load(std::memory_order_relaxed);

        // First of all we need to call each Elements dtor. Then we are allowed to delete the array.
        for (std::size_t position = 0; position < std::min(tmpCurrentSize, tmpCapacity); ++position)
            reinterpret_cast<const Element*>(&storageToGetDeleted[position])->~Element();
        delete[] storageToGetDeleted;
    };

    // Use different cache lines in order to avoid false sharing.
    alignas(64) std::atomic<std::size_t> readPosition_;
    alignas(64) std::atomic<std::size_t> writePosition_;
    alignas(64) std::atomic<std::size_t> currentSize_;
    alignas(64) std::atomic<std::size_t> capacity_;
    std::unique_ptr<ElementStorageType, decltype(customizedStorageDeleter_)> buffer_;

	/** Destructs a range of elements.
		\param from ... The index of the first element.
		\param to ... The indexof the last element.
	*/
	void destruct(const std::size_t from, const std::size_t to)
	{
		enumerate(
			make_forward_iterator<ElementStorageType>(&buffer_.get()[from]),
			make_forward_iterator<ElementStorageType>(&buffer_.get()[to]),
			0,
			[](std::size_t index, ElementStorageType&& element)
		    {
			    reinterpret_cast<Element*>(&element)->~Element();
		    });
	}

    ///////////////////////////////////////////////////////////////////////////////////////////////
	// Insert.
	///////////////////////////////////////////////////////////////////////////////////////////////

	/** Inserts a sample.
		\param sample ... Universal reference of an object of type Element.
	*/
    template <typename T>
    void insertImpl(T&& sample)
    {
        const std::size_t tmpCapacity = capacity_.load(std::memory_order_relaxed);
        std::size_t tmpCurrentSize = currentSize_.load(std::memory_order_relaxed);
        std::size_t tmpWritePosition = writePosition_.load(std::memory_order_relaxed);
        // This read-acquire synchronizes with a write-release in extract implementations.
        const std::size_t tmpReadPosition = readPosition_.load(std::memory_order_acquire);

        if (tmpCurrentSize == tmpCapacity)
            // Call the destructor of the element that gets overwritten.
            reinterpret_cast<Element*>(&buffer_.get()[tmpWritePosition])->~Element();
        else
            // Update the size of the stored array.
            ++tmpCurrentSize;

        // Assign memory at buffer_[writePosition_] to a pointer of Element.
        // Since we use placement new no memory needs to be allocated.
        // Sample gets moved (when possible).
        Element* element = new(&buffer_.get()[tmpWritePosition]) Element(std::forward<T>(sample));
        // Update the position.
        tmpWritePosition = (tmpWritePosition + 1) % tmpCapacity;

        // These write-release synchronize with read-acquire in extract implementations.
        currentSize_.store(tmpCurrentSize, std::memory_order_release);
        writePosition_.store(tmpWritePosition, std::memory_order_release);
    }

	/** Moves or copies number elements from source to buffer_.
		\param source ... Universal reference to an array of Element objects.
		\param sourceStart ... Start position of the move/copy operation in source.
		\param number ... The number of elements that is to be moved/copied from source to buffer_.
		\param destinationStart ... Start position of the move/copy operation in buffer_.
	*/
    template <typename T>
    void insertBlockElements(T&& source, const std::size_t sourceStart, const std::size_t destinationStart, const std::size_t number, const std::size_t capacity, std::size_t& currentSize)
    {
        if (currentSize == capacity)
            // Call the destructor of all elements that get overwritten.
            destruct(destinationStart, destinationStart + number);

        std::uninitialized_copy(
            make_forward_iterator<T>(&source[sourceStart]),
            // Create an iterator for the last element. Since uninitialized_copy copies like [first, ..., last)
            // we add 1 to point after the array.
            make_forward_iterator<T>(&source[sourceStart + number - 1]) + 1,
            reinterpret_cast<Element*>(&buffer_.get()[destinationStart]));

        // Update the size of the stored array.
        if (currentSize < capacity)
            currentSize = std::min(capacity, currentSize + number);
    }

	/** Inserts a block of length samples.
		\param block ... Universal reference to an array of Element objects.
		\param length ... The length of the array of Element objects.
	*/
	template <typename T>
	void insertBlockImpl(T&& block, std::size_t blockLength)
	{
        const std::size_t tmpCapacity = capacity_.load(std::memory_order_relaxed);
        std::size_t tmpCurrentSize = currentSize_.load(std::memory_order_relaxed);
        std::size_t tmpWritePosition = writePosition_.load(std::memory_order_relaxed);
        // This read-acquire synchronizes with a write-release in extract implementations.
        const std::size_t tmpReadPosition = readPosition_.load(std::memory_order_acquire);

		std::size_t adjustedBlockLength = blockLength;
		// Do we need to crop the block to fit into the buffer?
		if (blockLength > tmpCapacity)
			adjustedBlockLength = tmpCapacity;			// Limit the length to the buffer's length.

		// Do we need to divide the block at the physical end of the buffer?
		if ((tmpWritePosition + adjustedBlockLength) > tmpCapacity)
		{
			// Forwards samples to the end of the buffer.
			insertBlockElements(
				std::forward<T>(block),
				blockLength - adjustedBlockLength,
                tmpWritePosition,
                tmpCapacity - tmpWritePosition,
                tmpCapacity,
                tmpCurrentSize);
			// Forwards samples to the begin of the buffer.
			insertBlockElements(
				std::forward<T>(block),
                tmpCapacity - tmpWritePosition,
				0,
				adjustedBlockLength - tmpCapacity + tmpWritePosition,
                tmpCapacity,
                tmpCurrentSize);

			// Update current position.
            tmpWritePosition = adjustedBlockLength - tmpCapacity + tmpWritePosition;
		}
		else
		{
			// Forwards the entire block to the first free location.
			insertBlockElements(
				std::forward<T>(block),
				blockLength - adjustedBlockLength,
                tmpWritePosition, adjustedBlockLength,
                tmpCapacity,
                tmpCurrentSize);

			// Update current position.
            tmpWritePosition = tmpWritePosition + adjustedBlockLength;
		}

		// Reset the position of the first free element if buffer is full.
		if (tmpWritePosition > tmpCapacity - 1)
            tmpWritePosition = 0;

        // These write-release synchronize with read-acquire in extract implementations.
        currentSize_.store(tmpCurrentSize, std::memory_order_release);
        writePosition_.store(tmpWritePosition, std::memory_order_release);
	}

	///////////////////////////////////////////////////////////////////////////////////////////////
	// Extract.
	///////////////////////////////////////////////////////////////////////////////////////////////

	/** Copies the block of the samples first ... last.
		SourceType ... The type of the source array.
		DestinationType ... The type of the destination array.
		\param source ... Universal reference to the buffer array.
		\param destination ... Pointer to the destination array that is to be filled with source elements.
		\param first ... Index of the first element that is to be copied.
		\param last ... Index of the last element that is to be copied.
		\param std::true_type ... Is always std::true_type.
	*/
	template <typename SourceType, typename DestinationType>
	void extractBlockElementsImpl(SourceType&& source, DestinationType* destination, std::size_t first, std::size_t last, std::true_type)
	{
		enumerate(
			make_forward_iterator<SourceType>(&source.get()[first]),
			make_forward_iterator<SourceType>(&source.get()[last]),
			0,
			[&destination](std::size_t index, ElementStorageType&& element)
		    {
			    destination[index] = *reinterpret_cast<DestinationType*>(&element);
		    });
	}

	/** Moves the block of the samples first ... last.
		SourceType ... The type of the source array.
		DestinationType ... The type of the destination array.
		\param source ... Universal reference to the buffer array.
		\param destination ... Pointer to the destination array that is to be filled with source elements.
		\param first ... Index of the first element that is to be moved.
		\param last ... Index of the last element that is to be moved.
		\param std::false_type ... Is always std::false_type.
	*/
	template <typename SourceType, typename DestinationType>
	void extractBlockElementsImpl(SourceType&& source, DestinationType* destination, std::size_t first, std::size_t last, std::false_type)
	{
		enumerate(
			make_forward_iterator<SourceType>(&source.get()[first]),
			make_forward_iterator<SourceType>(&source.get()[last]),
			0,
			[&destination](std::size_t index, ElementStorageType&& element)
		    {
			    destination[index] = std::forward<DestinationType>(*reinterpret_cast<DestinationType*>(&element));
		    });
	    }

	/** Extracts the block of numberOfElements samples.
		Uses SFINAE (based on the forwarded type of the source array) to get the right method for moving/copying.
		SourceType ... The type of the source array.
		DestinationType ... The type of the destination array.
		\param source ... Universal reference to the buffer array.
		\param destination ... Pointer to the destination array that is to be filled with source elements.
		\param numberOfElements ... The number of elements that are to be moved/copied from source to destinatin.
	*/
	template <typename SourceType, typename DestinationType>
	std::size_t extractBlockElements(SourceType&& source, DestinationType* destination, std::size_t numberOfElements)
	{
		assert(destination != nullptr);

        // These read-acquire synchronize with a write-release in insert implementations.
        const std::size_t tmpCurrentSize = currentSize_.load(std::memory_order_acquire);
        const std::size_t tmpWritePosition = writePosition_.load(std::memory_order_acquire);

		// We cannot give back more than we have.
		if (numberOfElements > tmpCurrentSize)
			numberOfElements = tmpCurrentSize;

		if (numberOfElements > 0)
		{
			std::size_t start = (tmpWritePosition + (tmpCurrentSize - numberOfElements)) % tmpCurrentSize;
			std::size_t size = ((tmpCurrentSize - start) < numberOfElements) ? (tmpCurrentSize - start) : numberOfElements;

			extractBlockElementsImpl(
				std::forward<SourceType>(source),
				destination,
				start,
				start + size,
				std::is_lvalue_reference<SourceType&&>{});

			int numberOfRemaining = static_cast<int>(numberOfElements - size);
			std::size_t startOfRemaining = (start + size) % tmpCurrentSize;

			if (numberOfRemaining > 0)
				extractBlockElementsImpl(
					std::forward<SourceType>(source),
					destination + size,
					startOfRemaining,
					startOfRemaining + numberOfRemaining,
					std::is_lvalue_reference<SourceType&&>{});
		}

        // This write-release synchronizes with a read-acquire in insert implementations.
        readPosition_.store(tmpWritePosition, std::memory_order_release);

		return numberOfElements;
	}

    /** Copies the sample feeded sampleBackwards samples ago.
        \param samplesBackward ... The reverse sample index.
        \return The sample feeded sampleBackward samples ago.
    */
    Element extractImpl(const std::size_t samplesBackward)
    {
        const std::size_t tmpCapacity = capacity_.load(std::memory_order_relaxed);
        // These read-acquire synchronize with a write-release in insert implementations.
        const std::size_t tmpCurrentSize = currentSize_.load(std::memory_order_acquire);
        const std::size_t tmpWritePosition = writePosition_.load(std::memory_order_acquire);

        if (tmpCurrentSize == 0)
            return Element();

        auto element = *reinterpret_cast<Element*>(&buffer_.get()[(tmpWritePosition - 1 - samplesBackward % tmpCapacity + tmpCapacity) % tmpCapacity]);

        // This write-release synchronizes with a read-acquire in insert implementations.
        readPosition_.store(tmpWritePosition, std::memory_order_release);

        return element;
    }

public:
	RingBuffer(const RingBuffer<Element>&) = delete;
    RingBuffer& operator=(const RingBuffer<Element>&) = delete;
	RingBuffer(RingBuffer<Element>&&) = delete;
	RingBuffer& operator=(RingBuffer<Element>&&) = delete;

	RingBuffer(void)
        // make_unique has no overload allowing to specify a custom deleter. Hence we need to live with the naked new.
        : buffer_(new ElementStorageType[0], customizedStorageDeleter_),
          capacity_(0),
          readPosition_(0),
          writePosition_(0),
          currentSize_(0)
	{}

	explicit RingBuffer(const std::size_t capacity)
        // make_unique has no overload allowing to specify a custom deleter. Hence we need to live with the naked new.
        : buffer_(new ElementStorageType[capacity], customizedStorageDeleter_),
        capacity_(capacity),
        readPosition_(0),
        writePosition_(0),
        currentSize_(0)
	{}

	~RingBuffer(void)
	{
		reset();
	}

	/** Destructs all buffer elements and resets the buffer size.
	*/
    void reset(void)
    {
        const std::size_t tmpCapacity = capacity_.load(std::memory_order_relaxed);
        const std::size_t tmpWritePosition = writePosition_.load(std::memory_order_relaxed);
        const std::size_t tmpCurrentSize = currentSize_.load(std::memory_order_relaxed);

        if (tmpCurrentSize == 0)
            return;

        std::size_t oldestElement = (tmpWritePosition - tmpCurrentSize + tmpCapacity) % tmpCapacity;
        std::size_t supposedNewestElement = oldestElement + tmpCurrentSize;
        std::size_t factualNewestElement = std::min(supposedNewestElement, static_cast<std::size_t>(tmpCapacity));

        destruct(oldestElement, factualNewestElement);

        factualNewestElement = supposedNewestElement - tmpCapacity;
        if (factualNewestElement > 0)
            destruct(0, factualNewestElement);

        writePosition_.store(0, std::memory_order_release);
        readPosition_.store(0, std::memory_order_release);
        currentSize_.store(0, std::memory_order_release);
    }

	/** Destructs all buffer elements and reinitializes the buffer with a new capacity.
	*/
    void reset(std::size_t newCapacity)
    {
        reset();

        capacity_.store(newCapacity, std::memory_order_relaxed);
        buffer_.reset(new ElementStorageType[newCapacity]);
    }

	/** Inserts a sample.
	    Declaring and using T brings us an universal reference and we can profit from type deduction.
	    So lvalue and rvalue references can be passed to this member (pefectly forwarded).
		\param sample ... Universal reference of an object of type Element.
	*/
	template <typename T>
	void insert(T&& sample)
	{
		insertImpl(std::forward<T>(sample));
	}

	/** Inserts a block of length samples.
		\param block ... Universal reference to an array of Element objects.
		\param length ... The length of the array of Element objects.
	*/
	template <typename T>
	void insert(T&& block, std::size_t blockLength)
	{
		insertBlockImpl(std::forward<T>(block), blockLength);
	}

	/** Copies the sample feeded sampleBackwards samples ago.
		e.g. get(0) Returns the last feeded sample.
		get(capacity_ - 1) Returns the oldest sample.
		get(capacity_ * n) returns the last feeded sample.
		get(capacity_ * n + 1) returns the sample feeded in prior to the last one.
		\param samplesBackward ... The reverse sample index.
		\return The sample feeded sampleBackward samples ago.
	*/
	Element copy(const std::size_t samplesBackward)
	{
        return extractImpl(samplesBackward);
	}

    /** Copies the last numberOfElements samples from buffer_ into destination.
        Attention: destination needs to have enough space for numberOfElements samples.
        \param destination ... Pointer to an array of Element.
        \param numberOfElements ... The number of elements that are to be copied into destination.
        \return The number of samples being copied.
    */
    std::size_t copy(Element* destination, std::size_t numberOfElements)
    {
        return extractBlockElements(buffer_, destination, numberOfElements);
    }

	/** \return The number of elements that the buffer could contain without overwriting older ones.
	*/
    std::size_t capacity(void) const
    {
        return capacity_.load(std::memory_order_relaxed);
    }

	/** \return The number of elements in the buffer.
	*/
    std::size_t currentSize(void) const
    {
        return currentSize_.load(std::memory_order_relaxed);
    }
};

}