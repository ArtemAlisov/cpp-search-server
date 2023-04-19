#pragma once

#include "document.h"
#include "read_input_functions.h"
#include "string_processing.h"
#include "concurrent_map.h"

#include <vector>
#include <set>
#include <string>
#include <map>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <execution>
#include <functional>
#include <unordered_set>
#include <atomic>

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

class SearchServer {
public:
	template <typename StringContainer>
	explicit SearchServer(const StringContainer& stop_words);

	explicit SearchServer(const string& stop_words_text);
	explicit SearchServer(string_view stop_words_text);

	void AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

	template <typename DocumentPredicate>
	std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const;
	std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const ;
	std::vector<Document> FindTopDocuments(std::string_view raw_query) const;
    
	template <typename DocumentPredicate, class ExecutionPolicy>
	std::vector<Document> FindTopDocuments(ExecutionPolicy policy, std::string_view raw_query, DocumentPredicate document_predicate) const;
	template <class ExecutionPolicy>
	std::vector<Document> FindTopDocuments(ExecutionPolicy policy, std::string_view raw_query, DocumentStatus status) const ;
	template <class ExecutionPolicy>
	std::vector<Document> FindTopDocuments(ExecutionPolicy policy, std::string_view raw_query) const;
    
	std::set<int>::const_iterator begin() const;
	std::set<int>::const_iterator end() const;

	int GetDocumentCount() const;
	std::map<std::set<string>, std::vector<int>> GetInfo(int document_id);
	const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

	template <class ExecutionPolicy>
	void RemoveDocument(ExecutionPolicy policy, int document_id);
	void RemoveDocument(int document_id);
    
	using MatchedDocuments = std::tuple<std::vector<std::string_view>, DocumentStatus>;
	MatchedDocuments MatchDocument(const std::string_view raw_query, int document_id) const;
	MatchedDocuments MatchDocument(std::execution::sequenced_policy seq, const std::string_view raw_query, int document_id) const;
	MatchedDocuments MatchDocument(std::execution::parallel_policy par, const std::string_view raw_query, int document_id) const;
    
private:
	struct DocumentData {
		int rating;
		DocumentStatus status;
		std::string text;
	};
	const std::set<std::string, std::less<>> stop_words_;
	std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
	std::map<int, DocumentData> documents_;
	std::set<int> document_ids_;
	std::map<int, std::map<std::string_view, double>> freqs_;

	bool IsStopWord(std::string_view word) const;

	static bool IsValidWord(std::string word);

	std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;

	static int ComputeAverageRating(const std::vector<int>& ratings);

	struct QueryWord {
		std::string_view data;
		bool is_minus;
		bool is_stop;
	};

	QueryWord ParseQueryWord(std::string_view text) const;

	struct Query {
		std::vector<std::string_view> plus_words;
		std::vector<std::string_view> minus_words;
	};

	Query ParseQuery(std::string_view text, bool sorted) const;

	double ComputeWordInverseDocumentFreq(const string_view word) const {
		return std::log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
	}

