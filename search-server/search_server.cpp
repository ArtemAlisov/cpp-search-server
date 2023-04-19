#include "search_server.h"

using namespace std;

SearchServer::SearchServer(string_view stop_words_text)
	: SearchServer(SplitIntoWords(stop_words_text))
{
}

SearchServer::SearchServer(const string& stop_words_text)
	: SearchServer(SplitIntoWords(stop_words_text))
{
}

void SearchServer::AddDocument(int document_id, string_view document, DocumentStatus status, const std::vector<int>& ratings) {
	if ((document_id < 0) || (documents_.count(document_id) > 0)) {
    	throw invalid_argument("Invalid document_id"s);
	}
	const auto[it, inserted] = documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status, document.data()});
	const std::vector<std::string_view> words = SplitIntoWordsNoStop(it->second.text);

	const double inv_word_count = 1.0 / words.size();
	for (string_view word : words) {
		word_to_document_freqs_[word][document_id] += inv_word_count;
		freqs_[document_id][word] += inv_word_count;
	}
	document_ids_.insert(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(string_view raw_query, DocumentStatus status) const {
	return SearchServer::FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
		return document_status == status;
	});
}

std::vector<Document> SearchServer::FindTopDocuments(string_view raw_query) const {
	return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

std::set<int>::const_iterator SearchServer::begin() const {
	return document_ids_.begin();
}

std::set<int>::const_iterator SearchServer::end() const {
	return document_ids_.end();
}

int SearchServer::GetDocumentCount() const {
	return documents_.size();
}

//не до конца понял что нужно делать со статической константой, да и в целом задачу по данному методу. Сделал как понял :-). Наверняка ее еще нужно вынести из класса :-)
const std::map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
	static const map<string_view, double> empty_map= {};
	if (freqs_.count(document_id) != 0) {
		return freqs_.at(document_id);
	}
	return empty_map;
}

void SearchServer::RemoveDocument(int document_id) {
	RemoveDocument(std::execution::seq, document_id);
}

using MatchedDocuments = std::tuple<std::vector<std::string_view>, DocumentStatus>;

MatchedDocuments SearchServer::MatchDocument(string_view raw_query, int document_id) const {
	if (documents_.count(document_id) == 0) {
		throw std::out_of_range("");
	}
	const auto query = ParseQuery(raw_query, true);
	vector<string_view> matched_words(query.plus_words.size());
	
	auto checker = [&] (const string_view word) {return freqs_.at(document_id).count(word);};
	if (std::any_of(query.minus_words.begin(), query.minus_words.end(),checker)) {
		return { {}, documents_.at(document_id).status};
	}
	
	auto iter_end = std::copy_if(query.plus_words.begin(), query.plus_words.end(), matched_words.begin(), checker);
	matched_words.resize(distance(matched_words.begin(), iter_end));
  
	return {matched_words, documents_.at(document_id).status};
}

MatchedDocuments SearchServer::MatchDocument(
	std::execution::sequenced_policy seq, const std::string_view raw_query, int document_id) const {
		return MatchDocument(raw_query, document_id);
	}

MatchedDocuments SearchServer::MatchDocument(std::execution::parallel_policy par, const std::string_view raw_query, int document_id) const {
	if (documents_.count(document_id) == 0) {
		throw std::out_of_range("");
	}
	const auto query = ParseQuery(raw_query, false);
	vector<string_view> matched_words(query.plus_words.size());
	
	auto checker = [&] (const string_view word) {return freqs_.at(document_id).count(word);};
	if (std::any_of(query.minus_words.begin(), query.minus_words.end(),checker)) {
		return { {}, documents_.at(document_id).status};
	}
	
	auto iter_end = std::copy_if(query.plus_words.begin(), query.plus_words.end(), matched_words.begin(), checker);
	matched_words.resize(distance(matched_words.begin(), iter_end));
	
	sort(execution::par, matched_words.begin(), matched_words.end());
	auto last = unique(matched_words.begin(), matched_words.end());
	matched_words.resize(distance(matched_words.begin(), last));
	
	return {matched_words, documents_.at(document_id).status};
}

bool SearchServer::IsStopWord(string_view word) const {
	return stop_words_.count(word) > 0;
}

std::vector<string_view> SearchServer::SplitIntoWordsNoStop(string_view text) const {
	std::vector<string_view> words;
	for (const string_view word : SplitIntoWords(text)) {
		if (!IsValidWord(string(word))) {
			throw invalid_argument("Word "s + string(word) + " is invalid"s);
		}
		if (!IsStopWord(word)) {
			words.push_back(word);
		}
	}
	return words;
}

bool SearchServer::IsValidWord(const string word) {
	return none_of(word.begin(), word.end(), [](char c) {
		return c >= '\0' && c < ' ';
	});
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
	if (ratings.empty()) {
		return 0;
	}
	int rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);
	return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view text) const {
	if (text.empty()) {
		throw invalid_argument("Query word is empty"s);
	}
	bool is_minus = false;
	auto word = text;
	if (word[0] == '-') {
		is_minus = true;
		word = word.substr(1);
	}
	if (word.empty() || word[0] == '-' || !IsValidWord(string(word))) {
		throw invalid_argument("Query word "s + string(word) + " is invalid");
	}

	return {word, is_minus, IsStopWord(text)};
}

SearchServer::Query SearchServer::ParseQuery(string_view text, bool sorted) const {
	Query result;
	vector<string_view> words = SplitIntoWords(text);
    
	if(sorted) {
		sort(execution::par, words.begin(), words.end());
		auto last = unique(words.begin(), words.end());
		words.resize(distance(words.begin(), last));
	}
    
	for_each(words.begin(), words.end(), [&](auto& word) {
		const auto query_word = ParseQueryWord(word);
		if (!query_word.is_stop) {
			if (query_word.is_minus) {
				result.minus_words.push_back(query_word.data);
			} else {
				result.plus_words.push_back(query_word.data);
			}
		}
	});
    
	return result;
}


