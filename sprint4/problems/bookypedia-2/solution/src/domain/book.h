// === Файл: domain/book.h ===
#pragma once
#include <string>
#include <vector>
#include "../util/tagged_uuid.h"
#include "author_fwd.h"
#include "author.h"
namespace domain {

namespace detail {
struct BookTag {};
}  // namespace detail

using BookId = util::TaggedUUID<detail::BookTag>;

class Book {
public:
    Book(BookId id, AuthorId author_id, std::string title, int publication_year)
        : id_(std::move(id)), author_id_(std::move(author_id)), title_(std::move(title)), publication_year_(publication_year) {}

    const BookId& GetId() const noexcept { return id_; }
    const AuthorId& GetAuthorId() const noexcept { return author_id_; }
    const std::string& GetTitle() const noexcept { return title_; }
    int GetPublicationYear() const noexcept { return publication_year_; }

private:
    BookId id_;
    AuthorId author_id_;
    std::string title_;
    int publication_year_;
};
struct BookWithAuthor {
    BookId id;
    std::string title;
    std::string author_name;
    int publication_year;
};
class BookRepository {
public:
    virtual void Save(const Book& book) = 0;
    virtual std::vector<Book> GetAll() const = 0;
    virtual std::vector<Book> GetByAuthor(AuthorId author_id) const = 0;
    virtual std::vector<BookWithAuthor> GetAllWithAuthors() const = 0;
protected:
    ~BookRepository() = default;
};

class BookTag {
public:
    BookTag(BookId book_id, std::string tag)
    : book_id_(std::move(book_id))
    , tag_(std::move(tag)) {
}
    const BookId& GetBookId() const noexcept{
	return book_id_;
	}
    const std::string& GetTag() const noexcept{
	return tag_;
	}

private:
    BookId book_id_;
    std::string tag_;
};
class BookTagRepository {
public:
    virtual void Save(const BookTag& tag) = 0;
    virtual void DeleteByBookId(BookId book_id) = 0;
    virtual std::vector<std::string> GetTags(BookId book_id) const = 0;
    virtual ~BookTagRepository() = default;
};


}  // namespace domain
