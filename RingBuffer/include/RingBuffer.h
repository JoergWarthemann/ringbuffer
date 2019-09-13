#pragma once

#include <algorithm>
#include <assert.h>
#include <functional>
#include <memory>
#include <type_traits>

namespace Buffer
{
/** Does not cast into move iterator. Simply return the passed iterator.
	Iterator ... The type of iterator. std::is_lvalue_reference<Type&&>::value returns true.
	@param[in] iterator ... The iterator.
	@param[in] std::true_type ... Is always std::true_type.
	\return auto ... Iterator.
*/
template <typename Iterator>
Iterator make_forward_iterator(Iterator iterator, std::true_type)
{
	return iterator;
}

/** Casts iterator into a move iterator.
	Iterator ... The type of iterator. std::is_lvalue_reference<Type&&>::value returns false.
	@param[in] iterator ... The iterator.
	@param[in] std::false_type ... Is always std::false_type.
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
	@param[in] iterator ... The iterator.
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
	@param[in] first ... A start iterator.
	@param[in] last ... An end iterator.
	@param[in] initial ... An initial value that gets incremented inside.
	@param[in] func ... The function object that is to be called on the element.
	\return Function ... The function object.
*/
template <typename Iterator, typename Function>
Function enumerate(Iterator first, Iterator last, typename std::iterator_traits<Iterator>::difference_type initial, Function func)
{
	for (; first != last; ++first, ++initial)
		func(initial, std::forward<std::iterator_traits<Iterator>::value_type>(*first));
	return func;
}


/** This defines a ring buffer. When reaching the buffer end while inserting a new element it will overwrite the oldest element.
	Uses aligned storage for performance gain.
	Element ... The type of elements that are getting stored.
*/
template<typename Element>
class RingBuffer
{
	// Create a type which optimally aligns Element for internal use.
    typedef typename std::aligned_storage<sizeof(Element), __alignof(Element)>::type ElementStorageType;

	// ATTENTION: Use of unique_ptr is not possible with msvc12 since its deleter parameter is misleadingly copied instead of being moved.
	//            So we cannot use a lambda or std::bind for the deleter of an array.
	//            Moreover make_unique cannot be used since it does not take care of the deleter.
	// http://www.ciiycode.com/7SSHSigQggXX/noncopyable-deleter-in-stduniqueptr
	// http://stackoverflow.com/questions/23613104/non-copyable-deleter-in-stdunique-ptr
	////std::unique_ptr<ElementStorageType[], std::function<void(ElementStorageType*)> > buffer_;
	//std::unique_ptr<ElementStorageType, std::function<void(ElementStorageType*)> > buffer_;

	std::shared_ptr<ElementStorageType> buffer_;
	std::size_t capacity_;
	std::size_t writePosition_;
	std::size_t currentSize_;

	/** Initializes the internal buffer and positions.
		@param[in] newCapacity ... The new capacity of the internal buffer.
	*/
	void initialize(std::size_t newCapacity)
	{
		capacity_ = newCapacity;

		// Create the buffer with the specified capacity and a custom deleter.
		buffer_.reset(new ElementStorageType[capacity_], [this](ElementStorageType* buffer)	// Cannot use std::make_shared with a custom deleter.
		{
			// First of all we need to call each Elements dtor. Then we are allowed to delete the array.
			for (std::size_t position = 0; position < std::min(currentSize_, capacity_); ++position)
				reinterpret_cast<const Element*>(&buffer[position])->~Element();
			delete[] buffer;
		});

		writePosition_ = 0;
		currentSize_ = 0;

		////buffer_(new ElementStorageType[capacity], [this] (ElementStorageType* buf)
		//buffer_(new ElementStorageType, [this](ElementStorageType* buf)
		//{
		//	// Test
		//	//auto deleter = [this](int* ptr){ int i = 10; };
		//	//std::unique_ptr<int[], decltype(deleter)> ptr4(new int[4], deleter);

		//	for (std::size_t position = 0; position < currentSize_; ++position)
		//		reinterpret_cast<const Element*>(&buf[position])->~Element();
		//}),
		//buffer_(new ElementStorageType[capacity_], std::bind(&RingBuffer<Element>::destroyBuffer, this)),
	}

	/** Destructs a range of elements.
		@param[in] from ... The index of the first element.
		@param[in] to ... The indexof the last element.
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

	/** Recalculates writePosition_ and currentSize_ after moving a specified number of elements.
		@param[in] movedSamples ... The number of samples that have been moved.
	*/
	void recalculatePositionAfterMoving(const std::size_t movedSamples)
	{
		writePosition_ = (writePosition_ - movedSamples + capacity_) % capacity_;
		currentSize_ -= movedSamples;
	}

	///////////////////////////////////////////////////////////////////////////////////////////////
	// Insert.
	///////////////////////////////////////////////////////////////////////////////////////////////

