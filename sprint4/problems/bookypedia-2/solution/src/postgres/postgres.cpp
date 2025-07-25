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
bool AuthorRepositoryImpl::DeleteById(domain::AuthorId id) {
    pqxx::work work{connection_};
    auto result = work.exec_params(
        "DELETE FROM authors WHERE id = $1",
        id.ToString()
    );
    bool deleted = result.affected_rows() > 0;
    work.commit();
    return deleted;
}

bool AuthorRepositoryImpl::DeleteByName(const std::string& name) {
    pqxx::work work{connection_};
    auto result = work.exec_params(
        "DELETE FROM authors WHERE name = $1",
        name
    );
    bool deleted = result.affected_rows() > 0;
    work.commit();
    return deleted;
}
bool AuthorRepositoryImpl::UpdateName(domain::AuthorId id, const std::string& new_name) {
    pqxx::work work{connection_};
    auto result = work.exec_params(
        "UPDATE authors SET name = $1 WHERE id = $2",
        new_name, id.ToString()
    );
    bool updated = result.affected_rows() > 0;
    work.commit();
    return updated;
}

std::optional<domain::AuthorId> AuthorRepositoryImpl::FindIdByName(const std::string& name) const {
    pqxx::read_transaction tx{connection_};
    auto result = tx.exec_params(
        "SELECT id FROM authors WHERE name = $1 LIMIT 1",
        name
    );
    if (!result.empty()) {
        return domain::AuthorId::FromString(result[0][0].as<std::string>());
    }
    return std::nullopt;
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
void BookTagRepositoryImpl::Save(const domain::BookTag& tag) {
    pqxx::work work{connection_};
    work.exec_params(
        "INSERT INTO book_tags (book_id, tag) VALUES ($1, $2)",
        tag.GetBookId().ToString(),
        tag.GetTag()
    );
    work.commit();
}

void BookTagRepositoryImpl::DeleteByBookId(domain::BookId book_id) {
    pqxx::work work{connection_};
    work.exec_params(
        "DELETE FROM book_tags WHERE book_id = $1",
        book_id.ToString()
    );
    work.commit();
}

std::vector<std::string> BookTagRepositoryImpl::GetTags(domain::BookId book_id) const {
    pqxx::read_transaction tx{connection_};
    std::vector<std::string> tags;

    auto result = tx.exec_params(
        "SELECT tag FROM book_tags WHERE book_id = $1 ORDER BY tag ASC",
        book_id.ToString()
    );

    for (const auto& row : result) {
        tags.push_back(row[0].as<std::string>());
    }

    return tags;
}
std::vector<domain::BookWithAuthor> BookRepositoryImpl::GetAllWithAuthors() const {
    pqxx::read_transaction tx{connection_};
    std::vector<domain::BookWithAuthor> books;

    for (auto [id, title, author, year] : tx.query<domain::BookId, std::string, std::string, int>(
            "SELECT b.id, b.title, a.name, b.publication_year "
            "FROM books b JOIN authors a ON b.author_id = a.id "
            "ORDER BY b.title, a.name, b.publication_year")) {
        books.push_back({id, title, author, year});
    }

    return books;
}

Database::Database(pqxx::connection connection)
    : connection_{std::move(connection)},
     authors_{connection_}
    , books_{connection_}
    , book_tags_{connection_}  {
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
    work.exec(R"(
CREATE TABLE IF NOT EXISTS book_tags (
    book_id UUID NOT NULL REFERENCES books(id),
    tag VARCHAR(30) NOT NULL
);
)"_zv);


    // коммитим изменения
    work.commit();
}

}  // namespace postgres
