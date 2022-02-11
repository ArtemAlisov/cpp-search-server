#pragma once
#include "search_server.h"

#include <queue>
#include <vector>
#include <string>
#include <iostream>

using namespace std;

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server)
    : search_server_(search_server)
    {
    	// �������� ����������
    }
    // ������� "������" ��� ���� ������� ������, ����� ��������� ���������� ��� ����� ����������
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const string& raw_query, DocumentPredicate document_predicate) {
    	// �������� ����������
    	DeleteOldRequests();
    	requests_.push_back({timer_, search_server_.FindTopDocuments(raw_query, document_predicate), raw_query});
    	return requests_.back().result_;
    }

    std::vector<Document> AddFindRequest(const string& raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const string& raw_query);

    int GetNoResultRequests() const;
private:
    struct QueryResult {
    	int coming_time_;
    	std::vector<Document> result_;
    	string raw_query_;
        // ����������, ��� ������ ���� � ���������
    };
    deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    const SearchServer& search_server_;
    int timer_;
    // ��������, ����� ��� ����������� ���-�� ���

    void DeleteOldRequests();
};