	/** Inserts a sample.
		@param[in] sample ... Universal reference of an object of type Element.
	*/
	template <typename T>
	void insertImpl(T&& sample)
	{
		if (currentSize_ == capacity_)
			// Call the destructor of the element that gets overwritten.
			reinterpret_cast<Element*>(&buffer_.get()[writePosition_])->~Element();
		else
			// Update the size of the stored array.
			++currentSize_;

		// Assign memory at buffer_[writePosition_] to a pointer of Element.
		// Since we use placement new no memory needs to be allocated.
		// Sample gets moved (when possible).
		Element* element = new(&buffer_.get()[writePosition_]) Element(std::forward<T>(sample));
		// Update the position.
		writePosition_ = (writePosition_ + 1) % capacity_;
	}

	/** Moves or copies number elements from source to buffer_.
		@param[in] source ... Universal reference to an array of Element objects.
		@param[in] sourceStart ... Start position of the move/copy operation in source.
		@param[in] number ... The number of elements that is to be moved/copied from source to buffer_.
		@param[in] destinationStart ... Start position of the move/copy operation in buffer_.
	*/
	template <typename T>
	void insertBlockElements(T&& source, const std::size_t sourceStart, const std::size_t destinationStart, const std::size_t number)
	{
		if (currentSize_ == capacity_)
			// Call the destructor of all elements that get overwritten.
			destruct(destinationStart, destinationStart + number);

		std::uninitialized_copy(
			make_forward_iterator<T>(&source[sourceStart]),
			// Create an iterator for the last element. Since uninitialized_copy copies like [first, ..., last)
			// we add 1 to point after the array.
			make_forward_iterator<T>(&source[sourceStart + number - 1]) + 1,
			reinterpret_cast<Element*>(&buffer_.get()[destinationStart]));

		// Update the size of the stored array.
		if (currentSize_ < capacity_)
			currentSize_ = std::min(capacity_, currentSize_ + number);
	}

	/** Inserts a block of length samples.
		@param[in] block ... Universal reference to an array of Element objects.
		@param[in] length ... The length of the array of Element objects.
	*/
	template <typename T>
	void insertBlockImpl(T&& block, std::size_t blockLength)
	{
		std::size_t adjustedBlockLength = blockLength;
		// Do we need to crop the block to fit into the buffer?
		if (blockLength > capacity_)
			adjustedBlockLength = capacity_;			// Limit the length to the buffer's length.

		// Do we need to divide the block at the physical end of the buffer?
		if ((writePosition_ + adjustedBlockLength) > capacity_)
		{
			// Forwards samples to the end of the buffer.
			insertBlockElements(
				std::forward<T>(block),
				blockLength - adjustedBlockLength,
				writePosition_,
				capacity_ - writePosition_);
			// Forwards samples to the begin of the buffer.
			insertBlockElements(
				std::forward<T>(block),
				capacity_ - writePosition_,
				0,
				adjustedBlockLength - capacity_ + writePosition_);

			// Update current position.
			writePosition_ = adjustedBlockLength - capacity_ + writePosition_;
		}
		else
		{
			// Forwards the entire block to the first free location.
			insertBlockElements(
				std::forward<T>(block),
				blockLength - adjustedBlockLength,
				writePosition_, adjustedBlockLength);

			// Update current position.
			writePosition_ = writePosition_ + adjustedBlockLength;
		}

		// Reset the position of the first free element if buffer is full.
		if (writePosition_ > capacity_ - 1)
			writePosition_ = 0;
	}

	///////////////////////////////////////////////////////////////////////////////////////////////
	// Extract.
	///////////////////////////////////////////////////////////////////////////////////////////////

	/** Copies the block of the samples first ... last.
		SourceType ... The type of the source array.
		DestinationType ... The type of the destination array.
		@param[in] source ... Universal reference to the buffer array.
		@param[in] destination ... Pointer to the destination array that is to be filled with source elements.
		@param[in] first ... Index of the first element that is to be copied.
		@param[in] last ... Index of the last element that is to be copied.
		@param[in] std::true_type ... Is always std::true_type.
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
		@param[in] source ... Universal reference to the buffer array.
		@param[in] destination ... Pointer to the destination array that is to be filled with source elements.
		@param[in] first ... Index of the first element that is to be moved.
		@param[in] last ... Index of the last element that is to be moved.
		@param[in] std::false_type ... Is always std::false_type.
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
		@param[in] source ... Universal reference to the buffer array.
		@param[in] destination ... Pointer to the destination array that is to be filled with source elements.
		@param[in] numberOfElements ... The number of elements that are to be moved/copied from source to destinatin.
	*/
	template <typename SourceType, typename DestinationType>
	std::size_t extractBlockElements(SourceType&& source, DestinationType* destination, std::size_t numberOfElements)
	{
		assert(destination != nullptr);

		// We cannot give back more than we have.
		if (numberOfElements > currentSize_)
			numberOfElements = currentSize_;

		if (numberOfElements > 0)
		{
			std::size_t start = (writePosition_ + (currentSize_ - numberOfElements)) % currentSize_;
			std::size_t size = ((currentSize_ - start) < numberOfElements) ? (currentSize_ - start) : numberOfElements;

			extractBlockElementsImpl(
				std::forward<SourceType>(source),
				destination,
				start,
				start + size,
				std::is_lvalue_reference<SourceType&&>{});

			int numberOfRemaining = numberOfElements - size;
			std::size_t startOfRemaining = (start + size) % currentSize_;

			if (numberOfRemaining > 0)
				extractBlockElementsImpl(
					std::forward<SourceType>(source),
					destination + size,
					startOfRemaining,
					startOfRemaining + numberOfRemaining,
					std::is_lvalue_reference<SourceType&&>{});
		}

		return numberOfElements;
	}

public:
	RingBuffer(const RingBuffer<Element>&) = delete;
    RingBuffer& operator=(const RingBuffer<Element>&) = delete;
	RingBuffer(RingBuffer<Element>&&) = delete;
	RingBuffer& operator=(RingBuffer<Element>&&) = delete;

