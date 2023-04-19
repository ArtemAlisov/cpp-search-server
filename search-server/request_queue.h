#pragma once

#include <vector>
#include <deque>
#include <string>
#include "search_server.h"

struct Document;

class RequestQueue {
public:
	explicit RequestQueue(const SearchServer& search_server): search_server_(&search_server)
{
}

	template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);

	std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus document_status);

	std::vector<Document> AddFindRequest(const std::string& raw_query);

	inline int GetNoResultRequests() const {
		return requests_.size() - requests_.back().query_count;
	}
private:
	struct QueryResult {
		int query_count;
	};

	std::deque<QueryResult> requests_;
	const static int min_in_day_ = 1440;

	const SearchServer* search_server_;
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
    std::vector<Document> result = search_server_->FindTopDocuments(raw_query, document_predicate);

    QueryResult query_result;

    if(requests_.empty()) {
        query_result = {0};
    } else {
        query_result = requests_.back();
    }

    if(result.empty()) {
        requests_.push_back(query_result);
    } else {
        requests_.push_back({query_result.query_count + 1});
    }

    if(requests_.size() > min_in_day_) {
        requests_.pop_front();
    }

    return result;

}
