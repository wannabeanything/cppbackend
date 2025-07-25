#pragma once

#include <string>
#include <vector>
namespace app {

class UseCases {
public:
    virtual void AddAuthor(const std::string& name) = 0;

    virtual void AddBook(std::string title, std::string author_id, int publication_year) = 0;


    virtual std::vector<ui::detail::AuthorInfo> GetAuthors() const = 0;


    virtual std::vector<ui::detail::BookInfo> GetBooks() const = 0;


    virtual std::vector<ui::detail::BookInfo> GetAuthorBooks(const std::string& author_id) const = 0;

protected:
    ~UseCases() = default;
};

}  // namespace app
