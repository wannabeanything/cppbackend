#pragma once
#include <pqxx/connection>
#include <pqxx/transaction>

#include "../domain/author.h"
#include "../domain/book.h"
namespace postgres {

class AuthorRepositoryImpl : public domain::AuthorRepository {
public:
    explicit AuthorRepositoryImpl(pqxx::connection& connection)
        : connection_{connection} {
    }

    void Save(const domain::Author& author) override;
    std::vector<domain::Author> GetAll() const override;
    bool DeleteById(domain::AuthorId id) override;
    bool DeleteByName(const std::string& name) override;
    bool UpdateName(domain::AuthorId id, const std::string& new_name) override;
    std::optional<domain::AuthorId> FindIdByName(const std::string& name) const override;
private:
    pqxx::connection& connection_;
};
class BookRepositoryImpl : public domain::BookRepository {
public:
    explicit BookRepositoryImpl(pqxx::connection& connection) : connection_{connection} {}

    void Save(const domain::Book& book) override;
    std::vector<domain::Book> GetAll() const override;
    std::vector<domain::Book> GetByAuthor(domain::AuthorId author_id) const override;
    std::vector<domain::BookWithAuthor> GetAllWithAuthors() const override;
private:
    pqxx::connection& connection_;
};
class BookTagRepositoryImpl : public domain::BookTagRepository {
public:
    explicit BookTagRepositoryImpl(pqxx::connection& connection)
        : connection_{connection} {}

    void Save(const domain::BookTag& tag) override;
    void DeleteByBookId(domain::BookId book_id) override;
    std::vector<std::string> GetTags(domain::BookId book_id) const override;

private:
    pqxx::connection& connection_;
};

class Database {
public:
    explicit Database(pqxx::connection connection);

    AuthorRepositoryImpl& GetAuthors() & { return authors_; }
    BookRepositoryImpl& GetBooks() & { return books_; }
	
    domain::BookTagRepository& GetBookTags() {
        return book_tags_;
    }

private:
    pqxx::connection connection_;
    AuthorRepositoryImpl authors_{connection_};
    BookRepositoryImpl books_{connection_};
    BookTagRepositoryImpl book_tags_;
};

}  // namespace postgres