	RingBuffer(void)
	{
		initialize(0);
	}

	explicit RingBuffer(const std::size_t capacity)
	{
		initialize(capacity);
	}

	~RingBuffer(void)
	{
		reset();
	}

	/** Destructs all buffer elements and resets the buffer size.
	*/
	void reset(void)
	{
		if (currentSize_ > 0)
		{
			std::size_t oldestElement = (writePosition_ - currentSize_ + capacity_) % capacity_;
            std::size_t supposedNewestElement = oldestElement + currentSize_;
            std::size_t factualNewestElement = std::min(supposedNewestElement, static_cast<std::size_t>(capacity_));

			destruct(oldestElement, factualNewestElement);

			factualNewestElement = supposedNewestElement - capacity_;
			if (factualNewestElement > 0)
				destruct(0, factualNewestElement);

			writePosition_ = 0;
			currentSize_ = 0;
		}
	}

	/** Destructs all buffer elements and reinitializes the buffer with a new capacity.
	*/
	void reset(std::size_t newCapacity)
	{
		reset();
		initialize(newCapacity);
	}

	/** Inserts a sample.
	    Declaring and using T brings us an universal reference and we can profit from type deduction.
	    So lvalue and rvalue references can be passed to this member (pefectly forwarded).
		@param[in] sample ... Universal reference of an object of type Element.
	*/
	template <typename T>
	void insert(T&& sample)
	{
		insertImpl(std::forward<T>(sample));
	}

	/** Inserts a block of length samples.
		@param[in] block ... Universal reference to an array of Element objects.
		@param[in] length ... The length of the array of Element objects.
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
		@param[in] samplesBackward ... The reverse sample index.
		\return The sample feeded sampleBackward samples ago.
	*/
	Element copy(const std::size_t samplesBackward) const
	{
		if (currentSize_ == 0)
			return Element();

		return *reinterpret_cast<Element*>(&buffer_.get()[(writePosition_ - 1 - samplesBackward % capacity_ + capacity_) % capacity_]);
	}

	/** Copies the last numberOfElements samples from buffer_ into destination.
		Attention: destination needs to have enough space for numberOfElements samples.
		@param[in] destination ... Pointer to an array of Element.
		@param[in] numberOfElements ... The number of elements that are to be copied into destination.
	*/
	void copyBlock(Element* destination, std::size_t numberOfElements)
	{
		extractBlockElements(buffer_, destination, numberOfElements);
	}

	/** Moves the last sample that has been feeded in.
		\return Element&& ... The rvalue of the last sample.
	*/
	Element&& moveLast(void)
	{
		assert(currentSize_ != 0);

		std::size_t position = (writePosition_ - 1 + capacity_) % capacity_;

		recalculatePositionAfterMoving(1);

		return std::move(*reinterpret_cast<Element*>(&buffer_.get()[position]));
	}

	/** Moves the last numberOfElements samples from buffer_ into destination.
		Attention: destination needs to have enough space for numberOfElements samples.
		@param[in] destination ... Pointer to an array of Element.
		@param[in] numberOfElements ... The number of elements that are to be moved into destination.
		\return std::size_t ... The number of actually moved elements.
	*/
	std::size_t moveBlock(Element* destination, std::size_t numberOfElements)
	{
		numberOfElements = extractBlockElements(std::move(buffer_), destination, numberOfElements);
		// Use the actual number of moved elements to recalculate the position.
		recalculatePositionAfterMoving(numberOfElements);

		return numberOfElements;
	}

	/** Returns the number of elements that the bufefr could contain without overwriting older ones.
		\return The number of elements that the bufefr could contain without overwriting older ones.
	*/
	std::size_t capacity(void) const
	{
		return capacity_;
	}

	/** Returns the number of elements in the buffer.
		\return The number of elements in the buffer.
	*/
	std::size_t currentSize(void) const
	{
		return currentSize_;
	}
};

}