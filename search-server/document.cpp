#include "document.h"

#include <iostream>

using namespace std;

std::ostream& operator<<(std::ostream& out, const Document& document) {
    out << "{ "
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s;
    return out;
}

