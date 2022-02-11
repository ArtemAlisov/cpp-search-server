#include "request_queue.h"

void RequestQueue::DeleteOldRequests() {
    	++timer_;
    	if(timer_ - requests_.front().coming_time_ >= min_in_day_) {
			requests_.pop_front();
		}
    }

std::vector<Document> RequestQueue::AddFindRequest(const string& raw_query, DocumentStatus status) {
    	DeleteOldRequests();
    	requests_.push_back({timer_, search_server_.FindTopDocuments(raw_query, status), raw_query});
    	return requests_.back().result_;
    	// напишите реализацию
    }

std::vector<Document> RequestQueue::AddFindRequest(const string& raw_query) {
	DeleteOldRequests();
	requests_.push_back({timer_, search_server_.FindTopDocuments(raw_query), raw_query});
	return requests_.back().result_;
	// напишите реализацию
}

int RequestQueue::GetNoResultRequests() const {
	 return count_if(requests_.begin(), requests_.end(), [](const auto& data) {
		return data.result_.empty();
	 });
	// напишите реализацию
}
