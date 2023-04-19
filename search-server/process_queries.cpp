#include "process_queries.h"

#include <execution>
#include <algorithm>

std::vector<std::vector<Document>> ProcessQueries(
	const SearchServer& search_server,
	const std::vector<std::string>& queries) {
	std::vector<std::vector<Document>> process_queries(queries.size());
	std::transform(std::execution::par, queries.begin(), queries.end(), 
	process_queries.begin(), [&search_server](const std::string& query) {
		return search_server.FindTopDocuments(query); });
    return process_queries;
}

std::vector<Document> ProcessQueriesJoined(
	const SearchServer& search_server,
	const std::vector<std::string>& queries){
	std::vector<Document> queries_joined;
	std::vector<std::vector<Document>> process_queries = ProcessQueries(search_server, queries);
	for (size_t i = 0; i < queries.size(); i++) {
		for (const Document doc: process_queries.at(i)) {
        	queries_joined.push_back(doc);
		}
	}
	return queries_joined;
} 