#include "postgres.h"
#include <pqxx/result>
#include <pqxx/zview.hxx>

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

void AuthorRepositoryImpl::Save(const domain::Author& author) {
    // Пока каждое обращение к репозиторию выполняется внутри отдельной транзакции
    // В будущих уроках вы узнаете про паттерн Unit of Work, при помощи которого сможете несколько
    // запросов выполнить в рамках одной транзакции.
    // Вы также может самостоятельно почитать информацию про этот паттерн и применить его здесь.
    pqxx::work work{connection_};
    work.exec_params(
        R"(
INSERT INTO authors (id, name) VALUES ($1, $2)
ON CONFLICT (id) DO UPDATE SET name=$2;
)"_zv,
        author.GetId().ToString(), author.GetName());
    work.commit();
}
std::vector<domain::Author> AuthorRepositoryImpl::GetAll() const{
    pqxx::read_transaction tx{connection_};
    std::vector<domain::Author> result;

    for (auto [id, name] : tx.query<std::string, std::string>(
             "SELECT id, name FROM authors ORDER BY name ASC")) {
        result.emplace_back(domain::AuthorId::FromString(id), std::move(name));
    }

    return result;
}
void BookRepositoryImpl::Save(const domain::Book& book) {
    pqxx::work work{connection_};
    work.exec_params(
        R"sql(
INSERT INTO books (id, author_id, title, publication_year)
VALUES ($1, $2, $3, $4)
)sql",
        book.GetId().ToString(),
        book.GetAuthorId().ToString(),
        book.GetTitle(),
        book.GetPublicationYear()
    );
    work.commit();
}

std::vector<domain::Book> BookRepositoryImpl::GetAll() const {
    pqxx::read_transaction tx{connection_};
    std::vector<domain::Book> result;
    for (auto [id, author_id, title, year] :
         tx.query<std::string, std::string, std::string, int>(
             "SELECT id, author_id, title, publication_year FROM books ORDER BY title ASC")) {
        result.emplace_back(domain::BookId::FromString(id),
                            domain::AuthorId::FromString(author_id),
                            std::move(title), year);
    }
    return result;
}
std::vector<domain::Book> BookRepositoryImpl::GetByAuthor(domain::AuthorId author_id) const {
    pqxx::read_transaction tx{connection_, "get_books_by_author"};

    std::vector<domain::Book> books;

    auto rows = tx.exec_params(
        R"sql(
SELECT id, title, publication_year 
FROM books 
WHERE author_id = $1 
ORDER BY publication_year ASC, title ASC
)sql",
        author_id.ToString()
    );

    for (const auto& row : rows) {
        const auto id = domain::BookId::FromString(row[0].as<std::string>());
        const auto title = row[1].as<std::string>();
        const auto year = row[2].as<int>();
        books.emplace_back(id, author_id, title, year);
    }

    return books;
}

Database::Database(pqxx::connection connection)
    : connection_{std::move(connection)} {
    pqxx::work work{connection_};
    work.exec(R"(
CREATE TABLE IF NOT EXISTS authors (
    id UUID CONSTRAINT author_id_constraint PRIMARY KEY,
    name varchar(100) UNIQUE NOT NULL
);
)"_zv);
    work.exec(R"(
CREATE TABLE IF NOT EXISTS books (
    id UUID CONSTRAINT book_id_constraint PRIMARY KEY,
    author_id UUID NOT NULL REFERENCES authors(id),
    title varchar(100) NOT NULL,
    publication_year INTEGER
);
)"_zv);


    // коммитим изменения
    work.commit();
}

}  // namespace postgres
