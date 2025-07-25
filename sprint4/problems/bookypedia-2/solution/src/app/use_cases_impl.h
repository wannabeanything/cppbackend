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
    std::vector<domain::BookWithAuthor> GetAuthorBooks(const std::string& author_id) const override;
    void AddBook(std::string title, std::string author_id, int publication_year,
                 std::vector<std::string> tags) override;
    std::vector<std::string> GetBookTags(std::string book_id) const override;
    bool DeleteAuthorById(std::string& id) override;
    bool DeleteAuthorByName(const std::string& name);
    bool EditAuthor(const std::string& id, const std::string& new_name);
    std::optional<std::string> FindAuthorIdByName(const std::string& name) const;
    std::vector<domain::BookWithAuthor> GetBooksWithAuthors() const override;

private:
    domain::AuthorRepository& authors_;
    domain::BookRepository& books_;
    domain::BookTagRepository& book_tags_;
};

}  // namespace app
