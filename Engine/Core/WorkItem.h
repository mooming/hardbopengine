// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once
#include <cstddef>
#include "TaskID.h"
#include "TaskStreamAffinity.h"

namespace hbe
{
class Task;

/// @brief Called when work is dropped without running, on the thread that dropped it.
/// @param abandonedTask the ID the dropped item carried, which is the only identity that still means something
/// @param userData the pointer the requestor supplied, opaque to the engine
/// @note At most one call per work item, and never for work that ran. The callee must not block, must not acquire a
///       task-stream lock, and must not look up the task record - it is being dropped around this call.
using FAbandonedNotice = void (*)(TaskID abandonedTask, void* userData) noexcept;

/// @brief What a task stream's queue actually holds: one task, one slice of its index range, and how far that slice
///        has got.
/// @details Replaces `RangedTask`, which was this plus a copy of the task's name and measured 128 bytes. A queue item
///          is copied on every enqueue, every re-add after an unfinished run and every sweep that moves items between
///          lanes, so the item's size is paid per lane change rather than per task, which is what made the name worth
///          giving up. The name had exactly one reader: the warning issued when an item is dropped because its task
///          was released - and there the registry can no longer confirm the name is the one that task had, while the
///          ID is still the identity that means something. Dropping it also removes the item's only dependency on
/// @note The item measures 72 bytes, 16 of them the optional abandonment notice. The width is guarded by `decidedWorkItemBytes`, which exists to make a size change loud rather than silent.
/// @note Before the notice it measured 56 bytes. 8 of those were `TaskStreamAffinity`, which o 8 of them are `TaskStreamAffinity`, which once cost 64 because its word count was
///        derived from bytes per word instead of bits per word; that is fixed in `TaskStreamAffinity.h` and pinned by a
///        test there, because the defect was internally consistent and therefore invisible to any behavioural test.
///          `StaticString`, so a queued item can outlive the tracked task without holding a stale name alive.
/// @note The type is trivially copyable apart from the affinity mask, and `current` is mutable because re-adding an
///       unfinished item to a lane is a queue operation, not a change of the work it describes.
class WorkItem final
{
	using TIndex = std::size_t;

public:
	/// @brief Ordering key inside the priority queue, read at insertion only: it is the level this item is filed under.
	/// @details Not a live value. `BoundedPriorityQueue` buckets by this byte at `Push` and never re-sorts, so writing it
	///          while the item is queued changes nothing about when the item runs - the level it sits in is what decides.
	///          Changing a queued item's priority means removing it and pushing it again, because assignment cannot
	///          repair an order, only relocate the item. On the FIFO lane this byte is not consulted at all.
	uint8_t priority;
	mutable TaskStreamAffinity affinity;

	/// @brief Which task this work item belongs to, as the task registry issued it.
	/// @details Not a reference to the task. A work item can sit in a queue long after the object it was made from
	///          has stopped meaning anything, and an ID is what lets the stream notice that and drop the work
	///          rather than run it through a dead task's fields.
	TaskID taskID;

	/// @brief First index of this item's slice, as the splitter handed it out.
	TIndex start;
	/// @brief One past the last index of this item's slice. An item whose `current` reaches it has done its work.
	TIndex end;
	/// @brief Where this slice resumes: the first index the task's runnable has not reported completing.
	/// @details This field is the reason the item is not just `{taskID, start, end, priority}`. A runnable is allowed
	///          to return part of its range, and the stream re-adds the item to the lane it came from, where the next
	///          run starts here. A task whose runnable never finishes in one call is only ever correct because of this
	///          field; the incremental-resume test in the task system tests exists to catch its removal.
	mutable TIndex current;

	/// @brief Optional notice fired if this item is dropped without running; `nullptr` means nobody is listening.
	/// @details Copied like every other field, so an item re-added after a partial run and every slice of a split job
	///          keep the notice their task was offered with. Stamped from the task by the constructor, which is what
	///          makes a slice inherit its parent's notice instead of needing a line to remember it.
	/// @see TaskSystem::SetAbandonedNotice
	FAbandonedNotice abandonedNotice{ nullptr };

	/// @brief The requestor's context, passed through untouched. Its lifetime belongs to whoever set the notice: it
	///        must outlive every drop this item can suffer, and the engine will not clear it.
	void* abandonedUserData{ nullptr };

public:
	~WorkItem() = default;
	WorkItem& operator=(const WorkItem& other) = default;

	bool operator<(const WorkItem& other) const noexcept
	{
		return priority < other.priority;
	}

	[[nodiscard]] bool HasFinished() const noexcept
	{
		return current >= end;
	}

	/// @brief Run this item's slice of the task's work and report whether it was the last one.
	/// @param task The task this item was issued for, resolved by the stream from TaskID before calling.
	/// @note The caller has already checked that the ID names a live task. A work item whose task has been released
	///       is dropped by whoever dequeued it; nothing here reaches for a task by itself.
	/// @return True when this call closed the task's join, meaning every reserved subtask has now reported in and the
	///         outcome, if any, is ready to route. Exactly one item of a task ever gets true, and under R29 that item
	///         is the last reserved one rather than merely one of the ones past the count.
	/// @note The caller that gets true is the thread that finished the work, and it owes the successor a dispatch:
	///       TaskSystem::DispatchSuccessor. Nothing else delivers an outcome.
	bool Run(Task& task) noexcept;

private:
	WorkItem(Task& task, TIndex start, TIndex end, uint8_t priority) noexcept;

	friend class Task;
	friend class TaskStream;
};
} // namespace hbe
