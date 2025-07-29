#pragma once
#include "../domain/book.h"

#include <string>
#include <vector>
#include <optional>
namespace app {

class UseCases {
public:
    virtual void AddAuthor(const std::string& name) = 0;

    virtual void AddBook(std::string title, std::string author_id, int publication_year,
                     std::vector<std::string> tags) = 0;
    virtual std::vector<ui::detail::AuthorInfo> GetAuthors() const = 0;


    virtual std::vector<ui::detail::BookInfo> GetBooks() const = 0;
    virtual std::vector<std::string> GetBookTags(const domain::BookId& book_id) const = 0;
    virtual std::vector<domain::BookWithAuthor> FindBooksByTitle(const std::string& title) const = 0;
    virtual bool DeleteBook(const domain::BookId& id) = 0;
    virtual bool EditBook(const domain::BookId& id, const std::string& title, int year, const std::vector<std::string>& tags) = 0;
    virtual std::vector<ui::detail::BookInfo> GetAuthorBooks(const std::string& author_id) const = 0;
    virtual bool DeleteAuthorById(const std::string& id) = 0;
    virtual bool DeleteAuthorByName(const std::string& name) = 0;
    virtual bool EditAuthor(const std::string& id, const std::string& new_name) = 0;
    virtual std::optional<std::string> FindAuthorIdByName(const std::string& name) const = 0;
    virtual std::vector<domain::BookWithAuthor> GetBooksWithAuthors() const = 0;

protected:
    ~UseCases() = default;
};

}  // namespace app
