#include "use_cases_impl.h"

#include "../domain/author.h"
#include "../domain/book.h"
namespace app {
using namespace domain;

void UseCasesImpl::AddAuthor(const std::string& name) {
    authors_.Save({AuthorId::New(), name});
}
std::vector<ui::detail::AuthorInfo> UseCasesImpl::GetAuthors() const {
    std::vector<ui::detail::AuthorInfo> result;
    for (const auto& author : authors_.GetAll()) {
	ui::detail::AuthorInfo info;
	info.id = author.GetId().ToString();
	info.name = author.GetName();
	result.push_back(std::move(info));

    }
    return result;
}
void UseCasesImpl::AddBook(std::string title, std::string author_id, int publication_year) {
    domain::Book book{
        domain::BookId::New(),
        domain::AuthorId::FromString(author_id),
        std::move(title),
        publication_year
    };
    books_.Save(book);
}

std::vector<ui::detail::BookInfo> UseCasesImpl::GetBooks() const {
    std::vector<ui::detail::BookInfo> result;
    for (const auto& book : books_.GetAll()) {
        result.push_back({.title = book.GetTitle(), .publication_year = book.GetPublicationYear()});
    }
    return result;
}

std::vector<ui::detail::BookInfo> UseCasesImpl::GetAuthorBooks(const std::string& author_id) const {
    std::vector<ui::detail::BookInfo> result;
    domain::AuthorId id = domain::AuthorId::FromString(author_id);
    for (const auto& book : books_.GetByAuthor(id)) {
        result.push_back({.title = book.GetTitle(), .publication_year = book.GetPublicationYear()});
    }
    return result;
}
}  // namespace app
