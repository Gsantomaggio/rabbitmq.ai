#include "stream/dispatcher.hpp"

#include <utility>

namespace rmqstream {

std::future<Result<IncomingFrame>> Dispatcher::register_pending(
    std::uint32_t correlation_id, std::chrono::steady_clock::time_point deadline) {
    auto entry = std::make_unique<PendingEntry>();
    entry->deadline = deadline;
    auto future = entry->promise.get_future();
    {
        std::lock_guard<std::mutex> lock(mu_);
        pending_.emplace(correlation_id, std::move(entry));
    }
    return future;
}

bool Dispatcher::dispatch(IncomingFrame frame) {
    if (key::is_response(frame.key)) {
        if (!frame.correlation_id.has_value()) {
            return false;
        }
        std::unique_ptr<PendingEntry> entry;
        {
            std::lock_guard<std::mutex> lock(mu_);
            auto it = pending_.find(*frame.correlation_id);
            if (it == pending_.end()) {
                return false;
            }
            entry = std::move(it->second);
            pending_.erase(it);
        }
        entry->promise.set_value(Result<IncomingFrame>::ok(std::move(frame)));
        return true;
    }
    // One-way frames.
    auto kind = key::request_of(frame.key);
    if (kind == key::Heartbeat && on_heartbeat_) {
        on_heartbeat_(frame);
        return true;
    }
    if (kind == key::MetadataUpdate && on_metadata_update_) {
        on_metadata_update_(frame);
        return true;
    }
    return false;
}

void Dispatcher::fail_all_pending(StreamError err) {
    std::unordered_map<std::uint32_t, std::unique_ptr<PendingEntry>> taken;
    {
        std::lock_guard<std::mutex> lock(mu_);
        taken = std::move(pending_);
        pending_.clear();
    }
    for (auto& kv : taken) {
        kv.second->promise.set_value(Result<IncomingFrame>::err(err));
    }
}

std::size_t Dispatcher::reap_expired(std::chrono::steady_clock::time_point now) {
    std::vector<std::unique_ptr<PendingEntry>> expired;
    {
        std::lock_guard<std::mutex> lock(mu_);
        for (auto it = pending_.begin(); it != pending_.end();) {
            if (now >= it->second->deadline) {
                expired.emplace_back(std::move(it->second));
                it = pending_.erase(it);
            } else {
                ++it;
            }
        }
    }
    for (auto& e : expired) {
        e->promise.set_value(Result<IncomingFrame>::err(
            StreamError(StreamError::Kind::RequestTimeout, "request timed out")));
    }
    return expired.size();
}

void Dispatcher::on_heartbeat(OnewayHandler h) { on_heartbeat_ = std::move(h); }
void Dispatcher::on_metadata_update(OnewayHandler h) { on_metadata_update_ = std::move(h); }

}  // namespace rmqstream
