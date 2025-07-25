#include <boost/json.hpp>
#include <pqxx/pqxx>
#include <iostream>
#include <string>
#include <optional>

namespace json = boost::json;

void EnsureTable(pqxx::connection& conn) {
    pqxx::work txn(conn);
    txn.exec(R"(
        CREATE TABLE IF NOT EXISTS books (
            id SERIAL PRIMARY KEY,
            title VARCHAR(100) NOT NULL,
            author VARCHAR(100) NOT NULL,
            year INTEGER NOT NULL,
            ISBN CHAR(13) UNIQUE
        )
    )");
    txn.commit();
}

bool AddBook(pqxx::connection& conn, const json::object& payload) {
    try {
        pqxx::work txn(conn);
        const auto& title = payload.at("title").as_string();
        const auto& author = payload.at("author").as_string();
        int year = static_cast<int>(payload.at("year").as_int64());

        if (payload.at("ISBN").is_null()) {
            txn.exec_params(
                "INSERT INTO books (title, author, year, ISBN) VALUES ($1, $2, $3, NULL)",
                std::string(title), std::string(author), year
            );
        } else {
            const auto& isbn = payload.at("ISBN").as_string();
            txn.exec_params(
                "INSERT INTO books (title, author, year, ISBN) VALUES ($1, $2, $3, $4)",
                std::string(title), std::string(author), year, std::string(isbn)
            );
        }

        txn.commit();
        return true;
    } catch (const pqxx::sql_error&) {
        return false;
    }
}

json::array GetAllBooks(pqxx::connection& conn) {
    pqxx::read_transaction txn(conn);
    auto res = txn.exec(R"(
        SELECT id, title, author, year, ISBN
        FROM books
        ORDER BY year DESC, title ASC, author ASC, ISBN ASC NULLS LAST
    )");

    json::array books;
    for (const auto& row : res) {
        json::object book;
        book["id"] = row["id"].as<int>();
        book["title"] = row["title"].as<std::string>();
        book["author"] = row["author"].as<std::string>();
        book["year"] = row["year"].as<int>();
        if (row["ISBN"].is_null())
            book["ISBN"] = nullptr;
        else
            book["ISBN"] = row["ISBN"].as<std::string>();
        books.push_back(book);
    }

    return books;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: book_manager <db_connection_string>\n";
        return 1;
    }

    try {
        pqxx::connection conn(argv[1]);
        EnsureTable(conn);

        std::string line;
        while (std::getline(std::cin, line)) {
            json::value parsed = json::parse(line);
            const json::object& obj = parsed.as_object();
	    const std::string action = std::string(obj.at("action").as_string());

            const json::object& payload = obj.at("payload").as_object();

            if (action == "add_book") {
                bool success = AddBook(conn, payload);
                json::object result;
                result["result"] = success;
                std::cout << json::serialize(result) << std::endl;
            } else if (action == "all_books") {
                json::array result = GetAllBooks(conn);
                std::cout << json::serialize(result) << std::endl;
            } else if (action == "exit") {
                break;
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
