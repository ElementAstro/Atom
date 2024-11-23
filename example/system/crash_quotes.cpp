#include "atom/system/crash_quotes.hpp"

#include <iostream>

using namespace atom::system;

int main() {
    // Create a QuoteManager object
    QuoteManager manager;

    // Add quotes to the collection
    manager.addQuote(Quote(
        "The only limit to our realization of tomorrow is our doubts of today.",
        "Franklin D. Roosevelt"));
    manager.addQuote(Quote("In the middle of difficulty lies opportunity.",
                           "Albert Einstein"));
    manager.addQuote(
        Quote("Success is not final, failure is not fatal: It is the courage "
              "to continue that counts.",
              "Winston Churchill"));

    // Search for quotes containing a keyword
    auto searchResults = manager.searchQuotes("opportunity");
    std::cout << "Search results for 'opportunity':" << std::endl;
    for (const auto& quote : searchResults) {
        std::cout << quote.getText() << " - " << quote.getAuthor() << std::endl;
    }

    // Filter quotes by author
    auto filteredQuotes = manager.filterQuotesByAuthor("Albert Einstein");
    std::cout << "Quotes by Albert Einstein:" << std::endl;
    for (const auto& quote : filteredQuotes) {
        std::cout << quote.getText() << " - " << quote.getAuthor() << std::endl;
    }

    // Get a random quote from the collection
    std::string randomQuote = manager.getRandomQuote();
    std::cout << "Random quote: " << randomQuote << std::endl;

    // Shuffle the quotes in the collection
    manager.shuffleQuotes();
    std::cout << "Quotes after shuffling:" << std::endl;
#ifdef DEBUG
    manager.displayQuotes();
#endif

    // Clear all quotes in the collection
    manager.clearQuotes();
    std::cout << "Quotes after clearing:" << std::endl;
#ifdef DEBUG
    manager.displayQuotes();
#endif

    // Load quotes from a JSON file
    manager.loadQuotesFromJson("quotes.json");

    // Save quotes to a JSON file
    manager.saveQuotesToJson("saved_quotes.json");

    return 0;
}