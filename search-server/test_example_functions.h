#pragma once

#include "search_server.h"

void AddDocument(SearchServer& search_server, int id, const std::string& query,
		DocumentStatus doc_status, const std::vector<int>& rating);
