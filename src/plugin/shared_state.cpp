#include "shared_state.hpp"

namespace hbp {

SharedState &SharedState::instance()
{
	static SharedState s;
	return s;
}

void SharedState::publish(const Snapshot &snapshot)
{
	std::lock_guard<std::mutex> lock(mutex_);
	snapshot_ = snapshot;
	snapshot_.version = ++counter_;
}

Snapshot SharedState::read() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return snapshot_;
}

} // namespace hbp

namespace hbp {

uint64_t SharedState::version() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return counter_;
}

} // namespace hbp
