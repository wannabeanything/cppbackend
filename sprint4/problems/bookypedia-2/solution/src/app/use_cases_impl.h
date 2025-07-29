#pragma once
#include "../domain/author_fwd.h"
#include "../domain/author.h"
#include "../domain/book.h"
#include <vector>
#include "../ui/view.h"
#include "use_cases.h"

namespace app {

class UseCasesImpl : public UseCases {
public:
    UseCasesImpl(domain::AuthorRepository& authors,
                 domain::BookRepository& books,
                 domain::BookTagRepository& book_tags)
        : authors_{authors}, books_{books}, book_tags_{book_tags} {}
    void AddAuthor(const std::string& name) override;
    std::vector<ui::detail::AuthorInfo> GetAuthors() const override;
    std::vector<ui::detail::BookInfo> GetBooks() const override;
    std::vector<ui::detail::BookInfo> GetAuthorBooks(const std::string& author_id) const override;
    
    void AddBook(std::string title, std::string author_id, int publication_year,
                 std::vector<std::string> tags) override;
    virtual std::vector<std::string> GetBookTags(const domain::BookId& book_id) const override;
    bool DeleteAuthorById(const std::string& id) override;
    bool DeleteAuthorByName(const std::string& name);
    bool EditAuthor(const std::string& id, const std::string& new_name);
    bool DeleteBook(const domain::BookId& id) override;

    bool EditBook(const domain::BookId& id, const std::string& title, int year,
              const std::vector<std::string>& tags) override;
    std::optional<std::string> FindAuthorIdByName(const std::string& name) const;
    std::vector<domain::BookWithAuthor> GetBooksWithAuthors() const override;
    std::vector<domain::BookWithAuthor> FindBooksByTitle(const std::string& title) const override;

    
private:
    domain::AuthorRepository& authors_;
    domain::BookRepository& books_;
    domain::BookTagRepository& book_tags_;
};

}  // namespace app
