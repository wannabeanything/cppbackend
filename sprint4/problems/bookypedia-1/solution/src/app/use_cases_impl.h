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
    explicit UseCasesImpl(domain::AuthorRepository& authors, domain::BookRepository& books)
        : authors_{authors}, books_{books} {
    }

    void AddAuthor(const std::string& name) override;
    void AddBook(std::string title, std::string author_id, int publication_year) override;
    std::vector<ui::detail::AuthorInfo> GetAuthors() const override;
    std::vector<ui::detail::BookInfo> GetBooks() const override;
    std::vector<ui::detail::BookInfo> GetAuthorBooks(const std::string& author_id) const override;

private:
    domain::AuthorRepository& authors_;
    domain::BookRepository& books_;
};

}  // namespace app
