#pragma once

#include <atomic>

// A simple queue. No locks, but only works with a single thread as producer and a single thread as
// a consumer. Mutex up as needed.

template <typename ElementType, size_t QueueSize>
class MsgQueue
{
	ElementType queue_[QueueSize];
	std::atomic_uint nextAdd_;
	std::atomic_uint nextSend_;
	std::atomic_uint pendingSends_;
public:
	MsgQueue()
	{
		std::atomic_init( &nextAdd_, 0U );
		std::atomic_init( &nextSend_, 0U );
		std::atomic_init( &pendingSends_, 0U );
	}

	ElementType* GetNextAddMessage()
	{
		// if we are falling behind, bail
		if ( pendingSends_.load() >= QueueSize )
			return nullptr;
		const auto index = nextAdd_++ % QueueSize;
		return &queue_[index];
	}

	void CommitAdd() { ++pendingSends_; }

	bool HavePendingSends() const { return pendingSends_.load() != 0; }

	ElementType* GetNextSendMessage()
	{
		const auto index = nextSend_++ % QueueSize;
		return &queue_[index];
	}

	void CommitSend() { --pendingSends_; }
};