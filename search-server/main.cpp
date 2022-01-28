const double EPSILON = 1e-6;

void TestExcludeStopWords() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT(found_docs.size() == 1);
        const Document& doc0 = found_docs[0];
        ASSERT(doc0.id == doc_id);
    }
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }
}

void TestExcludeMinusWords() {
	const int doc_id = 41;
	const int doc_add_id = 40;
    const string content = "bat on the house"s;
    const string content_add = "father on the top"s;
    const vector<int> ratings = {2, 3, 4, 5};
    const vector<int> ratings_add = {1, 2, 3, 4};
    {
        SearchServer server;
        server.SetStopWords("in"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("-house the"s).empty());
        server.AddDocument(doc_add_id, content_add, DocumentStatus::ACTUAL, ratings_add);
        const auto found_docs = server.FindTopDocuments("-house the"s);
        ASSERT(found_docs.size() == 1);
    }
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto [document_add, ids_add] = server.MatchDocument("-house the"s, doc_id);
        ASSERT(document_add.empty());
        const auto [document, ids] = server.MatchDocument("the house"s, doc_id);
        vector<string> response = {"house"s, "the"s};
        ASSERT_HINT(document == response, "Response from MatchDocument doesn't contain all matching query words");
    }
}

void TestInternalParts() {

	{
		SearchServer server;
		server.SetStopWords("и в на"s);
		server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
		server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
		server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
	    server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });
		const vector<Document>& data = server.FindTopDocuments("пушистый ухоженный кот"s);
		ASSERT_HINT(data.at(0).id == 1, "Incorrect order by relevance");
		ASSERT_HINT(data.at(1).id == 0, "Incorrect order by relevance");
		ASSERT_HINT(data.at(2).id == 2, "Incorrect order by relevance");
		ASSERT_HINT(data.at(0).rating == 5, "Incorrect rating calculation");
		ASSERT_HINT(data.at(0).relevance < 0.866434 + EPSILON && data.at(0).relevance > 0.866434 - EPSILON, "Incorrect relevance calculation");
		ASSERT_HINT(server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED).at(0).id == 3, "Incorrect filtration by status");
	    const auto data_1 = server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
	    ASSERT_HINT(data_1.at(0).id == 0, "Incorrect search by predicate");
	}
}

void TestSearchServer() {
    TestExcludeStopWords();
    TestExcludeMinusWords();
    TestInternalParts();
}