	template <typename DocumentPredicate>
	std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;
	template <typename DocumentPredicate, class ExecutionPolicy>
	std::vector<Document> FindAllDocuments(ExecutionPolicy policy, const Query& query, DocumentPredicate document_predicate) const;
};

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const {
	const auto query = ParseQuery(raw_query, true);
	auto matched_documents = FindAllDocuments(query, document_predicate);

	std::sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
		if (std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
			return lhs.rating > rhs.rating;
		} else {
			return lhs.relevance > rhs.relevance;
		}
	});
	if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
		matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
	}
	return matched_documents;
}

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
	: stop_words_(MakeUniqueNonEmptyStrings(stop_words))
{
	if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
		throw std::invalid_argument("Some of stop words are invalid"s);
	}
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
	std::map<int, double> document_to_relevance;
	for (const std::string_view word : query.plus_words) {
		if (word_to_document_freqs_.count(word) == 0) {
			continue;
		}
		const double inverse_document_freq = ComputeWordInverseDocumentFreq(string(word));
		for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
			const auto& document_data = documents_.at(document_id);
			if (document_predicate(document_id, document_data.status, document_data.rating)) {
				document_to_relevance[document_id] += term_freq * inverse_document_freq;
			}
		}
	}

	for (const string_view word : query.minus_words) {
		if (word_to_document_freqs_.count(word) == 0) {
			continue;
		}
		for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
			document_to_relevance.erase(document_id);
		}
	}

	std::vector<Document> matched_documents;
	for (const auto [document_id, relevance] : document_to_relevance) {
		matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
	}
	return matched_documents;
}

	template <typename DocumentPredicate, class ExecutionPolicy>
	std::vector<Document> SearchServer::FindAllDocuments(ExecutionPolicy policy, const Query& query, DocumentPredicate document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance(8);

    const auto checker = [&](std::string_view word) 
        { 
            if (word_to_document_freqs_.count(word) != 0) 
            { 
                const double inverse_document_freq = ComputeWordInverseDocumentFreq(word); 
                for (const auto& [document_id, term_freq] : (*word_to_document_freqs_.find(word)).second) 
                { 
                    const auto& document_data = documents_.at(document_id); 
                    if (document_predicate(document_id, document_data.status, document_data.rating) &&  
                        std::all_of(query.minus_words.begin(), query.minus_words.end(), [&](std::string_view minus_word) 
                            { 
                                return (freqs_.at(document_id).count(minus_word) == 0); 
                            })
                        ) 
                    { 
                        document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq; 
                    } 
                }
            }
        };
    
    std::for_each(policy, query.plus_words.begin(), query.plus_words.end(), checker);

    /*const auto plus_word_checker =
        [this, &document_predicate, &document_to_relevance](std::string_view word) {
        if (word_to_document_freqs_.count(word) == 0) {
            return;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id].ref_to_value += static_cast<double>(term_freq * inverse_document_freq);
            }
        }
    };
    for_each(policy, query.plus_words.begin(), query.plus_words.end(), plus_word_checker);

    const auto minus_word_checker =
        [this, &document_predicate, &document_to_relevance](std::string_view word) {
        if (word_to_document_freqs_.count(word) == 0) {
            return;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance[document_id].ref_to_value = 0;
        }
    };
    for_each(policy, query.minus_words.begin(), query.minus_words.end(), minus_word_checker);*/

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}
    

template <class ExecutionPolicy>
void SearchServer::RemoveDocument(ExecutionPolicy policy, int document_id) {
	if (documents_.count(document_id) == 0) {
		return;
	}
	std::map<std::string_view, double>& query = freqs_.at(document_id);
	vector<std::string_view> words(query.size());
	std::transform(policy, query.begin(), query.end(), words.begin(), 
	[](const auto& info) {
		return string_view(info.first);
	});

	std::for_each(policy, words.begin(), words.end(), [&](const auto& word) {
		word_to_document_freqs_.at(word.data()).erase(document_id);
	});

	documents_.erase(document_id);
	freqs_.erase(document_id);
	document_ids_.erase(document_id);
}

template <typename DocumentPredicate, class ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy policy, std::string_view raw_query, DocumentPredicate document_predicate) const {
	const auto query = ParseQuery(raw_query, true);
	auto matched_documents = FindAllDocuments(policy, query, document_predicate);

	std::sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
		if (std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
			return lhs.rating > rhs.rating;
		} else {
			return lhs.relevance > rhs.relevance;
		}
	});
	if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
		matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
	}

	return matched_documents;
}

template <class ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy policy, std::string_view raw_query, DocumentStatus status) const {
	return FindTopDocuments(policy, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
		return document_status == status;
	});
}

template <class ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy policy, std::string_view raw_query) const{
	return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}