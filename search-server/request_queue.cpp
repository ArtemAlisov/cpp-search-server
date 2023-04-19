#include "request_queue.h"

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus document_status) {
	return AddFindRequest(raw_query, [document_status](int, DocumentStatus status, int) {
		return status == document_status;
	});
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query) {
    return AddFindRequest(raw_query, DocumentStatus::ACTUAL);
}

/*explicit RequestQueue::RequestQueue(const SearchServer& search_server)
	: search_server_(&search_server)
{
}*/
