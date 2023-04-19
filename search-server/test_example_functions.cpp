#include "test_example_functions.h"

using namespace std;

void AddDocument(SearchServer& search_server, int id, const string& query,
		DocumentStatus doc_status, const vector<int>& rating) {
	search_server.AddDocument(id, query, doc_status, rating);
}



